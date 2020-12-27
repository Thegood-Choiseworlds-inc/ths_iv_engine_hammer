#include "stdafx.h"
#include "IOEditor.h"
#include "HammerVGui.h"
#include "io/vnodeview.h"

#include "vgui_controls/EditablePanel.h"

#include "tier0/memdbgon.h"

static constexpr const char pszIniSection[] = "IO Editor";

class CIOEditorDialog : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CIOEditorDialog, vgui::EditablePanel );
public:
	CIOEditorDialog( vgui::Panel* parent ) : BaseClass( parent, "IOEditorDialog" )
	{
		auto s = new vgui::CBoxSizer( vgui::ESLD_VERTICAL );
		view = new CNodeView( this, "NODE" );
		view->SetKeyBoardInputEnabled( true );
		s->AddPanel( view, vgui::SizerAddArgs_t().Expand( 1.0f ) );
		SetSizer( s );
	}

	void PerformLayout()
	{
		BaseClass::PerformLayout();
		view->ResetView_User( true );
	}

	CNodeView* view;
};


class CIOEditorPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CIOEditorPanel, vgui::EditablePanel );
public:
	CIOEditorPanel( CIOEditor* pBrowser, const char* panelName, vgui::HScheme hScheme ) :
		vgui::EditablePanel( NULL, panelName, hScheme )
	{
		m_pBrowser = pBrowser;
	}

	void OnSizeChanged( int newWide, int newTall ) override
	{
		// call Panel and not EditablePanel OnSizeChanged.
		Panel::OnSizeChanged( newWide, newTall );
	}

	void OnCommand( const char* pCommand ) override
	{
		if ( V_stricmp( pCommand, "OK" ) == 0 )
		{
			m_pBrowser->EndDialog( IDOK );
		}
		else if ( V_stricmp( pCommand, "Cancel" ) == 0 || V_stricmp( pCommand, "Close" ) == 0 )
		{
			m_pBrowser->EndDialog( IDCANCEL );
		}
	}

	void OnKeyCodePressed( vgui::KeyCode code ) override
	{
		BaseClass::OnKeyCodePressed( code );
		m_pBrowser->m_pDialog->view->OnKeyCodePressed( code );
	}

	void OnKeyCodeReleased( vgui::KeyCode code ) override
	{
		BaseClass::OnKeyCodeReleased( code );
		m_pBrowser->m_pDialog->view->OnKeyCodeReleased( code );
	}

	void OnKeyCodeTyped( vgui::KeyCode code ) override
	{
		m_pBrowser->m_pDialog->view->OnKeyCodeTyped( code );
		BaseClass::OnKeyCodeTyped( code );

		if ( code == KEY_ESCAPE )
			m_pBrowser->EndDialog( IDCANCEL );
	}

private:
	CIOEditor* m_pBrowser;
};

IMPLEMENT_DYNAMIC( CIOEditor, CVguiDialog )
CIOEditor::CIOEditor( CWnd* pParent /*=NULL*/ )
	: CVguiDialog( CIOEditor::IDD, pParent )
{
	m_pDialog = new CIOEditorDialog( NULL );
	flLastPaint = Plat_FloatTime();
}

CIOEditor::~CIOEditor()
{
	// CDialog isn't going to clean up its vgui children
	delete m_pDialog;
}

void CIOEditor::SaveLoadSettings( bool bSave )
{
	CString str;
	CRect rect;
	CWinApp* pApp = AfxGetApp();

	if ( bSave )
	{
		GetWindowRect( rect );
		str.Format( "%ld %ld %ld %ld", rect.left, rect.top, rect.right, rect.bottom );
		pApp->WriteProfileString( pszIniSection, "Position", str );
	}
	else
	{
		str = pApp->GetProfileString( pszIniSection, "Position" );
		if ( !str.IsEmpty() )
		{
			const CRect screenRect{ POINT{ GetSystemMetrics( SM_XVIRTUALSCREEN ), GetSystemMetrics( SM_YVIRTUALSCREEN ) }, SIZE{ GetSystemMetrics( SM_CXVIRTUALSCREEN ), GetSystemMetrics( SM_CYVIRTUALSCREEN ) } };
			sscanf( str, "%ld %ld %ld %ld", &rect.left, &rect.top, &rect.right, &rect.bottom );
			const auto size = rect.Size();
			rect &= screenRect;
			if ( rect.Width() == 0 || rect.Height() == 0 )
				rect.InflateRect( 0, 0, size.cx, size.cy );
			MoveWindow( rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE );
			Resize();
		}
	}
}

void CIOEditor::DoDataExchange( CDataExchange* pDX )
{
	CDialog::DoDataExchange( pDX );
}

void CIOEditor::Resize()
{
	// reposition controls
	CRect rect;
	GetClientRect( &rect );

	m_VGuiWindow.MoveWindow( rect );

	m_pDialog->SetBounds( 0, 0, rect.Width(), rect.Height() );
}

void CIOEditor::OnSize(UINT nType, int cx, int cy)
{
	if ( nType == SIZE_MINIMIZED || !IsWindow( m_VGuiWindow.m_hWnd ) )
		return CDialog::OnSize( nType, cx, cy );

	Resize();

	CDialog::OnSize(nType, cx, cy);
}

BOOL CIOEditor::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

BEGIN_MESSAGE_MAP(CIOEditor, CDialog)
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

float vguiFrameTime = 1.f;
BOOL CIOEditor::PreTranslateMessage( MSG* pMsg )
{
	if ( pMsg && pMsg->message == WM_PAINT )
	{
		const auto cur = Plat_FloatTime();
		vguiFrameTime = cur - flLastPaint;
		flLastPaint = cur;
	}
	// don't filter dialog message
	return CWnd::PreTranslateMessage( pMsg );
}

BOOL CIOEditor::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_VGuiWindow.Create( NULL, _T("KeybindEditor"), WS_VISIBLE|WS_CHILD, CRect(0,0,100,100), this, IDD_KEYBIND_EDITOR );

	vgui::EditablePanel* pMainPanel = new CIOEditorPanel( this, "KeybindEditorPanel", HammerVGui()->GetHammerScheme() );

	m_VGuiWindow.SetParentWindow( &m_VGuiWindow );
	m_VGuiWindow.SetMainPanel( pMainPanel );
	pMainPanel->MakePopup( false, false );
	m_VGuiWindow.SetRepaintInterval( 30 );

	m_pDialog->SetParent( pMainPanel );
	m_pDialog->AddActionSignalTarget( pMainPanel );
	pMainPanel->InvalidateLayout( true );

	SaveLoadSettings( false ); // load

	//m_pPicker->Activate();
	m_pDialog->view->InitCanvas();
	m_pDialog->view->ResetView_User( true );

	return TRUE;
}

void CIOEditor::OnDestroy()
{
	SaveLoadSettings( true ); // save

	CDialog::OnDestroy();
}

void CIOEditor::Show()
{
	if ( m_pDialog )
		m_pDialog->SetVisible( true );

}
void CIOEditor::Hide()
{
	if ( m_pDialog )
		m_pDialog->SetVisible( false );
}