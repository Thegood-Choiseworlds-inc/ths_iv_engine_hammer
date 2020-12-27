//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//
//=============================================================================//
// OptionProperties.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionProperties

#include "optgeneral.h"
#include "optview2d.h"
#include "optview3d.h"
#include "opttextures.h"
#include "optconfigs.h"
#include "optbuild.h"
#include "optcolor.h"
#include "optdiscord.h"

class COptionProperties : public CPropertySheet
{
	DECLARE_DYNAMIC(COptionProperties)

// Construction
public:
	COptionProperties(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	COptionProperties(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);

// Attributes
public:
	COPTGeneral General;
	COPTView2D View2D;
	COPTView3D View3D;
	COPTTextures Textures;
	COPTConfigs Configs;
	COPTBuild Build;
	COPTColor Color;
	COPTDiscord Discord;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptionProperties)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~COptionProperties();
	void DoStandardInit();

	// Generated message map functions
protected:
	//{{AFX_MSG(COptionProperties)
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
