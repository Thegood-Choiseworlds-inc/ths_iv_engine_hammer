//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#ifndef MDLPANEL_H
#define MDLPANEL_H

#ifdef _WIN32
#pragma once
#endif


#include "vgui_controls/Panel.h"
#include "datacache/imdlcache.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "matsys_controls/potterywheelpanel.h"
#include "mdlutils.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class IScheme;
}


//-----------------------------------------------------------------------------
// MDL Viewer Panel
//-----------------------------------------------------------------------------
class CMDLPanel : public CPotteryWheelPanel, public CMergedMDL
{
	DECLARE_CLASS_SIMPLE( CMDLPanel, CPotteryWheelPanel );

public:
	// constructor, destructor
	CMDLPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CMDLPanel();

	// Overriden methods of vgui::Panel
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	// Sets the camera to look at the model
	void LookAtMDL( );

	void SetCollsionModel( bool bVisible );
	void SetGroundGrid( bool bVisible );
	void SetWireFrame( bool bVisible );
	void SetLockView( bool bLocked );
	void SetLookAtCamera( bool bLookAtCamera );

	void SetCameraOrientOverride( Vector vecNew ) { m_vecCameraOrientOverride = vecNew; }
	void SetCameraOrientOverrideEnabled( bool bEnabled ) { m_bCameraOrientOverrideEnabled = bEnabled; }
	bool IsCameraOrientOverrideEnabled( void ) { return m_bCameraOrientOverrideEnabled; }

	void SetCameraPositionOverride(Vector vecNew) { m_vecCameraPositionOverride = vecNew; }
	void SetCameraPositionOverrideEnabled(bool bEnabled) { m_bCameraPositionOverrideEnabled = bEnabled; }
	bool IsCameraPositionOverrideEnabled(void) { return m_bCameraPositionOverrideEnabled; }

protected:
	virtual void OnPostSetUpBonesPreDraw() OVERRIDE;
	virtual void OnModelDrawPassStart( int iPass, CStudioHdr *pStudioHdr, int &nFlags ) OVERRIDE;
	virtual void OnModelDrawPassFinished( int iPass, CStudioHdr *pStudioHdr, int &nFlags ) OVERRIDE;

private:
	// paint it!
	void OnPaint3D();
	void OnMouseDoublePressed( vgui::MouseCode code );

	void DrawCollisionModel();
	void UpdateStudioRenderConfig( void );

	float	m_flAutoPlayTimeBase;

	bool	m_bDrawCollisionModel : 1;
	bool	m_bGroundGrid : 1;
	bool	m_bLockView : 1;
	bool	m_bWireFrame : 1;
	bool	m_bLookAtCamera : 1;

	bool	m_bCameraOrientOverrideEnabled;
	Vector	m_vecCameraOrientOverride;

	bool	m_bCameraPositionOverrideEnabled;
	Vector	m_vecCameraPositionOverride;

	CMaterialReference m_modelWireframe;
};

#endif // MDLPANEL_H
