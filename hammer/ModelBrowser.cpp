// ModelBrowser.cpp : implementation file
//

#include "stdafx.h"
#include "ModelBrowser.h"
#include "matsys_controls/mdlpanel.h"
#include "matsys_controls/mdlpicker.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/Splitter.h"
#include "vgui_controls/Button.h"
#include "HammerVGui.h"

#include "KeyValues.h"
#include "vgui/KeyCode.h"
#include "vgui/ISurface.h"
#include "texturesystem.h"
#include "utlntree.h"
#include <istudiorender.h>

static constexpr LPCTSTR pszIniSection = "Model Browser";

// CModelBrowser dialog

class CModelBrowserPanel : public vgui::EditablePanel
{
public:
	CModelBrowserPanel( CModelBrowser *pBrowser, const char *panelName, vgui::HScheme hScheme ) :
	  vgui::EditablePanel( NULL, panelName, hScheme )
	{
		m_pBrowser = pBrowser;
	}

	virtual	void OnSizeChanged(int newWide, int newTall)
	{
		// call Panel and not EditablePanel OnSizeChanged.
		Panel::OnSizeChanged(newWide, newTall);
	}

	virtual void OnCommand( const char *pCommand )
	{
		if ( Q_strcmp( pCommand, "OK" ) == 0 )
		{
			m_pBrowser->EndDialog( IDOK );
		}
		else if ( Q_strcmp( pCommand, "Cancel" ) == 0 )
		{
			m_pBrowser->EndDialog( IDCANCEL );
		}
	}

	virtual void OnKeyCodeTyped(vgui::KeyCode code)
	{
		vgui::EditablePanel::OnKeyCodeTyped( code );

		if ( code == KEY_ENTER )
		{
			m_pBrowser->EndDialog( IDOK );
		}
		else if ( code == KEY_ESCAPE )
		{
			m_pBrowser->EndDialog( IDCANCEL );
		}
	}

	virtual void OnMessage(const KeyValues *params, vgui::VPANEL ifromPanel)
	{
		vgui::EditablePanel::OnMessage( params, ifromPanel );

		if ( Q_strcmp( params->GetName(), "MDLPreviewChanged" ) == 0 )
		{
			m_pBrowser->UpdateStatusLine();
		}
		else if ( Q_stricmp( params->GetName(), "AssetPickerFind" ) == 0 )
		{
			m_pBrowser->EndDialog( ID_FIND_ASSET );
		}
	}

	CModelBrowser *m_pBrowser;
};

IMPLEMENT_DYNAMIC(CModelBrowser, CVguiDialog)
CModelBrowser::CModelBrowser(CWnd* pParent /*=NULL*/)
	: CVguiDialog(CModelBrowser::IDD, pParent)
{
	m_pPicker = new CMDLPicker( NULL, CMDLPicker::PAGE_RENDER | CMDLPicker::PAGE_SKINS | CMDLPicker::PAGE_SEQUENCES | CMDLPicker::PAGE_ACTIVITIES | CMDLPicker::PAGE_INFO );
	m_pStatusLine = new vgui::TextEntry( NULL, "StatusLine" );

	m_pButtonOK = new vgui::Button( NULL, "OpenButton", "OK" );
	m_pButtonCancel = new vgui::Button( NULL, "CancelButton", "Cancel" );
}

CModelBrowser::~CModelBrowser()
{
	delete m_pPicker;
	delete m_pStatusLine;
	delete m_pButtonOK;
	delete m_pButtonCancel;
}

void CModelBrowser::SetUsedModelList( CUtlVector<AssetUsageInfo_t> &usedModels )
{
	m_pPicker->SetUsedAssetList( usedModels );
}

void CModelBrowser::SetModelName( const char *pModelName )
{
	char pszTempModelName[255];
	strcpy( pszTempModelName, pModelName );

	char * pszSelectedModel = strchr( pszTempModelName, '/' );
	if( pszSelectedModel)
	{
		pszSelectedModel += 1;
		Q_FixSlashes( pszSelectedModel, '\\' );
	}

	m_pPicker->SelectMDL( pModelName );
	m_pPicker->SetInitialSelection( pszSelectedModel );

	m_pStatusLine->SetText( pModelName );
}

void CModelBrowser::GetModelName( char *pModelName, int length )
{
	m_pPicker->GetSelectedMDLName( pModelName, length );

	Q_FixSlashes( pModelName, '/' );
}

void CModelBrowser::GetSkin( int &nSkin )
{
	nSkin = m_pPicker->GetSelectedSkin();
}

void CModelBrowser::SetSkin( int nSkin )
{
	m_pPicker->SelectSkin( nSkin );
}

void CModelBrowser::UpdateStatusLine()
{
	char szModel[1024];

	m_pPicker->GetSelectedMDLName( szModel, sizeof(szModel) );

	m_pStatusLine->SetText( szModel );

/*	MDLHandle_t hMDL = g_pMDLCache->FindMDL( szModel );

	studiohdr_t *hdr = g_pMDLCache->GetStudioHdr( hMDL );

	g_pMDLCache->Release( hMDL ); */
}

void CModelBrowser::SaveLoadSettings( bool bSave )
{
	CString	str;
	CRect	rect;
	CWinApp	*pApp = AfxGetApp();

	if ( bSave )
	{
		GetWindowRect(rect);
		str.Format("%ld %ld %ld %ld", rect.left, rect.top, rect.right, rect.bottom);
		pApp->WriteProfileString(pszIniSection, "Position", str);
		pApp->WriteProfileString(pszIniSection, "Filter", m_pPicker->GetFilter() );
	}
	else
	{
		str = pApp->GetProfileString(pszIniSection, "Position");

		if (!str.IsEmpty())
		{
			sscanf(str, "%ld %ld %ld %ld", &rect.left, &rect.top, &rect.right, &rect.bottom);

			if (rect.left < 0)
			{
				ShowWindow(SW_SHOWMAXIMIZED);
			}
			else
			{
				const CRect screenRect{ POINT{ GetSystemMetrics( SM_XVIRTUALSCREEN ), GetSystemMetrics( SM_YVIRTUALSCREEN ) }, SIZE{ GetSystemMetrics( SM_CXVIRTUALSCREEN ), GetSystemMetrics( SM_CYVIRTUALSCREEN ) } };
				const auto size = rect.Size();
				rect &= screenRect;
				if ( rect.Width() == 0 || rect.Height() == 0 )
					rect.InflateRect( 0, 0, size.cx, size.cy );
				MoveWindow(rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, FALSE);
			}

			Resize();
		}

		str = pApp->GetProfileString(pszIniSection, "Filter");

		if (!str.IsEmpty())
		{
			m_pPicker->SetFilter( str );
		}
	}
}



void CModelBrowser::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

void CModelBrowser::Resize()
{
	// reposition controls
	CRect rect;
	GetClientRect( &rect );

	m_VGuiWindow.MoveWindow( rect );

	m_pPicker->SetBounds( 0, 0, rect.Width(), rect.Height() );
}

void CModelBrowser::OnSize(UINT nType, int cx, int cy)
{
	if (nType == SIZE_MINIMIZED || !IsWindow(m_VGuiWindow.m_hWnd) )
	{
		CDialog::OnSize(nType, cx, cy);
		return;
	}

	Resize();

	CDialog::OnSize(nType, cx, cy);
}


BEGIN_MESSAGE_MAP(CModelBrowser, CVguiDialog)
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL CModelBrowser::PreTranslateMessage( MSG* pMsg )
{
	// don't filter dialog message
	return CWnd::PreTranslateMessage( pMsg );
}

BOOL CModelBrowser::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_VGuiWindow.Create( NULL, _T("ModelViewer"), WS_VISIBLE|WS_CHILD, CRect(0,0,100,100), this, IDD_MODEL_BROWSER);

	vgui::EditablePanel *pMainPanel = new CModelBrowserPanel( this, "ModelBrowerPanel", HammerVGui()->GetHammerScheme() );

	m_VGuiWindow.SetParentWindow( &m_VGuiWindow );
	m_VGuiWindow.SetMainPanel( pMainPanel );
	pMainPanel->MakePopup( false, false );
	m_VGuiWindow.SetRepaintInterval( 30 );


	m_pPicker->SetParent( pMainPanel );
	m_pPicker->AddActionSignalTarget( pMainPanel );

	m_pButtonOK->SetParent( pMainPanel );
	m_pButtonOK->AddActionSignalTarget( pMainPanel );
	m_pButtonOK->SetCommand( "OK" );

	m_pButtonCancel->SetParent( pMainPanel );
	m_pButtonCancel->AddActionSignalTarget( pMainPanel );
	m_pButtonCancel->SetCommand( "Cancel" );

	m_pStatusLine->SetParent( pMainPanel );
	m_pStatusLine->SetEditable( false );

	SaveLoadSettings( false ); // load

	auto s = new vgui::CBoxSizer( vgui::ESLD_VERTICAL );
	auto s2 = new vgui::CBoxSizer( vgui::ESLD_HORIZONTAL );
	s->AddPanel( m_pPicker, vgui::SizerAddArgs_t().Expand( 1.0f ).Padding( 0 ) );

	s2->AddPanel( m_pButtonOK, vgui::SizerAddArgs_t().Padding( 0 ) );
	s2->AddSpacer( vgui::SizerAddArgs_t().Padding( 2 ) );
	s2->AddPanel( m_pButtonCancel, vgui::SizerAddArgs_t().Padding( 0 ) );
	s2->AddSpacer( vgui::SizerAddArgs_t().Padding( 2 ) );
	s2->AddPanel( m_pStatusLine, vgui::SizerAddArgs_t().Expand( 1.0f ).Padding( 0 ) );

	s->AddSpacer( vgui::SizerAddArgs_t().Padding( 2 ) );
	s->AddSizer( s2, vgui::SizerAddArgs_t().Padding( 2 ) );

	pMainPanel->SetSizer( s );
	pMainPanel->InvalidateLayout( true );
	m_pPicker->Activate();

	Resize();

	return TRUE;
}

void CModelBrowser::OnDestroy()
{
	SaveLoadSettings( true ); // save

	// model browser destoys our default cube map, reload it
	g_Textures.RebindDefaultCubeMap();

	CDialog::OnDestroy();
}

void CModelBrowser::Show()
{
	if (m_pPicker)
	{
		m_pPicker->SetVisible( true );
	}
	if (m_pStatusLine)
		m_pStatusLine->SetVisible( true );
	if (m_pButtonOK)
		m_pButtonOK->SetVisible( true );
	if (m_pButtonCancel)
		m_pButtonCancel->SetVisible( true );

}
void CModelBrowser::Hide()
{
	if (m_pPicker)
		m_pPicker->SetVisible( false );

	if (m_pStatusLine)
		m_pStatusLine->SetVisible( false );

	if (m_pButtonOK)
		m_pButtonOK->SetVisible( false );

	if (m_pButtonCancel)
		m_pButtonCancel->SetVisible( false );
}

BOOL CModelBrowser::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}
