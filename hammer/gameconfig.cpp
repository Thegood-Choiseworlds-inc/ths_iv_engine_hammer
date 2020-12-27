//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ====
//
// Purpose:
//
//=============================================================================

#include "stdafx.h"
#include <direct.h>
#include <io.h>
#include "worldsize.h"
#include "gameconfig.h"
#include "globalfunctions.h"
#include "fgdlib/helperinfo.h"
#include "hammer.h"
#include "KeyValues.h"
#include "mapdoc.h"
#include "mapentity.h"
#include "MapInstance.h"
#include "mapworld.h"
#include "filesystem_tools.h"
#include "texturesystem.h"
#include "tier1/strtools.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#pragma warning(disable:4244)

class CSteamGameDB
{
public:
	CSteamGameDB()
	{
		HKEY steam;
		if ( RegOpenKeyExA( HKEY_LOCAL_MACHINE, R"(SOFTWARE\Valve\Steam)", 0, KEY_QUERY_VALUE, &steam ) != ERROR_SUCCESS )
			return;

		char steamLocation[MAX_PATH*2];
		DWORD dwSize = sizeof( steamLocation );
		if ( RegQueryValueExA( steam, "InstallPath", NULL, NULL, (LPBYTE)steamLocation, &dwSize ) != ERROR_SUCCESS )
			return;

		RegCloseKey( steam );

		libFolders.AddToTail( steamLocation );

		V_strcat_safe( steamLocation, R"(\steamapps\libraryfolders.vdf)" );

		{
			CUtlBuffer lib( 0, 0, CUtlBuffer::TEXT_BUFFER );
			if ( !g_pFullFileSystem->ReadFile( steamLocation, nullptr, lib ) )
				return;

			KeyValuesAD libFolder( "LibraryFolders" );
			libFolder->UsesEscapeSequences( true );
			libFolder->UsesConditionals( false );
			if ( !libFolder->LoadFromBuffer( "LibraryFolders", lib ) )
				return;

			FOR_EACH_SUBKEY( libFolder, folder )
			{
				auto name = folder->GetName();
				if ( !V_stricmp( name, "TimeNextStatsReport" ) || !V_stricmp( name, "ContentStatsID" ) )
					continue;
				if ( auto p = folder->GetString() )
					libFolders.AddToTail( p );
			}
		}

		for ( auto& folder : libFolders )
			folder = CUtlString::PathJoin( folder, "steamapps" );

		for ( const auto& folder : libFolders )
		{
			g_pFullFileSystem->AddSearchPath( folder, "__HAMMER_HACK__" );

			FileFindHandle_t h = 0;
			auto manifest = g_pFullFileSystem->FindFirstEx( "appmanifest_*.acf", "__HAMMER_HACK__", &h );
			while ( manifest )
			{
				games.AddToTail( Game{ static_cast<uint32>( V_atoi64( manifest + 12 ) ), CUtlString::PathJoin( folder, manifest ), &folder } );
				manifest = g_pFullFileSystem->FindNext( h );
			}

			g_pFullFileSystem->FindClose( h );

			g_pFullFileSystem->RemoveSearchPath( folder, "__HAMMER_HACK__" );
		}

		for ( auto& folder : libFolders )
			folder = CUtlString::PathJoin( folder, "common" );

		games.Sort( []( const Game* a, const Game* b ) { return int( a->appid ) - int( b->appid ); } );
	}

	bool GetAppInstallDir( uint32 appid, CUtlString& path ) const
	{
		const auto game = games.FindMatch( [appid]( const Game& g ) { return g.appid == appid; } );
		if ( !games.IsValidIndex( game ) )
			return false;

		CUtlBuffer b( 0, 0, CUtlBuffer::TEXT_BUFFER );
		if ( !g_pFullFileSystem->ReadFile( games[game].manifest, nullptr, b ) )
			return false;

		auto f = b.String();
		auto instD = V_stristr( f, "installdir" );
		if ( !instD )
			return false;

		auto dirStart = strchr( instD + ARRAYSIZE( "installdir" ), '"' );
		auto dirEnd = strchr( dirStart + 1, '"' );

		path = CUtlString::PathJoin( *games[game].library, CUtlString( dirStart + 1, dirEnd - dirStart - 1 ) );
		return true;
	}

private:
	struct Game
	{
		uint32 appid;
		CUtlString manifest;
		const CUtlString* library;
	};
	CUtlVector<CUtlString> libFolders;
	CUtlVector<Game> games;
};

const int MAX_ERRORS = 5;


GameData *pGD;
CGameConfig g_DefaultGameConfig;
CGameConfig *g_pGameConfig = &g_DefaultGameConfig;


float g_MAX_MAP_COORD = 4096;
float g_MIN_MAP_COORD = -4096;


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CGameConfig *CGameConfig::GetActiveGame(void)
{
	return g_pGameConfig;
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : pGame -
//-----------------------------------------------------------------------------
void CGameConfig::SetActiveGame(CGameConfig *pGame)
{
	if (pGame != NULL)
	{
		g_pGameConfig = pGame;
		pGD = &pGame->GD;

		g_MAX_MAP_COORD = pGD->GetMaxMapCoord();
		g_MIN_MAP_COORD = pGD->GetMinMapCoord();
	}
	else
	{
		g_pGameConfig = &g_DefaultGameConfig;
		pGD = NULL;

		g_MAX_MAP_COORD = 4096;
		g_MIN_MAP_COORD = -4096;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor. Maintains a static	counter uniquely identifying each
//			game configuration.
//-----------------------------------------------------------------------------
CGameConfig::CGameConfig(void)
{
	nGDFiles = 0;
	m_fDefaultTextureScale = DEFAULT_TEXTURE_SCALE;
	m_nDefaultLightmapScale = DEFAULT_LIGHTMAP_SCALE;

	memset(szName, 0, sizeof(szName));
	memset(szExecutable, 0, sizeof(szExecutable));
	memset(szDefaultPoint, 0, sizeof(szDefaultPoint));
	memset(szDefaultSolid, 0, sizeof(szDefaultSolid));
	memset(szBSP, 0, sizeof(szBSP));
	memset(szLIGHT, 0, sizeof(szLIGHT));
	memset(szVIS, 0, sizeof(szVIS));
	memset(szMapDir, 0, sizeof(szMapDir));
	memset(m_szGameExeDir, 0, sizeof(m_szGameExeDir));
	memset(szBSPDir, 0, sizeof(szBSPDir));
	memset(m_szModDir, 0, sizeof(m_szModDir));
	strcpy(m_szCordonTexture, "BLACK");

	m_szSteamDir[0] = '\0';
	m_szSteamAppID[0] = '\0';

	static DWORD __dwID = 0;
	dwID = __dwID++;
}


//-----------------------------------------------------------------------------
// Purpose: Loads this game configuration from a keyvalue block.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGameConfig::Load(KeyValues *pkv)
{
	char szKey[MAX_PATH];

	// We should at least be able to get the game name and the game dir.
	Q_strncpy(szName, pkv->GetName(), sizeof(szName));
	Q_strncpy(m_szModDir, pkv->GetString("GameDir"), sizeof(m_szModDir));

	// Try to get the Hammer settings.
	KeyValues *pkvHammer = pkv->FindKey("Hammer");
	if (!pkvHammer)
		return true;

	//
	// Load the game data filenames from the "GameData0..GameDataN" keys.
	//
	nGDFiles = 0;
	bool bAdded = true;
	do
	{
		sprintf(szKey, "GameData%d", nGDFiles);
		const char *pszGameData = pkvHammer->GetString(szKey);
		if (pszGameData[0] != '\0')
		{
			GDFiles.Add(pszGameData);
			nGDFiles++;
		}
		else
		{
			bAdded = false;
		}

	} while (bAdded);

	m_fDefaultTextureScale = pkvHammer->GetFloat("DefaultTextureScale", DEFAULT_TEXTURE_SCALE);
	if (m_fDefaultTextureScale == 0)
	{
		m_fDefaultTextureScale = DEFAULT_TEXTURE_SCALE;
	}

	m_nDefaultLightmapScale = pkvHammer->GetInt("DefaultLightmapScale", DEFAULT_LIGHTMAP_SCALE);

	Q_strncpy(szExecutable, pkvHammer->GetString("GameExe"), sizeof(szExecutable));
	Q_strncpy(szDefaultSolid, pkvHammer->GetString("DefaultSolidEntity"), sizeof(szDefaultSolid));
	Q_strncpy(szDefaultPoint, pkvHammer->GetString("DefaultPointEntity"), sizeof(szDefaultPoint));

	Q_strncpy(szBSP, pkvHammer->GetString("BSP"), sizeof(szBSP));
	Q_strncpy(szVIS, pkvHammer->GetString("Vis"), sizeof(szVIS));
	Q_strncpy(szLIGHT, pkvHammer->GetString("Light"), sizeof(szLIGHT));
	Q_strncpy(m_szGameExeDir, pkvHammer->GetString("GameExeDir"), sizeof(m_szGameExeDir));
	Q_strncpy(szMapDir, pkvHammer->GetString("MapDir"), sizeof(szMapDir));
	Q_strncpy(szBSPDir, pkvHammer->GetString("BSPDir"), sizeof(szBSPDir));

	SetCordonTexture( pkvHammer->GetString("CordonTexture", "BLACK") );

	char szExcludeDir[MAX_PATH];
	const int materialExcludeCount = pkvHammer->GetInt("MaterialExcludeCount");
	for (int i = 0; i < materialExcludeCount; i++)
	{
		MatExlcusions_s& exclusion = m_MaterialExclusions[m_MaterialExclusions.AddToTail()];
		sprintf(szExcludeDir, "-MaterialExcludeDir%d", i );
		V_strcpy_safe( exclusion.szDirectory, pkvHammer->GetString( szExcludeDir ) );
		V_StripTrailingSlash( exclusion.szDirectory );
		exclusion.bUserGenerated = true;
	}

	LoadGDFiles();

	for ( int i = 0; i < GD.m_FGDMaterialExclusions.Count(); ++i )
	{
		MatExlcusions_s& exclusion = m_MaterialExclusions[m_MaterialExclusions.AddToTail()];
		const FGDMatExlcusions_s& fgdExclusion = GD.m_FGDMaterialExclusions[i];
		V_strcpy_safe( exclusion.szDirectory, fgdExclusion.szDirectory );
		exclusion.bUserGenerated = false;
	}

	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: Saves this config's data into a keyvalues object.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGameConfig::Save(KeyValues *pkv)
{
	pkv->SetName(szName);
	pkv->SetString("GameDir", m_szModDir);


	// Try to get the Hammer settings.
	KeyValues *pkvHammer = pkv->FindKey("Hammer");
	if (pkvHammer)
	{
		pkv->RemoveSubKey(pkvHammer);
		pkvHammer->deleteThis();
	}

	pkvHammer = pkv->CreateNewKey();
	if (!pkvHammer)
		return false;

	pkvHammer->SetName("Hammer");

	//
	// Load the game data filenames from the "GameData0..GameDataN" keys.
	//
	for (int i = 0; i < nGDFiles; i++)
	{
		char szKey[MAX_PATH];
		sprintf(szKey, "GameData%d", i);
		pkvHammer->SetString(szKey, GDFiles.GetAt(i));
	}

	pkvHammer->SetFloat("DefaultTextureScale", m_fDefaultTextureScale);
	pkvHammer->SetInt("DefaultLightmapScale", m_nDefaultLightmapScale);

	pkvHammer->SetString("GameExe", szExecutable);
	pkvHammer->SetString("DefaultSolidEntity", szDefaultSolid);
	pkvHammer->SetString("DefaultPointEntity", szDefaultPoint);

	pkvHammer->SetString("BSP", szBSP);
	pkvHammer->SetString("Vis", szVIS);
	pkvHammer->SetString("Light", szLIGHT);
	pkvHammer->SetString("GameExeDir", m_szGameExeDir);
	pkvHammer->SetString("MapDir", szMapDir);
	pkvHammer->SetString("BSPDir", szBSPDir);

	pkvHammer->SetString("CordonTexture", m_szCordonTexture);

	char szExcludeDir[MAX_PATH];
	pkvHammer->SetInt("MaterialExcludeCount", m_MaterialExclusions.CountIf( [](const MatExlcusions_s& e) { return e.bUserGenerated; }));
	for (int i = 0; i < m_MaterialExclusions.Count(); i++)
	{
		const MatExlcusions_s& exclusion = m_MaterialExclusions[i];
		if ( !exclusion.bUserGenerated )
			continue;
		sprintf(szExcludeDir, "-MaterialExcludeDir%d", i );
		pkvHammer->SetString(szExcludeDir, exclusion.szDirectory);
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pConfig -
//-----------------------------------------------------------------------------
void CGameConfig::CopyFrom(CGameConfig *pConfig)
{
	nGDFiles = pConfig->nGDFiles;

	GDFiles.RemoveAll();
	GDFiles.Append(pConfig->GDFiles);

	strcpy(szName, pConfig->szName);
	strcpy(szExecutable, pConfig->szExecutable);
	strcpy(szDefaultPoint, pConfig->szDefaultPoint);
	strcpy(szDefaultSolid, pConfig->szDefaultSolid);
	strcpy(szBSP, pConfig->szBSP);
	strcpy(szLIGHT, pConfig->szLIGHT);
	strcpy(szVIS, pConfig->szVIS);
	strcpy(szMapDir, pConfig->szMapDir);
	strcpy(m_szGameExeDir, pConfig->m_szGameExeDir);
	strcpy(szBSPDir, pConfig->szBSPDir);
	strcpy(m_szModDir, pConfig->m_szModDir);

	pConfig->m_MaterialExclusions.CopyArray( m_MaterialExclusions.Base(), m_MaterialExclusions.Count() );
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : pEntity -
//			pGD -
// Output : Returns TRUE to keep enumerating.
//-----------------------------------------------------------------------------
static BOOL UpdateClassPointer(CMapEntity *pEntity, GameData *pGD)
{
	GDclass *pClass = pGD->ClassForName(pEntity->GetClassName());
	pEntity->SetClass(pClass);
	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGameConfig::LoadGDFiles(void)
{
	GD.ClearData();

	// Save the old working directory
	char szOldDir[MAX_PATH];
	_getcwd( szOldDir, sizeof(szOldDir) );

	// Set our working directory properly
	char szAppDir[MAX_PATH];
	APP()->GetDirectory( DIR_PROGRAM, szAppDir );
	_chdir( szAppDir );

	for (int i = 0; i < nGDFiles; i++)
	{
		GD.Load(GDFiles[i]);
	}

	// Reset our old working directory
	_chdir( szOldDir );

	// All the class pointers have changed - now we have to
	// reset all the class pointers in each map doc that
	// uses this game.
	for ( int i=0; i<CMapDoc::GetDocumentCount(); i++ )
	{
		CMapDoc *pDoc = CMapDoc::GetDocument(i);

		if (pDoc->GetGame() == this)
		{
			CMapWorld *pWorld = pDoc->GetMapWorld();
			pWorld->SetClass(GD.ClassForName(pWorld->GetClassName()));
			pWorld->EnumChildren(UpdateClassPointer, &GD, MAPCLASS_TYPE(CMapEntity));
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Searches for the given filename, starting in szStartDir and looking
//			up the directory tree.
// Input:	szFile - the name of the file to search for.
//			szStartDir - the folder to start searching from, towards the root.
//			szFoundPath - receives the full path of the FOLDER where szFile was found.
// Output : Returns true if the file was found, false if not. If the file was
//			found the full path (not including the filename) is returned in szFoundPath.
//-----------------------------------------------------------------------------
bool FindFileInTree(const char *szFile, const char *szStartDir, char *szFoundPath)
{
	if ((szFile == NULL) || (szStartDir == NULL) || (szFoundPath == NULL))
	{
		return false;
	}

	char szRoot[MAX_PATH];
	strcpy(szRoot, szStartDir);
	Q_AppendSlash(szRoot, sizeof(szRoot));

	char szTemp[MAX_PATH];
	do
	{
		strcpy(szTemp, szRoot);
		strcat(szTemp, szFile);

		if (!_access(szTemp, 0))
		{
			strcpy(szFoundPath, szRoot);
			Q_StripTrailingSlash(szFoundPath);
			return true;
		}

	} while (Q_StripLastDir(szRoot, sizeof(szRoot)));

	return false;
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : *szDir -
//			*szSteamDir -
//			*szSteamUserDir -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool FindSteamUserDir(const char *szAppDir, const char *szSteamDir, char *szSteamUserDir)
{
	if ((szAppDir == NULL) || (szSteamDir == NULL) || (szSteamUserDir == NULL))
	{
		return false;
	}

	// If the szAppDir was run from within the steam tree, we should be able to find the steam user dir.
	int nSteamDirLen = strlen(szSteamDir);
	if (!Q_strnicmp(szAppDir, szSteamDir, nSteamDirLen ) && (szAppDir[nSteamDirLen] == '\\'))
	{
		strcpy(szSteamUserDir, szAppDir);

		char *pszSlash = strchr(&szSteamUserDir[nSteamDirLen + 1], '\\');
		if (pszSlash)
		{
			pszSlash++;

			pszSlash = strchr(pszSlash, '\\');
			if (pszSlash)
			{
				*pszSlash = '\0';
				return true;
			}
		}
	}

	szSteamUserDir[0] = '\0';

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Loads the settings from <mod dir>\gameinfo.txt into data members.
//-----------------------------------------------------------------------------
void CGameConfig::ParseGameInfo()
{
	KeyValuesAD pkv("gameinfo.txt");
	if (!pkv->LoadFromFile(g_pFileSystem, "gameinfo.txt", "GAME"))
		return;

	KeyValues *pKey = pkv->FindKey("FileSystem");
	if (pKey)
	{
		V_strcpy_safe(m_szSteamAppID, pKey->GetString("SteamAppId", ""));
	}

	const char *InstancePath = pkv->GetString( "InstancePath", NULL );
	if ( InstancePath )
	{
		CMapInstance::SetInstancePath( InstancePath );
	}

	char szAppDir[MAX_PATH];
	APP()->GetDirectory(DIR_PROGRAM, szAppDir);
	if (!FindFileInTree("steam.exe", szAppDir, m_szSteamDir))
	{
		// Couldn't find steam.exe in the hammer tree
		m_szSteamDir[0] = '\0';
	}

	if (!FindSteamUserDir(szAppDir, m_szSteamDir, m_szSteamUserDir))
	{
		m_szSteamUserDir[0] = '\0';
	}

	if ( auto mounts = pkv->FindKey( "mount" ) )
	{
		const CSteamGameDB steamDB;

		CUtlVector<CUtlString> dirs;
		FOR_EACH_TRUE_SUBKEY( mounts, pMount )
		{
			CUtlString path;
			const auto appId = static_cast<uint32>( V_atoi64( pMount->GetName() ) );
			if ( !steamDB.GetAppInstallDir( appId, path ) )
			{
				Warning( "AppId %lu not installed!\n", appId );
				continue;
			}
			const SearchPathAdd_t head = pMount->GetBool( "head" ) ? PATH_ADD_TO_HEAD : PATH_ADD_TO_TAIL;
			FOR_EACH_TRUE_SUBKEY( pMount, pModDir )
			{
				const char* const modDir = pModDir->GetName();
				const CFmtStr mod( "%s" CORRECT_PATH_SEPARATOR_S "%s", path.Get(), modDir );
				dirs.AddToTail( mod );
				FOR_EACH_VALUE( pModDir, pPath )
				{
					const char* const keyName = pPath->GetName();
					if ( V_stricmp( keyName, "vpk" ) == 0 )
					{
						const CFmtStr file( "%s" CORRECT_PATH_SEPARATOR_S "%s.vpk", mod.Get(), pPath->GetString() );
						g_pFullFileSystem->AddSearchPath( file, "GAME", head );
						g_pFullFileSystem->AddSearchPath( file, "MOD", head );
					}
					else if ( V_stricmp( keyName, "dir" ) == 0 )
					{
						const CFmtStr folder( "%s" CORRECT_PATH_SEPARATOR_S "%s", mod.Get(), pPath->GetString() );
						g_pFullFileSystem->AddSearchPath( folder, "GAME", head );
						g_pFullFileSystem->AddSearchPath( folder, "MOD", head );
					}
					else
						Warning( "Unknown key \"%s\" in mounts\n", keyName );
				}
			}
		}

		for ( const auto& dir : dirs )
		{
			g_pFullFileSystem->AddSearchPath( dir, "GAME" );
			g_pFullFileSystem->AddSearchPath( dir, "MOD" );
		}
	}
}

//-----------------------------------------------------------------------------
// Accessor methods to get at the mod + the game (*not* full paths)
//-----------------------------------------------------------------------------
const char *CGameConfig::GetMod()
{
	// Strip path from modDir
	char szModPath[MAX_PATH];
	static char szMod[MAX_PATH];
	Q_strncpy( szModPath, m_szModDir, MAX_PATH );
	Q_StripTrailingSlash( szModPath );
	if ( !szModPath[0] )
	{
		Q_strcpy( szModPath, "hl2" );
	}

	Q_FileBase( szModPath, szMod, MAX_PATH );

	return szMod;
}

const char *CGameConfig::GetGame()
{
	return "hl2";

//	// Strip path from modDir
//	char szGamePath[MAX_PATH];
//	static char szGame[MAX_PATH];
//	Q_strncpy( szGamePath, m_szGameDir, MAX_PATH );
//	Q_StripTrailingSlash( szGamePath );
//	Q_FileBase( szGamePath, szGame, MAX_PATH );

//	return szGame;
}


