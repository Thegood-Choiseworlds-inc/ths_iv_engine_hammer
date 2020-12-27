//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:
//
// $NoKeywords: $
//===========================================================================//

#ifndef SELECTION3D_H
#define SELECTION3D_H
#ifdef _WIN32
#pragma once
#endif


#include "box3d.h"
#include "mapclass.h"			// For CMapObjectList
#include "toolinterface.h"
#include "utlvector.h"


class CMapWorld;
class CMapView;
class CMapView2D;
class CMapView3D;
class GDinputvariable;
class CRender2D;

class Selection3D : public Box3D
{

public:

	Selection3D();
	~Selection3D();

	void Init( CMapDoc *pDocument );

	inline bool IsBoxSelecting();
	void EndBoxSelection();

	// Tool3D implementation.
	virtual void SetEmpty();
	virtual bool IsEmpty();

	//
	// CBaseTool implementation.
	//
	virtual void OnActivate();
	virtual void OnDeactivate();
	virtual ToolID_t GetToolID() { return TOOL_POINTER; }

	virtual bool OnContextMenu2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);
	virtual bool OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);

	virtual bool OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);
	virtual bool OnLMouseDblClk3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);

	virtual void RenderTool2D(CRender2D *pRender);
	virtual void RenderTool3D(CRender3D *pRender);

	void UpdateSelectionBounds();

	bool		m_bBoxSelection;

protected:

	void TransformSelection();

	void FinishTranslation(bool bSave, bool bClone );
	void StartTranslation(CMapView *pView, const Vector2D &vPoint, const Vector &vHandleOrigin );
	bool StartBoxSelection( CMapView *pView, const Vector2D &vPoint, const Vector &vStart);

	void UpdateHandleState();

	virtual unsigned int GetConstraints(unsigned int nKeyFlags);

	void NudgeObjects(CMapView *pView, int nChar, bool bSnap, bool bClone);

	CMapEntity *FindEntityInTree(CMapClass *pObject);

	void SelectInBox(CMapDoc *pDoc, bool bInsideOnly);
	CBaseTool *GetToolObject( CMapView2D *pView, const Vector2D &ptScreen, bool bAttach );

	void OnEscape(CMapDoc *pDoc);

	//
	// Tool3D implementation.
	//
	virtual int HitTest(CMapView *pView, const Vector2D &pt, bool bTestHandles = false);

	CSelection	*m_pSelection;	// the documents selection opject

	bool m_bSelected;			// Did we select an object on left button down?
	bool m_b3DEditMode;			// editing mode in 3D on/off

	bool m_bDrawAsSolidBox;		// sometimes we want to render the tool bbox solid
};


//-----------------------------------------------------------------------------
// Are we in box selection?
//-----------------------------------------------------------------------------
inline bool Selection3D::IsBoxSelecting()
{
	return m_bBoxSelection;
}

#endif // SELECTION3D_H