#pragma once

#include "vbasenode.h"

class CAddoutputNode : public CBaseNode
{
public:
	CAddoutputNode( CNodeView* pView );

	int GetNodeType() const override { return IONODE_ADDOUTPUT; }

private:
};