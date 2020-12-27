//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#include "matsys_controls/assetpicker.h"

#include "vgui_controls/Button.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
//
// Asset Picker with no preview
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CAssetPicker::CAssetPicker( vgui::Panel *pParent, const char *pAssetType,
	const char *pExt, const char *pSubDir, const char *pTextType ) :
	BaseClass( pParent, pAssetType, pExt, pSubDir, pTextType )
{
	CreateStandardControls( this, false, false );
	AutoLayoutStandardControls();
}


//-----------------------------------------------------------------------------
//
// Purpose: Modal picker frame
//
//-----------------------------------------------------------------------------
CAssetPickerFrame::CAssetPickerFrame( vgui::Panel *pParent, const char *pTitle,
	const char *pAssetType, const char *pExt, const char *pSubDir, const char *pTextType ) :
	BaseClass( pParent )
{
	SetMinimumSize( 300, 500 );
	SetAutoResize( PIN_TOPLEFT, AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );
	SetAssetPicker( new CAssetPicker( this, pAssetType, pExt, pSubDir, pTextType ) );
	auto s = new vgui::CBoxSizer( vgui::ESLD_VERTICAL );
	auto s2 = new vgui::CBoxSizer( vgui::ESLD_HORIZONTAL );
	m_pPicker->SetAutoResize( PIN_TOPLEFT, AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );
	s->AddPanel( m_pPicker, vgui::SizerAddArgs_t().Expand( 1.0f ).Padding( 0 ).MinSize( 256, 256 ) );
	s2->AddSpacer( vgui::SizerAddArgs_t().Expand( 1.0f ) );
	s2->AddPanel( m_pCancelButton, vgui::SizerAddArgs_t().Padding( 0 ) );
	s2->AddSpacer( vgui::SizerAddArgs_t().Padding( 2 ) );
	s2->AddPanel( m_pOpenButton, vgui::SizerAddArgs_t().Padding( 0 ) );
	s2->AddSpacer( vgui::SizerAddArgs_t().Padding( 2 ) );

	s->AddSpacer( vgui::SizerAddArgs_t().Padding( 2 ) );
	s->AddSizer( s2, vgui::SizerAddArgs_t().Padding( 0 ) );
	s->AddSpacer( vgui::SizerAddArgs_t().Padding( 2 ) );
	SetSizer( s );
	SetTitle( pTitle, false );
}