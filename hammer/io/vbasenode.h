#ifndef CBASENODE_H
#define CBASENODE_H
#pragma once

#include "mathlib/vector.h"
#include "mathlib/vector2d.h"
#include "mathlib/vector4d.h"
#include "tier1/utlvector.h"
#include "Color.h"

class CJack;
class CBaseNode;
class CBridge;
class CBaseContainerNode;
class CNodeView;

typedef unsigned int HNODE;

static inline constexpr float NODEDEFSIZE_SMALL = 50.0f;

static inline constexpr int NODE_DRAW_TITLE_Y = 15;
static inline constexpr int NODE_DRAW_TITLE_SPACE = 3;

static inline constexpr Color NODE_DRAW_COLOR_TITLE( 60, 60, 70, 255 );
static inline constexpr Color NODE_DRAW_COLOR_BOX( 110, 110, 110, 255 );
static inline constexpr Color NODE_DRAW_COLOR_BOX_COMMENT( 18, 18, 18, 255 );
static inline constexpr Color NODE_DRAW_COLOR_BORDER( 10, 10, 10, 255 );
static inline constexpr Color NODE_DRAW_COLOR_TEXT_OP( 150, 150, 100, 255 );

static inline constexpr Color NODE_DRAW_COLOR_SELECTED_TITLE( 75, 75, 90, 255 );
static inline constexpr Color NODE_DRAW_COLOR_SELECTED_BOX( 220, 150, 110, 255 );
static inline constexpr Color NODE_DRAW_COLOR_SELECTED_BORDER( 10, 10, 10, 255 );
static inline constexpr Color NODE_DRAW_COLOR_SELECTED_TEXT_OP( 170, 170, 120, 255 );

static inline constexpr Color NODE_DRAW_COLOR_HASSOLVER_BOX_ALLOC( 110, 196, 110, 255 );

static inline constexpr Color NODE_DRAW_COLOR_HASCONTAINER_TITLE( 70, 70, 100, 255 );
static inline constexpr Color NODE_DRAW_COLOR_HASCONTAINER_BOX( 110, 110, 148, 255 );

static inline constexpr Color NODE_DRAW_COLOR_CUSTOMTITLE( 160, 150, 180, 255 ); // Color( 120, 110, 140, 255 )

enum ErrorLevel
{
	ERRORLEVEL_NONE,
	ERRORLEVEL_MISSING_IO,
	ERRORLEVEL_WRONG_TYPE
};

enum NodeType
{
	IONODE_INVALID = 0,
	IONODE_ENTITY,
	IONODE_ADDOUTPUT,
	IONODE_GENERAL_OUT
};

class CBaseNode
{
	friend class CBaseContainerNode;
public:
	typedef CBaseNode ThisClass;

	CBaseNode( const char* opName, CNodeView* p );
	virtual ~CBaseNode();

	virtual void SetName( const char* opName );
	virtual const char* GetName() const;
	virtual const char* GetUserName() const;
	virtual int GetFinalTextSize() const;

	virtual void Spawn();

	HNODE GetUniqueIndex() const { return m_iUniqueIndex; }

	CNodeView* GetParent() const { return pNodeView; }

	virtual CBaseContainerNode* GetAsContainer() { return nullptr; }

private:
	CUtlVector<CBaseContainerNode*> m_hParentContainers;
	bool RecursiveTestContainerError_Internal( CUtlVector<CBaseNode*>& m_hNodesProcessed, bool& bLeftContainerOnce, const bool& bHierachyUp, CBaseContainerNode* container );
	void ListContainersChronologically_Internal( CUtlVector<CBaseNode*>& m_hNodesProcessed, CUtlVector<CBaseContainerNode*>& hList );

protected:
	virtual bool InsertToContainer( CBaseContainerNode* container );   // don't call directly
	virtual bool RemoveFromContainer( CBaseContainerNode* container ); // don't call directly

public:
	bool RecursiveTestContainerError( const bool& bHierachyUp, CBaseContainerNode* container ); // true on error
	virtual bool HasContainerParent( CBaseContainerNode* container ) const;

	virtual int GetNumContainers() const;
	virtual CBaseContainerNode* GetContainer( int idx ) const;

	virtual Vector2D GetContainerSensitiveCenter() const;
	virtual void ListContainersChronologically( CUtlVector<CBaseContainerNode*>& hList );

	virtual bool CanBeInContainer() const { return true; }
	virtual void UpdateParentContainers();

	virtual int GetNodeType() const { return IONODE_INVALID; }
	virtual bool IsNodeCrucial() const { return false; }
	virtual bool ShouldErrorOnUndefined() const { return false; }

	virtual int PerNodeErrorLevel() const { return ERRORLEVEL_NONE; }
	virtual int TestJackFlags_In();

	virtual bool VguiDraw( bool bShadow = false ) const;
	virtual void VguiDraw_Jacks( bool bShadow = false ) const;

	virtual void OnLeftClick( const Vector2D& pos );
	virtual void OnDragStart();
	virtual void OnDrag( const Vector2D& delta );
	virtual void OnDragEnd();
	virtual bool MustDragAlone() const;
	virtual bool IsWithinBounds_Base( const Vector2D& pos ) const;

	virtual Vector2D GetBoundsMin() const;
	virtual Vector2D GetBoundsMax() const;
	virtual Vector2D GetBoundsMinNodeSpace() const;
	virtual Vector2D GetBoundsMaxNodeSpace() const;
	virtual Vector2D GetSelectionBoundsMinNodeSpace() const;
	virtual Vector2D GetSelectionBoundsMaxNodeSpace() const;
	virtual Vector2D GetCenter() const;
	const virtual Vector4D& GetBoundsFast() const;

	virtual Vector2D GetBoundsTitleMin() const;
	virtual Vector2D GetBoundsTitleMax() const;
	virtual Vector2D GetBoundsBoxMin() const;
	virtual Vector2D GetBoundsBoxMax() const;

	virtual void SetPosition( Vector2D vec, bool bCenter = false );
	virtual Vector2D GetPosition() const;
	virtual Vector2D GetSize() const;

	virtual void SetSelected( bool b ) { bSelected = b; };
	virtual void ToggleSelection() { bSelected = !bSelected; };
	virtual const bool IsSelected() const { return bSelected; };

protected:
	bool m_bAllInputsRequired;

public:
	virtual int UpdateInputsValid();
	virtual void UpdateOutputs();
	virtual void SetOutputsUndefined();

	virtual void UpdateBridgeValidity( CBridge* pBridge, CJack* pCaller, int inputErrorLevel );

	//virtual void OnBridgeConnect
	virtual void GenerateJacks_Input( int num );
	virtual void GenerateJacks_Output( int num );
	virtual int GetNumJacks_Out() const;
	virtual int GetNumJacks_In() const;
	virtual CJack* GetJack_Out( int i ) const;
	virtual CJack* GetJack_In( int i ) const;
	virtual int GetNumJacks_Out_Connected() const;
	virtual int GetNumJacks_In_Connected() const;
	CJack* GetJackByName_Out( const char* name ) const;
	CJack* GetJackByName_In( const char* name ) const;
	virtual bool JacksAllConnected_Out() const;
	virtual bool JacksAllConnected_In() const;

	virtual void PurgeBridges( bool bInputs = true, bool bOutputs = true );

	virtual void UpdateSize();

private:
	bool RecursiveFindNode_Internal( CUtlVector<const CBaseNode*>& m_hList, CBaseNode* n, bool bHierachyUp ) const;
	void Recursive_AddTailNodes_Internal( CUtlVector<CBaseNode*>& m_hProcessedNodes, CUtlVector<CBaseNode*>& m_hList,
										  bool bHierachyUp, CBaseContainerNode* pContainer, bool bAddContainers );

public:
	bool RecursiveFindNode( CBaseNode* n, bool bHierachyUp ) const;
	void Recursive_AddTailNodes( CUtlVector<CBaseNode*>& m_hList, bool bHierachyUp, CBaseContainerNode* pContainer = nullptr, bool bAddContainers = false );

	int GetErrorLevel() const;
	void SetErrorLevel( int e );

	void SetAllocating( bool a );
	bool IsAllocating() const;

protected:
	bool m_bIsAllocating;

	float m_flMinSizeX;
	float m_flMinSizeY;

	HNODE m_iUniqueIndex;
	static HNODE m_iUniqueIndexCount;

	Vector2D m_vecBorderInfo;
	Vector2D m_vecPosition;
	Vector2D m_vecSize;
	Vector4D m_vecBounds;

	char szOpName[MAX_PATH];
	char szNodeName[MAX_PATH];

	CNodeView* pNodeView;

	bool bSelected;
	int iErrorLevel;

	CUtlVector<CJack*> m_hInputs;
	CUtlVector<CJack*> m_hOutputs;
	void TouchJacks();

	// these lock smarttype!!!
	void LockJackOutput_Flags( int idx, int Flag, const char* name = nullptr );
	void LockJackInput_Flags( int idx, int Flag, const char* name = nullptr );

public:
	void MarkForDeletion();
	bool IsMarkedForDeletion() const;

private:
	bool m_bMarkedForDeletion;
};

#endif