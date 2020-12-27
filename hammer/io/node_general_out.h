#pragma once

#include "vbasenode.h"

class COutNode : public CBaseNode
{
public:
	COutNode( CNodeView* pView );

	int GetNodeType() const override { return IONODE_GENERAL_OUT; }

private:
};