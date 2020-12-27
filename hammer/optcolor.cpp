//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ====
//
// Purpose:
//
//=============================================================================

#include "stdafx.h"
#include "hammer.h"
#include "gameconfig.h"
#include "optcolor.h"
#include "options.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

COPTColor::COPTColor()
	: CPropertyPage(COPTColor::IDD)
{
	//{{AFX_DATA_INIT(COPTColor)
	//}}AFX_DATA_INIT
}


void COPTColor::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COPTColor)
	DDX_Control(pDX, IDC_COLOR_ENABLE, m_Enable);
	DDX_Control(pDX, IDC_SCALE_AXIS, m_ScaleAxis);
	DDX_Control(pDX, IDC_SCALE_GRID, m_ScaleGrid);
	DDX_Control(pDX, IDC_SCALE_DOT_GRID, m_ScaleDotGrid);
	DDX_Control(pDX, IDC_SCALE_10_GRID, m_Scale10Grid);
	DDX_Control(pDX, IDC_SCALE_1024_GRID, m_Scale1024Grid);
	//}}AFX_DATA_MAP

	m_Colors[0].Setup( IDC_COLOR_AXIS, this, &Options.colors.clrAxis );
	m_Colors[1].Setup( IDC_COLOR_GRID, this, &Options.colors.clrGrid );
	m_Colors[2].Setup( IDC_COLOR_DOT_GRID, this, &Options.colors.clrGridDot );
	m_Colors[3].Setup( IDC_COLOR_10_GRID, this, &Options.colors.clrGrid10 );
	m_Colors[4].Setup( IDC_COLOR_1024_GRID, this, &Options.colors.clrGrid1024 );
	m_Colors[5].Setup( IDC_COLOR_BACKGROUND, this, &Options.colors.clrBackground );
	m_Colors[6].Setup( IDC_COLOR_BRUSH, this, &Options.colors.clrBrush );
	m_Colors[7].Setup( IDC_COLOR_ENTITY, this, &Options.colors.clrEntity );
	m_Colors[8].Setup( IDC_COLOR_SELECTION, this, &Options.colors.clrSelection );
	m_Colors[9].Setup( IDC_COLOR_VERTEX, this, &Options.colors.clrVertex );
	m_Colors[10].Setup( IDC_COLOR_TOOL_HANDLE, this, &Options.colors.clrToolHandle );
	m_Colors[11].Setup( IDC_COLOR_TOOL_BLOCK, this, &Options.colors.clrToolBlock );
	m_Colors[12].Setup( IDC_COLOR_TOOL_SELECTION, this, &Options.colors.clrToolSelection );
	m_Colors[13].Setup( IDC_COLOR_TOOL_MORPH, this, &Options.colors.clrToolMorph );
	m_Colors[14].Setup( IDC_COLOR_TOOL_PATH, this, &Options.colors.clrToolPath );
	m_Colors[15].Setup( IDC_COLOR_TOOL_DRAG, this, &Options.colors.clrToolDrag );
	m_Colors[16].Setup( IDC_COLOR_WIREFRAME, this, &Options.colors.clrModelCollisionWireframe );
	m_Colors[17].Setup( IDC_COLOR_WIREFRAME_NS, this, &Options.colors.clrModelCollisionWireframeDisabled );

	m_Enable.SetCheck( Options.colors.bUseCustom ? BST_CHECKED : BST_UNCHECKED );
	m_ScaleAxis.SetCheck( Options.colors.bScaleAxisColor ? BST_CHECKED : BST_UNCHECKED );
	m_ScaleGrid.SetCheck( Options.colors.bScaleGridColor ? BST_CHECKED : BST_UNCHECKED );
	m_ScaleDotGrid.SetCheck( Options.colors.bScaleGridDotColor ? BST_CHECKED : BST_UNCHECKED );
	m_Scale10Grid.SetCheck( Options.colors.bScaleGrid10Color ? BST_CHECKED : BST_UNCHECKED );
	m_Scale1024Grid.SetCheck( Options.colors.bScaleGrid1024Color ? BST_CHECKED : BST_UNCHECKED );

	for ( auto& c : m_Colors )
		c.EnableWindow( Options.colors.bUseCustom );
	m_ScaleAxis.EnableWindow( Options.colors.bUseCustom );
	m_ScaleGrid.EnableWindow( Options.colors.bUseCustom );
	m_ScaleDotGrid.EnableWindow( Options.colors.bUseCustom );
	m_Scale10Grid.EnableWindow( Options.colors.bUseCustom );
	m_Scale1024Grid.EnableWindow( Options.colors.bUseCustom );
}

BEGIN_MESSAGE_MAP(COPTColor, CPropertyPage)
	//{{AFX_MSG_MAP(COPTColor)
	ON_BN_CLICKED( IDC_COLOR_ENABLE, &ThisClass::OnCustomClick )
	ON_COMMAND_EX( IDC_SCALE_AXIS, &ThisClass::OnCheckBoxClicked )
	ON_COMMAND_EX( IDC_SCALE_GRID, &ThisClass::OnCheckBoxClicked )
	ON_COMMAND_EX( IDC_SCALE_DOT_GRID, &ThisClass::OnCheckBoxClicked )
	ON_COMMAND_EX( IDC_SCALE_10_GRID, &ThisClass::OnCheckBoxClicked )
	ON_COMMAND_EX( IDC_SCALE_1024_GRID, &ThisClass::OnCheckBoxClicked )
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL COPTColor::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	SetModified(TRUE);

	return TRUE;
}

BOOL COPTColor::OnApply()
{
	Options.PerformChanges(COptions::secView2D);
	return CPropertyPage::OnApply();
}

void COPTColor::OnCustomClick()
{
	Options.colors.bUseCustom = m_Enable.GetCheck() == BST_CHECKED;
	for ( auto& c : m_Colors )
		c.EnableWindow( Options.colors.bUseCustom );
	m_ScaleAxis.EnableWindow( Options.colors.bUseCustom );
	m_ScaleGrid.EnableWindow( Options.colors.bUseCustom );
	m_ScaleDotGrid.EnableWindow( Options.colors.bUseCustom );
	m_Scale10Grid.EnableWindow( Options.colors.bUseCustom );
	m_Scale1024Grid.EnableWindow( Options.colors.bUseCustom );
}

BOOL COPTColor::OnCheckBoxClicked( UINT nID )
{
	switch ( nID )
	{
	case IDC_SCALE_AXIS:
		Options.colors.bScaleAxisColor = m_ScaleAxis.GetCheck() == BST_CHECKED;
		break;
	case IDC_SCALE_GRID:
		Options.colors.bScaleGridColor = m_ScaleGrid.GetCheck() == BST_CHECKED;
		break;
	case IDC_SCALE_DOT_GRID:
		Options.colors.bScaleGridDotColor = m_ScaleDotGrid.GetCheck() == BST_CHECKED;
		break;
	case IDC_SCALE_10_GRID:
		Options.colors.bScaleGrid10Color = m_Scale10Grid.GetCheck() == BST_CHECKED;
		break;
	case IDC_SCALE_1024_GRID:
		Options.colors.bScaleGrid1024Color = m_Scale1024Grid.GetCheck() == BST_CHECKED;
		break;
	NO_DEFAULT
	}

	return TRUE;
}