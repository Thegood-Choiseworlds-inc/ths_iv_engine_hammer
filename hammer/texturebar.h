//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef TEXTUREBAR_H
#define TEXTUREBAR_H
#ifdef _WIN32
#pragma once
#endif


#include "texturebox.h"
#include "wndtex.h"
#include "controlbarids.h"
#include "hammerbar.h"

class IEditorTexture;


class CTextureBar : public CHammerBar
{
public:
	CTextureBar() : CHammerBar(), m_pCurTex(nullptr) {}
	BOOL Create(CWnd *pParentWnd, int IDD = IDD_TEXTUREBAR, int iBarID = IDCB_TEXTUREBAR);

	void NotifyGraphicsChanged(void);
	void NotifyNewMaterial( IEditorTexture *pTexture );

	void UpdateTextureList();

protected:

	void UpdateTexture(void);

	IEditorTexture *m_pCurTex;
	CTextureBox m_TextureList;
	CComboBox m_TextureGroupList;
	wndTex m_TexturePic;

	afx_msg void UpdateControl(CCmdUI *);
	afx_msg void OnBrowse(void);
	afx_msg void OnChangeTextureGroup(void);
	afx_msg void OnReplace(void);
	afx_msg void OnUpdateTexname(void);
	afx_msg void OnQuickNodraw(void);
	afx_msg void OnWindowPosChanged(WINDOWPOS *pPos);
	virtual afx_msg void OnSelChangeTexture(void);

	DECLARE_MESSAGE_MAP()
};


#endif // TEXTUREBAR_H
