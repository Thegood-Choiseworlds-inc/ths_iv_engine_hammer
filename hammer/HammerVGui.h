//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements all the functions exported by the GameUI dll
//
// $NoKeywords: $
//=============================================================================//

#pragma once

class IMatSystemSurface;
class CVGuiWnd;

extern IMatSystemSurface *g_pMatSystemSurface;

namespace vgui
{
	typedef unsigned long HScheme;
}

class CHammerVGui
{
public:
	CHammerVGui(void);
	~CHammerVGui(void);

	bool Init( HWND hWindow );
	void Simulate();
	void Shutdown();
	bool HasFocus( CVGuiWnd *pWnd );
	void SetFocus( CVGuiWnd *pWnd );
	bool IsInitialized() { return m_hMainWindow != NULL; };

	vgui::HScheme GetHammerScheme() const { return m_hHammerScheme; }

protected:

	HWND			m_hMainWindow;
	CVGuiWnd		*m_pActiveWindow;	// the VGUI window that has the focus

	vgui::HScheme	m_hHammerScheme;
};

CHammerVGui *HammerVGui();