//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: The application object.
//
//===========================================================================//

#include "stdafx.h"
#include <io.h>
#include <stdlib.h>
#include <direct.h>
#include "buildnum.h"
#include "editgameconfigs.h"
#include "splash.h"
#include "options.h"
#include "custommessages.h"
#include "mainfrm.h"
#include "messagewnd.h"
#include "childfrm.h"
#include "mapdoc.h"
#include "manifest.h"
#include "mapview3d.h"
#include "mapview2d.h"
#include "prefabs.h"
#include "globalfunctions.h"
#include "shell.h"
#include "shellmessagewnd.h"
#include "texturesystem.h"
#include "ToolManager.h"
#include "hammer.h"
#include "studiomodel.h"
#include "statusbarids.h"
#include "tier0/icommandline.h"
#include "soundsystem.h"
#include "IHammer.h"
#include "op_entity.h"
#include "tier0/dbg.h"
#include "istudiorender.h"
#include "filesystem.h"
#include "filesystem_init.h"
#include "utlmap.h"
#include "progdlg.h"
#include "mapworld.h"
#include "HammerVGui.h"
#include "vgui_controls/Controls.h"
#include "lpreview_thread.h"
#include "inputsystem/iinputsystem.h"
#include "datacache/idatacache.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "KeyBinds.h"
#include "fmtstr.h"
#include "KeyValues.h"
#include "particles/particles.h"
#include "afxvisualmanagerwindows.h"
#include "tier0/minidump.h"
#include "tier0/threadtools.h"
// #include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//
//	Note!
//
//		If this DLL is dynamically linked against the MFC
//		DLLs, any functions exported from this DLL which
//		call into MFC must have the AFX_MANAGE_STATE macro
//		added at the very beginning of the function.
//
//		For example:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// normal function body here
//		}
//
//		It is very important that this macro appear in each
//		function, prior to any calls into MFC.  This means that
//		it must appear as the first statement within the
//		function, even before any object variable declarations
//		as their constructors may generate calls into the MFC
//		DLL.
//
//		Please see MFC Technical Notes 33 and 58 for additional
//		details.
//

// dvs: hack
void EditorUtil_ConvertPath(CString &str, bool bSave);

void DiscordUpdatePresence();

CHammer theApp;
COptions Options;

CShell g_Shell;
CShellMessageWnd g_ShellMessageWnd;
CMessageWnd *g_pwndMessage = NULL;

// IPC structures for lighting preview thread
CMessageQueue<MessageToLPreview> g_HammerToLPreviewMsgQueue;
CMessageQueue<MessageFromLPreview> g_LPreviewToHammerMsgQueue;
ThreadHandle_t g_LPreviewThread;

bool	CHammer::m_bIsNewDocumentVisible = true;

//-----------------------------------------------------------------------------
// Expose singleton
//-----------------------------------------------------------------------------
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CHammer, IHammer, INTERFACEVERSION_HAMMER, theApp);


//-----------------------------------------------------------------------------
// global interfaces
//-----------------------------------------------------------------------------
IBaseFileSystem* g_pFileSystem;
IStudioDataCache* g_pStudioDataCache;
CreateInterfaceFn g_Factory;

struct MinidumpWrapperHelper_t
{
	int ( *m_pfn )( void* pParam );
	void* m_pParam;
	int m_iRetVal;
};

static void MinidumpWrapperHelper( void* arg )
{
	MinidumpWrapperHelper_t* info = (MinidumpWrapperHelper_t*)arg;
	info->m_iRetVal = info->m_pfn( info->m_pParam );
}

int WrapFunctionWithMinidumpHandler( int ( *pfn )( void* pParam ), void* pParam, int errorRetVal )
{
	MinidumpWrapperHelper_t info{ pfn, pParam, errorRetVal };
	CatchAndWriteMiniDumpForVoidPtrFn( MinidumpWrapperHelper, &info, true );
	return info.m_iRetVal;
}


//-----------------------------------------------------------------------------
// Purpose: Outputs a formatted debug string.
// Input  : fmt - format specifier.
//			... - arguments to format.
//-----------------------------------------------------------------------------
void DBG(const char *fmt, ...)
{
	char ach[128];
	va_list va;

	va_start(va, fmt);
	vsprintf(ach, fmt, va);
	va_end(va);
	OutputDebugString(ach);
}


void Msg(int type, const char *fmt, ...)
{
	if ( !g_pwndMessage )
		return;

	va_list vl;
	char szBuf[512];

	va_start(vl, fmt);
	int len = _vsnprintf(szBuf, 512, fmt, vl);
	va_end(vl);

	if ((type == mwError) || (type == mwWarning))
	{
		g_pwndMessage->ShowMessageWindow();
	}

	char temp = 0;
	char *pMsg = szBuf;
	do
	{
		if (len >= MESSAGE_WND_MESSAGE_LENGTH)
		{
			temp = pMsg[MESSAGE_WND_MESSAGE_LENGTH-1];
			pMsg[MESSAGE_WND_MESSAGE_LENGTH-1] = '\0';
		}

		g_pwndMessage->AddMsg((MWMSGTYPE)type, pMsg);

		if (len >= MESSAGE_WND_MESSAGE_LENGTH)
		{
			pMsg[MESSAGE_WND_MESSAGE_LENGTH-1] = temp;
			pMsg += MESSAGE_WND_MESSAGE_LENGTH-1;
		}

		len -= MESSAGE_WND_MESSAGE_LENGTH-1;

	} while (len > 0);
}


//-----------------------------------------------------------------------------
// Purpose: this routine calls the default doc template's OpenDocumentFile() but
//			with the ability to override the visible flag
// Input  : lpszPathName - the document to open
//			bMakeVisible - ignored
// Output : returns the opened document if successful
//-----------------------------------------------------------------------------
CDocument *CHammerDocTemplate::OpenDocumentFile( LPCTSTR lpszPathName, BOOL bMakeVisible )
{
	CDocument *pDoc = __super::OpenDocumentFile( lpszPathName, CHammer::IsNewDocumentVisible() );

	return pDoc;
}


//-----------------------------------------------------------------------------
// Purpose: this function will attempt an orderly shutdown of all maps.  It will attempt to
//			close only documents that have no references, hopefully freeing up additional documents
// Input  : bEndSession - ignored
//-----------------------------------------------------------------------------
void CHammerDocTemplate::CloseAllDocuments( BOOL bEndSession )
{
	bool	bFound = true;

	// rough loop to always remove the first map doc that has no references, then start over, try again.
	// if we still have maps with references ( that's bad ), we'll exit out of this loop and just do
	// the default shutdown to force them all to close.
	while( bFound )
	{
		bFound = false;

		POSITION pos = GetFirstDocPosition();
		while( pos != NULL )
		{
			CDocument *pDoc = GetNextDoc( pos );
			CMapDoc *pMapDoc = dynamic_cast< CMapDoc * >( pDoc );

			if ( pMapDoc && pMapDoc->GetReferenceCount() == 0 )
			{
				pDoc->OnCloseDocument();
				bFound = true;
				break;
			}
		}
	}

#if 0
	POSITION pos = GetFirstDocPosition();
	while( pos != NULL )
	{
		CDocument *pDoc = GetNextDoc( pos );
		CMapDoc *pMapDoc = dynamic_cast< CMapDoc * >( pDoc );

		if ( pMapDoc )
		{
			pMapDoc->ForceNoReference();
		}
	}

	__super::CloseAllDocuments( bEndSession );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: This function will allow hammer to control the initial visibility of an opening document
// Input  : pFrame - the new document's frame
//			pDoc - the new document
//			bMakeVisible - ignored as a parameter
//-----------------------------------------------------------------------------
void CHammerDocTemplate::InitialUpdateFrame( CFrameWnd* pFrame, CDocument* pDoc, BOOL bMakeVisible )
{
	bMakeVisible = CHammer::IsNewDocumentVisible();

	__super::InitialUpdateFrame( pFrame, pDoc, bMakeVisible );

	if ( bMakeVisible )
	{
		CMapDoc *pMapDoc = dynamic_cast< CMapDoc * >( pDoc );

		if ( pMapDoc )
		{
			pMapDoc->SetInitialUpdate();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will let all other maps know that an instanced map has been updated ( usually for volume size )
// Input  : pInstanceMapDoc - the document that has been updated
//-----------------------------------------------------------------------------
void CHammerDocTemplate::UpdateInstanceMap( CMapDoc *pInstanceMapDoc )
{
	POSITION pos = GetFirstDocPosition();
	while( pos != NULL )
	{
		CDocument *pDoc = GetNextDoc( pos );
		CMapDoc *pMapDoc = dynamic_cast< CMapDoc * >( pDoc );

		if ( pMapDoc && pMapDoc != pInstanceMapDoc )
		{
			pMapDoc->UpdateInstanceMap( pInstanceMapDoc );
		}
	}
}


template < typename T, int Instance >
static T& ReliableStaticStorage()
{
	static T storage;
	return storage;
}

enum Storage_t
{
	APP_FN_POST_INIT,
	APP_FN_PRE_SHUTDOWN,
	APP_FN_MESSAGE_LOOP,
	APP_FN_MESSAGE_PRETRANSLATE
};

#define s_appRegisteredPostInitFns		ReliableStaticStorage<CUtlVector<void(*)()>, APP_FN_POST_INIT>()
#define s_appRegisteredPreShutdownFns	ReliableStaticStorage<CUtlVector<void(*)()>, APP_FN_PRE_SHUTDOWN>()
#define s_appRegisteredMessageLoop		ReliableStaticStorage<CUtlVector<void(*)()>, APP_FN_MESSAGE_LOOP>()
#define s_appRegisteredMessagePreTrans	ReliableStaticStorage<CUtlVector<void(*)(MSG*)>, APP_FN_MESSAGE_PRETRANSLATE>()

void AppRegisterPostInitFn( void (*fn)() )
{
	s_appRegisteredPostInitFns.AddToTail( fn );
}

void AppRegisterMessageLoopFn( void (*fn)() )
{
	s_appRegisteredMessageLoop.AddToTail( fn );
}

void AppRegisterMessagePretranslateFn( void (*fn)( MSG * ) )
{
	s_appRegisteredMessagePreTrans.AddToTail( fn );
}

void AppRegisterPreShutdownFn( void (*fn)() )
{
	s_appRegisteredPreShutdownFns.AddToTail( fn );
}


class CHammerCmdLine : public CCommandLineInfo
{
	public:

		CHammerCmdLine(void)
		{
			m_bShowLogo = true;
			m_bConfigDir = false;
			m_bSetCustomConfigDir = false;
		}

		void ParseParam(LPCTSTR lpszParam, BOOL bFlag, BOOL bLast)
		{
			if (bFlag && !stricmp(lpszParam, "nologo"))
			{
				m_bShowLogo = false;
			}
			else if ((!m_bConfigDir) && (bFlag && !stricmp(lpszParam, "configdir")))
			{
				m_bConfigDir = true;
			}
			else if (m_bConfigDir)
			{
				if ( !bFlag )
				{
					Options.configs.m_strConfigDir = lpszParam;
					g_pFullFileSystem->AddSearchPath(lpszParam, "hammer_cfg", PATH_ADD_TO_HEAD);
					m_bSetCustomConfigDir = true;
				}
				m_bConfigDir = false;
			}
			else
			{
				CCommandLineInfo::ParseParam(lpszParam, bFlag, bLast);
			}
		}


	bool m_bShowLogo;
	bool m_bConfigDir;		// Used to find and parse the "-configdir blah" parameter pair.
	bool m_bSetCustomConfigDir; // Used with above
};


BEGIN_MESSAGE_MAP(CHammer, CWinApp)
	//{{AFX_MSG_MAP(CHammer)
	ON_COMMAND(ID_APP_ABOUT, &ThisClass::OnAppAbout)
	ON_COMMAND(ID_FILE_OPEN, &ThisClass::OnFileOpen)
	ON_COMMAND(ID_FILE_NEW, &ThisClass::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, &ThisClass::OnFileOpen)
	ON_COMMAND(ID_FILE_PRINT_SETUP, &ThisClass::OnFilePrintSetup)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CHammer::CHammer(void)
{
	m_bActiveApp = true;
	m_SuppressVideoAllocation = false;
	m_bForceRenderNextFrame = false;

	m_CmdLineInfo = new CHammerCmdLine();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees scratch buffer used when loading WAD files.
//			Deletes all command sequences used when compiling maps.
//-----------------------------------------------------------------------------
CHammer::~CHammer(void)
{
	if ( m_CmdLineInfo )
		delete m_CmdLineInfo;
	m_CmdLineInfo = nullptr;
}


//-----------------------------------------------------------------------------
// Inherited from IAppSystem
//-----------------------------------------------------------------------------
bool CHammer::Connect( CreateInterfaceFn factory )
{
	EnableCrashingOnCrashes();
	if ( !BaseClass::Connect( factory ) )
		return false;

//	bool bCVarOk = ConnectStudioRenderCVars( factory );
	g_pFileSystem = ( IBaseFileSystem* )factory( BASEFILESYSTEM_INTERFACE_VERSION, NULL );
	g_pStudioRender = ( IStudioRender* )factory( STUDIO_RENDER_INTERFACE_VERSION, NULL );
	g_pStudioDataCache = ( IStudioDataCache* )factory( STUDIO_DATA_CACHE_INTERFACE_VERSION, NULL );
	g_pMDLCache = ( IMDLCache* )factory( MDLCACHE_INTERFACE_VERSION, NULL );
	g_Factory = factory;

	if ( !g_pMDLCache || !g_pFileSystem || !g_pFullFileSystem || !materials || !g_pMaterialSystemHardwareConfig || !g_pStudioRender || !g_pStudioDataCache )
		return false;

	InstallDmElementFactories();

	// ensure we're in the same directory as the .EXE
	char module[MAX_PATH];
	GetModuleFileName(NULL, module, MAX_PATH);
	V_ExtractFilePath(module, m_szAppDir, MAX_PATH);
	V_StripTrailingSlash(m_szAppDir);

	// Build hammer paths
	char hammerDir[MAX_PATH];
	V_ComposeFileName(m_szAppDir, "hammer", hammerDir, MAX_PATH);
	g_pFullFileSystem->AddSearchPath(hammerDir, "hammer", PATH_ADD_TO_HEAD);
	g_pFullFileSystem->AddSearchPath(hammerDir, "SKIN", PATH_ADD_TO_HEAD);
	char hammerPrefabs[MAX_PATH];
	V_ComposeFileName(hammerDir, "prefabs", hammerPrefabs, MAX_PATH);
	g_pFullFileSystem->CreateDirHierarchy("prefabs", "hammer"); // Create the prefabs folder if it doesn't already exist
	g_pFullFileSystem->AddSearchPath(hammerPrefabs, "hammer_prefabs", PATH_ADD_TO_HEAD);

	// g_pVGuiLocalize->AddFile("resource/hammer_english.txt", "hammer");

	// Create the message window object for capturing errors and warnings.
	// This does NOT create the window itself. That happens later in CMainFrame::Create.
	g_pwndMessage = CMessageWnd::CreateMessageWndObject();

	ParseCommandLine(*m_CmdLineInfo);
	if (!m_CmdLineInfo->m_bSetCustomConfigDir)
	{
		// Default location for GameConfig.txt is in ./hammer/cfg/ but this may be overridden on the command line
		char szGameConfigDir[MAX_PATH];
		GetDirectory( DIR_PROGRAM, szGameConfigDir );
		CFmtStrN<MAX_PATH> dir("%s/hammer/cfg", szGameConfigDir);
		g_pFullFileSystem->AddSearchPath(dir.Get(), "hammer_cfg", PATH_ADD_TO_HEAD);
		V_FixupPathName( szGameConfigDir, MAX_PATH, dir.Get() );
		Options.configs.m_strConfigDir = szGameConfigDir;
	}

	m_pConfig = new KeyValues( "Config" );
	m_pConfig->UsesConditionals( false );
	m_pConfig->LoadFromFile( g_pFileSystem, "HammerConfig.vdf", "hammer_cfg" );

	// Load the options
	// NOTE: Have to do this now, because we need it before Inits() are called
	// NOTE: SetRegistryKey will cause hammer to look into the registry for its values
	m_pszAppName = strdup( "Hammer 2K13" );
	SetRegistryKey( "Valve" );
	Options.Init();
	CMFCVisualManager::SetDefaultManager( RUNTIME_CLASS( CMFCVisualManagerWindows ) );
	return true;
}


void CHammer::Disconnect()
{
	g_pStudioRender = NULL;
	g_pStudioDataCache = NULL;
	g_pFileSystem = NULL;
	g_pMDLCache = NULL;
	BaseClass::Disconnect();
}

void *CHammer::QueryInterface( const char *pInterfaceName )
{
	// We also implement the IMatSystemSurface interface
	if (!Q_strncmp(	pInterfaceName, INTERFACEVERSION_HAMMER, Q_strlen(INTERFACEVERSION_HAMMER) + 1))
		return (IHammer*)this;

	return NULL;
}


//-----------------------------------------------------------------------------
// Methods related to message pumping
//-----------------------------------------------------------------------------
bool CHammer::HammerPreTranslateMessage(MSG * pMsg)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	// Copy this into the current message, needed for MFC
#if _MSC_VER >= 1300
	_AFX_THREAD_STATE* pState = AfxGetThreadState();
	pState->m_msgCur = *pMsg;
#else
	m_msgCur = *pMsg;
#endif

	return (/*pMsg->message == WM_KICKIDLE ||*/ PreTranslateMessage(pMsg) != FALSE);
}

bool CHammer::HammerIsIdleMessage(MSG * pMsg)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	// We generate lots of WM_TIMER messages and shouldn't call OnIdle because of them.
	// This fixes tool tips not popping up when a map is open.
	if ( pMsg->message == WM_TIMER )
		return false;

	return IsIdleMessage(pMsg) != FALSE;
}

// return TRUE if more idle processing
bool CHammer::HammerOnIdle(long count)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return OnIdle(count) != FALSE;
}


//-----------------------------------------------------------------------------
// Purpose: Adds a backslash to the end of a string if there isn't one already.
// Input  : psz - String to add the backslash to.
//-----------------------------------------------------------------------------
static void EnsureTrailingBackslash(char *psz)
{
	if ((psz[0] != '\0') && (psz[strlen(psz) - 1] != '\\'))
	{
		strcat(psz, "\\");
	}
}


//-----------------------------------------------------------------------------
// Purpose: Tweaks our data members to enable us to import old Hammer settings
//			from the registry.
//-----------------------------------------------------------------------------
static const char *s_pszOldAppName = NULL;
void CHammer::BeginImportWCSettings(void)
{
	s_pszOldAppName = m_pszAppName;
	m_pszAppName = "Worldcraft";
	SetRegistryKey("Valve");
}


//-----------------------------------------------------------------------------
// Purpose: Tweaks our data members to enable us to import old Valve Hammer Editor
//			settings from the registry.
//-----------------------------------------------------------------------------
void CHammer::BeginImportVHESettings(void)
{
	s_pszOldAppName = m_pszAppName;
	m_pszAppName = "Valve Hammer Editor";
	SetRegistryKey("Valve");
}


//-----------------------------------------------------------------------------
// Purpose: Restores our tweaked data members to their original state.
//-----------------------------------------------------------------------------
void CHammer::EndImportSettings(void)
{
	m_pszAppName = s_pszOldAppName;
	SetRegistryKey("Valve");
}


//-----------------------------------------------------------------------------
// Purpose: Retrieves various important directories.
// Input  : dir - Enumerated directory to retrieve.
//			p - Pointer to buffer that receives the full path to the directory.
//-----------------------------------------------------------------------------
void CHammer::GetDirectory(DirIndex_t dir, char *p) const
{
	switch (dir)
	{
		case DIR_PROGRAM:
		{
			strcpy(p, m_szAppDir);
			EnsureTrailingBackslash(p);
			break;
		}

		case DIR_PREFABS:
		{
			g_pFullFileSystem->GetSearchPath("hammer_prefabs", false, p, MAX_PATH);
			break;
		}

		//
		// Get the game directory with a trailing backslash. This is
		// where the base game's resources are, such as "C:\Half-Life\valve\".
		//
		case DIR_GAME_EXE:
		{
			strcpy(p, g_pGameConfig->m_szGameExeDir);
			EnsureTrailingBackslash(p);
			break;
		}

		//
		// Get the mod directory with a trailing backslash. This is where
		// the mod's resources are, such as "C:\Half-Life\tfc\".
		//
		case DIR_MOD:
		{
			strcpy(p, g_pGameConfig->m_szModDir);
			EnsureTrailingBackslash(p);
			break;
		}

		//
		// Get the materials directory with a trailing backslash. This is where
		// the mod's materials are, such as "C:\Half-Life\tfc\materials".
		//
		case DIR_MATERIALS:
		{
			strcpy(p, g_pGameConfig->m_szModDir);
			EnsureTrailingBackslash(p);
			Q_strcat(p, "materials\\", MAX_PATH);
			break;
		}

		case DIR_AUTOSAVE:
		{
			strcpy( p, m_szAutosaveDir );
			EnsureTrailingBackslash(p);
			break;
		}
		default:
			break;
	}
}

void CHammer::SetDirectory(DirIndex_t dir, const char *p)
{
	switch(dir)
	{
		case DIR_AUTOSAVE:
		{
			strcpy( m_szAutosaveDir, p );
			break;
		}
	}
}

UINT CHammer::GetProfileIntA( LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault )
{
	KeyValues* data = m_pConfig->FindKey( lpszSection, true );
	return data->GetInt( lpszEntry, nDefault );
}

CString CHammer::GetProfileStringA( LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault )
{
	KeyValues* data = m_pConfig->FindKey( lpszSection, true );
	return data->GetString( lpszEntry, lpszDefault );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a color from the application configuration storage.
//-----------------------------------------------------------------------------
COLORREF CHammer::GetProfileColor(LPCTSTR lpszSection, LPCTSTR lpszEntry, int r, int g, int b)
{
	KeyValues* data = m_pConfig->FindKey( lpszSection, true );
	const Color& color = data->GetColor( lpszEntry, Color( r, g, b ) );
	return COLORREF( RGB( color.r(), color.g(), color.b() ) );
}

BOOL CHammer::WriteProfileInt( LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue )
{
	KeyValues* data = m_pConfig->FindKey( lpszSection, true );
	data->SetInt( lpszEntry, nValue );
	return true;
}

BOOL CHammer::WriteProfileStringA( LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue )
{
	KeyValues* data = m_pConfig->FindKey( lpszSection, true );
	data->SetString( lpszEntry, lpszValue );
	return true;
}

void CHammer::WriteProfileColor( LPCTSTR lpszSection, LPCTSTR lpszEntry, COLORREF clr )
{
	KeyValues* data = m_pConfig->FindKey( lpszSection, true );
	data->SetColor( lpszEntry, Color::RawColor( clr ) );
}

KeyValues* CHammer::GetProfileKeyValues( LPCTSTR lpszSection )
{
	return m_pConfig->FindKey( lpszSection, true );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pszURL -
//-----------------------------------------------------------------------------
void CHammer::OpenURL(const char *pszURL, HWND hwnd)
{
	if (HINSTANCE(32) > ::ShellExecute(hwnd, "open", pszURL, NULL, NULL, 0))
	{
		AfxMessageBox("The website couldn't be opened.");
	}
}


//-----------------------------------------------------------------------------
// Purpose: Opens a URL in the default web browser by string ID.
//-----------------------------------------------------------------------------
void CHammer::OpenURL(UINT nID, HWND hwnd)
{
	CString str;
	str.LoadString(nID);
	OpenURL(str, hwnd);
}



static SpewRetval_t HammerDbgOutput( SpewType_t spewType, const char* pMsg )
{
	// FIXME: The messages we're getting from the material system
	// are ones that we really don't care much about.
	// I'm disabling this for now, we need to decide about what to do with this

	// Ugly message
	// Too many popups! Rendering will be bad!
	if ( spewType == SPEW_WARNING && !V_strnicmp( pMsg, "Too many popups!", ARRAYSIZE( "Too many popups!" ) - 1 ) )
		return SPEW_CONTINUE;

	// Also skip "GetStandardTextureDimensions: Couldn't find the texture to get the dimensions!"
	if ( spewType == SPEW_WARNING && !V_strnicmp( pMsg, "GetStandardTextureDimensions", ARRAYSIZE( "GetStandardTextureDimensions" ) - 1 ) )
		return SPEW_CONTINUE;

	// Also skip "Material %s..."
	if ( spewType == SPEW_WARNING && !V_strnicmp( pMsg, "Material ", ARRAYSIZE( "Material " ) - 1 ) )
		return SPEW_CONTINUE;

	// Also skip "Requesting texture value ..."
	if ( spewType == SPEW_WARNING && !V_strnicmp( pMsg, "Requesting texture ", ARRAYSIZE( "Requesting texture " ) - 1 ) )
		return SPEW_CONTINUE;

	if ( g_pwndMessage && g_pwndMessage->IsValid() )
	{
		Color clr = *GetSpewOutputColor();
		if ( clr.GetRawColor() == 0xFFFFFFFF )
		{
			switch( spewType )
			{
			case SPEW_WARNING:
				clr.SetColor( 196, 80, 80 );
				break;
			case SPEW_ASSERT:
				clr.SetColor( 0, 255, 0 );
				break;
			case SPEW_ERROR:
				clr.SetColor( 255, 0, 0 );
				break;
			case SPEW_MESSAGE:
			case SPEW_LOG:
				clr.SetColor( 0, 0, 0 );
				break;
			}
		}
		g_pwndMessage->AddMsg( clr, pMsg );
	}

	switch( spewType )
	{
	case SPEW_ERROR:
		MessageBox( NULL, pMsg, "Fatal Error", MB_OK | MB_ICONINFORMATION );
#ifdef _DEBUG
		return SPEW_DEBUGGER;
#else
		TerminateProcess( GetCurrentProcess(), 1 );
		return SPEW_ABORT;
#endif

	default:
		OutputDebugString( pMsg );
		return (spewType == SPEW_ASSERT) ? SPEW_DEBUGGER : SPEW_CONTINUE;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static HANDLE dwChangeHandle = NULL;
void UpdatePrefabs_Init()
{

	// Watch the prefabs tree for file or directory creation
	// and deletion.
	if (dwChangeHandle == NULL)
	{
		char szPrefabDir[MAX_PATH];
		APP()->GetDirectory(DIR_PREFABS, szPrefabDir);

		dwChangeHandle = FindFirstChangeNotification(
			szPrefabDir,													// directory to watch
			TRUE,															// watch the subtree
			FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_FILE_NAME);	// watch file and dir name changes

		if (dwChangeHandle == INVALID_HANDLE_VALUE)
		{
			ExitProcess(GetLastError());
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void UpdatePrefabs()
{
	// Wait for notification.
	DWORD dwWaitStatus = WaitForSingleObject(dwChangeHandle, 0);

	if (dwWaitStatus == WAIT_OBJECT_0)
	{
		// A file was created or deleted in the prefabs tree.
		// Refresh the prefabs and restart the change notification.
		CPrefabLibrary::FreeAllLibraries();
		CPrefabLibrary::LoadAllLibraries();
		GetMainWnd()->m_ObjectBar.UpdateListForTool(ToolManager()->GetActiveToolID());

		if (FindNextChangeNotification(dwChangeHandle) == FALSE)
		{
			ExitProcess(GetLastError());
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void UpdatePrefabs_Shutdown()
{
	FindCloseChangeNotification(dwChangeHandle);
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
BOOL CHammer::InitInstance()
{
	SetRegistryKey( "Valve" );
	return CWinApp::InitInstance();
}


//-----------------------------------------------------------------------------
// Purpose: Prompt the user to select a game configuration.
//-----------------------------------------------------------------------------
CGameConfig *CHammer::PromptForGameConfig()
{
	CEditGameConfigs dlg(TRUE, GetMainWnd());
	if (dlg.DoModal() != IDOK)
	{
		return NULL;
	}

	return dlg.GetSelectedGame();
}


//-----------------------------------------------------------------------------
// Purpose:
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHammer::InitSessionGameConfig(const char *szGame)
{
	CGameConfig *pConfig = NULL;
	bool bManualChoice = false;

	if ( CommandLine()->FindParm( "-chooseconfig" ) )
	{
		pConfig = PromptForGameConfig();
		bManualChoice = true;
	}

	if (!bManualChoice)
	{
		if (szGame && szGame[0] != '\0')
		{
			// They passed in -game on the command line, use that.
			pConfig = Options.configs.FindConfigByName(szGame);
			if (!pConfig)
			{
				Msg(mwError, "Invalid config \"%s\" specified on the command-line, ignoring.", szGame);
			}
		}
		else
		{
			// No -game on the command line, try using VPROJECT.
			const char *pszGameDir = getenv("vproject");
			if ( pszGameDir )
			{
				pConfig = Options.configs.FindConfigForGame(pszGameDir);
				if (!pConfig)
				{
					Msg(mwError, "Invalid game \"%s\" found in VPROJECT environment variable, ignoring.", pszGameDir);
				}
			}
		}
	}

	if (pConfig == NULL)
	{
		// Nothing useful was passed in or found in VPROJECT.

		// If there's only one config, use that.
		if (Options.configs.GetGameConfigCount() == 1)
		{
			pConfig = Options.configs.GetGameConfig(0);
		}
		else
		{
			// Otherwise, prompt for a config to use.
			pConfig = PromptForGameConfig();
		}
	}

	if (pConfig)
	{
		CGameConfig::SetActiveGame(pConfig);
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if Hammer is in the process of shutting down.
//-----------------------------------------------------------------------------
InitReturnVal_t CHammer::Init()
{
	return (InitReturnVal_t)WrapFunctionWithMinidumpHandler( StaticHammerInternalInit, this, INIT_FAILED );
}


int CHammer::StaticHammerInternalInit( void *pParam )
{
	return (int)((CHammer*)pParam)->HammerInternalInit();
}

static SpewOutputFunc_t oldSpewFunc = NULL;
InitReturnVal_t CHammer::HammerInternalInit()
{
	oldSpewFunc = GetSpewOutputFunc();
	SpewActivate( "console", 1 );
	SpewOutputFunc( HammerDbgOutput );
	MathLib_Init();
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	//
	// Create a custom window class for this application so that engine's
	// FindWindow will find us.
	//
	WNDCLASS wndcls;
	memset(&wndcls, 0, sizeof(WNDCLASS));
	wndcls.style			= CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
	wndcls.lpfnWndProc		= AfxWndProc;
	wndcls.hInstance		= AfxGetInstanceHandle();
	wndcls.hIcon			= LoadIcon(IDR_MAINFRAME);
	wndcls.hCursor			= LoadCursor( IDC_ARROW );
	wndcls.hbrBackground	= (HBRUSH) 0; //  (COLOR_WINDOW + 1);
	wndcls.lpszMenuName		= "IDR_MAINFRAME";
	wndcls.cbWndExtra		= 0;

	// HL Shell class name
	wndcls.lpszClassName = "VALVEWORLDCRAFT";

	// Register it, exit if it fails
	if(!AfxRegisterClass(&wndcls))
	{
		AfxMessageBox("Could not register Hammer's main window class");
		return INIT_FAILED;
	}

	srand(time(NULL));

	WriteProfileString("General", "Directory", m_szAppDir);

	//
	// Create a window to receive shell commands from the engine, and attach it
	// to our shell processor.
	//
	g_ShellMessageWnd.Create();
	g_ShellMessageWnd.SetShell(&g_Shell);

	//
	// Create and optionally display the splash screen.
	//
	CSplashWnd::EnableSplashScreen(m_CmdLineInfo->m_bShowLogo);

	LoadSequences();	// load cmd sequences - different from options because
						//  users might want to share (darn registry)

	// other init:
	randomize();

	/*
#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif
	*/

	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.
	pMapDocTemplate = new CHammerDocTemplate(
		IDR_MAPDOC,
		RUNTIME_CLASS(CMapDoc),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CMapView2D));
	AddDocTemplate(pMapDocTemplate);

	pManifestDocTemplate = new CHammerDocTemplate(
		IDR_MANIFESTDOC,
		RUNTIME_CLASS(CManifest),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CMapView2D));
	HINSTANCE hInst = AfxFindResourceHandle( MAKEINTRESOURCE( IDR_MAPDOC ), RT_MENU );
	pManifestDocTemplate->m_hMenuShared = ::LoadMenu( hInst, MAKEINTRESOURCE( IDR_MAPDOC ) );
	AddDocTemplate(pManifestDocTemplate);

	// register shell file types
	RegisterShellFileTypes();

	//
	// Initialize the rich edit control so we can use it in the entity help dialog.
	//
	AfxInitRichEdit();

	//
	// Create main MDI Frame window. Must be done AFTER registering the multidoc template!
	//
	CMainFrame *pMainFrame = new CMainFrame;
	if (!pMainFrame->LoadFrame(IDR_MAINFRAME))
		return INIT_FAILED;

	m_pMainWnd = pMainFrame;

	// try to init VGUI
	HammerVGui()->Init( m_pMainWnd->GetSafeHwnd() );

	// The main window has been initialized, so show and update it.
	//
	m_nCmdShow = SW_SHOWMAXIMIZED;
	pMainFrame->ShowWindow(m_nCmdShow);
	pMainFrame->UpdateWindow();

	//
	// Init the game and mod dirs in the file system.
	// This needs to happen before calling Init on the material system.
	//
	CFSSearchPathsInit initInfo;
	initInfo.m_pFileSystem = g_pFullFileSystem;
	initInfo.m_pDirectoryName = g_pGameConfig->m_szModDir;
	if ( !initInfo.m_pDirectoryName[0] )
	{
		static char pTempBuf[MAX_PATH];
		GetDirectory(DIR_PROGRAM, pTempBuf);
		strcat( pTempBuf, "..\\hl2" );
		initInfo.m_pDirectoryName = pTempBuf;
	}

	// Initialize the sound system engine
	g_Sounds.InitializeEngine();

	CSplashWnd::ShowSplashScreen(pMainFrame);

	if ( FileSystem_LoadSearchPaths( initInfo ) != FS_OK )
	{
		Error( "Unable to load search paths!\n" );
	}

	// Now that we've initialized the file system, we can parse this config's gameinfo.txt for the additional settings there.
	g_pGameConfig->ParseGameInfo();

	materials->ModInit();

	// Initialize Keybinds
	if (!g_pKeyBinds->Init())
		Error("Unable to load keybinds!");

	if (!g_pKeyBinds->GetAccelTableFor("IDR_MAINFRAME", pMainFrame->m_hAccelTable))
		Error("Failed to load custom keybinds for IDR_MAINFRAME!");

	if (!g_pKeyBinds->GetAccelTableFor("IDR_MAPDOC", pMapDocTemplate->m_hAccelTable))
		Error("Failed to load custom keybinds for the IDR_MAPDOC!");

	if (!g_pKeyBinds->GetAccelTableFor("IDR_MAPDOC", pManifestDocTemplate->m_hAccelTable))
		Error("Failed to load custom keybinds for the IDR_MAPDOC!");

	//
	// Initialize the texture manager and load all textures.
	//
	if (!g_Textures.Initialize(m_pMainWnd->m_hWnd))
	{
		Msg(mwError, "Failed to initialize texture system.");
	}
	else
	{
		//
		// Initialize studio model rendering (must happen after g_Textures.Initialize since
		// g_Textures.Initialize kickstarts the material system and sets up g_MaterialSystemClientFactory)
		//
		StudioModel::Initialize();
		g_Textures.LoadAllGraphicsFiles();
		g_Textures.SetActiveConfig(g_pGameConfig);
	}

	//
	// Initialize the particle system manager
	//
	g_pParticleSystemMgr->Init( NULL );
	g_pParticleSystemMgr->AddBuiltinSimulationOperators();
	g_pParticleSystemMgr->AddBuiltinRenderingOperators();

	// Watch for changes to models.
	InitStudioFileChangeWatcher();

	LoadFileSystemDialogModule();

	// Load detail object descriptions.
	char	szGameDir[_MAX_PATH];
	GetDirectory(DIR_MOD, szGameDir);
	DetailObjects::LoadEmitDetailObjectDictionary( szGameDir );

	// Initialize the sound system
	g_Sounds.Initialize();

	extern void ScriptInit();
	ScriptInit();

	UpdatePrefabs_Init();

	// Indicate that we are ready to use.
	m_pMainWnd->FlashWindow(TRUE);

	// Parse command line for standard shell commands, DDE, file open
	if ( Q_stristr(m_CmdLineInfo->m_strFileName, ".vmf" ) )
	{
		// we don't want to make a new file (default behavior if no file
		//  is specified on the commandline.)

		// Dispatch commands specified on the command line
		if (!ProcessShellCommand(*m_CmdLineInfo))
			return INIT_FAILED;
	}

	if ( Options.general.bClosedCorrectly == FALSE )
	{
		CString strLastGoodSave = GetProfileString("General", "Last Good Save", "");

		if ( strLastGoodSave.GetLength() != 0 )
		{
			char msg[1024];
			V_snprintf( msg, sizeof( msg ), "Hammer did not shut down correctly the last time it was used.\nWould you like to load the last saved file?\n(%s)", (const char*)strLastGoodSave );
			if ( AfxMessageBox( msg, MB_YESNO ) == IDYES )
			{
				LoadLastGoodSave();
			}
		}
	}

#ifdef VPROF_HAMMER
	g_VProfCurrentProfile.Start();
#endif

	// Execute the post-init registered callbacks
	for ( int iFn = 0; iFn < s_appRegisteredPostInitFns.Count(); ++ iFn )
	{
		void (*fn)() = s_appRegisteredPostInitFns[ iFn ];
		(*fn)();
	}

	CSplashWnd::HideSplashScreen();

	// create the lighting preview thread
	g_LPreviewThread = CreateSimpleThread( LightingPreviewThreadFN, 0 );

	return INIT_OK;
}

void CHammer::EditKeyBindings()
{
	DestroyAcceleratorTable( static_cast<CMainFrame*>( m_pMainWnd )->m_hAccelTable );
	DestroyAcceleratorTable( pMapDocTemplate->m_hAccelTable );
	DestroyAcceleratorTable( pManifestDocTemplate->m_hAccelTable );

	pManifestDocTemplate->m_hAccelTable = nullptr;
	pMapDocTemplate->m_hAccelTable = nullptr;
	static_cast<CMainFrame*>( m_pMainWnd )->m_hAccelTable = nullptr;

	CKeybindEditor editor( m_pMainWnd );
	editor.Show();
	editor.DoModal();

	g_pKeyBinds->GetAccelTableFor( "IDR_MAINFRAME", static_cast<CMainFrame*>( m_pMainWnd )->m_hAccelTable );
	g_pKeyBinds->GetAccelTableFor( "IDR_MAPDOC", pMapDocTemplate->m_hAccelTable );
	g_pKeyBinds->GetAccelTableFor( "IDR_MAPDOC", pManifestDocTemplate->m_hAccelTable );

	CHammerDocTemplate* templates[] = { pMapDocTemplate, pManifestDocTemplate };

	for ( CHammerDocTemplate* pTemplate : templates )
	{
		for ( POSITION pos = pTemplate->GetFirstDocPosition(); pos != NULL; )
		{
			CDocument* pDoc = pTemplate->GetNextDoc( pos );
			ASSERT_VALID( pDoc );

			for ( POSITION posView = pDoc->GetFirstViewPosition(); posView != NULL; )
			{
				CView* pView = pDoc->GetNextView( posView );
				ASSERT_VALID( pView );

				CFrameWnd* pFrame = pView->GetParentFrame();
				ASSERT_VALID( pFrame );

				pFrame->m_hAccelTable = pTemplate->m_hAccelTable;

				if ( auto view = dynamic_cast<CMapView3D*>( pView ) )
					view->InitializeKeyMap();
			}
		}
	}
}

static unsigned WriteCrashSave( void* )
{
	CHammerDocTemplate* templates[] = { APP()->pMapDocTemplate, APP()->pManifestDocTemplate };

	char szRootDir[MAX_PATH];
	APP()->GetDirectory( DIR_AUTOSAVE, szRootDir );
	CString strAutosaveDirectory( szRootDir );
	EditorUtil_ConvertPath( strAutosaveDirectory, true );

	for ( CHammerDocTemplate* pTemplate : templates )
	{
		for ( POSITION pos = pTemplate->GetFirstDocPosition(); pos != NULL; )
		{
			CDocument* pDoc = pTemplate->GetNextDoc( pos );
			ASSERT_VALID( pDoc );
			CMapDoc* pMapDoc = dynamic_cast<CMapDoc*>( pDoc );

			CString strExtension  = ".vmf_autosave";
			//this will hold the name of the map w/o leading directory info or file extension
			CString strMapTitle;
			//full path of map file
			CString strMapFilename = pDoc->GetPathName();

			// the map hasn't been saved before and doesn't have a filename; using default: 'autosave'
			if ( strMapFilename.IsEmpty() )
			{
				strMapTitle = "autosave";
			}
			// the map already has a filename
			else
			{
				int nFilenameBeginOffset = strMapFilename.ReverseFind( '\\' ) + 1;
				int nFilenameEndOffset = strMapFilename.Find( '.' );
				//get the filename of the map, between the leading '\' and the '.'
				strMapTitle = strMapFilename.Mid( nFilenameBeginOffset, nFilenameEndOffset - nFilenameBeginOffset );
			}

			CString strSaveName = strAutosaveDirectory + strMapTitle + "_crash" + strExtension;

			pMapDoc->SaveVMF( strSaveName, SAVEFLAGS_AUTOSAVE | SAVEFLAGS_NO_UI_UPDATE );
		}
	}
	return 0;
}

static LONG WINAPI WrapperException( _EXCEPTION_POINTERS* pExceptionInfo )
{
	auto thread = CreateSimpleThread( WriteCrashSave, nullptr ); // new thread with free stack space
	ThreadJoin( thread );
	ReleaseThreadHandle( thread );

	return EXCEPTION_CONTINUE_SEARCH;
}

int CHammer::MainLoop()
{
	SetUnhandledExceptionFilter( WrapperException );
	AddVectoredExceptionHandler( 1, WrapperException );
	return WrapFunctionWithMinidumpHandler( StaticInternalMainLoop, this, -1 );
}


int CHammer::StaticInternalMainLoop( void *pParam )
{
	return ((CHammer*)pParam)->InternalMainLoop();
}


int CHammer::InternalMainLoop()
{
	MSG msg;

	g_pDataCache->SetSize( 128 * 1024 * 1024 );

	// For tracking the idle time state
	bool bIdle = true;
	long lIdleCount = 0;

	// We've got our own message pump here
	g_pInputSystem->EnableMessagePump( false );

	// Acquire and dispatch messages until a WM_QUIT message is received.
	for (;;)
	{
		RunFrame();

		if ( bIdle && !HammerOnIdle(lIdleCount++) )
		{
			bIdle = false;
		}

		// Execute the message loop registered callbacks
		for ( int iFn = 0; iFn < s_appRegisteredMessageLoop.Count(); ++ iFn )
		{
			void (*fn)() = s_appRegisteredMessageLoop[ iFn ];
			(*fn)();
		}

		//
		// Pump messages until the message queue is empty.
		//
		while (::PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE))
		{
			if ( msg.message == WM_QUIT )
				return 1;

			// Pump the message through a custom message
			// pre-translation chain
			for ( int iFn = 0; iFn < s_appRegisteredMessagePreTrans.Count(); ++ iFn )
			{
				void (*fn)( MSG * ) = s_appRegisteredMessagePreTrans[ iFn ];
				(*fn)( &msg );
			}

			if ( !HammerPreTranslateMessage(&msg) )
			{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}

			// Reset idle state after pumping idle message.
			if ( HammerIsIdleMessage(&msg) )
			{
				bIdle = true;
				lIdleCount = 0;
			}
		}
	}

	Assert(0);  // not reachable
}

//-----------------------------------------------------------------------------
// Shuts down hammer
//-----------------------------------------------------------------------------
void CHammer::Shutdown()
{
	if ( g_LPreviewThread )
	{
		MessageToLPreview StopMsg( LPREVIEW_MSG_EXIT );
		g_HammerToLPreviewMsgQueue.QueueMessage( StopMsg );
		ThreadJoin( g_LPreviewThread );
		g_LPreviewThread = 0;
	}

	// Execute the pre-shutdown registered callbacks
	for ( int iFn = s_appRegisteredPreShutdownFns.Count(); iFn --> 0 ; )
	{
		void (*fn)() = s_appRegisteredPreShutdownFns[ iFn ];
		(*fn)();
	}

#ifdef VPROF_HAMMER
	g_VProfCurrentProfile.Stop();
#endif

	// PrintBudgetGroupTimes_Recursive( g_VProfCurrentProfile.GetRoot() );

	extern void ScriptShutdown();
	ScriptShutdown();

	HammerVGui()->Shutdown();

	UnloadFileSystemDialogModule();

	// Delete the command sequences.
	int nSequenceCount = m_CmdSequences.GetSize();
	for (int i = 0; i < nSequenceCount; i++)
	{
		CCommandSequence *pSeq = m_CmdSequences[i];
		if ( pSeq != NULL )
		{
			delete pSeq;
			m_CmdSequences[i] = NULL;
		}
	}

	g_Textures.ShutDown();

	// Shutdown the sound system
	g_Sounds.ShutDown();

	materials->ModShutdown();
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Methods used by the engine
//-----------------------------------------------------------------------------
const char *CHammer::GetDefaultMod()
{
	return g_pGameConfig->GetMod();
}

const char *CHammer::GetDefaultGame()
{
	return g_pGameConfig->GetGame();
}

const char *CHammer::GetDefaultModFullPath()
{
	return g_pGameConfig->m_szModDir;
}


//-----------------------------------------------------------------------------
// Pops up the options dialog
//-----------------------------------------------------------------------------
RequestRetval_t CHammer::RequestNewConfig()
{
	if ( !Options.RunConfigurationDialog() )
		return REQUEST_QUIT;

	return REQUEST_OK;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if Hammer is in the process of shutting down.
//-----------------------------------------------------------------------------
bool CHammer::IsClosing()
{
	return m_bClosing;
}


//-----------------------------------------------------------------------------
// Purpose: Signals the beginning of app shutdown. Should be called before
//			rendering views.
//-----------------------------------------------------------------------------
void CHammer::BeginClosing()
{
	m_bClosing = true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CHammer::ExitInstance()
{
	if ( GetSpewOutputFunc() == HammerDbgOutput )
	{
		SpewOutputFunc( oldSpewFunc );
	}

	g_ShellMessageWnd.DestroyWindow();

	UpdatePrefabs_Shutdown();

	SaveStdProfileSettings();

	SaveConfig();
	m_pConfig->deleteThis();
	m_pConfig = NULL;

	return CWinApp::ExitInstance();
}

void CHammer::SaveConfig()
{
	// Too bad filesystem doesn't exist at shutdown
	CUtlBuffer buffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	m_pConfig->RecursiveSaveToFile( buffer, 0, true, true );

	FILE* file;
	if ( fopen_s( &file, CFmtStrN<MAX_PATH>( "%s" CORRECT_PATH_SEPARATOR_S "HammerConfig.vdf", (const char*)Options.configs.m_strConfigDir ), "w" ) == 0 )
	{
		fwrite( buffer.String(), sizeof( char ), buffer.TellPut(), file );
		fflush( file );
		fclose( file );
	}
}

//-----------------------------------------------------------------------------
// Purpose: this function sets the global flag indicating if new documents should
//			be visible.
// Input  : bIsVisible - flag to indicate visibility status.
//-----------------------------------------------------------------------------
void CHammer::SetIsNewDocumentVisible( bool bIsVisible )
{
	CHammer::m_bIsNewDocumentVisible = bIsVisible;
}


//-----------------------------------------------------------------------------
// Purpose: this functionr eturns the global flag indicating if new documents should
//			be visible.
//-----------------------------------------------------------------------------
bool CHammer::IsNewDocumentVisible( void )
{
	return CHammer::m_bIsNewDocumentVisible;
}


/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	afx_msg void OnBnClickedReportIssue();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}



// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CAboutDlg::OnInitDialog(void)
{
	CDialog::OnInitDialog();

	//
	// Display the build number.
	//
	CWnd *pWnd = GetDlgItem(IDC_BUILD_NUMBER);
	if (pWnd != NULL)
	{
		char szTemp2[MAX_PATH];
		constexpr int nBuild = build_number();
		sprintf(szTemp2, "Build %d", nBuild);
		pWnd->SetWindowText(szTemp2);
	}

	return TRUE;
}

void CAboutDlg::OnBnClickedReportIssue()
{
	APP()->OpenURL("https://github.com/Gocnak/sdk-2013-hammer/issues", m_hWnd);
}


BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
	ON_BN_CLICKED(IDC_REPORT_ISSUE, &ThisClass::OnBnClickedReportIssue)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHammer::OnAppAbout(void)
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();

#ifdef VPROF_HAMMER
	g_VProfCurrentProfile.OutputReport();
	g_VProfCurrentProfile.Reset();
	g_pMemAlloc->DumpStats();
#endif

}



//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHammer::OnFileNew(void)
{
	pMapDocTemplate->OpenDocumentFile(NULL);
	if(Options.general.bLoadwinpos && Options.general.bIndependentwin)
	{
		::GetMainWnd()->LoadWindowStates();
	}
	DiscordUpdatePresence();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHammer::OnFileOpen(void)
{
	static char szInitialDir[MAX_PATH] = "";
	if (szInitialDir[0] == '\0')
	{
		strcpy(szInitialDir, g_pGameConfig->szMapDir);
	}

	CFileDialog dlg(TRUE, NULL, NULL, OFN_LONGNAMES | OFN_HIDEREADONLY | OFN_NOCHANGEDIR, "Valve Map Files (*.vmf;*.vmm)|*.vmf;*.vmm|Valve Map Files Autosave (*.vmf_autosave)|*.vmf_autosave||");
	dlg.m_ofn.lpstrInitialDir = szInitialDir;
	int iRvl = dlg.DoModal();

	if (iRvl == IDCANCEL)
	{
		return;
	}

	//
	// Get the directory they browsed to for next time.
	//
	CString str = dlg.GetPathName();
	int nSlash = str.ReverseFind('\\');
	if (nSlash != -1)
	{
		strcpy(szInitialDir, str.Left(nSlash));
	}

	if (str.Find('.') == -1)
	{
		switch (dlg.m_ofn.nFilterIndex)
		{
			case 1:
			{
				str += ".vmf";
				break;
			}

			case 2:
			{
				str += ".vmf_autosave";
				break;
			}
		}
	}

	OpenDocumentFile(str);
}


//-----------------------------------------------------------------------------
// This is the generic file open function that is called by the framework.
//-----------------------------------------------------------------------------
CDocument *CHammer::OpenDocumentFile(LPCTSTR lpszFileName)
{
	CDocument *pDoc = OpenDocumentOrInstanceFile( lpszFileName );

	// Do work that needs to happen after opening all instances here.
	// NOTE: Make sure this work doesn't need to happen per instance!!!

	return pDoc;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CDocument *CHammer::OpenDocumentOrInstanceFile(LPCTSTR lpszFileName)
{
	// CWinApp::OnOpenRecentFile may get its file history cycled through by instances being opened, thus the pointer becomes invalid
	CString		SaveFileName = lpszFileName;

	if(GetFileAttributes( SaveFileName ) == 0xFFFFFFFF)
	{
		CString		Message;

		Message = "The file " + SaveFileName + " does not exist.";
		AfxMessageBox( Message );
		return NULL;
	}

	CDocument	*pDoc = m_pDocManager->OpenDocumentFile( SaveFileName );
	CMapDoc		*pMapDoc = dynamic_cast< CMapDoc * >( pDoc );

	if ( pMapDoc )
	{
		CMapDoc::SetActiveMapDoc( pMapDoc );
		pMapDoc->CheckFileStatus();
	}

	if( pDoc && Options.general.bLoadwinpos && Options.general.bIndependentwin)
	{
		::GetMainWnd()->LoadWindowStates();
	}

	if ( pMapDoc && !CHammer::IsNewDocumentVisible() )
	{
		pMapDoc->ShowWindow( false );
	}
	else
	{
		pMapDoc->ShowWindow( true );
	}

	if ( pMapDoc && pMapDoc->IsAutosave() )
	{
		char szRenameMessage[MAX_PATH+MAX_PATH+256];
		CString newMapPath = *pMapDoc->AutosavedFrom();

		sprintf( szRenameMessage, "This map was loaded from an autosave file.\nWould you like to rename it from \"%s\" to \"%s\"?\nNOTE: This will not save the file with the new name; it will only rename it.", lpszFileName, (LPCTSTR)newMapPath );

		if ( AfxMessageBox( szRenameMessage, MB_ICONHAND | MB_YESNO ) == IDYES )
		{
			pMapDoc->SetPathName( newMapPath );
		}
	}
	else
	{
		if ( CHammer::m_bIsNewDocumentVisible == true )
		{
			pMapDoc->CheckFileStatus();
			if ( pMapDoc->IsReadOnly() == true )
			{
				char szMessage[ MAX_PATH + MAX_PATH + 256 ];
				sprintf( szMessage, "This map is marked as READ ONLY.  You will not be able to save this file.\n\n%s", (LPCSTR)SaveFileName );
				AfxMessageBox( szMessage );
			}
		}
	}

	DiscordUpdatePresence();
	return pDoc;
}


//-----------------------------------------------------------------------------
// Returns true if this is a key message that is not a special dialog navigation message.
//-----------------------------------------------------------------------------
inline bool IsKeyStrokeMessage( MSG *pMsg )
{
	if ( ( pMsg->message != WM_KEYDOWN ) && ( pMsg->message != WM_CHAR ) )
		return false;

	// Check for special dialog navigation characters -- they don't count
	if ( ( pMsg->wParam == VK_ESCAPE ) || ( pMsg->wParam == VK_RETURN ) || ( pMsg->wParam == VK_TAB ) )
		return false;

	if ( ( pMsg->wParam == VK_UP ) || ( pMsg->wParam == VK_DOWN ) || ( pMsg->wParam == VK_LEFT ) || ( pMsg->wParam == VK_RIGHT ) )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : pMsg -
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CHammer::PreTranslateMessage(MSG* pMsg)
{
	// CG: The following lines were added by the Splash Screen component.
	if (CSplashWnd::PreTranslateAppMessage(pMsg))
		return TRUE;

	// This is for raw input, these shouldn't be translated so skip that here.
	if ( pMsg->message == WM_INPUT )
		return TRUE;

	// Suppress the accelerator table for edit controls so that users can type
	// uppercase characters without invoking Hammer tools.
	if ( IsKeyStrokeMessage( pMsg ) )
	{
		char className[80];
		::GetClassNameA( pMsg->hwnd, className, sizeof( className ) );

		// The classname of dialog window in the VGUI model browser and particle browser is AfxWnd100sd in Debug and AfxWnd100s in Release
		if ( !V_stricmp( className, "edit" ) || V_stristr( className, "AfxWnd" ) )
		{
			// Typing in an edit control. Don't pretranslate, just translate/dispatch.
			return FALSE;
		}
	}

	return CWinApp::PreTranslateMessage(pMsg);
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHammer::LoadSequences(void)
{
	KeyValues *pKvSequences = new KeyValues("Command Sequences");

	int bLoaded = 0;

	if (pKvSequences->LoadFromFile(g_pFullFileSystem, "CmdSeq.wc", "hammer_cfg"))
	{
		bLoaded = 1;
	}
	else if (pKvSequences->LoadFromFile(g_pFullFileSystem, "CmdSeq_default.wc", "hammer_cfg"))
	{
		bLoaded = 2;
	}

	if (bLoaded)
	{
		FOR_EACH_TRUE_SUBKEY(pKvSequences, pKvSequence)
		{
			CCommandSequence *pSeq = new CCommandSequence;
			Q_strncpy(pSeq->m_szName, pKvSequence->GetName(), 128);
			FOR_EACH_TRUE_SUBKEY(pKvSequence, pKvCmd)
			{
				CCOMMAND cmd;
				Q_memset(&cmd, 0, sizeof(CCOMMAND));
				cmd.Load(pKvCmd);
				pSeq->m_Commands.Add(cmd);
			}

			m_CmdSequences.Add(pSeq);
		}

		// Save em out if defaults were loaded
		if (bLoaded == 2)
			pKvSequences->SaveToFile(g_pFullFileSystem, "CmdSeq.wc", "hammer_cfg", true);
	}

	pKvSequences->deleteThis();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHammer::SaveSequences(void)
{
	KeyValues *pKvSequences = new KeyValues("Command Sequences");

	int numSeq = m_CmdSequences.GetSize();
	for (int i = 0; i < numSeq; i++)
	{
		CCommandSequence *pSeq = m_CmdSequences[i];
		KeyValues *pKvSequence = new KeyValues(pSeq->m_szName);
		int numCmds = pSeq->m_Commands.GetSize();
		for (int j = 0; j < numCmds; j++)
		{
			pSeq->m_Commands[j].Save( pKvSequence->CreateNewKey() );
		}
		pKvSequences->AddSubKey(pKvSequence);
	}

	pKvSequences->SaveToFile(g_pFullFileSystem, "CmdSeq.wc", "hammer_cfg", true);
	pKvSequences->deleteThis();
}


void CHammer::SetForceRenderNextFrame()
{
	m_bForceRenderNextFrame = true;
}


bool CHammer::GetForceRenderNextFrame()
{
	return m_bForceRenderNextFrame;
}

//-----------------------------------------------------------------------------
// Purpose: Performs idle processing. Runs the frame and does MFC idle processing.
// Input  : lCount - The number of times OnIdle has been called in succession,
//				indicating the relative length of time the app has been idle without
//				user input.
// Output : Returns TRUE if there is more idle processing to do, FALSE if not.
//-----------------------------------------------------------------------------
BOOL CHammer::OnIdle(LONG lCount)
{
	UpdatePrefabs();

	g_Textures.UpdateFileChangeWatchers();
	UpdateStudioFileChangeWatcher();
	return(CWinApp::OnIdle(lCount));
}


//-----------------------------------------------------------------------------
// Purpose: Renders the realtime views.
//-----------------------------------------------------------------------------
void CHammer::RunFrame(void)
{
	// Note: since hammer may well not even have a 3D window visible
	// at any given time, we have to call into the material system to
	// make it deal with device lost. Usually this happens during SwapBuffers,
	// but we may well not call SwapBuffers at any given moment.
	materials->HandleDeviceLost();

	if (!IsActiveApp())
	{
		Sleep(50);
	}

#ifdef VPROF_HAMMER
	g_VProfCurrentProfile.MarkFrame();
#endif

	HammerVGui()->Simulate();

	if ( ( CMapDoc::GetActiveMapDoc() && !IsClosing() ) || m_bForceRenderNextFrame )
		HandleLightingPreview();

	// never render without document or when closing down
	// usually only render when active, but not compiling a map unless forced
	if ( CMapDoc::GetActiveMapDoc() && !IsClosing() &&
		 ( ( !IsRunningCommands() && IsActiveApp() ) || m_bForceRenderNextFrame ) &&
		 CMapDoc::GetActiveMapDoc()->HasInitialUpdate() )

	{
		// get the time
		CMapDoc::GetActiveMapDoc()->UpdateCurrentTime();

		// run any animation
		CMapDoc::GetActiveMapDoc()->UpdateAnimation();

		// redraw the 3d views
		CMapDoc::GetActiveMapDoc()->RenderAllViews();
	}

	// No matter what, we want to keep caching in materials...
	if ( IsActiveApp() )
	{
		//g_Textures.LazyLoadTextures();
	}

	m_bForceRenderNextFrame = false;
}


//-----------------------------------------------------------------------------
// Purpose: Overloaded Run so that we can control the frameratefor realtime
//			rendering in the 3D view.
// Output : As MFC CWinApp::Run.
//-----------------------------------------------------------------------------
int CHammer::Run(void)
{
	Assert(0);
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the editor is the active app, false if not.
//-----------------------------------------------------------------------------
bool CHammer::IsActiveApp(void)
{
	return m_bActiveApp;
}


//-----------------------------------------------------------------------------
// Purpose: Called from CMainFrame::OnSysCommand, this informs the app when it
//			is minimized and unminimized. This allows us to stop rendering the
//			3D view when we are minimized.
// Input  : bMinimized - TRUE when minmized, FALSE otherwise.
//-----------------------------------------------------------------------------
void CHammer::OnActivateApp(bool bActive)
{
//	static int nCount = 0;
//	if (bActive)
//		DBG("ON %d\n", nCount);
//	else
//		DBG("OFF %d\n", nCount);
//	nCount++;
	m_bActiveApp = bActive;
}

//-----------------------------------------------------------------------------
// Purpose: Called from the shell to relinquish our video memory in favor of the
//			engine. This is called by the engine when it starts up.
//-----------------------------------------------------------------------------
void CHammer::ReleaseVideoMemory()
{
	POSITION pos = GetFirstDocTemplatePosition();

	while (pos)
	{
		CDocTemplate* pTemplate = (CDocTemplate*)GetNextDocTemplate(pos);
		POSITION pos2 = pTemplate->GetFirstDocPosition();
		while (pos2)
		{
			CDocument* pDocument;
			if ( ( pDocument = pTemplate->GetNextDoc( pos2 ) ) != NULL )
			{
				static_cast<CMapDoc*>( pDocument )->ReleaseVideoMemory();
			}
		}
	}
}

void CHammer::SuppressVideoAllocation( bool bSuppress )
{
	m_SuppressVideoAllocation = bSuppress;
}

bool CHammer::CanAllocateVideo() const
{
	return !m_SuppressVideoAllocation;
}

//-------------------------------------------------------------------------------
// Purpose: Runs through the autosave directory and fills the autosave map.
//			Also sets the total amount of space used by the directory.
// Input  : pFileMap - CUtlMap that will hold the list of files in the dir
//			pdwTotalDirSize - pointer to the DWORD that will hold directory size
//			pstrMapTitle - the name of the current map to be saved
// Output : returns an int containing the next number to use for the autosave
//-------------------------------------------------------------------------------
int CHammer::GetNextAutosaveNumber( const CString& autoSaveDir, CUtlMap<FILETIME, WIN32_FIND_DATA, int> *pFileMap, DWORD *pdwTotalDirSize, const CString *pstrMapTitle )
{
	FILETIME oldestAutosaveTime;
	oldestAutosaveTime.dwHighDateTime = 0;
	oldestAutosaveTime.dwLowDateTime = 0;

	int nNumberActualAutosaves = 0;
	int nCurrentAutosaveNumber = 1;
	int nOldestAutosaveNumber = 1;
	int nExpectedNextAutosaveNumber = 1;
	int nLastHole = 0;
	int nMaxAutosavesPerMap = Options.general.iMaxAutosavesPerMap;

	WIN32_FIND_DATA fileData;
	HANDLE hFile;
	DWORD dwTotalAutosaveDirectorySize = 0;

	hFile = FindFirstFile( autoSaveDir + "*.vmf_autosave", &fileData );

	if ( hFile != INVALID_HANDLE_VALUE )
	{
		//go through and for each file check to see if it is an autosave for this map; also keep track of total file size
		//for directory.
		while( GetLastError() != ERROR_NO_MORE_FILES && hFile != INVALID_HANDLE_VALUE )
		{
			(*pFileMap).Insert( fileData.ftLastAccessTime, fileData );

			DWORD dwFileSize = fileData.nFileSizeLow;
			dwTotalAutosaveDirectorySize += dwFileSize;
			FILETIME fileAccessTime = fileData.ftLastAccessTime;

			CString currentFilename( fileData.cFileName );

			//every autosave file ends in three digits; this code separates the name from the digits
			CString strMapName = currentFilename.Left( currentFilename.GetLength() - 17 );
			CString strCurrentNumber = currentFilename.Mid( currentFilename.GetLength() - 16, 3 );
			int nMapNumber = atoi( (char *)strCurrentNumber.GetBuffer() );

			if ( strMapName.CompareNoCase( (*pstrMapTitle) ) == 0 )
			{
				//keep track of real number of autosaves with map name; deals with instance where older maps get deleted
				//and create sequence holes in autosave map names.
				nNumberActualAutosaves++;

				if ( oldestAutosaveTime.dwLowDateTime == 0 )
				{
					//the first file is automatically the oldest
					oldestAutosaveTime = fileAccessTime;
				}

				if ( nMapNumber != nExpectedNextAutosaveNumber )
				{
					//the current map number is different than what was expected
					//there is a hole in the sequence
					nLastHole = nMapNumber;
				}

				nExpectedNextAutosaveNumber = nMapNumber + 1;
				if ( nExpectedNextAutosaveNumber > 999 )
				{
					nExpectedNextAutosaveNumber = 1;
				}
				if ( CompareFileTime( &fileAccessTime, &oldestAutosaveTime ) == -1 )
				{
					//this file is older than previous oldest file
					oldestAutosaveTime = fileAccessTime;
					nOldestAutosaveNumber = nMapNumber;
				}
			}
			FindNextFile(hFile, &fileData);
		}
		FindClose(hFile);
	}

	if ( nNumberActualAutosaves < nMaxAutosavesPerMap )
	{
		//there are less autosaves than wanted for the map; use the larger
		//of the next expected or the last found hole as the number.
		nCurrentAutosaveNumber = max( nExpectedNextAutosaveNumber, nLastHole );
	}
	else
	{
		//there are no holes, use the oldest.
		nCurrentAutosaveNumber = nOldestAutosaveNumber;
	}

	*pdwTotalDirSize = dwTotalAutosaveDirectorySize;

	return nCurrentAutosaveNumber;
}


static bool LessFunc( const FILETIME &lhs, const FILETIME &rhs )
{
	return CompareFileTime(&lhs, &rhs) < 0;
}

struct AutoSaveData
{
	CString  autoSaveDir;
	CMapDoc* pDoc;
};

unsigned CHammer::DoAutosave( void* _data )
{
	auto data = static_cast<AutoSaveData*>( _data );
	auto pDoc = data->pDoc;
	const auto& strAutosaveDirectory = data->autoSaveDir;

	//value from options is in megs
	DWORD dwMaxAutosaveSpace = Options.general.iMaxAutosaveSpace * 1024 * 1024;

	CUtlMap<FILETIME, WIN32_FIND_DATA, int> autosaveFiles( LessFunc );

	//this will hold the name of the map w/o leading directory info or file extension
	CString strMapTitle;
	//full path of map file
	CString strMapFilename = pDoc->GetPathName();

	DWORD dwTotalAutosaveDirectorySize = 0;
	int nCurrentAutosaveNumber = 0;

	// the map hasn't been saved before and doesn't have a filename; using default: 'autosave'
	if ( strMapFilename.IsEmpty() )
	{
		strMapTitle = "autosave";
	}
	// the map already has a filename
	else
	{
		int nFilenameBeginOffset = strMapFilename.ReverseFind( '\\' ) + 1;
		int nFilenameEndOffset = strMapFilename.Find( '.' );
		//get the filename of the map, between the leading '\' and the '.'
		strMapTitle = strMapFilename.Mid( nFilenameBeginOffset, nFilenameEndOffset - nFilenameBeginOffset );
	}

	nCurrentAutosaveNumber = GetNextAutosaveNumber( strAutosaveDirectory, &autosaveFiles, &dwTotalAutosaveDirectorySize, &strMapTitle );

	//creating the proper suffix for the autosave file
	char szNumberChars[4];
	CString strAutosaveString = itoa( nCurrentAutosaveNumber, szNumberChars, 10 );
	CString strAutosaveNumber = "000";
	strAutosaveNumber += strAutosaveString;
	strAutosaveNumber = strAutosaveNumber.Right( 3 );
	strAutosaveNumber = "_" + strAutosaveNumber;

	CString strSaveName = strAutosaveDirectory + strMapTitle + strAutosaveNumber + ".vmf_autosave";

	pDoc->SaveVMF( strSaveName, SAVEFLAGS_AUTOSAVE | SAVEFLAGS_NO_UI_UPDATE );
	//don't autosave again unless they make changes
	pDoc->SetAutosaveFlag( FALSE );

	//if there is too much space used for autosaves, delete the oldest file until the size is acceptable
	while( dwTotalAutosaveDirectorySize > dwMaxAutosaveSpace )
	{
		int nFirstElementIndex = autosaveFiles.FirstInorder();
		if ( !autosaveFiles.IsValidIndex( nFirstElementIndex ) )
		{
			Assert( false );
			break;
		}

		WIN32_FIND_DATA fileData = autosaveFiles.Element( nFirstElementIndex );
		DWORD dwOldestFileSize =  fileData.nFileSizeLow;
		char filename[MAX_PATH];
		strcpy( filename, fileData.cFileName );
		DeleteFile( strAutosaveDirectory + filename );
		dwTotalAutosaveDirectorySize -= dwOldestFileSize;
		autosaveFiles.RemoveAt( nFirstElementIndex );
	}

	autosaveFiles.RemoveAll();

	delete data;
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: This is called when the autosave timer goes off.  It checks to
//			make sure the document has changed and handles deletion of old
//			files when the total directory size is too big
//-----------------------------------------------------------------------------
void CHammer::Autosave( void )
{
	if ( !Options.general.bEnableAutosave )
		return;

	SaveConfig();

	if ( !VerifyAutosaveDirectory() )
	{
		Options.general.bEnableAutosave = false;
		return;
	}

	CHammerDocTemplate* templates[] = { APP()->pMapDocTemplate, APP()->pManifestDocTemplate };

	char szRootDir[MAX_PATH];
	APP()->GetDirectory( DIR_AUTOSAVE, szRootDir );
	CString strAutosaveDirectory( szRootDir );
	EditorUtil_ConvertPath( strAutosaveDirectory, true );

	for ( CHammerDocTemplate* pTemplate : templates )
	{
		for ( POSITION pos = pTemplate->GetFirstDocPosition(); pos != NULL; )
		{
			CMapDoc* pDoc = static_cast<CMapDoc*>( pTemplate->GetNextDoc( pos ) );
			if ( !pDoc->NeedsAutosave() )
				continue;

			auto data = new AutoSaveData{ strAutosaveDirectory, pDoc };

			auto thread = CreateSimpleThread( DoAutosave, data );
			ThreadDetach( thread );
			ReleaseThreadHandle( thread );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Verifies that the autosave directory exists and attempts to create it if
//			it doesn't.  Also returns various failure errors.
//			This function is now called at two different times: immediately after a new
//			directory is entered in the options screen and during every autosave call.
//			If called with a directory, the input directory is checked for correctness.
//			Otherwise, the system directory DIR_AUTOSAVE is checked
//-----------------------------------------------------------------------------
bool CHammer::VerifyAutosaveDirectory( char *szAutosaveDirectory ) const
{
	HANDLE hDir;
	HANDLE hTestFile;

	char szRootDir[MAX_PATH];
	if ( szAutosaveDirectory )
	{
		strcpy( szRootDir, szAutosaveDirectory );
		EnsureTrailingBackslash( szRootDir );
	}
	else
	{
		GetDirectory(DIR_AUTOSAVE, szRootDir);
	}

	if ( szRootDir[0] == 0 )
	{
		AfxMessageBox( "No autosave directory has been selected.\nThe autosave feature will be disabled until a directory is entered.", MB_OK );
		return false;
	}
	CString strAutosaveDirectory( szRootDir );
	{
		EditorUtil_ConvertPath(strAutosaveDirectory, true);
		if ( ( strAutosaveDirectory[1] != ':' ) || ( strAutosaveDirectory[2] != '\\' ) )
		{
			AfxMessageBox( "The current autosave directory does not have an absolute path.\nThe autosave feature will be disabled until a new directory is entered.", MB_OK );
			return false;
		}
	}

	hDir = CreateFile (
		strAutosaveDirectory,
		GENERIC_READ,
		FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		NULL
		);

	if ( hDir == INVALID_HANDLE_VALUE )
	{
		bool bDirResult = CreateDirectory( strAutosaveDirectory, NULL );
		if ( !bDirResult )
		{
			AfxMessageBox( "The current autosave directory does not exist and could not be created.  \nThe autosave feature will be disabled until a new directory is entered.", MB_OK );
			return false;
		}
	}
	else
	{
		CloseHandle( hDir );

		hTestFile = CreateFile( strAutosaveDirectory + "test.txt",
			GENERIC_READ,
			FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
			NULL,
			CREATE_NEW,
			FILE_FLAG_BACKUP_SEMANTICS,
			NULL
			);

		if ( hTestFile == INVALID_HANDLE_VALUE )
		{
			 if ( GetLastError() == ERROR_ACCESS_DENIED )
			 {
				 AfxMessageBox( "The autosave directory is marked as read only.  Please remove the read only attribute or select a new directory in Tools->Options->General.\nThe autosave feature will be disabled.", MB_OK );
				 return false;
			 }
			 else
			 {
				 AfxMessageBox( "There is a problem with the autosave directory.  Please select a new directory in Tools->Options->General.\nThe autosave feature will be disabled.", MB_OK );
				 return false;
			 }


		}

		CloseHandle( hTestFile );
		DeleteFile( strAutosaveDirectory + "test.txt" );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Called when Hammer starts after a crash.  It loads the newest
//			autosave file after Hammer has initialized.
//-----------------------------------------------------------------------------
void CHammer::LoadLastGoodSave( void )
{
	CString strLastGoodSave = GetProfileString("General", "Last Good Save", "");
	char szMapDir[MAX_PATH];
	strcpy(szMapDir, g_pGameConfig->szMapDir);
	CDocument *pCurrentDoc;

	if ( !strLastGoodSave.IsEmpty() )
	{
		pCurrentDoc = OpenDocumentFile( strLastGoodSave );

		if ( !pCurrentDoc )
		{
			AfxMessageBox( "There was an error loading the last saved file.", MB_OK );
			return;
		}

		char szAutoSaveDir[MAX_PATH];
		GetDirectory(DIR_AUTOSAVE, szAutoSaveDir);

		if ( !((CMapDoc *)pCurrentDoc)->IsAutosave() && Q_stristr( pCurrentDoc->GetPathName(), szAutoSaveDir ) )
		{
			//This handles the case where someone recovers from a crash and tries to load an autosave file that doesn't have the new autosave chunk in it
			//It assumes the file should go into the gameConfig map directory
			char szRenameMessage[MAX_PATH+MAX_PATH+256];
			char szLastSaveCopy[MAX_PATH];
			Q_strcpy( szLastSaveCopy, strLastGoodSave );
			char *pszFileName = Q_strrchr( strLastGoodSave, '\\') + 1;
			char *pszFileNameEnd = Q_strrchr( strLastGoodSave, '_');
			if ( !pszFileNameEnd )
			{
				pszFileNameEnd = Q_strrchr( strLastGoodSave, '.');
			}
			strcpy( pszFileNameEnd, ".vmf" );
			CString newMapPath( szMapDir );
			newMapPath.Append( "\\" );
			newMapPath.Append( pszFileName );
			sprintf( szRenameMessage, "The last saved map was found in the autosave directory.\nWould you like to rename it from \"%s\" to \"%s\"?\nNOTE: This will not save the file with the new name; it will only rename it.", szLastSaveCopy, (LPCTSTR)newMapPath );

			if ( AfxMessageBox( szRenameMessage, MB_YESNO ) == IDYES )
			{
				pCurrentDoc->SetPathName( newMapPath );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called from the General Options dialog when the autosave timer or
//			directory values have changed.
//-----------------------------------------------------------------------------
void CHammer::ResetAutosaveTimer()
{
	Options.general.bEnableAutosave = true;

	CMainFrame *pMainWnd = ::GetMainWnd();
	if ( pMainWnd )
	{
		pMainWnd->ResetAutosaveTimer();
	}
}