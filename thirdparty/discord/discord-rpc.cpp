#define RAPIDJSON_SSE2

#include "discord-rpc.h"

#pragma warning(push)
#pragma warning(disable: 4100 4244)
#include "tier0/vprof.h"

#include "backoff.h"
#include "msg_queue.h"
#include "rpc_connection.h"
#include "serialization.h"

#include <atomic>

static constexpr size_t MaxMessageSize = 16 * 1024;
static constexpr size_t MessageQueueSize = 8;
static constexpr size_t JoinQueueSize = 8;

struct QueuedMessage
{
	size_t length;
	char buffer[MaxMessageSize];

	void Copy( const QueuedMessage& other )
	{
		length = other.length;
		if ( length )
			memcpy( buffer, other.buffer, length );
	}
};

struct User
{
	// snowflake (64bit int), turned into a ascii decimal string, at most 20 chars +1 null
	// terminator = 21
	char userId[32];
	// 32 unicode glyphs is max name size => 4 bytes per glyph in the worst case, +1 for null
	// terminator = 129
	char username[344];
    // 4 decimal digits + 1 null terminator = 5
    char discriminator[8];
	// optional 'a_' + md5 hex digest (32 bytes) + null terminator = 35
	char avatar[128];
	// +1 on each because: it's even / I'm paranoid
};

static RpcConnection* Connection = nullptr;
static Discord::EventHandlers QueuedHandlers;
static Discord::EventHandlers Handlers;
static std::atomic_bool WasJustConnected {false};
static std::atomic_bool WasJustDisconnected {false};
static std::atomic_bool GotErrorMessage {false};
static std::atomic_bool WasJoinGame {false};
static std::atomic_bool WasSpectateGame {false};
static std::atomic_bool UpdatePresence {false};
static char JoinGameSecret[256];
static char SpectateGameSecret[256];
static int LastErrorCode = 0;
static char LastErrorMessage[256];
static int LastDisconnectErrorCode = 0;
static char LastDisconnectErrorMessage[256];
static CThreadMutex PresenceMutex;
static CThreadMutex HandlerMutex;
static QueuedMessage QueuedPresence;
static MsgQueue<QueuedMessage, MessageQueueSize> SendQueue;
static MsgQueue<User, JoinQueueSize> JoinAskQueue;
static User ConnectedUser;

// We want to auto connect, and retry on failure, but not as fast as possible. This does expoential
// backoff from 0.5 seconds to 1 minute
static Backoff ReconnectTimeMs( 500, 60 * 1000 );
static unsigned int NextConnect = 0;
static int Pid = 0;
static int Nonce = 1;

#ifdef OSX
typedef struct cpu_set
{
	uint32_t    count;
} cpu_set_t;
static inline void CPU_ZERO( cpu_set_t* cs ) { cs->count = 0; }
static inline void CPU_SET( int num, cpu_set_t* cs ) { cs->count |= ( 1 << num ); }
static inline int CPU_ISSET( int num, cpu_set_t* cs ) { return ( cs->count & ( 1 << num ) ); }
int pthread_setaffinity_np( pthread_t thread, size_t cpu_size, cpu_set_t* cpu_set )
{
	thread_port_t mach_thread;
	int core = 0;

	for ( core = 0; core < 8 * cpu_size; core++ )
	{
		if ( CPU_ISSET( core, cpu_set ) ) break;
	}
	thread_affinity_policy_data_t policy = { core };
	mach_thread = pthread_mach_thread_np( thread );
	thread_policy_set( mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1 );
	return 0;
}
#endif

static FORCEINLINE void UpdateReconnectTime()
{
	NextConnect = Plat_MSTime() + ReconnectTimeMs.nextDelay();
}

static class DiscordUpdateWorker : public CThread
{
public:
	DiscordUpdateWorker()
	{
		m_bRun = false;
		SetName( "DiscordIOThread" );
		SetPriority( -1 );
	}

	int Run() override
	{
		{
			const CPUInformation& info = *GetCPUInformation();
			ThreadSetAffinity( GetThreadID(), 1 << ( info.m_nLogicalProcessors - 1 ) );
		}

		DevMsg( "Starting Discord IO thread...\n" );
		m_bRun = true;
		while ( m_bRun )
		{
			if ( !Connection )
				goto sleep;

			if ( !Connection->IsOpen() )
			{
				if ( Plat_MSTime() >= NextConnect )
				{
					UpdateReconnectTime();
					Connection->Open();
				}
			}
			else
			{
				// reads
				for ( ;; )
				{
					JsonDocument message;
					if ( !Connection->Read( message ) )
						break;

					const char* evtName = GetStrMember( &message, "evt" );
					const char* nonce = GetStrMember( &message, "nonce" );

					if ( nonce )
					{
						// in responses only -- should use to match up response when needed.
						if ( evtName && strcmp( evtName, "ERROR" ) == 0 )
						{
							JsonValue* data = GetObjMember( &message, "data" );
							LastErrorCode = GetIntMember( data, "code" );
							StringCopy( LastErrorMessage, GetStrMember( data, "message", "" ) );
							GotErrorMessage.store( true );
						}
					}
					else
					{
						// should have evt == name of event, optional data
						if ( evtName == nullptr )
							continue;

						JsonValue* data = GetObjMember( &message, "data" );
						if ( strcmp( evtName, "ACTIVITY_JOIN" ) == 0 )
						{
							const char* secret = GetStrMember( data, "secret" );
							if ( secret )
							{
								StringCopy( JoinGameSecret, secret );
								WasJoinGame.store( true );
							}
						}
						else if ( strcmp( evtName, "ACTIVITY_SPECTATE" ) == 0 )
						{
							const char* secret = GetStrMember( data, "secret" );
							if ( secret )
							{
								StringCopy( SpectateGameSecret, secret );
								WasSpectateGame.store( true );
							}
						}
						else if ( strcmp( evtName, "ACTIVITY_JOIN_REQUEST" ) == 0 )
						{
							JsonValue* user = GetObjMember( data, "user" );
							const char* userId = GetStrMember( user, "id" );
							const char* username = GetStrMember( user, "username" );
							const char* avatar = GetStrMember( user, "avatar" );
							User* joinReq = JoinAskQueue.GetNextAddMessage();
							if ( userId && username && joinReq )
							{
								StringCopy( joinReq->userId, userId );
								StringCopy( joinReq->username, username );
								const char* discriminator = GetStrMember( user, "discriminator" );
								if ( discriminator )
									StringCopy( joinReq->discriminator, discriminator );
								if ( avatar )
									StringCopy( joinReq->avatar, avatar );
								else
									joinReq->avatar[0] = 0;
								JoinAskQueue.CommitAdd();
							}
						}
					}
				}

				// writes
				if ( UpdatePresence.exchange( false ) && QueuedPresence.length )
				{
					QueuedMessage local;
					{
						AUTO_LOCK( PresenceMutex );
						local.Copy( QueuedPresence );
					}
					if ( !Connection->Write( local.buffer, local.length ) )
					{
						AUTO_LOCK( PresenceMutex );
						// if we fail to send, requeue
						QueuedPresence.Copy( local );
						UpdatePresence.exchange( true );
					}
				}

				while ( SendQueue.HavePendingSends() )
				{
					const QueuedMessage* qmessage = SendQueue.GetNextSendMessage();
					Connection->Write( qmessage->buffer, qmessage->length );
					SendQueue.CommitSend();
				}
			}

		sleep:
			Sleep( 2500 );
			ThreadPause();
		}
		DevMsg( "Stopped Discord IO thread.\n" );
		return 0;
	}
	CInterlockedUInt m_bRun;
} s_update_worker;

static void StopDiscordUpdateThread()
{
	DevMsg( "Stopping Discord IO thread...\n" );
	s_update_worker.m_bRun = false;
	s_update_worker.Join();
}

static bool RegisterForEvent( const char* evtName )
{
	if ( QueuedMessage* qmessage = SendQueue.GetNextAddMessage() )
	{
		qmessage->length = JsonWriteSubscribeCommand( qmessage->buffer, sizeof( qmessage->buffer ), Nonce++, evtName );
		SendQueue.CommitAdd();
		return true;
	}
	return false;
}

static bool DeregisterForEvent( const char* evtName )
{
	if ( QueuedMessage* qmessage = SendQueue.GetNextAddMessage() )
	{
		qmessage->length = JsonWriteUnsubscribeCommand( qmessage->buffer, sizeof( qmessage->buffer ), Nonce++, evtName );
		SendQueue.CommitAdd();
		return true;
	}
	return false;
}

static void Discord_UpdateHandlers( const Discord::EventHandlers& newHandlers )
{
#define HANDLE_EVENT_REGISTRATION( handler_name, event ) \
    if ( !Handlers.handler_name && newHandlers.handler_name ) \
        RegisterForEvent( event ); \
    else if ( Handlers.handler_name && !newHandlers.handler_name ) \
        DeregisterForEvent( event );

	AUTO_LOCK( HandlerMutex );
	HANDLE_EVENT_REGISTRATION( joinGame, "ACTIVITY_JOIN" )
	HANDLE_EVENT_REGISTRATION( spectateGame, "ACTIVITY_SPECTATE" )
	HANDLE_EVENT_REGISTRATION( joinRequest, "ACTIVITY_JOIN_REQUEST" )

#undef HANDLE_EVENT_REGISTRATION

	Handlers = newHandlers;
}

void Discord::Initialize( const char* applicationId,
						  Discord::EventHandlers& handlers )
{
	if ( Connection )
		return;

	Pid = GetProcessId();

	{
		AUTO_LOCK( HandlerMutex );
		QueuedHandlers = handlers;
		Handlers = {};
	}

	Connection = RpcConnection::Create( applicationId );
	Connection->onConnect = []( JsonDocument& readyMessage ) {
		Discord_UpdateHandlers( QueuedHandlers );
		if ( QueuedPresence.length > 0 )
			::UpdatePresence.exchange( true );
		JsonValue* data = GetObjMember( &readyMessage, "data" );
		JsonValue* user = GetObjMember( data, "user" );
		const char* userId = GetStrMember( user, "id" );
		const char* username = GetStrMember( user, "username" );
		const char* avatar = GetStrMember( user, "avatar" );
		if ( userId && username )
		{
			StringCopy( ConnectedUser.userId, userId );
			StringCopy( ConnectedUser.username, username );
			const char* discriminator = GetStrMember( user, "discriminator" );
			if ( discriminator )
				StringCopy( ConnectedUser.discriminator, discriminator );
			if ( avatar )
				StringCopy( ConnectedUser.avatar, avatar );
			else
				ConnectedUser.avatar[0] = 0;
		}
		WasJustConnected.exchange( true );
		ReconnectTimeMs.reset();
	};
	Connection->onDisconnect = []( int err, const char* message ) {
		LastDisconnectErrorCode = err;
		StringCopy( LastDisconnectErrorMessage, message );
		WasJustDisconnected.exchange( true );
		UpdateReconnectTime();
	};
	s_update_worker.Start();
}

void Discord::Shutdown()
{
	if ( !Connection )
		return;
	Connection->onConnect = nullptr;
	Connection->onDisconnect = nullptr;
	Handlers = {};
	QueuedPresence.length = 0;
	::UpdatePresence.exchange( false );
	StopDiscordUpdateThread();
	RpcConnection::Destroy( Connection );
}

void Discord::UpdatePresence( const Discord::RichPresence& presence )
{
	AUTO_LOCK( PresenceMutex );
	QueuedPresence.length = JsonWriteRichPresenceObj( QueuedPresence.buffer, sizeof( QueuedPresence.buffer ), Nonce++, Pid, presence );
	::UpdatePresence.exchange( true );
}

void Discord::Respond( const char* userId, Discord::Response reply )
{
	// if we are not connected, let's not batch up stale messages for later
	if ( !Connection || !Connection->IsOpen() )
		return;
	QueuedMessage* qmessage = SendQueue.GetNextAddMessage();
	if ( qmessage )
	{
		qmessage->length = JsonWriteJoinReply( qmessage->buffer, sizeof( qmessage->buffer ), userId, reply, Nonce++ );
		SendQueue.CommitAdd();
	}
}

void Discord::RunCallbacks()
{
	// Note on some weirdness: internally we might connect, get other signals, disconnect any number
	// of times inbetween calls here. Externally, we want the sequence to seem sane, so any other
	// signals are book-ended by calls to ready and disconnect.

	if ( !Connection )
		return;

	const bool wasDisconnected = WasJustDisconnected.exchange( false );
	const bool isConnected = Connection->IsOpen();

	if ( isConnected )
	{
		AUTO_LOCK( HandlerMutex );
		// if we are connected, disconnect cb first
		if ( wasDisconnected && Handlers.disconnected )
			Handlers.disconnected( LastDisconnectErrorCode, LastDisconnectErrorMessage );
	}

	if ( WasJustConnected.exchange( false ) )
	{
		AUTO_LOCK( HandlerMutex );
		if ( Handlers.ready )
		{
			const Discord::User req {
				ConnectedUser.userId,
				ConnectedUser.username,
				ConnectedUser.discriminator,
				ConnectedUser.avatar
			};
			Handlers.ready( req );
		}
	}

	if ( GotErrorMessage.exchange( false ) )
	{
		AUTO_LOCK( HandlerMutex );
		if ( Handlers.errored )
			Handlers.errored( LastErrorCode, LastErrorMessage );
	}

	if ( WasJoinGame.exchange( false ) )
	{
		AUTO_LOCK( HandlerMutex );
		if ( Handlers.joinGame )
			Handlers.joinGame( JoinGameSecret );
	}

	if ( WasSpectateGame.exchange( false ) )
	{
		AUTO_LOCK( HandlerMutex );
		if ( Handlers.spectateGame )
			Handlers.spectateGame( SpectateGameSecret );
	}

	// Right now this batches up any requests and sends them all in a burst; I could imagine a world
	// where the implementer would rather sequentially accept/reject each one before the next invite
	// is sent. I left it this way because I could also imagine wanting to process these all and
	// maybe show them in one common dialog and/or start fetching the avatars in parallel, and if
	// not it should be trivial for the implementer to make a queue themselves.
	while ( JoinAskQueue.HavePendingSends() )
	{
		auto req = JoinAskQueue.GetNextSendMessage();
		{
			AUTO_LOCK( HandlerMutex );
			if ( Handlers.joinRequest )
			{
				const Discord::User djr { req->userId, req->username, req->discriminator, req->avatar };
				Handlers.joinRequest( djr );
			}
		}
		JoinAskQueue.CommitSend();
	}

	if ( !isConnected && wasDisconnected )
	{
		AUTO_LOCK( HandlerMutex );
		// if we are not connected, disconnect message last
		if ( Handlers.disconnected )
			Handlers.disconnected( LastDisconnectErrorCode, LastDisconnectErrorMessage );
	}
}

#pragma warning(pop)