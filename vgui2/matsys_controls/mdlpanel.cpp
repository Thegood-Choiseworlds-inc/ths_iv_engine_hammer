//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#include "matsys_controls/mdlpanel.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imesh.h"
#include "vgui/IVGui.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/Frame.h"
#include "tier1/convar.h"
#include "tier0/dbg.h"
#include "tier1/fmtstr.h"
#include "istudiorender.h"
#include "matsys_controls/matsyscontrols.h"
#include "vcollide.h"
#include "vcollide_parse.h"
#include "bone_setup.h"
#include "vphysics_interface.h"
#include "renderparm.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

DECLARE_BUILD_FACTORY( CMDLPanel );

//-----------------------------------------------------------------------------
// Purpose: Keeps a global clock to autoplay sequences to run from
//			Also deals with speedScale changes
//-----------------------------------------------------------------------------
static float GetAutoPlayTime( void )
{
	static int g_prevTicks;
	static float g_time;

	int ticks = Plat_MSTime();

	// limit delta so that float time doesn't overflow
	if (g_prevTicks == 0)
	{
		g_prevTicks = ticks;
	}

	g_time += ( ticks - g_prevTicks ) / 1000.0f;
	g_prevTicks = ticks;

	return g_time;
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CMDLPanel::CMDLPanel( vgui::Panel *pParent, const char *pName ) : BaseClass( pParent, pName )
{
	SetVisible( true );
	SetRender3DSupersampled( true ); // draw to texture, avoids stencil bug

	// Used to poll input
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	SetIdentityMatrix( m_RootMDL.m_MDLToWorld );
	m_bDrawCollisionModel = false;
	m_bWireFrame = false;
	m_bGroundGrid = false;
	m_bLockView = false;
	m_bLookAtCamera = true;

	m_flAutoPlayTimeBase = GetAutoPlayTime();

	m_bCameraOrientOverrideEnabled = false;
	m_bCameraPositionOverrideEnabled = false;
	m_vecCameraOrientOverride.Init();
	m_vecCameraPositionOverride.Init();
}

CMDLPanel::~CMDLPanel()
{
	m_aMergeMDLs.Purge();
}


//-----------------------------------------------------------------------------
// Scheme settings
//-----------------------------------------------------------------------------
void CMDLPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetBackgroundColor( GetBgColor() );
	SetBorder( pScheme->GetBorder( "MenuBorder") );
}


//-----------------------------------------------------------------------------
// Rendering options
//-----------------------------------------------------------------------------
void CMDLPanel::SetCollsionModel( bool bVisible )
{
	m_bDrawCollisionModel = bVisible;
}

void CMDLPanel::SetGroundGrid( bool bVisible )
{
	m_bGroundGrid = bVisible;
}

void CMDLPanel::SetWireFrame( bool bVisible )
{
	m_bWireFrame = bVisible;
}

void CMDLPanel::SetLockView( bool bLocked )
{
	m_bLockView = bLocked;
}

void CMDLPanel::SetLookAtCamera( bool bLookAtCamera )
{
	m_bLookAtCamera = bLookAtCamera;
}

//-----------------------------------------------------------------------------
// Sets the camera to look at the model
//-----------------------------------------------------------------------------
void CMDLPanel::LookAtMDL()
{
	// Check to see if we have a valid model to look at.
	if ( m_RootMDL.m_MDL.GetMDL() == MDLHANDLE_INVALID )
		return;

	if ( m_bLockView )
		return;

	float flRadius;
	Vector vecCenter;
	GetBoundingSphere( vecCenter, flRadius );
	LookAt( vecCenter, flRadius );
}


//-----------------------------------------------------------------------------
// FIXME: This should be moved into studiorender
//-----------------------------------------------------------------------------
static ConVar	r_showenvcubemap( "r_showenvcubemap", "0", FCVAR_CHEAT );
static ConVar	r_eyegloss		( "r_eyegloss", "1", FCVAR_ARCHIVE ); // wet eyes
static ConVar	r_eyemove		( "r_eyemove", "1", FCVAR_ARCHIVE ); // look around
static ConVar	r_eyeshift_x	( "r_eyeshift_x", "0", FCVAR_ARCHIVE ); // eye X position
static ConVar	r_eyeshift_y	( "r_eyeshift_y", "0", FCVAR_ARCHIVE ); // eye Y position
static ConVar	r_eyeshift_z	( "r_eyeshift_z", "0", FCVAR_ARCHIVE ); // eye Z position
static ConVar	r_eyesize		( "r_eyesize", "0", FCVAR_ARCHIVE ); // adjustment to iris textures
static ConVar	mat_softwareskin( "mat_softwareskin", "0", FCVAR_CHEAT );
static ConVar	r_nohw			( "r_nohw", "0", FCVAR_CHEAT );
static ConVar	r_nosw			( "r_nosw", "0", FCVAR_CHEAT );
static ConVar	r_teeth			( "r_teeth", "1" );
static ConVar	r_drawentities	( "r_drawentities", "1", FCVAR_CHEAT );
static ConVar	r_flex			( "r_flex", "1" );
static ConVar	r_eyes			( "r_eyes", "1" );
static ConVar	r_skin			( "r_skin","0", FCVAR_CHEAT );
static ConVar	r_maxmodeldecal ( "r_maxmodeldecal", "50" );
static ConVar	r_modelwireframedecal ( "r_modelwireframedecal", "0", FCVAR_CHEAT );
static ConVar	mat_normals		( "mat_normals", "0", FCVAR_CHEAT );
static ConVar	r_eyeglintlodpixels ( "r_eyeglintlodpixels", "0", FCVAR_CHEAT );
static ConVar	r_rootlod		( "r_rootlod", "0" );

static StudioRenderConfig_t s_StudioRenderConfig;

void CMDLPanel::UpdateStudioRenderConfig( void )
{
	memset( &s_StudioRenderConfig, 0, sizeof(s_StudioRenderConfig) );

	s_StudioRenderConfig.bEyeMove = !!r_eyemove.GetInt();
	s_StudioRenderConfig.fEyeShiftX = r_eyeshift_x.GetFloat();
	s_StudioRenderConfig.fEyeShiftY = r_eyeshift_y.GetFloat();
	s_StudioRenderConfig.fEyeShiftZ = r_eyeshift_z.GetFloat();
	s_StudioRenderConfig.fEyeSize = r_eyesize.GetFloat();
	if( mat_softwareskin.GetInt() || m_bWireFrame )
	{
		s_StudioRenderConfig.bSoftwareSkin = true;
	}
	else
	{
		s_StudioRenderConfig.bSoftwareSkin = false;
	}
	s_StudioRenderConfig.bNoHardware = !!r_nohw.GetInt();
	s_StudioRenderConfig.bNoSoftware = !!r_nosw.GetInt();
	s_StudioRenderConfig.bTeeth = !!r_teeth.GetInt();
	s_StudioRenderConfig.drawEntities = r_drawentities.GetInt();
	s_StudioRenderConfig.bFlex = !!r_flex.GetInt();
	s_StudioRenderConfig.bEyes = !!r_eyes.GetInt();
	s_StudioRenderConfig.bDrawNormals = mat_normals.GetBool();
	s_StudioRenderConfig.skin = r_skin.GetInt();
	s_StudioRenderConfig.maxDecalsPerModel = r_maxmodeldecal.GetInt();
	s_StudioRenderConfig.bWireframeDecals = r_modelwireframedecal.GetInt() != 0;

	s_StudioRenderConfig.fullbright = false;
	s_StudioRenderConfig.bSoftwareLighting = false;

	s_StudioRenderConfig.bShowEnvCubemapOnly = r_showenvcubemap.GetBool();
	s_StudioRenderConfig.fEyeGlintPixelWidthLODThreshold = r_eyeglintlodpixels.GetFloat();

	StudioRender()->UpdateConfig( s_StudioRenderConfig );
}

void CMDLPanel::DrawCollisionModel()
{
	vcollide_t *pCollide = MDLCache()->GetVCollide( m_RootMDL.m_MDL.GetMDL() );

	if ( !pCollide || pCollide->solidCount <= 0 )
		return;

	static color32 color = {255,0,0,0};

	IVPhysicsKeyParser *pParser = g_pPhysicsCollision->VPhysicsKeyParserCreate( pCollide->pKeyValues );
	CStudioHdr studioHdr( g_pMDLCache->GetStudioHdr( m_RootMDL.m_MDL.GetMDL() ), g_pMDLCache );

	matrix3x4_t pBoneToWorld[MAXSTUDIOBONES];
	m_RootMDL.m_MDL.SetUpBones( m_RootMDL.m_MDLToWorld, MAXSTUDIOBONES, pBoneToWorld );

	// PERFORMANCE: Just parse the script each frame.  It's fast enough for tools.  If you need
	// this to go faster then cache off the bone index mapping in an array like HLMV does
	while ( !pParser->Finished() )
	{
		const char *pBlock = pParser->GetCurrentBlockName();
		if ( !stricmp( pBlock, "solid" ) )
		{
			solid_t solid;

			pParser->ParseSolid( &solid, NULL );
			int boneIndex = Studio_BoneIndexByName( &studioHdr, solid.name );
			Vector *outVerts;
			int vertCount = g_pPhysicsCollision->CreateDebugMesh( pCollide->solids[solid.index], &outVerts );

			if ( vertCount )
			{
				CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
				pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );
				// NOTE: assumes these have been set up already by the model render code
				// So this is a little bit of a back door to a cache of the bones
				// this code wouldn't work unless you draw the model this frame before calling
				// this routine.  CMDLPanel always does this, but it's worth noting.
				// A better solution would be to move the ragdoll visulization into the CDmeMdl
				// and either draw it there or make it queryable and query/draw here.
				matrix3x4_t xform;
				SetIdentityMatrix( xform );
				if ( boneIndex >= 0 )
				{
					MatrixCopy( pBoneToWorld[ boneIndex ], xform );
				}
				IMesh *pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, GetWireframeMaterial() );

				CMeshBuilder meshBuilder;
				meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, vertCount/3 );

				for ( int j = 0; j < vertCount; j++ )
				{
					Vector out;
					VectorTransform( outVerts[j].Base(), xform, out.Base() );
					meshBuilder.Position3fv( out.Base() );
					meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
					meshBuilder.TexCoord2f( 0, 0, 0 );
					meshBuilder.AdvanceVertex();
				}
				meshBuilder.End();
				pMesh->Draw();
			}

			g_pPhysicsCollision->DestroyDebugMesh( vertCount, outVerts );
		}
		else
		{
			pParser->SkipBlock();
		}
	}
	g_pPhysicsCollision->VPhysicsKeyParserDestroy( pParser );
}

//-----------------------------------------------------------------------------
// paint it!
//-----------------------------------------------------------------------------
void CMDLPanel::OnPaint3D()
{
	if ( m_RootMDL.m_MDL.GetMDL() == MDLHANDLE_INVALID )
		return;

	// FIXME: Move this call into DrawModel in StudioRender
	StudioRenderConfig_t oldStudioRenderConfig;
	StudioRender()->GetCurrentConfig( oldStudioRenderConfig );

	UpdateStudioRenderConfig();

	CMatRenderContextPtr pRenderContext( vgui::MaterialSystem() );

	// We want the models to use their natural alpha, not depth in alpha
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_WRITE_DEPTH_TO_DESTALPHA, 0 );

	if ( IsPaint3dForRenderCapture() )
	{
		// We are rendering into flashlight depth texture
		pRenderContext->SetFlashlightMode( false ); // disable shadows since we should be using DEPTH_WRITE material
	}
	else if ( GetRenderingWithFlashlightConfiguration() )
	{
		// Setup shadow state that we configured
		g_pStudioRender->ClearAllShadows();
		// NOTE: flashlight shadow is added post bone setup
	}
	else
	{
		// flashlights can't work in the model panel under queued mode (the state isn't ready yet, so causes a crash)
		pRenderContext->SetFlashlightMode( false );
	}

	ITexture *pMyCube = materials->FindTexture( "engine/defaultcubemap", TEXTURE_GROUP_CUBE_MAP, true );

	if ( HasLightProbe() )
	{
		pMyCube = GetLightProbeCubemap( vgui::MaterialSystemHardwareConfig()->GetHDRType() != HDR_TYPE_NONE );
	}
	pRenderContext->BindLocalCubemap( pMyCube );

	if ( m_bGroundGrid )
	{
		DrawGrid();
	}

	if ( m_bLookAtCamera )
	{
		matrix3x4_t worldToCamera;
		ComputeCameraTransform( &worldToCamera );

		Vector vecPosition;
		MatrixGetColumn( worldToCamera, 3, vecPosition );
		m_RootMDL.m_MDL.m_bWorldSpaceViewTarget = true;
		m_RootMDL.m_MDL.m_vecViewTarget = vecPosition;
	}

	if ( !m_modelWireframe.IsValid() )
	{
		m_modelWireframe.Init( "debug/debugwireframevertexcolor", TEXTURE_GROUP_OTHER );
	}

	Draw();

	if ( m_bDrawCollisionModel )
	{
		DrawCollisionModel();
	}

	if ( IsPaint3dForRenderCapture() )
	{
		// We are finished rendering into flashlight buffer
	}
	else if ( GetRenderingWithFlashlightConfiguration() )
	{
		// Clear all shadow state that we configured and used now
		g_pStudioRender->ClearAllShadows();
	}

	pRenderContext->Flush();
	StudioRender()->UpdateConfig( oldStudioRenderConfig );
}

void CMDLPanel::OnModelDrawPassStart( int iPass, CStudioHdr *pStudioHdr, int &nFlags )
{
	if ( IsPaint3dForRenderCapture() )
		nFlags |= STUDIORENDER_SHADOWDEPTHTEXTURE;
	else if ( GetRenderingWithFlashlightConfiguration() )
		nFlags &=~STUDIORENDER_DRAW_NO_SHADOWS;

	if ( m_bWireFrame && iPass == 0 )
		StudioRender()->ForcedMaterialOverride( m_modelWireframe );
}

void CMDLPanel::OnModelDrawPassFinished( int iPass, CStudioHdr *pStudioHdr, int &nFlags )
{
	if ( iPass == 0 && m_bWireFrame )
		StudioRender()->ForcedMaterialOverride( nullptr );
}

void CMDLPanel::OnPostSetUpBonesPreDraw()
{
	bool bFlashlightPosKnown = false;
	Vector vecPositionFlashlight;
	QAngle anglesFlashlight;

	if ( bFlashlightPosKnown && GetRenderingWithFlashlightConfiguration() != NULL )
	{
		CRenderCaptureConfigurationState *pFlashlightInfo = reinterpret_cast< CRenderCaptureConfigurationState * >( GetRenderingWithFlashlightConfiguration() );

		//
		// Update flashlight position
		//
		{
			pFlashlightInfo->m_renderFlashlightState.m_vecLightOrigin = vecPositionFlashlight;
			AngleQuaternion( anglesFlashlight, pFlashlightInfo->m_renderFlashlightState.m_quatOrientation );
		}

		//
		// Build world to shadow matrix, then perspective projection and concatenate
		//
		{
			VMatrix matWorldToShadowView, matPerspective;
			matrix3x4_t matOrientation;
			QuaternionMatrix( pFlashlightInfo->m_renderFlashlightState.m_quatOrientation, matOrientation );		// Convert quat to matrix3x4
			PositionMatrix( vec3_origin, matOrientation );				// Zero out translation elements

			VMatrix matBasis( matOrientation );							// Convert matrix3x4 to VMatrix

			Vector vForward, vLeft, vUp;
			matBasis.GetBasisVectors( vForward, vLeft, vUp );
			matBasis.SetForward( vLeft );								// Bizarre vector flip inherited from earlier code, WTF?
			matBasis.SetLeft( vUp );
			matBasis.SetUp( vForward );
			matWorldToShadowView = matBasis.Transpose();					// Transpose

			Vector translation;
			Vector3DMultiply( matWorldToShadowView, pFlashlightInfo->m_renderFlashlightState.m_vecLightOrigin, translation );

			translation *= -1.0f;
			matWorldToShadowView.SetTranslation( translation );

			// The the bottom row.
			matWorldToShadowView[3][0] = matWorldToShadowView[3][1] = matWorldToShadowView[3][2] = 0.0f;
			matWorldToShadowView[3][3] = 1.0f;

			MatrixBuildPerspective( matPerspective, pFlashlightInfo->m_renderFlashlightState.m_fHorizontalFOVDegrees,
				pFlashlightInfo->m_renderFlashlightState.m_fVerticalFOVDegrees,
				pFlashlightInfo->m_renderFlashlightState.m_NearZ, pFlashlightInfo->m_renderFlashlightState.m_FarZ );

			MatrixMultiply( matPerspective, matWorldToShadowView, pFlashlightInfo->m_renderMatrixWorldToShadow );
		}
	}

	if ( IsPaint3dForRenderCapture() )
	{
		Assert( bFlashlightPosKnown );
		CRenderCaptureConfigurationState *pFlashlightInfo = reinterpret_cast< CRenderCaptureConfigurationState * >( GetRenderingWithFlashlightConfiguration() );
		SetCameraPositionAndAngles( vecPositionFlashlight, anglesFlashlight );

		Camera_t &cameraSettings = GetCameraSettings();
		float flZnear = cameraSettings.m_flZNear, flZfar = cameraSettings.m_flZFar;
		cameraSettings.m_flZNear = pFlashlightInfo->m_renderFlashlightState.m_NearZ;
		cameraSettings.m_flZFar = pFlashlightInfo->m_renderFlashlightState.m_FarZ;
		SetupRenderStateDelayed();	// Configure view with the updated camera settings and restore Z planes
		cameraSettings.m_flZNear = flZnear, cameraSettings.m_flZFar = flZfar;
	}
	else
	{
		SetupRenderStateDelayed();
		if ( GetRenderingWithFlashlightConfiguration() )
		{
			Assert( bFlashlightPosKnown );
			// Add the shadow we rendered in previous pass to our model
			CRenderCaptureConfigurationState *pFlashlightInfo = reinterpret_cast< CRenderCaptureConfigurationState * >( GetRenderingWithFlashlightConfiguration() );
			g_pStudioRender->AddShadow( NULL, NULL, &pFlashlightInfo->m_renderFlashlightState, &pFlashlightInfo->m_renderMatrixWorldToShadow, pFlashlightInfo->m_pFlashlightDepthTexture );
		}
	}
}

//-----------------------------------------------------------------------------
// input
//-----------------------------------------------------------------------------
void CMDLPanel::OnMouseDoublePressed( vgui::MouseCode code )
{
	float flRadius;
	Vector vecCenter;
	GetBoundingSphere( vecCenter, flRadius );
	LookAt( vecCenter, flRadius );

	BaseClass::OnMouseDoublePressed( code );
}


