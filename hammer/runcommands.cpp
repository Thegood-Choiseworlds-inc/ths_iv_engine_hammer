//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ====
//
// Purpose: Handles running the OS commands for map compilation.
//
//=============================================================================

#include "stdafx.h"
#include <afxtempl.h>
#include "gameconfig.h"
#include "runcommands.h"
#include "options.h"
#include <process.h>
#include <io.h>
#include <direct.h>
#include "globalfunctions.h"
#include "hammer.h"
#include "KeyValues.h"

#include <string_view>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static constexpr const std::string_view base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static FORCEINLINE bool is_base64( const unsigned char c ) { return ( isalnum( c ) || ( c == '+' ) || ( c == '/' ) ); }

static size_t base64_encode( const void* pInput, unsigned int in_len, char* pOutput, const size_t nOutSize )
{
	const unsigned char* bytes_to_encode = static_cast<const unsigned char*>( pInput );
	CUtlVector<char> ret;
	ret.EnsureCapacity( 4 * ( ( in_len + 2 ) / 3 ) );
	int i = 0;
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];

	while ( in_len-- )
	{
		char_array_3[i++] = *( bytes_to_encode++ );
		if ( i == 3 )
		{
			char_array_4[0] = ( char_array_3[0] & 0xfc ) >> 2;
			char_array_4[1] = ( ( char_array_3[0] & 0x03 ) << 4 ) + ( ( char_array_3[1] & 0xf0 ) >> 4 );
			char_array_4[2] = ( ( char_array_3[1] & 0x0f ) << 2 ) + ( ( char_array_3[2] & 0xc0 ) >> 6 );
			char_array_4[3] = char_array_3[2] & 0x3f;

			for ( i = 0; i < 4; i++ )
				ret.AddToTail( base64_chars[char_array_4[i]] );
			i = 0;
		}
	}

	if ( i )
	{
		for ( int j = i; j < 3; j++ )
			char_array_3[j] = '\0';

		char_array_4[0] = ( char_array_3[0] & 0xfc ) >> 2;
		char_array_4[1] = ( ( char_array_3[0] & 0x03 ) << 4 ) + ( ( char_array_3[1] & 0xf0 ) >> 4 );
		char_array_4[2] = ( ( char_array_3[1] & 0x0f ) << 2 ) + ( ( char_array_3[2] & 0xc0 ) >> 6 );

		for ( int j = 0; j < i + 1; j++ )
			ret.AddToTail( base64_chars[char_array_4[j]] );

		while ( i++ < 3 )
			ret.AddToTail( '=' );
	}

	if ( nOutSize <= static_cast<size_t>( ret.Count() ) )
	{
		Warning( "Base64 encode: insufficient output buffer size\n" );
		memset( pOutput, 0, nOutSize );
		return 0;
	}

	memcpy( pOutput, ret.Base(), ret.Count() );
	pOutput[ret.Count()] = '\0';

	return ret.Count();
}


static bool s_bRunsCommands = false;

bool IsRunningCommands() { return s_bRunsCommands; }

static char *pszDocPath, *pszDocName, *pszDocExt;

static void RemoveQuotes( char* pBuf )
{
	if ( pBuf[0] == '\"' )
		strcpy( pBuf, pBuf + 1 );
	if ( pBuf[strlen( pBuf ) - 1] == '\"' )
		pBuf[strlen( pBuf ) - 1] = 0;
}

class CommandRunner
{
public:
	CommandRunner( bool waitForKey ) : m_bWaitForKey( waitForKey ), m_cmdData( 1, 0 ), m_nCommands( 0 ) {}
	~CommandRunner() = default;

	void AddCommand( int specialID, const char* from, const char* to = nullptr )
	{
		CmdType type;
		switch ( specialID )
		{
		case CCChangeDir:
			type = CmdType::ChangeDir;
			break;
		case CCCopyFile:
			type = CmdType::Copy;
			break;
		case CCDelFile:
			type = CmdType::Delete;
			break;
		case CCRenameFile:
			type = CmdType::Rename;
			break;
		NO_DEFAULT
		}

		int size = 0;
		switch ( type )
		{
		case CommandRunner::CmdType::Delete:
		case CommandRunner::CmdType::ChangeDir:
			size = V_strlen( from ) + 1;
			break;
		case CommandRunner::CmdType::Copy:
		case CommandRunner::CmdType::Rename:
			size = V_strlen( from ) + V_strlen( to ) + 2; // \0 and |
			break;
		NO_DEFAULT
		}

		auto cmd = AllocCmd( size, type );
		switch ( type )
		{
		case CommandRunner::CmdType::Delete:
		case CommandRunner::CmdType::ChangeDir:
			V_strncpy( cmd->command, from, cmd->size );
			break;
		case CommandRunner::CmdType::Copy:
		case CommandRunner::CmdType::Rename:
			V_snprintf( cmd->command, cmd->size, "%s|%s", from, to );
			break;
		NO_DEFAULT
		}
	}

	void FileCheck( const char* filePath )
	{
		char file[MAX_PATH];
		V_strcpy_safe( file, filePath );
		RemoveQuotes( file );
		auto cmd = AllocCmd( V_strlen( file ) + 1, CmdType::EnsureExists );
		V_strncpy( cmd->command, file, cmd->size );
	}

	void AddCommand( const char* _cmd, const char* args )
	{
		const int size = V_strlen( _cmd ) + V_strlen( args ) + 2; // space + \0
		auto cmd = AllocCmd( size, CmdType::Exec );
		V_snprintf( cmd->command, cmd->size, "%s %s", _cmd, args );
	}

	void Launch()
	{
		if ( m_cmdData.IsEmpty() )
			return;

		CUtlVector<char> encCmd;
		encCmd.SetCount( 4 * ( ( m_cmdData.Count() + 2 ) / 3 ) + 1 );
		base64_encode( m_cmdData.Base(), m_cmdData.Count(), encCmd.Base(), encCmd.Count() );

		char szFilename[MAX_PATH];
		GetModuleFileName( NULL, szFilename, sizeof( szFilename ) );
		V_StripLastDir( szFilename, sizeof( szFilename ) );
		V_AppendSlash( szFilename, sizeof( szFilename ) );
		V_strcat_safe( szFilename, "hammer_map_launcher.exe" );

		CString launchCmd;
		if ( m_bWaitForKey )
			launchCmd.Format( "%s -WaitForKeypress %u %s", szFilename, m_nCommands, encCmd.Base() );
		else
			launchCmd.Format( "%s %u %s", szFilename, m_nCommands, encCmd.Base() );

		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		memset( &si, 0, sizeof( si ) );
		si.cb = sizeof( si );

		CreateProcess( NULL, (char*)(const char*)launchCmd, NULL, NULL, FALSE,
						CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi );

		CloseHandle( pi.hProcess );
		CloseHandle( pi.hThread );
	}

private:

	// keep in sync with hammer_map_launcher!
	enum class CmdType : unsigned char
	{
		Exec,
		ChangeDir,
		Copy,
		Delete,
		Rename,
		EnsureExists,

		Last
	};

	#pragma pack(push, 1)
	#pragma warning(push)
	#pragma warning(disable: 4200)

	struct Cmd
	{
		unsigned short size;
		CmdType type;
		char command[];
	};

	#pragma warning(pop)
	#pragma pack(pop)

	Cmd* AllocCmd( int size, CmdType type )
	{
		auto curPos = m_cmdData.Size();
		m_cmdData.EnsureCount( curPos + sizeof( Cmd ) + size );
		auto cmd = reinterpret_cast<Cmd*>( m_cmdData.Base() + curPos );
		cmd->size = size;
		cmd->type = type;
		++m_nCommands;
		return cmd;
	}

	bool				m_bWaitForKey;
	CUtlVector<byte>	m_cmdData;
	size_t				m_nCommands;
};

void FixGameVars(char *pszSrc, char *pszDst, BOOL bUseQuotes)
{
	// run through the parms list and substitute $variable strings for
	//  the real thing
	char *pSrc = pszSrc, *pDst = pszDst;
	BOOL bInQuote = FALSE;
	while(pSrc[0])
	{
		if(pSrc[0] == '$')	// found a parm
		{
			if(pSrc[1] == '$')	// nope, it's a single symbol
			{
				*pDst++ = '$';
				++pSrc;
			}
			else
			{
				// figure out which parm it is ..
				++pSrc;

				if (!bInQuote && bUseQuotes)
				{
					// not in quote, and subbing a variable.. start quote
					*pDst++ = '\"';
					bInQuote = TRUE;
				}

				if(!strnicmp(pSrc, "file", 4))
				{
					pSrc += 4;
					strcpy(pDst, pszDocName);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "ext", 3))
				{
					pSrc += 3;
					strcpy(pDst, pszDocExt);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "path", 4))
				{
					pSrc += 4;
					strcpy(pDst, pszDocPath);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "exedir", 6))
				{
					pSrc += 6;
					strcpy(pDst, g_pGameConfig->m_szGameExeDir);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "bspdir", 6))
				{
					pSrc += 6;
					strcpy(pDst, g_pGameConfig->szBSPDir);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "bsp_exe", 7))
				{
					pSrc += 7;
					strcpy(pDst, g_pGameConfig->szBSP);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "vis_exe", 7))
				{
					pSrc += 7;
					strcpy(pDst, g_pGameConfig->szVIS);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "light_exe", 9))
				{
					pSrc += 9;
					strcpy(pDst, g_pGameConfig->szLIGHT);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "game_exe", 8))
				{
					pSrc += 8;
					strcpy(pDst, g_pGameConfig->szExecutable);
					pDst += strlen(pDst);
				}
				else if (!strnicmp(pSrc, "gamedir", 7))
				{
					pSrc += 7;
					strcpy(pDst, g_pGameConfig->m_szModDir);
					pDst += strlen(pDst);
				}
			}
		}
		else
		{
			if(*pSrc == ' ' && bInQuote)
			{
				bInQuote = FALSE;
				*pDst++ = '\"';	// close quotes
			}

			// just copy the char into the destination buffer
			*pDst++ = *pSrc++;
		}
	}

	if(bInQuote)
	{
		bInQuote = FALSE;
		*pDst++ = '\"';	// close quotes
	}

	pDst[0] = 0;
}

void CCOMMAND::Load(KeyValues* pKvCmd)
{
	bEnable = pKvCmd->GetBool("enabled");
	iSpecialCmd = pKvCmd->GetInt("special_cmd");
	Q_strncpy(szRun, pKvCmd->GetString("run"), MAX_PATH);
	Q_strncpy(szParms, pKvCmd->GetString("params"), MAX_PATH);
	bEnsureCheck = pKvCmd->GetBool("ensure_check");
	Q_strncpy(szEnsureFn, pKvCmd->GetString("ensure_fn"), MAX_PATH);
}

void CCOMMAND::Save(KeyValues* pKvCmd) const
{
	pKvCmd->SetBool("enabled", bEnable);
	pKvCmd->SetInt("special_cmd", iSpecialCmd);
	pKvCmd->SetString("run", szRun);
	pKvCmd->SetString("params", szParms);
	pKvCmd->SetBool("ensure_check", bEnsureCheck);
	pKvCmd->SetString("ensure_fn", szEnsureFn);
}

bool RunCommands(CCommandArray& Commands, LPCTSTR pszOrigDocName, bool bWaitForKeypress)
{
	s_bRunsCommands = true;

	// cut up document name into file and extension components.
	//  create two sets of buffers - one set with the long filename
	//  and one set with the 8.3 format.

	char szDocLongPath[MAX_PATH] = {0}, szDocLongName[MAX_PATH] = {0},
		szDocLongExt[MAX_PATH] = {0};
	char szDocShortPath[MAX_PATH] = {0}, szDocShortName[MAX_PATH] = {0},
		szDocShortExt[MAX_PATH] = {0};

	GetFullPathName(pszOrigDocName, MAX_PATH, szDocLongPath, NULL);
	GetShortPathName(pszOrigDocName, szDocShortPath, MAX_PATH);

	// split them up
	char *p = strrchr(szDocLongPath, '.');
	if(p && strrchr(szDocLongPath, '\\') < p && strrchr(szDocLongPath, '/') < p)
	{
		// got the extension
		strcpy(szDocLongExt, p+1);
		p[0] = 0;
	}

	p = strrchr(szDocLongPath, '\\');
	if(!p)
		p = strrchr(szDocLongPath, '/');
	if(p)
	{
		// got the filepart
		strcpy(szDocLongName, p+1);
		p[0] = 0;
	}

	// split the short part up
	p = strrchr(szDocShortPath, '.');
	if(p && strrchr(szDocShortPath, '\\') < p && strrchr(szDocShortPath, '/') < p)
	{
		// got the extension
		strcpy(szDocShortExt, p+1);
		p[0] = 0;
	}

	p = strrchr(szDocShortPath, '\\');
	if(!p)
		p = strrchr(szDocShortPath, '/');
	if(p)
	{
		// got the filepart
		strcpy(szDocShortName, p+1);
		p[0] = 0;
	}

	CommandRunner commandExecuter( bWaitForKeypress );

	int iSize = Commands.GetSize(), i = 0;
	char *ppParms[32];
	while(iSize--)
	{
		CCOMMAND& cmd = Commands[i++];

		// anything there?
		if ( ( !cmd.szRun[0] && !cmd.iSpecialCmd ) || !cmd.bEnable )
			continue;

		// set name pointers for long filenames
		pszDocExt = szDocLongExt;
		pszDocName = szDocLongName;
		pszDocPath = szDocLongPath;

		char szNewParms[MAX_PATH*5], szNewRun[MAX_PATH*5];

		FixGameVars( cmd.szRun, szNewRun, TRUE );
		FixGameVars( cmd.szParms, szNewParms, TRUE );

		if ( !cmd.iSpecialCmd )
		{
			if ( !V_stricmp( cmd.szRun, "$game_exe" ) )
			{
				// Change to the game exe folder before spawning the engine.
				// This is necessary for Steam to find the correct Steam DLL (it
				// uses the current working directory to search).
				char szDir[MAX_PATH];
				V_strncpy( szDir, szNewRun, sizeof(szDir) );
				RemoveQuotes( szDir );
				V_StripFilename( szDir );
				commandExecuter.AddCommand( CCChangeDir, szDir );
			}
			commandExecuter.AddCommand( szNewRun, szNewParms );

			// check for existence?
			if ( cmd.bEnsureCheck )
				commandExecuter.FileCheck( cmd.szEnsureFn );
			continue;
		}

		// create a parameter list (not always required)
		char* p = szNewParms;
		int iArg = 0;
		BOOL bDone = FALSE;
		while ( p[0] )
		{
			ppParms[iArg++] = p;
			while ( p[0] )
			{
				if ( p[0] == ' ' )
				{
					// found a space-separator
					p[0] = 0;

					p++;

					// skip remaining white space
					while ( *p == ' ' )
						p++;

					break;
				}

				// found the beginning of a quoted parameters
				if ( p[0] == '\"' )
				{
					while ( true )
					{
						p++;
						if ( p[0] == '\"' )
						{
							// found the end
							if ( p[1] == 0 )
								bDone = TRUE;
							p[1] = 0;	// kick its ass
							p += 2;

							// skip remaining white space
							while ( *p == ' ' )
								p++;

							break;
						}
					}
					break;
				}

				// else advance p
				++p;
			}

			if ( !p[0] || bDone )
				break; // done.
		}

		ppParms[iArg] = NULL;

		if ( ( cmd.iSpecialCmd == CCCopyFile || cmd.iSpecialCmd == CCRenameFile ) && iArg == 2 ) // 2 args
		{
			RemoveQuotes( ppParms[0] );
			RemoveQuotes( ppParms[1] );
			commandExecuter.AddCommand( cmd.iSpecialCmd, ppParms[0], ppParms[1] );
		}
		else if ( ( cmd.iSpecialCmd == CCChangeDir || cmd.iSpecialCmd == CCDelFile ) && iArg == 1 ) // 1 arg
		{
			RemoveQuotes( ppParms[0] );
			commandExecuter.AddCommand( cmd.iSpecialCmd, ppParms[0] );
		}

		// check for existence?
		if ( cmd.bEnsureCheck )
			commandExecuter.FileCheck( cmd.szEnsureFn );
	}

	commandExecuter.Launch();

	s_bRunsCommands = false;

	return TRUE;
}