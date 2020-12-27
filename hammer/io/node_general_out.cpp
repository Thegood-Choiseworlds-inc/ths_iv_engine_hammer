
#include "node_general_out.h"
#include "vjack.h"

#include "tier0/memdbgon.h"

COutNode::COutNode( CNodeView* pView ) : CBaseNode( "Output", pView )
{
	GenerateJacks_Input( 1 );

	GetJack_In( 0 )->SetSmartType( TYPE_EHANDLE );
}