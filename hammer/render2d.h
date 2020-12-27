//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines the interface for rendering in the 2D views.
//
// $NoKeywords: $
//=============================================================================//

#ifndef RENDER2D_H
#define RENDER2D_H
#ifdef _WIN32
#pragma once
#endif

#include "Render.h"

class CMapView2DBase;

class CRender2D : public CRender
{
public:
	//
	// construction/deconstruction
	//
	CRender2D();
	~CRender2D();

	bool SetView( CMapView *pView ) override;

	//
	// setup (view) data
	//

	void MoveTo( const Vector &vPoint );
	void DrawLineTo( const Vector &vPoint );
	void DrawRectangle( const Vector &vMins, const Vector &vMaxs, bool bFill = false, int extent = 0 );
	void DrawBox( const Vector &vMins, const Vector &vMaxs, bool bFill = false );
	void DrawCircle( const Vector &vCenter, float fRadius );
	void DrawArrow( const Vector& vStart, const Vector& vDir, float flLengthBase = 16.0f, float flLengthTip = 8.0f, float flRadiusBase = 1.2f, float flRadiusTip = 4.0f );

protected:

	Vector m_vCurLine;
	CMapView2DBase* m_pView;
};


#endif // RENDER2D_H
