//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=====================================================================================//

#include "shaderlib/BaseShader.h"
#include "shaderlib/ShaderDLL.h"
#include "tier0/dbg.h"
#include "ShaderDLL_Global.h"
#include "IShaderSystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/itexture.h"
#include "materialsystem/ishaderapi.h"
#include "materialsystem/materialsystem_config.h"
#include "shaderlib/cshader.h"
#include "mathlib/vmatrix.h"
#include "tier1/strtools.h"
#include "convar.h"
#include "tier0/vprof.h"

// NOTE: This must be the last include file in a .cpp file!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
const char* CBaseShader::s_pTextureGroupName = nullptr;
IMaterialVar** CBaseShader::s_ppParams;
IShaderShadow* CBaseShader::s_pShaderShadow;
IShaderDynamicAPI* CBaseShader::s_pShaderAPI;
IShaderInit* CBaseShader::s_pShaderInit;
int CBaseShader::s_nModulationFlags;
CMeshBuilder *CBaseShader::s_pMeshBuilder;
ConVar mat_fullbright( "mat_fullbright", "0", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CBaseShader::CBaseShader()
{
	GetShaderDLL()->InsertShader( this );
}


//-----------------------------------------------------------------------------
// Shader parameter info
//-----------------------------------------------------------------------------
// Look in BaseShader.h for the enumeration for these.
// Update there if you update here.
static constexpr const ShaderParamInfo_t s_StandardParams[NUM_SHADER_MATERIAL_VARS] =
{
	{ "$flags",						"flags",															SHADER_PARAM_TYPE_INTEGER,	"0",		SHADER_PARAM_NOT_EDITABLE },
	{ "$flags_defined",				"flags_defined",													SHADER_PARAM_TYPE_INTEGER,	"0",		SHADER_PARAM_NOT_EDITABLE },
	{ "$flags2",					"flags2",															SHADER_PARAM_TYPE_INTEGER,	"0",		SHADER_PARAM_NOT_EDITABLE },
	{ "$flags_defined2",			"flags2_defined",													SHADER_PARAM_TYPE_INTEGER,	"0",		SHADER_PARAM_NOT_EDITABLE },
	{ "$color",		 				"color",															SHADER_PARAM_TYPE_COLOR,	"[1 1 1]",	0 },
	{ "$alpha",						"alpha",															SHADER_PARAM_TYPE_FLOAT,	"1.0",		0 },
	{ "$basetexture",				"Base Texture with lighting built in",								SHADER_PARAM_TYPE_TEXTURE,	"shadertest/BaseTexture", 0 },
	{ "$frame",	  					"Animation Frame",													SHADER_PARAM_TYPE_INTEGER,	"0", 0 },
	{ "$basetexturetransform",		"Base Texture Texcoord Transform",									SHADER_PARAM_TYPE_MATRIX,	"center .5 .5 scale 1 1 rotate 0 translate 0 0", 0 },
	{ "$flashlighttexture",			"flashlight spotlight shape texture",								SHADER_PARAM_TYPE_TEXTURE,	"effects/flashlight001", SHADER_PARAM_NOT_EDITABLE },
	{ "$flashlighttextureframe",	"Animation Frame for $flashlight",									SHADER_PARAM_TYPE_INTEGER,	"0", 		SHADER_PARAM_NOT_EDITABLE },
	{ "$color2",					"color2",															SHADER_PARAM_TYPE_COLOR,	"[1 1 1]",	0 },
	{ "$srgbtint",					"tint value to be applied when running on new-style srgb parts",	SHADER_PARAM_TYPE_COLOR,	"[1 1 1]",	0 },
};


//-----------------------------------------------------------------------------
// Gets the standard shader parameter names
// FIXME: Turn this into one function?
//-----------------------------------------------------------------------------
int CBaseShader::GetNumParams() const
{
	return NUM_SHADER_MATERIAL_VARS;
}

char const* CBaseShader::GetParamName( int nParamIndex ) const
{
	Assert( nParamIndex < NUM_SHADER_MATERIAL_VARS );
	return s_StandardParams[nParamIndex].m_pName;
}

const char *CBaseShader::GetParamHelp( int nParamIndex ) const
{
	Assert( nParamIndex < NUM_SHADER_MATERIAL_VARS );
	return s_StandardParams[nParamIndex].m_pHelp;
}

ShaderParamType_t CBaseShader::GetParamType( int nParamIndex ) const
{
	Assert( nParamIndex < NUM_SHADER_MATERIAL_VARS );
	return s_StandardParams[nParamIndex].m_Type;
}

const char *CBaseShader::GetParamDefault( int nParamIndex ) const
{
	Assert( nParamIndex < NUM_SHADER_MATERIAL_VARS );
	return s_StandardParams[nParamIndex].m_pDefaultValue;
}

int CBaseShader::GetParamFlags( int nParamIndex ) const
{
	Assert( nParamIndex < NUM_SHADER_MATERIAL_VARS );
	return s_StandardParams[nParamIndex].m_nFlags;
}

//-----------------------------------------------------------------------------
// Necessary to snag ahold of some important data for the helper methods
//-----------------------------------------------------------------------------
void CBaseShader::InitShaderParams( IMaterialVar** ppParams, const char *pMaterialName )
{
	// Re-entrancy check
	Assert( !s_ppParams );

	s_ppParams = ppParams;

	OnInitShaderParams( ppParams, pMaterialName );

	s_ppParams = nullptr;
}

void CBaseShader::InitShaderInstance( IMaterialVar** ppParams, IShaderInit* pShaderInit, const char* pMaterialName, const char* pTextureGroupName )
{
	// Re-entrancy check
	Assert( !s_ppParams );

	s_ppParams = ppParams;
	s_pShaderInit = pShaderInit;
	s_pTextureGroupName = pTextureGroupName;

	OnInitShaderInstance( ppParams, pShaderInit, pMaterialName );

	s_pTextureGroupName = nullptr;
	s_ppParams = nullptr;
	s_pShaderInit = nullptr;
}

void CBaseShader::DrawElements( IMaterialVar** ppParams, int nModulationFlags, IShaderShadow* pShaderShadow, IShaderDynamicAPI* pShaderAPI, VertexCompressionType_t vertexCompression, CBasePerMaterialContextData** pContextDataPtr )
{
	VPROF( "CBaseShader::DrawElements" );
	// Re-entrancy check
	Assert( !s_ppParams );

	s_ppParams = ppParams;
	s_pShaderAPI = pShaderAPI;
	s_pShaderShadow = pShaderShadow;
	s_nModulationFlags = nModulationFlags;
	s_pMeshBuilder = pShaderAPI ? pShaderAPI->GetVertexModifyBuilder() : nullptr;

	if ( IsSnapshotting() )
		// Set up the shadow state
		SetInitialShadowState();

	OnDrawElements( ppParams, pShaderShadow, pShaderAPI, vertexCompression, pContextDataPtr );

	s_nModulationFlags = 0;
	s_ppParams = nullptr;
	s_pShaderAPI = nullptr;
	s_pShaderShadow = nullptr;
	s_pMeshBuilder = nullptr;
}


//-----------------------------------------------------------------------------
// Sets the default shadow state
//-----------------------------------------------------------------------------
void CBaseShader::SetInitialShadowState()
{
	// Set the default state
	s_pShaderShadow->SetDefaultState();

	// Init the standard states...
	const int flags = s_ppParams[FLAGS]->GetIntValue();
	if ( flags & MATERIAL_VAR_IGNOREZ )
	{
		s_pShaderShadow->EnableDepthTest( false );
		s_pShaderShadow->EnableDepthWrites( false );
	}

	if ( flags & MATERIAL_VAR_DECAL )
	{
		s_pShaderShadow->EnablePolyOffset( SHADER_POLYOFFSET_DECAL );
		s_pShaderShadow->EnableDepthWrites( false );
	}

	if ( flags & MATERIAL_VAR_NOCULL )
		s_pShaderShadow->EnableCulling( false );

	if ( flags & MATERIAL_VAR_ZNEARER )
		s_pShaderShadow->DepthFunc( SHADER_DEPTHFUNC_NEARER );

	if ( flags & MATERIAL_VAR_WIREFRAME )
		s_pShaderShadow->PolyMode( SHADER_POLYMODEFACE_FRONT_AND_BACK, SHADER_POLYMODE_LINE );

	// Set alpha to coverage
	if ( flags & MATERIAL_VAR_ALLOWALPHATOCOVERAGE )
		// Force the bit on and then check against alpha blend and test states in CShaderShadowDX8::ComputeAggregateShadowState()
		s_pShaderShadow->EnableAlphaToCoverage( true );
}


//-----------------------------------------------------------------------------
// Draws a snapshot
//-----------------------------------------------------------------------------
void CBaseShader::Draw( bool bMakeActualDrawCall )
{
	if ( IsSnapshotting() )
	{
		// Turn off transparency if we're asked to....
		if ( g_pConfig->bNoTransparency && ( s_ppParams[FLAGS]->GetIntValue() & MATERIAL_VAR_NO_DEBUG_OVERRIDE ) == 0 )
		{
			s_pShaderShadow->EnableDepthWrites( true );
 			s_pShaderShadow->EnableBlending( false );
		}

		GetShaderSystem()->TakeSnapshot();
	}
	else
		GetShaderSystem()->DrawSnapshot( bMakeActualDrawCall );
}


//-----------------------------------------------------------------------------
// Finds a particular parameter	(works because the lowest parameters match the shader)
//-----------------------------------------------------------------------------
int CBaseShader::FindParamIndex( const char* pName ) const
{
	const int numParams = GetNumParams();
	for ( int i = 0; i < numParams; i++ )
	{
		if ( V_strnicmp( GetParamName( i ), pName, 64 ) == 0 )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Are we using graphics?
//-----------------------------------------------------------------------------
bool CBaseShader::IsUsingGraphics()
{
	return GetShaderSystem()->IsUsingGraphics();
}


//-----------------------------------------------------------------------------
// Are we using graphics?
//-----------------------------------------------------------------------------
bool CBaseShader::CanUseEditorMaterials()
{
	return GetShaderSystem()->CanUseEditorMaterials();
}


//-----------------------------------------------------------------------------
// Gets the builder...
//-----------------------------------------------------------------------------
CMeshBuilder* CBaseShader::MeshBuilder()
{
	return s_pMeshBuilder;
}


//-----------------------------------------------------------------------------
// Loads a texture
//-----------------------------------------------------------------------------
void CBaseShader::LoadTexture( int nTextureVar, int nAdditionalCreationFlags )
{
	if ( !s_ppParams || nTextureVar == -1 )
		return;

	IMaterialVar* pNameVar = s_ppParams[nTextureVar];
	if ( pNameVar && pNameVar->IsDefined() )
		s_pShaderInit->LoadTexture( pNameVar, s_pTextureGroupName, nAdditionalCreationFlags );
}


//-----------------------------------------------------------------------------
// Loads a bumpmap
//-----------------------------------------------------------------------------
void CBaseShader::LoadBumpMap( int nTextureVar )
{
	if ( !s_ppParams || nTextureVar == -1 )
		return;

	IMaterialVar* pNameVar = s_ppParams[nTextureVar];
	if ( pNameVar && pNameVar->IsDefined() )
		s_pShaderInit->LoadBumpMap( pNameVar, s_pTextureGroupName );
}


//-----------------------------------------------------------------------------
// Loads a cubemap
//-----------------------------------------------------------------------------
void CBaseShader::LoadCubeMap( int nTextureVar, int nAdditionalCreationFlags )
{
	if ( !s_ppParams || nTextureVar == -1 )
		return;

	IMaterialVar* pNameVar = s_ppParams[nTextureVar];
	if ( pNameVar && pNameVar->IsDefined() )
		s_pShaderInit->LoadCubeMap( s_ppParams, pNameVar, nAdditionalCreationFlags );
}


ShaderAPITextureHandle_t CBaseShader::GetShaderAPITextureBindHandle( int nTextureVar, int nFrameVar, int nTextureChannel )
{
	Assert( !IsSnapshotting() );
	Assert( nTextureVar != -1 );
	Assert ( s_ppParams );

	IMaterialVar* pTextureVar = s_ppParams[nTextureVar];
	IMaterialVar* pFrameVar = nFrameVar != -1 ? s_ppParams[nFrameVar] : nullptr;
	const int nFrame = pFrameVar ? pFrameVar->GetIntValue() : 0;
	return GetShaderAPITextureBindHandle( pTextureVar->GetTextureValue(), nFrame, nTextureChannel );
}

ShaderAPITextureHandle_t CBaseShader::GetShaderAPITextureBindHandle( ITexture* pTexture, int nFrame, int nTextureChannel )
{
	return g_pSLShaderSystem->GetShaderAPITextureBindHandle( pTexture, nFrame, nTextureChannel );
}

//-----------------------------------------------------------------------------
// Four different flavors of BindTexture(), handling the two-sampler
// case as well as ITexture* versus textureVar forms
//-----------------------------------------------------------------------------

void CBaseShader::BindTexture( Sampler_t sampler1, int nTextureVar, int nFrameVar )
{
	BindTexture( sampler1, static_cast<Sampler_t>( -1 ), nTextureVar, nFrameVar );
}


void CBaseShader::BindTexture( Sampler_t sampler1, Sampler_t sampler2, int nTextureVar, int nFrameVar )
{
	Assert( !IsSnapshotting() );
	Assert( nTextureVar != -1 );
	Assert ( s_ppParams );

	IMaterialVar* pTextureVar = s_ppParams[nTextureVar];
	IMaterialVar* pFrameVar = nFrameVar != -1 ? s_ppParams[nFrameVar] : nullptr;
	if ( pTextureVar )
	{
		const int nFrame = pFrameVar ? pFrameVar->GetIntValue() : 0;

		if ( sampler2 == static_cast<Sampler_t>( -1 ) )
			GetShaderSystem()->BindTexture( sampler1, pTextureVar->GetTextureValue(), nFrame );
		else
			GetShaderSystem()->BindTexture( sampler1, sampler2, pTextureVar->GetTextureValue(), nFrame );
	}
}


void CBaseShader::BindTexture( Sampler_t sampler1, ITexture* pTexture, int nFrame )
{
	BindTexture( sampler1, static_cast<Sampler_t>(-1), pTexture, nFrame );
}

void CBaseShader::BindTexture( Sampler_t sampler1, Sampler_t sampler2, ITexture* pTexture, int nFrame )
{
	Assert( !IsSnapshotting() );

	if ( sampler2 == static_cast<Sampler_t>( -1 ) )
		GetShaderSystem()->BindTexture( sampler1, pTexture, nFrame );
	else
		GetShaderSystem()->BindTexture( sampler1, sampler2, pTexture, nFrame );
}

void CBaseShader::GetTextureDimensions( float* pOutWidth, float* pOutHeight, int nTextureVar )
{
	Assert( nTextureVar != -1 );
	Assert( s_ppParams );

	if ( !s_ppParams )
		return;

	IMaterialVar* pTextureVar = s_ppParams[nTextureVar];
	if ( pTextureVar )
	{
		ITexture* texture = pTextureVar->GetTextureValue();
		if ( texture )
		{
			*pOutWidth = static_cast<float>( texture->GetMappingWidth() );
			*pOutHeight = static_cast<float>( texture->GetMappingHeight() );
			return;
		}
	}

	*pOutWidth = *pOutHeight = 1.f;
}

//-----------------------------------------------------------------------------
// Does the texture store translucency in its alpha channel?
//-----------------------------------------------------------------------------
bool CBaseShader::TextureIsTranslucent( int textureVar, bool isBaseTexture )
{
	if (textureVar < 0)
		return false;

	IMaterialVar** params = s_ppParams;
	if ( params[textureVar]->GetType() == MATERIAL_VAR_TYPE_TEXTURE )
	{
		if ( !isBaseTexture )
			return params[textureVar]->GetTextureValue()->IsTranslucent();
		else
		{
			// Override translucency settings if this flag is set.
			if ( IS_FLAG_SET( MATERIAL_VAR_OPAQUETEXTURE ) )
				return false;

			const int flags = CurrentMaterialVarFlags();
			if ( ( flags & ( MATERIAL_VAR_SELFILLUM | MATERIAL_VAR_BASEALPHAENVMAPMASK ) ) == 0 && ( flags & MATERIAL_VAR_TRANSLUCENT || flags & MATERIAL_VAR_ALPHATEST ) )
				return params[textureVar]->GetTextureValue()->IsTranslucent();
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
//
// Helper methods for color modulation
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Are we alpha or color modulating?
//-----------------------------------------------------------------------------
bool CBaseShader::IsAlphaModulating()
{
	return ( s_nModulationFlags & SHADER_USING_ALPHA_MODULATION ) != 0;
}

bool CBaseShader::IsColorModulating()
{
	return ( s_nModulationFlags & SHADER_USING_COLOR_MODULATION ) != 0;
}


void CBaseShader::GetColorParameter( IMaterialVar** params, float* pColorOut ) const
{
	float flColor2[3];
	params[COLOR]->GetVecValue( pColorOut, 3 );
	params[COLOR2]->GetVecValue( flColor2, 3 );

	pColorOut[0] *= flColor2[0];
	pColorOut[1] *= flColor2[1];
	pColorOut[2] *= flColor2[2];

	if ( g_pHardwareConfig->UsesSRGBCorrectBlending() )
	{
		float flSRGBTint[3];
		params[SRGBTINT]->GetVecValue( flSRGBTint, 3 );

		pColorOut[0] *= flSRGBTint[0];
		pColorOut[1] *= flSRGBTint[1];
		pColorOut[2] *= flSRGBTint[2];
	}
}

//-----------------------------------------------------------------------------
// FIXME: Figure out a better way to do this?
//-----------------------------------------------------------------------------
int CBaseShader::ComputeModulationFlags( IMaterialVar** params, IShaderDynamicAPI* pShaderAPI )
{
 	s_pShaderAPI = pShaderAPI;

	int mod = 0;
	if ( GetAlpha( params ) < 1.0f )
		mod |= SHADER_USING_ALPHA_MODULATION;

	float color[3];
	GetColorParameter( params, color );

	if ( color[0] != 1.0 || color[1] != 1.0 || color[2] != 1.0 )
		mod |= SHADER_USING_COLOR_MODULATION;

	if ( UsingFlashlight( params ) )
		mod |= SHADER_USING_FLASHLIGHT;

	if ( UsingEditor( params ) )
		mod |= SHADER_USING_EDITOR;

	if ( IS_FLAG2_SET( MATERIAL_VAR2_USE_FIXED_FUNCTION_BAKED_LIGHTING ) )
	{
		AssertOnce( IS_FLAG2_SET( MATERIAL_VAR2_NEEDS_BAKED_LIGHTING_SNAPSHOTS ) );
		if ( IS_FLAG2_SET( MATERIAL_VAR2_NEEDS_BAKED_LIGHTING_SNAPSHOTS ) )
			mod |= SHADER_USING_FIXED_FUNCTION_BAKED_LIGHTING;
	}

	s_pShaderAPI = nullptr;

	return mod;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CBaseShader::NeedsPowerOfTwoFrameBufferTexture( IMaterialVar** params, bool bCheckSpecificToThisFrame ) const
{
	return CShader_IsFlag2Set( params, MATERIAL_VAR2_NEEDS_POWER_OF_TWO_FRAME_BUFFER_TEXTURE );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CBaseShader::NeedsFullFrameBufferTexture( IMaterialVar** params, bool bCheckSpecificToThisFrame ) const
{
	return CShader_IsFlag2Set( params, MATERIAL_VAR2_NEEDS_FULL_FRAME_BUFFER_TEXTURE );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CBaseShader::IsTranslucent( IMaterialVar** params ) const
{
	return IS_FLAG_SET( MATERIAL_VAR_TRANSLUCENT );
}

//-----------------------------------------------------------------------------
// Returns the translucency...
//-----------------------------------------------------------------------------
float CBaseShader::GetAlpha( IMaterialVar** ppParams )
{
	if ( !ppParams )
		ppParams = s_ppParams;

	if ( !ppParams )
		return 1.0f;

	if ( ppParams[FLAGS]->GetIntValue() & MATERIAL_VAR_NOALPHAMOD )
		return 1.0f;

	const float flAlpha = ppParams[ALPHA]->GetFloatValue();
	return clamp( flAlpha, 0.0f, 1.0f );
}


//-----------------------------------------------------------------------------
// Sets the color + transparency
//-----------------------------------------------------------------------------
void CBaseShader::SetColorState( int colorVar, bool setAlpha )
{
	Assert( !IsSnapshotting() );
	if ( !s_ppParams )
		return;

	// Use tint instead of color if it was specified...
	IMaterialVar* pColorVar = colorVar != -1 ? s_ppParams[colorVar] : nullptr;

	float color[4] = { 1.0, 1.0, 1.0, 1.0 };
	if ( pColorVar )
	{
		if ( pColorVar->GetType() == MATERIAL_VAR_TYPE_VECTOR )
			pColorVar->GetVecValue( color, 3 );
		else
			color[0] = color[1] = color[2] = pColorVar->GetFloatValue();

		if ( !g_pHardwareConfig->SupportsPixelShaders_1_4() )		// Clamp 0..1 for ps_1_1 and below
		{
			color[0] = clamp( color[0], 0.0f, 1.0f );
			color[1] = clamp( color[1], 0.0f, 1.0f );
			color[2] = clamp( color[2], 0.0f, 1.0f );
		}
		else if ( !g_pHardwareConfig->SupportsPixelShaders_2_0() ) 	// Clamp 0..8 for ps_1_4
		{
			color[0] = clamp( color[0], 0.0f, 8.0f );
			color[1] = clamp( color[1], 0.0f, 8.0f );
			color[2] = clamp( color[2], 0.0f, 8.0f );
		}
	}
	ApplyColor2Factor( color );
	color[3] = setAlpha ? GetAlpha() : 1.0f;
	s_pShaderAPI->Color4fv( color );
}


void CBaseShader::SetModulationShadowState( int tintVar )
{
	// Have have no control over the tint var...
	// We activate color modulating when we're alpha or color modulating
	const bool doModulation = tintVar != -1 || IsAlphaModulating() || IsColorModulating();

	s_pShaderShadow->EnableConstantColor( doModulation );
}

void CBaseShader::SetModulationDynamicState( int tintVar )
{
	if ( tintVar != -1 )
		SetColorState( tintVar, true );
	else
		SetColorState( COLOR, true );
}

void CBaseShader::ApplyColor2Factor( float* pColorOut ) const // (*pColorOut) *= COLOR2
{
	IMaterialVar* pColor2Var = s_ppParams[COLOR2];
	if ( pColor2Var->GetType() == MATERIAL_VAR_TYPE_VECTOR )
	{
		float flColor2[3];
		pColor2Var->GetVecValue( flColor2, 3 );

		pColorOut[0] *= flColor2[0];
		pColorOut[1] *= flColor2[1];
		pColorOut[2] *= flColor2[2];
	}

	if ( g_pHardwareConfig->UsesSRGBCorrectBlending() )
	{
		IMaterialVar* pSRGBVar = s_ppParams[SRGBTINT];
		if ( pSRGBVar->GetType() == MATERIAL_VAR_TYPE_VECTOR )
		{
			float flSRGB[3];
			pSRGBVar->GetVecValue( flSRGB, 3 );

			pColorOut[0] *= flSRGB[0];
			pColorOut[1] *= flSRGB[1];
			pColorOut[2] *= flSRGB[2];
		}
	}
}

void CBaseShader::ComputeModulationColor( float* color )
{
	Assert( !IsSnapshotting() );
	if ( !s_ppParams )
		return;

	IMaterialVar* pColorVar = s_ppParams[COLOR];
	if ( pColorVar->GetType() == MATERIAL_VAR_TYPE_VECTOR )
		pColorVar->GetVecValue( color, 3 );
	else
		color[0] = color[1] = color[2] = pColorVar->GetFloatValue();

	ApplyColor2Factor( color );

	if ( !g_pConfig->bShowDiffuse )
		color[0] = color[1] = color[2] = 0.0f;
	if ( mat_fullbright.GetInt() == 2 )
		color[0] = color[1] = color[2] = 1.0f;
	color[3] = GetAlpha();
}


//-----------------------------------------------------------------------------
//
// Helper methods for alpha blending....
//
//-----------------------------------------------------------------------------
void CBaseShader::EnableAlphaBlending( ShaderBlendFactor_t src, ShaderBlendFactor_t dst )
{
	Assert( IsSnapshotting() );
	s_pShaderShadow->EnableBlending( true );
	s_pShaderShadow->BlendFunc( src, dst );
	s_pShaderShadow->EnableDepthWrites( false );
}

void CBaseShader::DisableAlphaBlending()
{
	Assert( IsSnapshotting() );
	s_pShaderShadow->EnableBlending( false );
}

void CBaseShader::SetNormalBlendingShadowState( int textureVar, bool isBaseTexture )
{
	Assert( IsSnapshotting() );

	// Either we've got a constant modulation
	bool isTranslucent = IsAlphaModulating();

	// Or we've got a vertex alpha
	isTranslucent = isTranslucent || ( CurrentMaterialVarFlags() & MATERIAL_VAR_VERTEXALPHA );

	// Or we've got a texture alpha
	isTranslucent = isTranslucent || ( TextureIsTranslucent( textureVar, isBaseTexture ) && !( CurrentMaterialVarFlags() & MATERIAL_VAR_ALPHATEST ) );

	if ( isTranslucent )
		EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
	else
		DisableAlphaBlending();
}

//ConVar mat_debug_flashlight_only( "mat_debug_flashlight_only", "0" );
void CBaseShader::SetAdditiveBlendingShadowState( int textureVar, bool isBaseTexture )
{
	Assert( IsSnapshotting() );

	// Either we've got a constant modulation
	bool isTranslucent = IsAlphaModulating();

	// Or we've got a vertex alpha
	isTranslucent = isTranslucent || ( CurrentMaterialVarFlags() & MATERIAL_VAR_VERTEXALPHA );

	// Or we've got a texture alpha
	isTranslucent = isTranslucent || ( TextureIsTranslucent( textureVar, isBaseTexture ) && !( CurrentMaterialVarFlags() & MATERIAL_VAR_ALPHATEST ) );

	if ( isTranslucent )
		EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE );
	else
		EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
}

void CBaseShader::SetDefaultBlendingShadowState( int textureVar, bool isBaseTexture )
{
	if ( CurrentMaterialVarFlags() & MATERIAL_VAR_ADDITIVE )
		SetAdditiveBlendingShadowState( textureVar, isBaseTexture );
	else
		SetNormalBlendingShadowState( textureVar, isBaseTexture );
}

void CBaseShader::SetBlendingShadowState( BlendType_t nMode )
{
	switch ( nMode )
	{
		case BT_NONE:
			DisableAlphaBlending();
			break;

		case BT_BLEND:
			EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			break;

		case BT_ADD:
			EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
			break;

		case BT_BLENDADD:
			EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE );
			break;
	}
}

//-----------------------------------------------------------------------------
//
// Helper methods for fog
//
//-----------------------------------------------------------------------------
void CBaseShader::FogToOOOverbright()
{
	Assert( IsSnapshotting() );
	if ( ( CurrentMaterialVarFlags() & MATERIAL_VAR_NOFOG ) == 0 )
		s_pShaderShadow->FogMode( SHADER_FOGMODE_OO_OVERBRIGHT );
	else
		s_pShaderShadow->FogMode( SHADER_FOGMODE_DISABLED );
}

void CBaseShader::FogToWhite()
{
	Assert( IsSnapshotting() );
	if ( ( CurrentMaterialVarFlags() & MATERIAL_VAR_NOFOG ) == 0 )
		s_pShaderShadow->FogMode( SHADER_FOGMODE_WHITE );
	else
		s_pShaderShadow->FogMode( SHADER_FOGMODE_DISABLED );
}

void CBaseShader::FogToBlack()
{
	Assert( IsSnapshotting() );
	if ( ( CurrentMaterialVarFlags() & MATERIAL_VAR_NOFOG ) == 0 )
		s_pShaderShadow->FogMode( SHADER_FOGMODE_BLACK );
	else
		s_pShaderShadow->FogMode( SHADER_FOGMODE_DISABLED );
}

void CBaseShader::FogToGrey()
{
	Assert( IsSnapshotting() );
	if ( ( CurrentMaterialVarFlags() & MATERIAL_VAR_NOFOG ) == 0 )
		s_pShaderShadow->FogMode( SHADER_FOGMODE_GREY );
	else
		s_pShaderShadow->FogMode( SHADER_FOGMODE_DISABLED );
}

void CBaseShader::FogToFogColor()
{
	Assert( IsSnapshotting() );
	if ( ( CurrentMaterialVarFlags() & MATERIAL_VAR_NOFOG ) == 0 )
		s_pShaderShadow->FogMode( SHADER_FOGMODE_FOGCOLOR );
	else
		s_pShaderShadow->FogMode( SHADER_FOGMODE_DISABLED );
}

void CBaseShader::DisableFog()
{
	Assert( IsSnapshotting() );
	s_pShaderShadow->FogMode( SHADER_FOGMODE_DISABLED );
}

void CBaseShader::DefaultFog()
{
	if ( CurrentMaterialVarFlags() & MATERIAL_VAR_ADDITIVE )
		FogToBlack();
	else
		FogToFogColor();
}

bool CBaseShader::UsingFlashlight( IMaterialVar** params ) const
{
	if ( IsSnapshotting() )
		return CShader_IsFlag2Set( params, MATERIAL_VAR2_USE_FLASHLIGHT );
	return s_pShaderAPI->InFlashlightMode();
}

bool CBaseShader::UsingEditor( IMaterialVar** params ) const
{
	if ( IsSnapshotting() )
		return CShader_IsFlag2Set( params, MATERIAL_VAR2_USE_EDITOR );

	return s_pShaderAPI->InEditorMode();
}

bool CBaseShader::IsHDREnabled()
{
	// HDRFIXME!  Need to fix this for vgui materials
	const HDRType_t hdr_mode = g_pHardwareConfig->GetHDRType();
	switch ( hdr_mode )
	{
		case HDR_TYPE_NONE:
			return false;

		case HDR_TYPE_INTEGER:
			return true;

		case HDR_TYPE_FLOAT:
		{
			ITexture* pRT = s_pShaderAPI->GetRenderTargetEx( 0 );
			if ( pRT && pRT->GetImageFormat() == IMAGE_FORMAT_RGBA16161616F )
				return true;
		}
	}
	return false;
}