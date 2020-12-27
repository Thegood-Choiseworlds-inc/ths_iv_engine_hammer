#ifndef CBRIDGE_H
#define CBRIDGE_H
#pragma once

#include "mathlib/vector2d.h"

class CJack;
class CBaseNode;
class CNodeView;

class CBridge
{
public:
	CBridge( CNodeView* p );
	~CBridge();

	void ConnectSource( CJack* j, CBaseNode* n );
	void ConnectDestination( CJack* j, CBaseNode* n );

	void DisconnectSource();
	void DisconnectDestination();

	void SetTemporaryTarget( const Vector2D& tg );

	void VguiDraw( bool bShadow = false );

	CBaseNode* GetEndNode( const CBaseNode* n ) const;
	CBaseNode* GetEndNode( const CJack* j ) const;
	CJack* GetEndJack( const CBaseNode* n ) const;
	CJack* GetEndJack( const CJack* j ) const;

	CJack* GetInputJack() const { return pJ_Src; };
	CJack* GetDestinationJack() const { return pJ_Dst; };
	CBaseNode* GetInputNode() const { return pN_Src; };
	CBaseNode* GetDestinationNode() const { return pN_Dst; };

	int GetErrorLevel() const { return iErrorLevel; };
	void SetErrorLevel( int e ) { iErrorLevel = e; };

	enum
	{
		TMPCOL_NONE = 0,
		TMPCOL_TRANS,
		TMPCOL_ORANGE,
	};
	void SetTemporaryColor( int mode );

private:
	Vector2D tmp;
	CNodeView* pNodeView;
	int iErrorLevel;
	int iTempColor;

	CJack* pJ_Src;
	CJack* pJ_Dst;

	CBaseNode* pN_Src;
	CBaseNode* pN_Dst;

	int m_iTex_Arrow;

	float m_flLength;
	Vector2D start_old;
	Vector2D end_old;
};


#endif