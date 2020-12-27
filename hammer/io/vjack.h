#ifndef CJACK_H
#define CJACK_H
#pragma once

#include "mathlib/vector2d.h"
#include "tier1/utlvector.h"

class CBridge;
class CBaseNode;
class CNodeView;

static inline constexpr int JACK_SIZE_Y = 6;
static inline constexpr int JACK_SIZE_X  = 8;
static inline constexpr int JACK_DELTA_Y = 5;

enum JackType
{
	TYPE_VOID = 0,
	TYPE_INT,
	TYPE_BOOL,
	TYPE_STRING,
	TYPE_FLOAT,
	TYPE_VECTOR,
	TYPE_EHANDLE,
	TYPE_COLOR,

	TYPE_TARGET,

	TYPE_COUNT
};

class CJack
{
public:
	CJack( CBaseNode* p, int slot, bool input );
	~CJack();

	void SetName( const char* name );
	const char* GetName() const { return szName; }
	CBaseNode* GetParentNode() const { return pNode; }

	void ConnectBridge( CBridge* b );
	void OnBridgeRemoved( CBridge* b );
	void PurgeBridges();

	int GetSmartType() const { return m_iSmartType; }
	void SetSmartType( int t );
	void SetSmartTypeLocked( bool b );
	bool IsSmartTypeLocked() const { return bLockSmartType; }
	void UpdateSmartType( CBridge* b );

	void VguiDraw( bool bShadow = false ) const;
	void VguiDrawName() const;

	Vector2D GetBoundsMin() const;
	Vector2D GetBoundsMax() const;
	Vector2D GetCenter() const;
	Vector2D GetBoundsMinPanelSpace() const;
	Vector2D GetBoundsMaxPanelSpace() const;
	Vector2D GetCenterPanelSpace() const;
	bool IsWithinBounds_Base( const Vector2D& pos ) const;

	void UpdatePositionData();
	void UpdatePosition();
	void SetPosition( Vector2D vec, bool bCenter = false );
	Vector2D GetPosition() const;

	bool IsInput() const { return m_bInput; }
	bool IsOutput() const { return !m_bInput; }
	int GetSlot() const { return m_iSlot; }

	void OnParentMoved();

	bool HasFocus() const { return bHasFocus; }
	void SetFocus( bool b ) { bHasFocus = b; }

	int GetFinalTextInset() const;

	int GetNumBridges() const;
	int GetNumBridgesConnected() const;
	CBridge* GetBridge( const int idx ) const { return m_hBridgeList[idx]; }
	CBridge* BridgeBeginBuild();
	void BridgeEndBuild( CBridge* pBridge );
	void DisconnectBridges();

	bool IsTypeCompatibleWith( const CJack* other ) const;

private:
	char szName[MAX_PATH];

	int m_iSmartType;
	bool bLockSmartType;

	Vector2D m_vecPosition;
	Vector2D m_vecSize;

	CBaseNode* pNode;
	CNodeView* pNodeView;
	CUtlVector<CBridge*> m_hBridgeList;

	bool bHasFocus;

	bool m_bInput;
	int m_iSlot;
};



#endif