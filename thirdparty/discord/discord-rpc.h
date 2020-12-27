#ifndef DISCORD_RPC_H
#define DISCORD_RPC_H

#pragma once

#pragma warning(push)
#pragma warning(disable: 4100 4324)
#include "tier0/basetypes.h"
#include "tier0/threadtools.h"
#pragma warning(pop)

namespace Discord
{
	struct RichPresence
	{
		const char* state;   /* max 128 bytes */
		const char* details; /* max 128 bytes */
		int64 startTimestamp;
		int64 endTimestamp;
		const char* largeImageKey;  /* max 32 bytes */
		const char* largeImageText; /* max 128 bytes */
		const char* smallImageKey;  /* max 32 bytes */
		const char* smallImageText; /* max 128 bytes */
		const char* partyId;        /* max 128 bytes */
		int partySize;
		int partyMax;
		const char* matchSecret;    /* max 128 bytes */
		const char* joinSecret;     /* max 128 bytes */
		const char* spectateSecret; /* max 128 bytes */
		int8 instance;
	};

	struct User
	{
		const char* userId;
		const char* username;
		const char* discriminator;
		const char* avatar;
	};

	struct EventHandlers
	{
		void( *ready )( const User& request );
		void( *disconnected )( int errorCode, const char* message );
		void( *errored )( int errorCode, const char* message );
		void( *joinGame )( const char* joinSecret );
		void( *spectateGame )( const char* spectateSecret );
		void( *joinRequest )( const User& request );
	};

	enum class Response : unsigned char
	{
		No = 0,
		Yes,
		Ignore
	};

	void Initialize( const char* applicationId,
					 EventHandlers& handlers );

	void Shutdown();

	/* checks for incoming messages, dispatches callbacks */
	void RunCallbacks();

	void UpdatePresence( const RichPresence& presence );

	void Respond( const char* userId, Response reply );
}

#endif // DISCORD_RPC_H