#ifndef CCONTAINER_NODE_H
#define CCONTAINER_NODE_H
#pragma once

#include "vbasenode.h"

static inline constexpr float CBORDER_SIZE = 10.0f;

static inline constexpr float GRABICO_OFFSET = 1.5f;

enum
{
	CBORDER_INVALID = 0,
	CBORDER_TOP_LEFT,
	CBORDER_TOP,
	CBORDER_TOP_RIGHT,
	CBORDER_RIGHT,
	CBORDER_BOTTOM_RIGHT,
	CBORDER_BOTTOM,
	CBORDER_BOTTOM_LEFT,
	CBORDER_LEFT,

	CBORDER_MAX,
	CBORDER_FIRST = CBORDER_TOP_LEFT,
	CBORDER_LAST = CBORDER_LEFT
};

class CBaseContainerNode : public CBaseNode
{
	typedef CBaseNode BaseClass;
	typedef CBaseContainerNode ThisClass;

public:
	CBaseContainerNode( const char* opName, CNodeView* p );
	~CBaseContainerNode() override;
	void Spawn() override;

	int GetNodeType() const override { return IONODE_INVALID; }

	void UpdateSize() override;
	virtual void UpdateContainerBounds();
	void SetPosition( Vector2D vec, bool bCenter = false ) override;
	bool VguiDraw( bool bShadow = false ) const override;

	void OnLeftClick( const Vector2D& pos ) override;
	void OnDrag( const Vector2D& delta ) override;
	void OnDragEnd() override;
	virtual bool ShouldSelectChildrenOnClick() const { return false; }
	void SelectAllInBounds( bool bSelected = true, CUtlVector<CBaseNode*>* hNodes = NULL );

	CBaseContainerNode* GetAsContainer() override { return this; }
	virtual bool HasAnyChildren() const;

	bool IsWithinBounds_Base( const Vector2D& pos ) const override;
	virtual Vector2D GetContainerMins() const;
	virtual Vector2D GetContainerMaxs() const;
	virtual bool IsInContainerBounds( const Vector2D& pos ) const;
	virtual Vector2D GetContainerBorderMin( int mode ) const;
	virtual Vector2D GetContainerBorderMax( int mode ) const;
	virtual int IsInBorderBounds( const Vector2D& pos ) const;

	virtual bool HasChild( CBaseNode* child ) const;
	virtual void RemoveAllChildren();
	virtual void RemoveChild( CBaseNode* child );
	virtual void AddChild( CBaseNode* child );
	virtual int GetNumChildren() const { return m_hChildren.Count(); }
	virtual CBaseNode* GetChild( int i ) const
	{
		Assert( m_hChildren.IsValidIndex( i ) );
		return m_hChildren[i];
	};
	virtual void UpdateOnMove();

	bool MustDragAlone() const override;

protected:
	virtual float GetBorderSize() const { return CBORDER_SIZE; }

	Vector m_vecContainerExtents;

	void DrawGrabIcon() const;

private:

	CUtlVector<CBaseNode*> m_hChildren;

	int m_iActiveScalingMode;
	int m_iGrabIcon;
	Vector2D m_vecMouseStartPos;
	float m_vecAccumulatedExtent[5];
};

#endif