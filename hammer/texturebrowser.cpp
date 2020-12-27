//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "stdafx.h"
#include "custommessages.h"
#include "globalfunctions.h"
#include "history.h"
#include "faceeditsheet.h"
#include "ieditortexture.h"
#include "mainfrm.h"
#include "mapdoc.h"
#include "mapworld.h"
#include "replacetexdlg.h"
#include "texturebrowser.h"
#include "texturesystem.h"
#include "hammer.h"
#include "Selection.h"

#include "tier1/KeyValues.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static constexpr LPCTSTR pszIniSection = "Texture Browser";


CStringArray CTextureBrowser::m_FilterHistory;
char CTextureBrowser::m_szLastKeywords[MAX_PATH];


BEGIN_MESSAGE_MAP(CTextureBrowser, CDialog)
	//{{AFX_MSG_MAP(CTextureBrowser)
	ON_WM_SIZE()
	ON_CBN_SELENDOK(IDC_TEXTURESIZE, &ThisClass::OnSelendokTexturesize)
	ON_WM_CLOSE()
	ON_WM_GETMINMAXINFO()
	ON_WM_TIMER()
	ON_CBN_EDITCHANGE(IDC_FILTER, &ThisClass::OnChangeFilterOrKeywords)
	ON_CBN_SELENDOK(IDC_FILTER, &ThisClass::OnUpdateFiltersNOW)
	ON_CBN_EDITCHANGE(IDC_KEYWORDS, &ThisClass::OnChangeFilterOrKeywords)
	ON_CBN_SELENDOK(IDC_KEYWORDS, &ThisClass::OnUpdateKeywordsNOW)
	ON_BN_CLICKED(IDC_FILTER_OPAQUE, &ThisClass::OnFilterOpaque)
	ON_BN_CLICKED(IDC_FILTER_TRANSLUCENT, &ThisClass::OnFilterTranslucent)
	ON_BN_CLICKED(IDC_FILTER_SELFILLUM, &ThisClass::OnFilterSelfIllum)
	ON_BN_CLICKED(IDC_FILTER_ENVMASK, &ThisClass::OnFilterEnvmask)
	ON_BN_CLICKED(IDC_FILTER_COLOR, &ThisClass::OnFilterColor)
	ON_BN_CLICKED(IDC_SHOW_ERROR, &ThisClass::OnShowErrors)
	ON_BN_CLICKED(IDC_USED, &ThisClass::OnUsed)
	ON_BN_CLICKED(IDC_MARK, &ThisClass::OnMark)
	ON_BN_CLICKED(IDC_REPLACE, &ThisClass::OnReplace)
	ON_BN_CLICKED(IDC_TEXTURES_OPEN_SOURCE, &ThisClass::OnOpenSource)
	ON_BN_CLICKED(IDC_TEXTURES_EXPLORE_SOURCE, &ThisClass::OnExploreToSource)
	ON_BN_CLICKED(IDC_TEXTURES_RELOAD, &ThisClass::OnReload)
	ON_BN_CLICKED(IDC_RECACHE, &ThisClass::OnRecache)
	ON_MESSAGE(TWN_SELCHANGED, &ThisClass::OnTexturewindowSelchange)
	ON_MESSAGE(TWN_LBUTTONDBLCLK, &ThisClass::OnTextureWindowDblClk)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose:
// Input  : pParent -
//-----------------------------------------------------------------------------
CTextureBrowser::CTextureBrowser(CWnd* pParent)
	: CDialog(IDD, pParent)
{
	m_szNameFilter[0] = '\0';
	szInitialTexture[0] = '\0';
	m_bFilterChanged = FALSE;
	m_uLastFilterChange = 0xffffffff;
	m_bUsed = FALSE;
	m_szLastKeywords[0] = '\0';
}

void CTextureBrowser::CColorButton::Setup( UINT nID, CWnd* parent )
{
	SubclassDlgItem( nID, parent );
	EnableOtherButton( "Other" );
	m_Colors.RemoveAll();
	m_Colors.Add( RGB( 255, 0, 0 ) );
	m_Colors.Add( RGB( 0, 255, 0 ) );
	m_Colors.Add( RGB( 0, 0, 255 ) );
	m_Colors.Add( RGB( 255, 255, 0 ) );
	m_Colors.Add( RGB( 0, 255, 255 ) );
	m_Colors.Add( RGB( 255, 0, 255 ) );
	m_Colors.Add( RGB( 255, 255, 255 ) );
	m_Colors.Add( RGB( 0, 0, 0 ) );
	SetColor( RGB( 255, 255, 255 ) );
}

void CTextureBrowser::OnGetMinMaxInfo( MINMAXINFO* lpMMI )
{
	lpMMI->ptMinTrackSize.x = 970;
	lpMMI->ptMinTrackSize.y = 390;
}

//-----------------------------------------------------------------------------
// Purpose: Handles resize messages. Moves the child windows to the proper positions.
// Input  : nType -
//			cx -
//			cy -
//-----------------------------------------------------------------------------
void CTextureBrowser::OnSize(UINT nType, int cx, int cy)
{
	if (nType == SIZE_MINIMIZED || !IsWindow(m_cTextureWindow.m_hWnd))
	{
		CDialog::OnSize(nType, cx, cy);
		return;
	}

	// reposition controls
	CRect clientrect;
	GetClientRect(&clientrect);

	CRect CtrlRect;
	GetDlgItem(IDC_CONTROLHEIGHT)->GetWindowRect(&CtrlRect);

	int iControlHeight = (CtrlRect.bottom - CtrlRect.top);

	//
	// Resize the texture window.
	//
	CtrlRect = clientrect;
	CtrlRect.bottom -= iControlHeight + 6;
	m_cTextureWindow.MoveWindow(CtrlRect);

	CDialog::OnSize(nType, cx, cy);
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : bUsed -
//-----------------------------------------------------------------------------
void CTextureBrowser::SetUsed(BOOL bUsed)
{
	m_bUsed = bUsed;

	if (m_bUsed)
	{
		GetActiveWorld()->GetUsedTextures(m_TextureSubList);
		m_cTextureWindow.SetSpecificList(&m_TextureSubList);
	}
	else
	{
		m_cTextureWindow.SetSpecificList(NULL);
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextureBrowser::OnClose(void)
{
	WriteSettings();
	SaveAndExit();
	CDialog::OnCancel();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextureBrowser::OnCancel()
{
	WriteSettings();
	SaveAndExit();
	CDialog::OnCancel();
}

void CTextureBrowser::OnOK()
{
	WriteSettings();
	SaveAndExit();
	CDialog::OnOK();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextureBrowser::OnUsed()
{
	if(!GetActiveWorld())
		return;

	SetUsed(m_cUsed.GetCheck());
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : pszTexture -
//-----------------------------------------------------------------------------
void CTextureBrowser::SetInitialTexture(LPCTSTR pszTexture)
{
	strcpy(szInitialTexture, pszTexture);
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextureBrowser::OnSelendokTexturesize()
{
	// change size of textures the texutre window displays
	int iCurSel = m_cSizeList.GetCurSel();

	switch(iCurSel)
	{
	case 0:
		m_cTextureWindow.SetDisplaySize(128);
		break;
	case 1:
		m_cTextureWindow.SetDisplaySize(256);
		break;
	case 2:
		m_cTextureWindow.SetDisplaySize(512);
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
BOOL CTextureBrowser::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Iterate all the active textures for debugging.
	//int nCount = g_Textures.GetActiveTextureCount();
	//for (int nTexture = 0; nTexture < nCount; nTexture++)
	//{
	//	IEditorTexture *pTexture = g_Textures.GetActiveTexture(nTexture);
	//	const char *pszName = pTexture->GetName();
	//	DBG("%d: %s\n", nTexture, pszName);
	//}

	m_cSizeList.SubclassDlgItem(IDC_TEXTURESIZE, this);
	m_cFilter.SubclassDlgItem(IDC_FILTER, this);
	m_cKeywords.SubclassDlgItem(IDC_KEYWORDS, this);
	m_cCurName.SubclassDlgItem(IDC_CURNAME, this);
	m_cCurDescription.SubclassDlgItem(IDC_CURDESCRIPTION, this);
	m_cUsed.SubclassDlgItem(IDC_USED, this);

	m_FilterOpaque.SubclassDlgItem(IDC_FILTER_OPAQUE, this);
	m_FilterTranslucent.SubclassDlgItem(IDC_FILTER_TRANSLUCENT, this);
	m_FilterSelfIllum.SubclassDlgItem(IDC_FILTER_SELFILLUM, this);
	m_FilterEnvMask.SubclassDlgItem(IDC_FILTER_ENVMASK, this);
	m_FilterColor.SubclassDlgItem(IDC_FILTER_COLOR, this);
	m_ShowErrors.SubclassDlgItem(IDC_SHOW_ERROR, this);
	m_ColorFilter.Setup(IDC_COLOR_FILTER, this);

	m_FilterOpaque.SetCheck( true );
	m_FilterTranslucent.SetCheck( true );
	m_FilterSelfIllum.SetCheck( true );
	m_FilterEnvMask.SetCheck( true );
	m_FilterColor.SetCheck( false );
	m_ShowErrors.SetCheck( true );

	//
	// Create CTextureWindow that takes up area of dummy control.
	//
	RECT r;
	GetDlgItem(IDC_BROWSERDUMMY)->GetClientRect(&r);
	m_cTextureWindow.Create(this, r);

	// Show everything initially
	m_cTextureWindow.SetTypeFilter( ~0, true );

	auto pApp = APP();
	if ( auto values = pApp->GetProfileKeyValues( pszIniSection )->FindKey( "Filter History" ) )
	{
		m_FilterHistory.RemoveAll();
		FOR_EACH_SUBKEY( values, i )
			m_FilterHistory.Add( i->GetString() );
	}

	V_strcpy_safe( m_szLastKeywords, pApp->GetProfileString( pszIniSection, "Keywords", "" ) );

	//
	// Add latest history to the filter combo.
	//
	for (int i = 0; i < m_FilterHistory.GetSize(); i++)
	{
		m_cFilter.AddString(m_FilterHistory[i]);
	}

	//
	// Set the name filter unless one was explicitly specified.
	//
	if (m_szNameFilter[0] == '\0')
	{
		//
		// No name filter specified. Use whatever is on top of the history.
		//
		if (m_cFilter.GetCount() > 0)
		{
			m_cFilter.GetLBText(0, m_szNameFilter);
			m_cFilter.SetCurSel(0);
		}
	}

	m_cFilter.SetWindowText(m_szNameFilter);
	m_cTextureWindow.SetNameFilter(m_szNameFilter);

	//
	// Done with the name filter; clear it for next time.
	//
	m_szNameFilter[0] = '\0';

	// Add the global list of keywords to the keywords combo.
	for( int i=0; i< g_Textures.GetNumKeywords(); i++ )
	{
		m_cKeywords.AddString( g_Textures.GetKeyword( i ) );
	}

	//
	// Set the keyword filter.
	//
	m_cKeywords.SetWindowText(m_szLastKeywords);
	m_cTextureWindow.SetKeywords(m_szLastKeywords);

	m_cUsed.SetCheck(m_bUsed);

	// Refresh the list of used textures if enabled.
	if (m_bUsed)
	{
		SetUsed(TRUE);
	}

	if ( auto values = pApp->GetProfileKeyValues( pszIniSection )->FindKey( "Color History" ) )
	{
		auto& colors = m_ColorFilter.ColorList();
		colors.RemoveAll();
		FOR_EACH_SUBKEY( values, i )
			colors.Add( i->GetColor().GetRawColor() );
	}

	m_ColorFilter.SetColor( pApp->GetProfileColor( pszIniSection, "Last Color", 255, 255, 255 ) );
	m_FilterColor.SetCheck( pApp->GetProfileInt( pszIniSection, "Filter By Color", 0 ) != 0 );
	m_ColorFilter.EnableWindow( m_FilterColor.GetCheck() != 0 );

	CString str = pApp->GetProfileString( pszIniSection, "Position" );
	if (!str.IsEmpty())
	{
		CRect r;
		sscanf(str, "%ld %ld %ld %ld", &r.left, &r.top, &r.right, &r.bottom);

		if (r.left < 0)
		{
			ShowWindow(SW_SHOWMAXIMIZED);
		}
		else
		{
			const CRect screenRect{ POINT{ GetSystemMetrics( SM_XVIRTUALSCREEN ), GetSystemMetrics( SM_YVIRTUALSCREEN ) }, SIZE{ GetSystemMetrics( SM_CXVIRTUALSCREEN ), GetSystemMetrics( SM_CYVIRTUALSCREEN ) } };
			const auto size = r.Size();
			r &= screenRect;
			if ( r.Width() == 0 || r.Height() == 0 )
				r.InflateRect( 0, 0, size.cx, size.cy );
			MoveWindow(r.left, r.top, r.right-r.left, r.bottom-r.top, FALSE);
		}
	}

	int iSize = pApp->GetProfileInt(pszIniSection, "ShowSize", 0);
	m_cSizeList.SetCurSel(iSize);
	OnSelendokTexturesize();

	if (szInitialTexture[0])
	{
		m_cTextureWindow.SelectTexture(szInitialTexture);
	}

	m_cTextureWindow.ShowWindow(SW_SHOW);

	SetTimer(1, 500, NULL);

	m_cFilter.SetFocus();

	return(FALSE);
}


//-----------------------------------------------------------------------------
// Purpose: Called when either the filter combo or the keywords combo text changes.
//-----------------------------------------------------------------------------
void CTextureBrowser::OnChangeFilterOrKeywords()
{
	//
	// Start a timer to repaint the texture window using the new filters.
	//
	m_uLastFilterChange = time(NULL);
	m_bFilterChanged = TRUE;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextureBrowser::OnUpdateFiltersNOW()
{
	m_uLastFilterChange = time(NULL);
	m_bFilterChanged = FALSE;

	CString str;
	int iSel = m_cFilter.GetCurSel();
	m_cFilter.GetLBText(iSel, str);
	m_cTextureWindow.SetNameFilter(str);
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextureBrowser::OnUpdateKeywordsNOW()
{
	m_uLastFilterChange = time(NULL);
	m_bFilterChanged = FALSE;

	CString str;
	int iSel = m_cKeywords.GetCurSel();
	m_cKeywords.GetLBText(iSel, str);
	m_cTextureWindow.SetKeywords(str);
}


//-----------------------------------------------------------------------------
// Purpose: Timer used to control updates when the filter terms change.
// Input  : nIDEvent -
//-----------------------------------------------------------------------------
void CTextureBrowser::OnTimer(UINT nIDEvent)
{
	if (!m_bFilterChanged)
	{
		return;
	}

	if ((time(NULL) - m_uLastFilterChange) > 0)
	{
		KillTimer(nIDEvent);
		m_bFilterChanged = FALSE;

		m_cTextureWindow.EnableUpdate(false);

		CString str;
		m_cFilter.GetWindowText(str);
		m_cTextureWindow.SetNameFilter(str);

		m_cTextureWindow.EnableUpdate(true);

		m_cKeywords.GetWindowText(str);
		m_cTextureWindow.SetKeywords(str);

		SetTimer(nIDEvent, 500, NULL);
	}

	CDialog::OnTimer(nIDEvent);
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : wParam -
//			lParam -
// Output : LRESULT
//-----------------------------------------------------------------------------
LRESULT CTextureBrowser::OnTextureWindowDblClk(WPARAM wParam, LPARAM lParam)
{
	WriteSettings();
	SaveAndExit();
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : wParam -
//			lParam -
// Output : LRESULT
//-----------------------------------------------------------------------------
LRESULT CTextureBrowser::OnTexturewindowSelchange(WPARAM wParam, LPARAM lParam)
{
	IEditorTexture *pTex = g_Textures.FindActiveTexture(m_cTextureWindow.szCurTexture);
	CString str;
	char szName[MAX_PATH];

	if (pTex != NULL)
	{
		// create description of texture
		if ( !pTex->IsDummy() )
			str.Format( "%dx%d", pTex->GetWidth(), pTex->GetHeight() );
		pTex->GetShortName(szName);
	}
	else
	{
		szName[0] = '\0';
	}

	m_cCurName.SetWindowText(szName);
	m_cCurDescription.SetWindowText(str);

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextureBrowser::WriteSettings()
{
	// write position information
	CWinApp *pApp = AfxGetApp();
	CString str;
	CRect r;
	GetWindowRect(r);
	str.Format("%ld %ld %ld %ld", r.left, r.top, r.right, r.bottom);
	pApp->WriteProfileString(pszIniSection, "Position", str);
	pApp->WriteProfileInt(pszIniSection, "ShowSize", m_cSizeList.GetCurSel());
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextureBrowser::SaveAndExit()
{
	// save current filter string
	CString str;
	m_cFilter.GetWindowText( str );

	int i;
	for ( i = 0; i < m_FilterHistory.GetSize(); ++i )
	{
		if ( !m_FilterHistory[i].CompareNoCase( str ) )
			break;
	}

	if ( i != m_FilterHistory.GetSize() )	// delete first
		m_FilterHistory.RemoveAt( i );

	m_FilterHistory.InsertAt( 0, str );

	m_cKeywords.GetWindowText( m_szLastKeywords, sizeof( m_szLastKeywords ) );

	CNumStr n;
	KeyValuesAD values( "Filter History" );
	for ( int i = 0; i < m_FilterHistory.GetSize(); i++ )
		values->SetString( ( n.SetInt32( i ), n.String() ), m_FilterHistory[i] );

	auto dest = APP()->GetProfileKeyValues( pszIniSection )->FindKey( "Filter History", true );
	while ( auto k = dest->GetFirstSubKey() )
		dest->RemoveSubKey( k );
	values->CopySubkeys( dest );

	AfxGetApp()->WriteProfileString( pszIniSection, "Keywords", m_szLastKeywords );

	const auto& colorVals = m_ColorFilter.ColorList();
	char key[] = { char( 96 ), '\0' }; // to keep colors sorted in config
	KeyValuesAD colors( "Color History" );
	for ( int i = 0; i < colorVals.GetSize(); ++i )
		colors->SetColor( ( ++key[0], key ), Color::RawColor( colorVals[i] ) );
	dest = APP()->GetProfileKeyValues( pszIniSection )->FindKey( "Color History", true );
	while ( auto k = dest->GetFirstSubKey() )
		dest->RemoveSubKey( k );
	colors->CopySubkeys( dest );
	APP()->WriteProfileColor( pszIniSection, "Last Color", m_ColorFilter.GetColor() );
	APP()->WriteProfileInt( pszIniSection, "Filter By Color", m_FilterColor.GetCheck() != 0 );

	EndDialog( IDOK );
}


//-----------------------------------------------------------------------------
// Purpose: Sets a name filter that will override whatever is in the history
//			for this browser session.
//-----------------------------------------------------------------------------
void CTextureBrowser::SetFilter( const char* pszFilter )
{
	if (pszFilter)
	{
		strcpy(m_szNameFilter, pszFilter);
	}
	else
	{
		m_szNameFilter[0] = '\0';
	}
}


//-----------------------------------------------------------------------------
// Filter buttons
//-----------------------------------------------------------------------------

void CTextureBrowser::OnFilterOpaque(void)
{
	bool checked = m_FilterOpaque.GetCheck( ) != 0;
	m_cTextureWindow.SetTypeFilter( CTextureWindow::TYPEFILTER_OPAQUE, checked );
}

void CTextureBrowser::OnFilterTranslucent(void)
{
	bool checked = m_FilterTranslucent.GetCheck( ) != 0;
	m_cTextureWindow.SetTypeFilter( CTextureWindow::TYPEFILTER_TRANSLUCENT, checked );
}

void CTextureBrowser::OnFilterSelfIllum(void)
{
	bool checked = m_FilterSelfIllum.GetCheck( ) != 0;
	m_cTextureWindow.SetTypeFilter( CTextureWindow::TYPEFILTER_SELFILLUM, checked );
}

void CTextureBrowser::OnFilterEnvmask(void)
{
	bool checked = m_FilterEnvMask.GetCheck( ) != 0;
	m_cTextureWindow.SetTypeFilter( CTextureWindow::TYPEFILTER_ENVMASK, checked );
}

void CTextureBrowser::OnShowErrors(void)
{
	bool checked = m_ShowErrors.GetCheck( ) != 0;
	m_cTextureWindow.ShowErrors( checked );
}

void CTextureBrowser::OnFilterColor()
{
	bool checked = m_FilterColor.GetCheck() != 0;
	m_ColorFilter.EnableWindow( checked );
	if ( checked )
	{
		const auto clr = m_ColorFilter.GetColor();
		const color24 col{ GetRValue( clr ), GetGValue( clr ), GetBValue( clr ) };
		m_cTextureWindow.SetColorFilter( &col );
	}
	else
		m_cTextureWindow.SetColorFilter( nullptr );
}

void CTextureBrowser::CColorButton::UpdateColor( COLORREF color )
{
	static_cast<CTextureBrowser*>( GetParent() )->UpdateColorFilter( color );
	CMFCColorButton::UpdateColor( color );

	// Special handling for color history
	for ( int i = 0; i < m_Colors.GetCount(); i++ )
	{
		if ( m_Colors[i] == color )
		{
			m_Colors.RemoveAt( i );
			break;
		}
	}

	m_Colors.InsertAt( 0, color );
	if ( m_Colors.GetCount() > 20 )
		m_Colors.RemoveAt( 20 );
}

void CTextureBrowser::UpdateColorFilter( COLORREF clr )
{
	if ( m_FilterColor.GetCheck() == 0 )
		return;
	const color24 col{ GetRValue( clr ), GetGValue( clr ), GetBValue( clr ) };
	m_cTextureWindow.SetColorFilter( &col );
}

//-----------------------------------------------------------------------------
// Opens the source file:
//-----------------------------------------------------------------------------
void CTextureBrowser::OnOpenSource()
{
	if ( m_cTextureWindow.szCurTexture[0] )
	{
		g_Textures.OpenSource( m_cTextureWindow.szCurTexture );
	}
}

//-----------------------------------------------------------------------------
// Explores to the source file:
//-----------------------------------------------------------------------------
void CTextureBrowser::OnExploreToSource()
{
	if ( m_cTextureWindow.szCurTexture[0] )
	{
		g_Textures.ExploreToSource( m_cTextureWindow.szCurTexture );
	}
}

void CTextureBrowser::OnReload()
{
	if ( m_cTextureWindow.szCurTexture[0] )
	{
		g_Textures.ReloadTextures( m_cTextureWindow.szCurTexture );
		m_cTextureWindow.Invalidate();

		if (GetMainWnd())
		{
			GetMainWnd()->m_TextureBar.NotifyGraphicsChanged();
			GetMainWnd()->m_pFaceEditSheet->NotifyGraphicsChanged();
		}
	}
}

void CTextureBrowser::OnRecache()
{
	g_Textures.CalcColorInfoForAllMaterials();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextureBrowser::OnMark(void)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		pDoc->ReplaceTextures(m_cTextureWindow.szCurTexture, "", TRUE, 0x100, FALSE, FALSE);
		EndDialog(IDOK);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Invokes the replace texture dialog.
//-----------------------------------------------------------------------------
void CTextureBrowser::OnReplace(void)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if(!pDoc)
		return;

	CReplaceTexDlg dlg(pDoc->GetSelection()->GetCount());

	dlg.m_strFind = m_cTextureWindow.szCurTexture;

	if(dlg.DoModal() != IDOK)
		return;

	// mark undo position
	GetHistory()->MarkUndoPosition(pDoc->GetSelection()->GetList(), "Replace Textures");

	if(dlg.m_bMarkOnly)
	{
		pDoc->SelectObject(NULL, scClear);	// clear selection first
	}

	dlg.DoReplaceTextures();

	//EndDialog(IDOK);

	if (m_bUsed)
	{
		SetUsed(TRUE);
	}
}