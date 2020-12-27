//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ====
//
// Purpose:
//
//=============================================================================

#ifndef MESSAGEWND_H
#define MESSAGEWND_H
#pragma once

#include <afxtempl.h>
#include "globalfunctions.h"
#include "Color.h"

const int MAX_MESSAGE_WND_LINES = 5000;

enum
{
	MESSAGE_WND_MESSAGE_LENGTH = 260
};


class CMessageWnd : private CMDIChildWnd
{
public:

	static CMessageWnd *CreateMessageWndObject();
	void CreateMessageWindow( CMDIFrameWnd *pwndParent, const CRect &rect );

	void ShowMessageWindow();
	void ToggleMessageWindow();
	bool IsVisible();
	bool IsValid() const { return m_hWnd != nullptr; }

	void Activate();

	void Resize( const CRect &rect );

	DECLARE_DYNCREATE(CMessageWnd)

protected:
	CMessageWnd();           // protected constructor used by dynamic creation

	struct MWMSGSTRUCT
	{
		Color	color;
		TCHAR	szMsg[MESSAGE_WND_MESSAGE_LENGTH];
		int		MsgLen; // length of message w/o 0x0
		uint32	repeatCount;
	};

// Attributes
public:
	void AddMsg(MWMSGTYPE type, const TCHAR* msg);
	void AddMsg( const Color& color, const TCHAR* msg );

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMessageWnd)
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CMessageWnd();

	void CalculateScrollSize();
	CArray<MWMSGSTRUCT> MsgArray;

	CFont Font;
	int iCharWidth;	// calculated in first paint
	int iNumMsgs;

	// Generated message map functions
	//{{AFX_MSG(CMessageWnd)
	afx_msg void OnPaint();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


#endif // MESSAGEWND_H
