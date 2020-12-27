//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:
//
//===========================================================================//

#include "shaderlib/ShaderDLL.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/ishaderapi.h"
#include "IShaderSystem.h"
#include "tier1/utlvector.h"
#include "ShaderlibCvar.h"
#include "tier1/tier1.h"
#include "shaderapi/ishaderapi.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

abstract_class ILoadShader
{
public:
	virtual void LoadShaderDll( const char* fullDllPath ) = 0;
};

//-----------------------------------------------------------------------------
// The standard implementation of CShaderDLL
//-----------------------------------------------------------------------------
class CShaderDLL final : public IShaderDLLInternal, public IShaderDLL, public ILoadShader
{
public:
	CShaderDLL();

	// methods of IShaderDLL
	void InsertShader( IShader* pShader ) override;

	// methods of IShaderDLLInternal
	bool Connect( CreateInterfaceFn factory, bool bIsMaterialSystem ) override;
	void Disconnect( bool bIsMaterialSystem ) override;
	int ShaderCount() const override;
	IShader* GetShader( int nShader ) override;

	void LoadShaderDll( const char* fullDllPath ) override;

private:
	CUtlVector<IShader*> m_ShaderList;
	CUtlVector<CSysModule*> m_ShaderModules;
	CUtlVector<IShaderDLLInternal*> m_Shaders;
};


//-----------------------------------------------------------------------------
// Global interfaces/structures
//-----------------------------------------------------------------------------
IMaterialSystemHardwareConfig* g_pHardwareConfig = nullptr;
const MaterialSystem_Config_t* g_pConfig = nullptr;
IShaderAPI* g_pShaderAPI = nullptr;

IShaderSystem* g_pSLShaderSystem = nullptr;

// Pattern necessary because shaders register themselves in global constructors
static CShaderDLL* s_pShaderDLL;
//-----------------------------------------------------------------------------
// Global accessor
//-----------------------------------------------------------------------------
IShaderDLL* GetShaderDLL()
{
	// Pattern necessary because shaders register themselves in global constructors
	if ( !s_pShaderDLL )
		s_pShaderDLL = new CShaderDLL;

	return s_pShaderDLL;
}

IShaderDLLInternal* GetShaderDLLInternal()
{
	// Pattern necessary because shaders register themselves in global constructors
	if ( !s_pShaderDLL )
		s_pShaderDLL = new CShaderDLL;

	return static_cast<IShaderDLLInternal*>( s_pShaderDLL );
}

ILoadShader* GetShaderShaderLoadIf()
{
	// Pattern necessary because shaders register themselves in global constructors
	if ( !s_pShaderDLL )
		s_pShaderDLL = new CShaderDLL;

	return static_cast<ILoadShader*>( s_pShaderDLL );
}

//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
EXPOSE_INTERFACE_FN( (InstantiateInterfaceFn)GetShaderDLLInternal, IShaderDLLInternal, SHADER_DLL_INTERFACE_VERSION );
EXPOSE_INTERFACE_FN( (InstantiateInterfaceFn)GetShaderShaderLoadIf, ILoadShader, "ILoadShaderDll001" );

//-----------------------------------------------------------------------------
// Connect, disconnect...
//-----------------------------------------------------------------------------
CShaderDLL::CShaderDLL()
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
}


//-----------------------------------------------------------------------------
// Connect, disconnect...
//-----------------------------------------------------------------------------
bool CShaderDLL::Connect( CreateInterfaceFn factory, bool bIsMaterialSystem )
{
	g_pHardwareConfig = static_cast<IMaterialSystemHardwareConfig*>( factory( MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION, nullptr ) );
	g_pConfig = static_cast<const MaterialSystem_Config_t*>( factory( MATERIALSYSTEM_CONFIG_VERSION, nullptr ) );
	g_pSLShaderSystem = static_cast<IShaderSystem*>( factory( SHADERSYSTEM_INTERFACE_VERSION, nullptr ) );
	CSysModule* shaderApi = Sys_LoadModule( "shaderapidx9", SYS_NOLOAD );
	if ( !shaderApi )
		shaderApi = Sys_LoadModule( "shaderapiempty", SYS_NOLOAD );
#ifndef WIN32
	if ( !shaderApi )
		shaderApi = Sys_LoadModule( "shaderapiempty_srv", SYS_NOLOAD );
#endif
	g_pShaderAPI = static_cast<IShaderAPI*>( Sys_GetFactory( shaderApi )( SHADERAPI_INTERFACE_VERSION, nullptr ) );

	if ( !bIsMaterialSystem )
	{
		ConnectTier1Libraries( &factory, 1 );
  		InitShaderLibCVars( factory );
	}

	for ( IShaderDLLInternal* i : m_Shaders )
		if ( !i->Connect( factory, bIsMaterialSystem ) )
			return false;

	return g_pConfig != nullptr && g_pHardwareConfig != nullptr && g_pSLShaderSystem != nullptr && g_pShaderAPI != nullptr;
}

void CShaderDLL::Disconnect( bool bIsMaterialSystem )
{
	for ( IShaderDLLInternal* i : m_Shaders )
		i->Disconnect( bIsMaterialSystem );

	if ( !bIsMaterialSystem )
	{
		ConVar_Unregister();
		DisconnectTier1Libraries();
	}

	g_pHardwareConfig = nullptr;
	g_pConfig = nullptr;
	g_pSLShaderSystem = nullptr;
	g_pShaderAPI = nullptr;
}

//-----------------------------------------------------------------------------
// Iterates over all shaders
//-----------------------------------------------------------------------------
int CShaderDLL::ShaderCount() const
{
	return m_ShaderList.Count();
}

IShader* CShaderDLL::GetShader( int nShader )
{
	if ( !m_ShaderList.IsValidIndex( nShader ) )
		return nullptr;

	return m_ShaderList[nShader];
}


//-----------------------------------------------------------------------------
// Adds to the shader lists
//-----------------------------------------------------------------------------
void CShaderDLL::InsertShader( IShader* pShader )
{
	Assert( pShader );
	m_ShaderList.AddToTail( pShader );
}

void CShaderDLL::LoadShaderDll( const char* fullPath )
{
	if ( Sys_LoadModule( fullPath, SYS_NOLOAD ) )
	{
		Assert( 0 ); // only load non-loaded dlls
		return;
	}

	CSysModule* module = nullptr;
	void* interface = nullptr;
	if ( !Sys_LoadInterface( fullPath, SHADER_DLL_INTERFACE_VERSION, &module, &interface ) )
	{
		Assert( 0 );
		return;
	}
	m_ShaderModules.AddToTail( module );
	m_Shaders.AddToTail( static_cast<IShaderDLLInternal*>( interface ) );

	auto current = m_Shaders.Tail();

	m_ShaderList.EnsureCapacity( m_ShaderList.Count() + current->ShaderCount() );
	for ( int i = 0; i < current->ShaderCount(); i++ )
		m_ShaderList.AddToTail( current->GetShader( i ) );
}