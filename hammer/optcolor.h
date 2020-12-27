//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#if !defined(AFX_OPTBUILD_H__33E3A4A0_933D_1E21_8C08_444553540000__INCLUDED_)
#define AFX_OPTBUILD_H__33E3A4A0_933D_1E21_8C08_444553540000__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "afxcolorbutton.h"

/////////////////////////////////////////////////////////////////////////////
// COPTColor dialog

class COPTColor : public CPropertyPage
{
// Construction
public:
	COPTColor();   // standard constructor

// Dialog Data
	//{{AFX_DATA(COPTColor)
	enum { IDD = IDD_OPTIONS_COLOR };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COPTColor)
public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COPTColor)
	virtual BOOL OnInitDialog();
	afx_msg void OnCustomClick();
	afx_msg BOOL OnCheckBoxClicked( UINT nID );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	class CColorButton : public CMFCColorButton
	{
	public:
		using CMFCColorButton::CMFCColorButton;

		void Setup( UINT nID, CWnd* parent, COLORREF* clr )
		{
			SubclassDlgItem( nID, parent );
			EnableOtherButton( "Other" );
			m_color = clr;
			SetColor( *clr );
		}

		void UpdateColor( COLORREF color ) override
		{
			*m_color = color;
			CMFCColorButton::UpdateColor( color );
		}

	private:
		COLORREF* m_color = nullptr;
	};

	CButton m_Enable;
	CButton m_ScaleAxis;
	CButton m_ScaleGrid;
	CButton m_ScaleDotGrid;
	CButton m_Scale10Grid;
	CButton m_Scale1024Grid;
	CColorButton m_Colors[18];
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTBUILD_H__33E3A4A0_933D_1E21_8C08_444553540000__INCLUDED_)
