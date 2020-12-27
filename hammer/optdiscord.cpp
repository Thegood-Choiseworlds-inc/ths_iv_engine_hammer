//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ====
//
// Purpose:
//
//=============================================================================

#include "stdafx.h"
#include "hammer.h"
#include "gameconfig.h"
#include "optdiscord.h"
#include "options.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

COPTDiscord::COPTDiscord()
	: CPropertyPage(COPTDiscord::IDD)
{
	//{{AFX_DATA_INIT(COPTDiscord)
	//}}AFX_DATA_INIT
}


void COPTDiscord::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COPTDiscord)
	DDX_Control(pDX, IDC_DISCORD_ENABLE, m_Enable);
	DDX_Control(pDX, IDC_DISCORD_TEMPLATE_L1, m_Line1);
	DDX_Control(pDX, IDC_DISCORD_TEMPLATE_L2, m_Line2);

	DDX_Check(pDX, IDC_DISCORD_ENABLE, Options.discord.bEnable);
	DDX_Text(pDX, IDC_DISCORD_TEMPLATE_L1, Options.discord.sLine1Template);
	DDX_Text(pDX, IDC_DISCORD_TEMPLATE_L2, Options.discord.sLine2Template);
	//}}AFX_DATA_MAP

	m_Line1.EnableWindow( Options.discord.bEnable );
	m_Line2.EnableWindow( Options.discord.bEnable );
}

BEGIN_MESSAGE_MAP(COPTDiscord, CPropertyPage)
	//{{AFX_MSG_MAP(COPTDiscord)
	ON_BN_CLICKED( IDC_DISCORD_ENABLE, &ThisClass::OnEnableClick )
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL COPTDiscord::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	SetModified(TRUE);

	return TRUE;
}

void DiscordCheckState();
BOOL COPTDiscord::OnApply()
{
	DiscordCheckState();
	return CPropertyPage::OnApply();
}

void COPTDiscord::OnEnableClick()
{
	Options.discord.bEnable = m_Enable.GetCheck() == BST_CHECKED;
	m_Line1.EnableWindow( Options.discord.bEnable );
	m_Line2.EnableWindow( Options.discord.bEnable );
}