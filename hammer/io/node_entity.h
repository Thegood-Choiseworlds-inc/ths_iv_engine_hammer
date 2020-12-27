#pragma once

#include "vbasenode.h"

class CEntityNode : public CBaseNode
{
public:
	CEntityNode( CNodeView* pView );

	int GetNodeType() const override { return IONODE_ENTITY; }

private:
};