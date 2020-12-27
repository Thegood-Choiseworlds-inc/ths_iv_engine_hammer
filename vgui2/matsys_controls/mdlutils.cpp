//===== Copyright � 2005-2013, Valve Corporation, All rights reserved. ======//
//
// Purpose: Utility methods for mdl files
//
//===========================================================================//

#include "matsys_controls/mdlutils.h"
#include "tier0/dbg.h"
#include "tier3/tier3.h"
#include "studio.h"
#include "istudiorender.h"
#include "bone_setup.h"
#include "bone_accessor.h"
#include "materialsystem/imaterialvar.h"
#include "vcollide_parse.h"
#include "renderparm.h"
#include "tier2/renderutils.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

void CMDL::Draw( const matrix3x4_t& rootToWorld, const matrix3x4_t *pBoneToWorld, int flags )
{
	if ( !g_pMaterialSystem || !g_pMDLCache || !g_pStudioRender )
		return;

	if ( m_MDLHandle == MDLHANDLE_INVALID )
		return;

	// Color + alpha modulation
	Vector white( m_Color.r() / 255.0f, m_Color.g() / 255.0f, m_Color.b() / 255.0f );
	g_pStudioRender->SetColorModulation( white.Base() );
	g_pStudioRender->SetAlphaModulation( m_Color.a() / 255.0f );

	DrawModelInfo_t info;
	info.m_pStudioHdr = g_pMDLCache->GetStudioHdr( m_MDLHandle );
	info.m_pHardwareData = g_pMDLCache->GetHardwareData( m_MDLHandle );
	info.m_Decals = STUDIORENDER_DECAL_INVALID;
	info.m_Skin = m_nSkin;
	info.m_Body = m_nBody;
	info.m_HitboxSet = 0;
	info.m_pClientEntity = m_pProxyData;
	info.m_pColorMeshes = NULL;
	info.m_bStaticLighting = false;
	info.m_Lod = m_nLOD;

	Vector vecWorldViewTarget;
	if ( m_bWorldSpaceViewTarget )
	{
		vecWorldViewTarget = m_vecViewTarget;
	}
	else
	{
		VectorTransform( m_vecViewTarget, rootToWorld, vecWorldViewTarget );
	}
	g_pStudioRender->SetEyeViewTarget( info.m_pStudioHdr, info.m_Body, vecWorldViewTarget );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	// Set default flex values
	float *pFlexWeights = NULL;
	const int nFlexDescCount = info.m_pStudioHdr->numflexdesc;
	CUtlMemory<float> rdFlexWeights;
	if ( nFlexDescCount )
	{
		CStudioHdr cStudioHdr( info.m_pStudioHdr, g_pMDLCache );
		rdFlexWeights.EnsureCapacity( nFlexDescCount );
		pFlexWeights = rdFlexWeights.Base();
		cStudioHdr.RunFlexRules( m_pFlexControls, pFlexWeights );
	}

	Vector vecModelOrigin;
	MatrixGetColumn( rootToWorld, 3, vecModelOrigin );

	g_pStudioRender->DrawModel( NULL, info, const_cast<matrix3x4_t*>( pBoneToWorld ),
		pFlexWeights, NULL, vecModelOrigin, STUDIORENDER_DRAW_ENTIRE_MODEL | flags );
}

MDLData_t::MDLData_t()
{
	SetIdentityMatrix( m_MDLToWorld );
	m_bRequestBoneMergeTakeover = false;
}

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CMergedMDL::CMergedMDL()
{
	m_nNumSequenceLayers = 0;
}

CMergedMDL::~CMergedMDL()
{
	m_aMergeMDLs.Purge();
}

//-----------------------------------------------------------------------------
// Stores the clip
//-----------------------------------------------------------------------------
void CMergedMDL::SetMDL( MDLHandle_t handle, void *pProxyData )
{
	m_RootMDL.m_MDL.SetMDL( handle );
	m_RootMDL.m_MDL.m_pProxyData = pProxyData;
	m_RootMDL.m_MDL.m_Color.SetRawColor( 0xFFFFFFFF );

	Vector vecMins, vecMaxs;
	GetMDLBoundingBox( &vecMins, &vecMaxs, handle, m_RootMDL.m_MDL.m_nSequence );

	m_RootMDL.m_MDL.m_bWorldSpaceViewTarget = false;
	m_RootMDL.m_MDL.m_vecViewTarget.Init( 100.0f, 0.0f, vecMaxs.z );

	// Set the pose parameters to the default for the mdl
	SetPoseParameters( NULL, 0 );

	// Clear any sequence layers
	SetSequenceLayers( NULL, 0 );
}

//-----------------------------------------------------------------------------
// An MDL was selected
//-----------------------------------------------------------------------------
void CMergedMDL::SetMDL( const char *pMDLName, void *pProxyData )
{
	MDLHandle_t hMDL = pMDLName ? g_pMDLCache->FindMDL( pMDLName ) : MDLHANDLE_INVALID;
	if ( g_pMDLCache->IsErrorModel( hMDL ) )
	{
		hMDL = MDLHANDLE_INVALID;
	}

	SetMDL( hMDL, pProxyData );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a model bounding box.
//-----------------------------------------------------------------------------
bool CMergedMDL::GetBoundingBox( Vector &vecBoundsMin, Vector &vecBoundsMax )
{
	// Check to see if we have a valid model to look at.
	if ( m_RootMDL.m_MDL.GetMDL() == MDLHANDLE_INVALID )
		return false;

	GetMDLBoundingBox( &vecBoundsMin, &vecBoundsMax, m_RootMDL.m_MDL.GetMDL(), m_RootMDL.m_MDL.m_nSequence );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a more accurate bounding sphere
//-----------------------------------------------------------------------------
bool CMergedMDL::GetBoundingSphere( Vector &vecCenter, float &flRadius )
{
	// Check to see if we have a valid model to look at.
	if ( m_RootMDL.m_MDL.GetMDL() == MDLHANDLE_INVALID )
		return false;

	Vector vecEngineCenter;
	GetMDLBoundingSphere( &vecEngineCenter, &flRadius, m_RootMDL.m_MDL.GetMDL(), m_RootMDL.m_MDL.m_nSequence );
	VectorTransform( vecEngineCenter, m_RootMDL.m_MDLToWorld, vecCenter );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMergedMDL::SetModelAnglesAndPosition(  const QAngle &angRot, const Vector &vecPos )
{
	SetIdentityMatrix( m_RootMDL.m_MDLToWorld );
	AngleMatrix( angRot, vecPos, m_RootMDL.m_MDLToWorld );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMergedMDL::SetupBonesForAttachmentQueries( void )
{
	if ( !g_pMDLCache )
		return;

	if ( m_RootMDL.m_MDL.GetMDL() == MDLHANDLE_INVALID )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	CStudioHdr *pRootStudioHdr = new CStudioHdr( g_pMDLCache->GetStudioHdr( m_RootMDL.m_MDL.GetMDL() ), g_pMDLCache );
	matrix3x4_t boneToWorld[MAXSTUDIOBONES];
	m_RootMDL.m_MDL.SetUpBones( m_RootMDL.m_MDLToWorld, pRootStudioHdr->numbones(), boneToWorld, m_PoseParameters, m_SequenceLayers, m_nNumSequenceLayers );

	delete pRootStudioHdr;
}

//-----------------------------------------------------------------------------
// paint it!
//-----------------------------------------------------------------------------
void CMergedMDL::Draw()
{
	if ( !g_pMDLCache )
		return;

	if ( m_RootMDL.m_MDL.GetMDL() == MDLHANDLE_INVALID )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	// Draw the MDL
	CStudioHdr *pRootStudioHdr = new CStudioHdr( g_pMDLCache->GetStudioHdr( m_RootMDL.m_MDL.GetMDL() ), g_pMDLCache );
	matrix3x4_t boneToWorld[MAXSTUDIOBONES];
	const matrix3x4_t *pRootMergeHdrModelToWorld = &m_RootMDL.m_MDLToWorld;
	const matrix3x4_t *pFollowBoneToWorld = boneToWorld;
	m_RootMDL.m_MDL.SetUpBones( m_RootMDL.m_MDLToWorld, pRootStudioHdr->numbones(), boneToWorld, m_PoseParameters, m_SequenceLayers, m_nNumSequenceLayers );

	OnPostSetUpBonesPreDraw();

	int nFlags = STUDIORENDER_DRAW_NO_SHADOWS;

	OnModelDrawPassStart( 0, pRootStudioHdr, nFlags );
	m_RootMDL.m_MDL.Draw( m_RootMDL.m_MDLToWorld, boneToWorld, nFlags );
	OnModelDrawPassFinished( 0, pRootStudioHdr, nFlags );

	// Draw the merge MDLs.
	matrix3x4_t *pStackCopyOfRootMergeHdrModelToWorld = NULL;
	matrix3x4_t matMergeBoneToWorld[MAXSTUDIOBONES];
	int nMergeCount = m_aMergeMDLs.Count();
	for ( int iMerge = 0; iMerge < nMergeCount; ++iMerge )
	{
		matrix3x4_t *pMergeBoneToWorld = &matMergeBoneToWorld[0];

		// Get the merge studio header.
		CStudioHdr *pMergeHdr = new CStudioHdr( g_pMDLCache->GetStudioHdr( m_aMergeMDLs[iMerge].m_MDL.GetMDL() ), g_pMDLCache );
		m_aMergeMDLs[iMerge].m_MDL.SetupBonesWithBoneMerge( pMergeHdr, pMergeBoneToWorld, pRootStudioHdr, boneToWorld, *pRootMergeHdrModelToWorld );

		OnModelDrawPassStart( 0, pMergeHdr, nFlags );
		m_aMergeMDLs[iMerge].m_MDL.Draw( m_aMergeMDLs[iMerge].m_MDLToWorld, pMergeBoneToWorld, nFlags );
		OnModelDrawPassFinished( 0, pMergeHdr, nFlags );

		if ( m_aMergeMDLs[iMerge].m_bRequestBoneMergeTakeover && ( iMerge + 1 < nMergeCount ) )
		{
			// This model is requesting bonemerge takeover and we have more models to render after it
			delete pRootStudioHdr;
			pRootStudioHdr = pMergeHdr;
			pRootMergeHdrModelToWorld = &m_aMergeMDLs[iMerge].m_MDLToWorld;

			// Make a copy of bone to world transforms in a separate stack buffer and repoint root transforms
			// for future bonemerge into that buffer
			if ( !pStackCopyOfRootMergeHdrModelToWorld )
				pStackCopyOfRootMergeHdrModelToWorld = ( matrix3x4_t * ) stackalloc( sizeof( matMergeBoneToWorld ) );
			Q_memcpy( pStackCopyOfRootMergeHdrModelToWorld, matMergeBoneToWorld, sizeof( matMergeBoneToWorld ) );
			pFollowBoneToWorld = pStackCopyOfRootMergeHdrModelToWorld;
		}
		else
		{
			delete pMergeHdr;
		}
	}

	delete pRootStudioHdr;
}


void CMergedMDL::Draw( const matrix3x4_t &rootToWorld )
{
	m_RootMDL.m_MDLToWorld = rootToWorld;
	Draw();
}


//-----------------------------------------------------------------------------
// Sets the current sequence
//-----------------------------------------------------------------------------
void CMergedMDL::SetSequence( int nSequence )
{
	m_RootMDL.m_MDL.m_nSequence = nSequence;
}


//-----------------------------------------------------------------------------
// Set the current pose parameters. If NULL the pose parameters will be reset
// to the default values.
//-----------------------------------------------------------------------------
void CMergedMDL::SetPoseParameters( const float *pPoseParameters, int nCount )
{
	if ( pPoseParameters )
	{
		int nParameters = MIN( MAXSTUDIOPOSEPARAM, nCount );
		for ( int iParam = 0; iParam < nParameters; ++iParam )
		{
			m_PoseParameters[ iParam ] = pPoseParameters[ iParam ];
		}
	}
	else if ( m_RootMDL.m_MDL.GetMDL() != MDLHANDLE_INVALID )
	{
		CStudioHdr studioHdr( g_pMDLCache->GetStudioHdr( m_RootMDL.m_MDL.GetMDL() ), g_pMDLCache );
		Studio_CalcDefaultPoseParameters( &studioHdr, m_PoseParameters, MAXSTUDIOPOSEPARAM );
	}
}


//-----------------------------------------------------------------------------
// Set the overlay sequence layers
//-----------------------------------------------------------------------------
void CMergedMDL::SetSequenceLayers( const MDLSquenceLayer_t *pSequenceLayers, int nCount )
{
	if ( pSequenceLayers )
	{
		m_nNumSequenceLayers = MIN( MAX_SEQUENCE_LAYERS, nCount );
		for ( int iLayer = 0; iLayer < m_nNumSequenceLayers; ++iLayer )
		{
			m_SequenceLayers[ iLayer ] = pSequenceLayers[ iLayer ];
		}
	}
	else
	{
		m_nNumSequenceLayers = 0;
		V_memset( m_SequenceLayers, 0, sizeof( m_SequenceLayers ) );
	}
}

void CMergedMDL::SetSkin( int nSkin )
{
	m_RootMDL.m_MDL.m_nSkin = nSkin;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMergedMDL::SetMergeMDL( MDLHandle_t handle, void *pProxyData, bool bRequestBonemergeTakeover )
{
	// Verify that we have a root model to merge to.
	if ( m_RootMDL.m_MDL.GetMDL() == MDLHANDLE_INVALID )
		return;

	int iIndex = m_aMergeMDLs.AddToTail();
	if ( !m_aMergeMDLs.IsValidIndex( iIndex ) )
		return;

	m_aMergeMDLs[iIndex].m_MDL.SetMDL( handle );
	m_aMergeMDLs[iIndex].m_MDL.m_pProxyData = pProxyData;
	m_aMergeMDLs[iIndex].m_bRequestBoneMergeTakeover = bRequestBonemergeTakeover;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
MDLHandle_t CMergedMDL::SetMergeMDL( const char *pMDLName, void *pProxyData, bool bRequestBonemergeTakeover )
{
	if ( g_pMDLCache == NULL )
		return MDLHANDLE_INVALID;

	MDLHandle_t hMDL = pMDLName ? g_pMDLCache->FindMDL( pMDLName ) : MDLHANDLE_INVALID;
	if ( g_pMDLCache->IsErrorModel( hMDL ) )
	{
		hMDL = MDLHANDLE_INVALID;
	}

	SetMergeMDL( hMDL, pProxyData, bRequestBonemergeTakeover );
	return hMDL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CMergedMDL::GetMergeMDLIndex( MDLHandle_t handle )
{
	int nMergeCount = m_aMergeMDLs.Count();
	for ( int iMerge = 0; iMerge < nMergeCount; ++iMerge )
	{
		if ( m_aMergeMDLs[iMerge].m_MDL.GetMDL() == handle )
			return iMerge;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CMDL *CMergedMDL::GetMergeMDL( MDLHandle_t handle )
{
	int nMergeCount = m_aMergeMDLs.Count();
	for ( int iMerge = 0; iMerge < nMergeCount; ++iMerge )
	{
		if ( m_aMergeMDLs[iMerge].m_MDL.GetMDL() == handle )
			return (&m_aMergeMDLs[iMerge].m_MDL);
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMergedMDL::ClearMergeMDLs( void )
{
	m_aMergeMDLs.Purge();
}