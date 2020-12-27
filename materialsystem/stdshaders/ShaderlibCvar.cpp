//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:
//
//===========================================================================//

#include "icvar.h"
#include "tier1/tier1.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ------------------------------------------------------------------------------------------- //
// ConVar stuff.
// ------------------------------------------------------------------------------------------- //
class CShaderLibConVarAccessor final : public IConCommandBaseAccessor
{
public:
	bool RegisterConCommandBase( ConCommandBase* pCommand ) override
	{
		// Link to engine's list instead
		g_pCVar->RegisterConCommand( pCommand );

		char const* pValue = g_pCVar->GetCommandLineValue( pCommand->GetName() );
		if ( pValue && !pCommand->IsCommand() )
			static_cast<ConVar*>( pCommand )->SetValue( pValue );
		return true;
	}
};

static CShaderLibConVarAccessor g_ConVarAccessor;
void InitShaderLibCVars( CreateInterfaceFn cvarFactory )
{
	if ( g_pCVar )
		ConVar_Register( 0, &g_ConVarAccessor );
}
