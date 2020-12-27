//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#if !defined(AFX_OPTBUILD_H__33E3A4A0_933D_1E21_8C08_444553540100__INCLUDED_)
#define AFX_OPTBUILD_H__33E3A4A0_933D_1E21_8C08_444553540100__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

/////////////////////////////////////////////////////////////////////////////
// COPTDiscord dialog

class COPTDiscord : public CPropertyPage
{
// Construction
public:
	COPTDiscord();   // standard constructor

// Dialog Data
	//{{AFX_DATA(COPTDiscord)
	enum { IDD = IDD_OPTIONS_DISCORD };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COPTDiscord)
public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COPTDiscord)
	virtual BOOL OnInitDialog();
	afx_msg void OnEnableClick();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	CButton m_Enable;
	CEdit m_Line1;
	CEdit m_Line2;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTBUILD_H__33E3A4A0_933D_1E21_8C08_444553540000__INCLUDED_)
