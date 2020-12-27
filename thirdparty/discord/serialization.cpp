#define RAPIDJSON_SSE2

#include "serialization.h"
#include "discord-rpc.h"

template <typename T, size_t destSize>
void NumberToString( char(&destArray)[destSize], T number )
{
	char* dest = destArray;
	if ( !number )
	{
		*dest++ = '0';
		*dest++ = 0;
		return;
	}
	if ( number < 0 )
	{
		*dest++ = '-';
		number = -number;
	}
	char temp[destSize];
	int place = 0;
	while ( number && destSize > place )
	{
		T digit = number % 10;
		number /= 10;
		temp[place++] = '0' + static_cast<char>( digit );
	}
	for ( --place; place >= 0; --place )
		*dest++ = temp[place];
	*dest = 0;
}

// it's ever so slightly faster to not have to strlen the key
template <size_t T>
static FORCEINLINE void WriteKey( JsonWriter& w, const char( &k )[T] )
{
	w.Key( k, T - 1 );
}

// it's ever so slightly faster to not have to strlen the string
template <size_t T>
static FORCEINLINE void WriteString( JsonWriter& w, const char( &k )[T] )
{
	w.String( k, T - 1 );
}

#pragma warning(push)
#pragma warning(disable:4512)

struct WriteObject
{
	JsonWriter& writer;
	WriteObject( JsonWriter& w ) : writer( w )
	{
		writer.StartObject();
	}

	template <size_t T>
	WriteObject( JsonWriter& w, const char( &name )[T] ) : writer( w )
	{
		WriteKey( writer, name );
		writer.StartObject();
	}

	~WriteObject()
	{
		writer.EndObject();
	}
};

struct WriteArray
{
	JsonWriter& writer;
	template <size_t T>
	WriteArray( JsonWriter& w, const char( &name )[T] ) : writer( w )
	{
		WriteKey( writer, name );
		writer.StartArray();
	}

	~WriteArray()
	{
		writer.EndArray();
	}
};

#pragma warning(pop)

template <size_t T>
static void WriteOptionalString( JsonWriter& w, const char( &k )[T], const char* value )
{
	if ( value && value[0] )
	{
		w.Key( k, T - 1 );
		w.String( value );
	}
}

static void JsonWriteNonce( JsonWriter& writer, int nonce )
{
	WriteKey( writer, "nonce" );
	char nonceBuffer[32];
	NumberToString( nonceBuffer, nonce );
	writer.String( nonceBuffer );
}

size_t JsonWriteRichPresenceObj( char* dest,
								 size_t maxLen,
								 int nonce,
								 int pid,
								 const Discord::RichPresence& presence )
{
	JsonWriter writer( dest, maxLen );

	{
		WriteObject top( writer );

		JsonWriteNonce( writer, nonce );

		WriteKey( writer, "cmd" );
		WriteString( writer, "SET_ACTIVITY" );

		{
			WriteObject args( writer, "args" );

			WriteKey( writer, "pid" );
			writer.Int( pid );

			{
				WriteObject activity( writer, "activity" );

				WriteOptionalString( writer, "state", presence.state );
				WriteOptionalString( writer, "details", presence.details );

				if ( presence.startTimestamp || presence.endTimestamp )
				{
					WriteObject timestamps( writer, "timestamps" );

					if ( presence.startTimestamp )
					{
						WriteKey( writer, "start" );
						writer.Int64( presence.startTimestamp );
					}

					if ( presence.endTimestamp )
					{
						WriteKey( writer, "end" );
						writer.Int64( presence.endTimestamp );
					}
				}

				if ( ( presence.largeImageKey && presence.largeImageKey[0] ) ||
					 ( presence.largeImageText && presence.largeImageText[0] ) ||
					 ( presence.smallImageKey && presence.smallImageKey[0] ) ||
					 ( presence.smallImageText && presence.smallImageText[0] ) )
				{
					WriteObject assets( writer, "assets" );
					WriteOptionalString( writer, "large_image", presence.largeImageKey );
					WriteOptionalString( writer, "large_text", presence.largeImageText );
					WriteOptionalString( writer, "small_image", presence.smallImageKey );
					WriteOptionalString( writer, "small_text", presence.smallImageText );
				}

				if ( ( presence.partyId && presence.partyId[0] ) || presence.partySize || presence.partyMax )
				{
					WriteObject party( writer, "party" );
					WriteOptionalString( writer, "id", presence.partyId );
					if ( presence.partySize )
					{
						WriteArray size( writer, "size" );
						writer.Int( presence.partySize );
						if ( 0 < presence.partyMax )
							writer.Int( presence.partyMax );
					}
				}

				if ( ( presence.matchSecret && presence.matchSecret[0] ) ||
					( presence.joinSecret && presence.joinSecret[0] ) ||
					 ( presence.spectateSecret && presence.spectateSecret[0] ) )
				{
					WriteObject secrets( writer, "secrets" );
					WriteOptionalString( writer, "match", presence.matchSecret );
					WriteOptionalString( writer, "join", presence.joinSecret );
					WriteOptionalString( writer, "spectate", presence.spectateSecret );
				}

				WriteKey( writer, "instance" );
				writer.Bool( presence.instance != 0 );
			}
		}
	}

	return writer.Size();
}

size_t JsonWriteHandshakeObj( char* dest, size_t maxLen, int version, const char* applicationId )
{
	JsonWriter writer( dest, maxLen );

	{
		WriteObject obj( writer );
		WriteKey( writer, "v" );
		writer.Int( version );
		WriteKey( writer, "client_id" );
		writer.String( applicationId );
	}

	return writer.Size();
}

size_t JsonWriteSubscribeCommand( char* dest, size_t maxLen, int nonce, const char* evtName )
{
	JsonWriter writer( dest, maxLen );

	{
		WriteObject obj( writer );

		JsonWriteNonce( writer, nonce );

		WriteKey( writer, "cmd" );
		WriteString( writer, "SUBSCRIBE" );

		WriteKey( writer, "evt" );
		writer.String( evtName );
	}

	return writer.Size();
}

size_t JsonWriteUnsubscribeCommand( char* dest, size_t maxLen, int nonce, const char* evtName )
{
	JsonWriter writer( dest, maxLen );

	{
		WriteObject obj( writer );

		JsonWriteNonce( writer, nonce );

		WriteKey( writer, "cmd" );
		WriteString( writer, "UNSUBSCRIBE" );

		WriteKey( writer, "evt" );
		writer.String( evtName );
	}

	return writer.Size();
}

size_t JsonWriteJoinReply( char* dest, size_t maxLen, const char* userId, Discord::Response reply, int nonce )
{
	JsonWriter writer( dest, maxLen );

	{
		WriteObject obj( writer );

		WriteKey( writer, "cmd" );
		if ( reply == Discord::Response::Yes )
			WriteString( writer, "SEND_ACTIVITY_JOIN_INVITE" );
		else
			WriteString( writer, "CLOSE_ACTIVITY_JOIN_REQUEST" );

		WriteKey( writer, "args" );
		{
			WriteObject args( writer );

			WriteKey( writer, "user_id" );
			writer.String( userId );
		}

		JsonWriteNonce( writer, nonce );
	}

	return writer.Size();
}