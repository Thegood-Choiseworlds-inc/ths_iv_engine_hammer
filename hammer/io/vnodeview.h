#ifndef CNODEVIEW_H
#define CNODEVIEW_H
#pragma once

#include "tier1/KeyValues.h"
#include "vgui_controls/Panel.h"
#include "mathlib/vector2d.h"
#include "mathlib/vector4d.h"

class CBaseNode;
class CBaseContainerNode;
class CJack;
class CBridge;
class CNodePropertySheet;
class CSmartTooltip;

typedef unsigned int HCURSTACKIDX;

class CNodeView : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CNodeView, vgui::Panel );

public:
	CNodeView( Panel* parent, const char* pElementName );
	~CNodeView();

	void OnThink() override;
	void Init();

	void PurgeCanvas();
	void InitCanvas();

	void OnBridgeRemoved( CBridge* b );
	void OnJackRemoved( const CJack* j );

	void OnContainerRemoved( CBaseContainerNode* container );
	void OnContainerAdded( CBaseContainerNode* container );
	void ListContainerAtPos( Vector2D& pos, CUtlVector<CBaseContainerNode*>& hList ); // in node space

private:
	CUtlVector<CBaseContainerNode*> m_hContainerNodes;
	int m_iCursorLast;

public:
	bool IsMouseOver() const;

	void OnKeyCodeTyped( vgui::KeyCode code ) override;
	void OnKeyCodePressed( vgui::KeyCode code ) override;
	void OnKeyCodeReleased( vgui::KeyCode code ) override;
	void OnParentKeyCodeTyped( vgui::KeyCode code );
	void OnParentKeyCodePressed( vgui::KeyCode code );
	void OnParentKeyCodeReleased( vgui::KeyCode code );

	void OnMousePressed( vgui::MouseCode code ) override;
	void OnMouseDoublePressed( vgui::MouseCode code ) override;
	void OnMouseReleased( vgui::MouseCode code ) override;
	void OnCursorMoved( int x, int y ) override;
	void OnMouseWheeled( int delta ) override;

	void OnCursorEntered() override;
	void OnCursorExited() override;

	void BeginDrag( bool bNodes );
	bool IsInDrag();
	void StopDrag( bool bNodes );

	void HandleSelectionInputPressed( CBaseNode* pNode );
	void HandleSelectionInputReleased( CBaseNode* pNode );
	void HandleSelectionInputReleasedBox();

	CBaseNode* GetNodeUnderCursor();
	CBaseNode* GetNodeUnderPos( const Vector2D& pos ) const;
	CJack* GetJackUnderCursor( float* tolerance = nullptr, bool bInputs = true, bool bOutputs = true );
	CJack* GetJackUnderPos( const Vector2D& pos, float* tolerance = nullptr, bool bInputs = true, bool bOutputs = true ) const;

	int GetNodeIndex( CBaseNode* n ) const;
	CBaseNode* GetNodeFromIndex( int idx ) const;
	CBaseNode* GetNodeFromType( int type ) const;

	float GetZoomScalar() const;
	void SetZoomScalar( float zoom );
	void ToNodeSpace( Vector2D& pos ) const;
	void ToPanelSpace( Vector2D& pos ) const;
	void GetGraphBoundaries( Vector4D& out ) const;

	void ResetView();
	void ResetView_User( bool bInitial = false );

	Vector2D GetMousePosInNodeSpace();

	vgui::HFont GetFontScaled( float Scale, bool& bVis, float* zoom = nullptr ) const;

	bool ShouldDraw_Datatypes() const;
	bool ShouldDraw_Shadows() const;
	bool ShouldDraw_Nodes() const;
	bool ShouldDraw_Jacks() const;
	bool ShouldDraw_Bridges() const;

	MESSAGE_FUNC_PARAMS( OnMenuClosed, "MenuClose", pData );

	void Paint() override;

protected:
	void ApplySchemeSettings( vgui::IScheme* pScheme ) override;
	void PerformLayout() override;

private:
	void InitColors();

	float m_flErrorTime;
	Color _col_Grid;
	Color _col_Vignette;
	Color _col_OutOfDate;
	Color _col_Error;

public:
	Vector2D& AccessViewPos() { return m_vecPosition; };
	float& AccessViewZoom() { return m_flZoom; };

private:
	void UpdateMousePos();
	void AccumulateMouseMove();
	void SaveMousePos();
	int Mx, My;
	int MxOld, MyOld;
	int MxDelta, MyDelta;

	long m_lTime_MouseLastMoved;

	Vector2D m_vecPosition;
	Vector2D m_vecPositionGoal;
	float m_flZoom;
	float m_flZoomGoal;
	enum
	{
		DRAG_NONE = 0,
		DRAG_VIEW,
		DRAG_NODES,
		DRAG_SBOX,
		DRAG_BRDIGE,
	};
	int iArmDrag;
	bool bInDrag;

	void CreateTooltip( int iNodeIndex );
	void DestroyTooltip();
	CSmartTooltip* m_pTooltip;

	Vector2D m_vecSelectionBoxStart;

private:
	int m_iTex_Darken;
	void Paint_Grid();
	void Paint_Nodes( bool bShadow );
	void Paint_Bridges( bool bShadow );
	void Paint_SelectionBox();
	void Paint_StatusBar();

	void Think_SmoothView();
	void Think_Drag();
	void Think_ArrowMove();
	Vector2D Think_PullView();
	void Think_HighlightJacks();
	void Think_UpdateCursorIcon();
	void Think_CreateTooltip();

private:
	bool IsCursorOutOfBounds( Vector2D* delta = nullptr );
	void ForceFocus();

	MESSAGE_FUNC_CHARPTR( OnCommand, "Command", command );
	MESSAGE_FUNC_INT( OnSpawnNode, "spawnNode", type );
	MESSAGE_FUNC_PARAMS( OnOpenProperties, "OpenProperties", data );

	Vector2D m_vecMenuCreationPosition;
	void CreateContextMenu( CBaseNode* pMouseOverNode );
	void AddNodesToContextMenu( vgui::Menu* pNodeMenu );
	void CreatePropertyDialog( CBaseNode* pNode );

	vgui::DHANDLE<vgui::Menu> m_ContextMenu;
	vgui::DHANDLE<CNodePropertySheet> m_Properties;

	bool AllowKeyInput();

	CUtlVector<CBaseNode*> m_hNodeList;
	CUtlVector<CBaseNode*> m_hNodesInMove;
	CUtlVector<CBridge*> m_hBridgeList;
	CBaseNode* SpawnNode( int type );

public:
	int GetNumNodes() const { return m_hNodeList.Count(); };
	CBaseNode* GetNode( int i ) const
	{
		Assert( m_hNodeList.IsValidIndex( i ) );
		return m_hNodeList[i];
	}

	void MakeSolversDirty();

private:
	bool IsSelectionBiggerThanOne();
	CBaseNode* GetFirstSelectedNode() const;
	void DeselectAllNodes( CBaseNode* pIgnore = nullptr );
	void SelectNodes( CUtlVector<CBaseNode*>& m_hNodes, bool bAdd = true );
	void DragSelection( const Vector2D& delta );
	void MoveNodeToFront( CBaseNode* p );
	void DeleteSelection();
	void DisconnectSelection();

	void StartOverrideIndices( CUtlVector<CBaseNode*>& m_hNodes );
	void FinishOverrideIndices();

	void MoveNodes( const Vector2D& offset, CBaseNode* n );
	void MoveNodes( const Vector2D& offset, CUtlVector<CBaseNode*>& m_hList );

	CJack* pJackLast;
	CBridge* m_pCurBridge;
	bool bOverrideNodeIndices;
	CUtlVector<CBaseNode*> m_hTmpNodeIndex;

public:
	void CreateDragBridge( CJack* j );
	void UpdateDragBridge( CJack* j, CBaseNode* n );
	void FinishDragBridge( CJack* j );
	void AddBridgeToList( CBridge* b );

	const HCURSTACKIDX& GetStackIndex() const { return m_StackIndex; };

private:
	void KillDragBridge();
	CJack* GetBridgeSnapJack();

	HCURSTACKIDX m_StackIndex;

	static constexpr int MAX_ZOOM_FONTS = 85;
	static vgui::HFont _fonts[MAX_ZOOM_FONTS];
};

inline bool __CrossesBounds( const float& p_min, const float& p_max, const float& b_min, const float& b_max )
{
	if ( p_max < b_min )
		return false;
	if ( p_min > b_max )
		return false;
	return true;
}

inline bool ShouldSimpleDrawObject( vgui::Panel* parent, CNodeView* coordSystem, Vector2D mins, Vector2D maxs )
{
	coordSystem->ToPanelSpace( mins );
	coordSystem->ToPanelSpace( maxs );

	int sx, sy;
	parent->GetSize( sx, sy );
	Vector2D p_min( 0, 0 );
	Vector2D p_max( sx, sy );

	if ( !__CrossesBounds( mins.x, maxs.x, p_min.x, p_max.x ) || !__CrossesBounds( mins.y, maxs.y, p_min.y, p_max.y ) )
		return false;

	return true;
}

inline void UpdateSimpleObjectBounds( Vector2D& pos, Vector2D& size, Vector4D& bounds )
{
	constexpr float BOUNDS_EXTRUDE = 30;
	bounds.x = pos.x - BOUNDS_EXTRUDE;
	bounds.y = pos.y + BOUNDS_EXTRUDE;
	bounds.z = pos.x + size.x + BOUNDS_EXTRUDE;
	bounds.w = pos.y + size.y - BOUNDS_EXTRUDE;
}

#endif