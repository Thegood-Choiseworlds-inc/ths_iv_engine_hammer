#include "stdafx.h"
#include "hammer.h"
#include "mapdoc.h"
#include "gameconfig.h"
#include "options.h"

#include "tier1/fmtstr.h"

#include "../thirdparty/discord/discord-rpc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CString BuildLine( CString line, const char* mapNameWithExt )
{
	line.Replace( "%(file)", mapNameWithExt );
	char mapNameWithoutExt[128];
	V_FileBase( mapNameWithExt, mapNameWithoutExt, 128 );
	line.Replace( "%(filename)", mapNameWithoutExt );

	line.Replace( "%(moddir)", g_pGameConfig->GetMod() );
	line.Replace( "%(mod)", g_pGameConfig->szName );
	return line;
}

void DiscordUpdatePresence()
{
	auto doc = CMapDoc::GetActiveMapDoc();
	if ( doc && doc->GetPathName().IsEmpty() && doc->GetTitle().IsEmpty() )
		return; // skip instances

	Discord::RichPresence presence;
	V_memset( &presence, 0, sizeof( Discord::RichPresence ) );
	presence.largeImageKey = "hammer";
	presence.largeImageText = "Hammering";

	char detailText[128];
	char stateText[128];
	if ( doc )
	{
		const bool newFile = !V_strncmp( doc->GetTitle(), "vmf", 3 );
		presence.startTimestamp = doc->OpenTime();
		const char* map = newFile ? "untitled.vmf" : V_UnqualifiedFileName( doc->GetPathName() );

		V_strcpy_safe( detailText, BuildLine( Options.discord.sLine1Template, map ) );
		V_strcpy_safe( stateText, BuildLine( Options.discord.sLine2Template, map ) );

		presence.details = detailText;
		presence.state = stateText;
	}

	Discord::UpdatePresence( presence );
}

static constexpr Color discordColor = Color::RawColor( 0xffda8972 );
static void OnDiscordReady( const Discord::User& request )
{
	ConColorMsg( discordColor, "DISCORD: Ready! Connected to user %s\n", request.username );

	DiscordUpdatePresence();
}

static void OnDiscordDisconnected( int errCode, const char* msg )
{
	ConColorMsg( discordColor, "DISCORD: Disconnected code: %d, message: %s!\n", errCode, msg );
}

static void OnDiscordErrored( int errCode, const char* msg )
{
	ConColorMsg( discordColor, "DISCORD: Error code: %d, message: %s!\n", errCode, msg );
}

static void OnDiscordJoinRequest( const Discord::User& request )
{
	Discord::Respond( request.userId, Discord::Response::No );
}

static bool discordEnabled = false;
constexpr const uint64 discordAppID = 753212646089687090ULL;
static void DiscordStartup()
{
	if ( !Options.discord.bEnable )
		return;
	discordEnabled = true;

	Discord::EventHandlers handlers{};
	handlers.ready = OnDiscordReady;
	handlers.disconnected = OnDiscordDisconnected;
	handlers.errored = OnDiscordErrored;
	handlers.joinRequest = OnDiscordJoinRequest;

	Discord::Initialize( CNumStr( discordAppID ), handlers );
}

void DiscordCheckState()
{
	if ( discordEnabled == (bool)Options.discord.bEnable )
		return DiscordUpdatePresence();
	discordEnabled = Options.discord.bEnable;
	if ( Options.discord.bEnable )
		DiscordStartup();
	else
		Discord::Shutdown();
}

static struct DiscordRegister
{
	DiscordRegister()
	{
		AppRegisterPostInitFn( DiscordStartup );
		AppRegisterPreShutdownFn( Discord::Shutdown );
		AppRegisterMessageLoopFn( Discord::RunCallbacks );
	}
} s_discordRegister;