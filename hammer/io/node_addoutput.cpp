
#include "node_addoutput.h"
#include "vjack.h"

CAddoutputNode::CAddoutputNode( CNodeView* pView ) : CBaseNode( "lol", pView )
{
	GenerateJacks_Input( 1 );
	GenerateJacks_Output( 1 );

	GetJack_In( 0 )->SetSmartType( TYPE_FLOAT );
	GetJack_In( 1 )->SetSmartType( TYPE_VOID );
	GetJack_Out( 0 )->SetSmartType( TYPE_BOOL );
	GetJack_Out( 1 )->SetSmartType( TYPE_VOID );

	GetJack_In( 0 )->SetName( "A" );
	GetJack_In( 1 )->SetName( "B" );
	GetJack_Out( 1 )->SetName( "B" );
}