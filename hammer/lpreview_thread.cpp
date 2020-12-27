//===== Copyright © 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: The thread which performs lighting preview
//
//===========================================================================//

#include "stdafx.h"
#include "lpreview_thread.h"
#include "floatbitmap.h"

//#define HAMMER_RAYTRACE
#include "raytrace.h"
#include "hammer.h"
#include "mainfrm.h"
#include "mapdoc.h"
#include "lprvwindow.h"
#include "threadtools.h"
#include "vstdlib/jobthread.h"
#include "mathlib/halton.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

extern const int32 ALIGN16 g_SIMD_EveryOtherMask[4] = { 0, ~0, 0, ~0 };

#define LPREVIEW_MULTITHREAD 1

CInterlockedInt n_gbufs_queued;

#define NUMBER_OF_LINES_TO_CALCULATE_PER_STEP 8

// the current lighting preview output, if we have one
Bitmap_t * g_pLPreviewOutputBitmap;
IThreadPool *s_pThreadPool;
static int s_nNumThreads;

enum IncrementalLightState
{
	INCR_STATE_NO_RESULTS = 0,							// we threw away the results for this light
	INCR_STATE_PARTIAL_RESULTS = 1,						// have done some but not all
	INCR_STATE_NEW = 2,									// we know nothing about this light
	INCR_STATE_HAVE_FULL_RESULTS = 3,					// we are done
};


class CLightingPreviewThread;

constexpr int MAX_IMAGE_HEIGHT = 1024;

// attributes for result soa containers

constexpr int RSLT_ATTR_DIFFUSE_RGB = 0;

constexpr int RSLT_BUFFER_RSLT_RGB = 3;

constexpr int GBUFFER_ATTR_POSITION = 0;
constexpr int GBUFFER_ATTR_ALBEDO = 1;
constexpr int GBUFFER_ATTR_NORMAL = 2;

class CIncrementalLightInfo
{
public:
	CIncrementalLightInfo* m_pNext;
	CLightingPreviewLightDescription* m_pLight;
	// incremental lighting tracking information
	int m_nObjectID;
	int m_nNumLinesCalculated;
	IncrementalLightState m_eIncrState;
	CSOAContainer m_CalculatedContribution;
	float m_fTotalContribution;								// current magnitude of light effect
	float m_flLastContribution;								// the last amount added

	int m_nBitmapGenerationCounter;							// set on receive of new data from master
	float m_fDistanceToEye;
	int m_nMostRecentNonZeroContributionTimeStamp;
	uint8 m_nCalculationLevel[MAX_IMAGE_HEIGHT];			// 0 = not calculated, 1 = calculated at 1:1, etc.
	int m_nMaxCalculatedLine;
	int m_nFirstCalculatedLine;
	bool m_bCreatedIndirectLights;

	bool m_bDisabled;

	CIncrementalLightInfo()
	{
		m_pNext = NULL;
		m_pLight = NULL;
		m_nObjectID = -1;
		m_nNumLinesCalculated = 0;
		m_eIncrState = INCR_STATE_NEW;
		m_fTotalContribution = 0.f;
		m_flLastContribution = 0.f;
		m_nBitmapGenerationCounter = 0;
		m_fDistanceToEye = 0.f;
		m_nMostRecentNonZeroContributionTimeStamp = 0;
		memset( m_nCalculationLevel, 0, sizeof( m_nCalculationLevel ) );
		m_nMaxCalculatedLine = -1;
		m_nFirstCalculatedLine = INT_MAX;
		m_bCreatedIndirectLights = false;
		m_bDisabled = false;
	}

	float PredictedContribution() const;

	void SetContributionSize( int nWidth, int nHeight )
	{
		if ( m_CalculatedContribution.NumCols() != nWidth || m_CalculatedContribution.NumRows() != nHeight )
		{
			m_CalculatedContribution.Purge();
			if ( nWidth && nHeight )
			{
				m_CalculatedContribution.SetAttributeType( RSLT_ATTR_DIFFUSE_RGB, ATTRDATATYPE_4V );
				m_CalculatedContribution.AllocateData( nWidth, nHeight );
				m_CalculatedContribution.FillAttr( RSLT_ATTR_DIFFUSE_RGB, vec3_origin );
			}
		}

	}

	void DiscardResults()
	{
		m_bDisabled = false;
		m_CalculatedContribution.Purge();
		memset( m_nCalculationLevel, 0, sizeof( m_nCalculationLevel ) );
		m_nMaxCalculatedLine = -1;
		m_nFirstCalculatedLine = INT_MAX;
		if ( m_eIncrState != INCR_STATE_NEW )
			m_eIncrState = INCR_STATE_NO_RESULTS;
		m_nNumLinesCalculated = 0;
	}

	void ClearIncremental()
	{
		m_eIncrState = INCR_STATE_NEW;
		// free calculated lighting matrix
		DiscardResults();
	}

	bool HasWorkToDo() const
	{
		if ( m_bDisabled )
			return false;
		return m_eIncrState != INCR_STATE_HAVE_FULL_RESULTS;
	}

	bool IsLowerPriorityThan( CLightingPreviewThread* pLPV, CIncrementalLightInfo const& other ) const;

	bool IsHighPriority( CLightingPreviewThread* pLPV ) const;
};

class CLightingPreviewThread
{
public:
	CUtlIntrusiveList<CLightingPreviewLightDescription> m_LightList;

	CSOAContainer m_GBuffer;
	CSOAContainer m_GBufferLowRes;

	RayTracingEnvironment* m_pRtEnv;
	CIncrementalLightInfo* m_pIncrementalLightInfoList;

	bool m_bAccStructureBuilt;
	Vector m_LastEyePosition;

	bool m_bResultChangedSinceLastSend;
	float m_fLastSendTime;

	int m_nBitmapGenerationCounter;
	int m_nContributionCounter;

	// bounidng box of the rendered scene+ the eye
	Vector m_MinViewCoords;
	Vector m_MaxViewCoords;

	// sets that we are doing the first update since a discard and should do more lights per pass
	bool m_bFirstWork;

	CLightingPreviewThread()
	{
		m_pRtEnv = NULL;
		m_pIncrementalLightInfoList = NULL;
		m_bAccStructureBuilt = false;
		m_LastEyePosition.Init();
		m_bResultChangedSinceLastSend = false;
		m_fLastSendTime = - 1.0e6;
		m_nBitmapGenerationCounter = -1;
		m_nContributionCounter = 1000000;
		m_MinViewCoords.Init();
		m_bFirstWork = true;
	}


	~CLightingPreviewThread()
	{
		m_LightList.Purge();
		while ( m_pIncrementalLightInfoList )
		{
			CIncrementalLightInfo* n = m_pIncrementalLightInfoList->m_pNext;
			delete m_pIncrementalLightInfoList;
			m_pIncrementalLightInfoList = n;
		}
	}

	// check if the master has new work for us to do, meaning we should abort rendering
	bool ShouldAbort() const
	{
		return g_HammerToLPreviewMsgQueue.MessageWaiting();
	}

	// main loop
	void Run();

	// handle new g-buffers from master
	void HandleGBuffersMessage( MessageToLPreview& msg_in );

	// accept triangle list from master
	void HandleGeomMessage( MessageToLPreview& msg_in );

	// send one of our output images back
	void SendResultRendering( CSOAContainer& rsltBuffer );

	// calculate m_MinViewCoords, m_MaxViewCoords - the bounding box of the rendered pixels+the eye
	void CalculateSceneBounds();

	// inner lighting loop. meant to be multithreaded on dual-core (or more)
	void CalculateForLightTask( int nLineStart, int nLineEnd, CLightingPreviewLightDescription* l, float* fContributionOut, CIncrementalLightInfo* pInfremental );

	void CalculateForLight( CLightingPreviewLightDescription* l );

	// send our current output back
	void SendResult();

	void UpdateIncrementalForNewLightList();

	void DiscardResults()
	{
//		Warning(" invalidate\n" );
		// invalidate all per light result data
		for ( CIncrementalLightInfo* i = m_pIncrementalLightInfoList; i; i = i->m_pNext )
			i->DiscardResults();

		// bump time stamp
		m_nContributionCounter++;
		// update distances to lights
		for ( CLightingPreviewLightDescription* l = m_LightList.Head(); l; l = l->m_pNext )
		{
			CIncrementalLightInfo* l_info = l->m_pIncrementalInfo;
			if ( l->m_Type == MATERIAL_LIGHT_DIRECTIONAL )
				l_info->m_fDistanceToEye = 0;			// high priority
			else
				l_info->m_fDistanceToEye = m_LastEyePosition.DistTo( l->m_Position );
			if ( l_info->m_pLight->m_Type != MATERIAL_LIGHT_DIRECTIONAL )
				l_info->m_pLight->m_bLowRes = l_info->m_fDistanceToEye > 2048.0f;
		}
		m_bResultChangedSinceLastSend = true;
		m_fLastSendTime = Plat_FloatTime() - 12;				// force send
		m_bFirstWork = true;
	}

	// handle a message. returns true if the thread shuold exit
	bool HandleAMessage();

	// returns whether or not there is useful work to do
	bool AnyUsefulWorkToDo();

	// do some work, like a rendering for one light
	void DoWork();

	Vector EstimatedUnshotAmbient() const
	{
		const float sum_weights = 0.0001f;
		Vector sum_colors( sum_weights, sum_weights, sum_weights );
		// calculate an ambient color based on light calculcated so far
		for ( CLightingPreviewLightDescription* l = m_LightList.Head(); l; l = l->m_pNext )
		{
			CIncrementalLightInfo* l_info = l->m_pIncrementalInfo;
			if ( l_info && ( l_info->m_eIncrState == INCR_STATE_HAVE_FULL_RESULTS || l_info->m_eIncrState == INCR_STATE_PARTIAL_RESULTS ) )
			{
				float flPredictedContribution = l_info->PredictedContribution();
				sum_colors.x += flPredictedContribution * l->m_Color.x;
				sum_colors.y += flPredictedContribution * l->m_Color.y;
				sum_colors.z += flPredictedContribution * l->m_Color.z;
			}
		}
		sum_colors.NormalizeInPlace();
		sum_colors *= 0.05;
		return sum_colors;
	}
	void AccumulateOuput( int nLineMask, CSOAContainer* pResult, CSOAContainer* pLowresResule ) const;

	void AddLowresResultToHires( CSOAContainer& lowres, CSOAContainer& hires );
};


bool CIncrementalLightInfo::IsHighPriority( CLightingPreviewThread* pLPV ) const
{
	// is this lighjt prioirty-boosted in some way?
	// uncalculated lights within the view range are highest priority
	if ( m_eIncrState == INCR_STATE_NEW && m_pLight->m_Position.WithinAABox( pLPV->m_MinViewCoords, pLPV->m_MaxViewCoords ) )
		return true;
	return false;

}

bool CIncrementalLightInfo::IsLowerPriorityThan( CLightingPreviewThread* pLPV, CIncrementalLightInfo const& other ) const
{
	// a NEW light within the view volume is highest priority
	bool highpriority = IsHighPriority( pLPV );
	bool other_highpriority = other.IsHighPriority( pLPV );

	if ( highpriority && !other_highpriority )
		return false;
	if ( other_highpriority && !highpriority )
		return true;

	int state_combo = m_eIncrState + 16 * other.m_eIncrState;
	switch ( state_combo )
	{
		case INCR_STATE_NEW + 16 * INCR_STATE_NEW:
		{
			// if both are new, closest to eye is best
			return m_fDistanceToEye > other.m_fDistanceToEye;
		}

		case INCR_STATE_NEW + 16 * INCR_STATE_NO_RESULTS:
		{
			// new loses to something we know is probably going to contribute light
			return other.m_fTotalContribution > 0;
		}

		case INCR_STATE_NEW + 16 * INCR_STATE_PARTIAL_RESULTS:
		{
			return false;
		}

		case INCR_STATE_PARTIAL_RESULTS + 16 * INCR_STATE_NEW:
		{
			return true;
		}

		case INCR_STATE_NO_RESULTS + 16 * INCR_STATE_NEW:
		{
			// partial or discarded with no brightness loses to new
			return m_fTotalContribution == 0;
		}


		case INCR_STATE_PARTIAL_RESULTS + 16 * INCR_STATE_PARTIAL_RESULTS:
		{
			// if incrmental vs incremental, and no light from either, do most recently lit one
			if ( m_fTotalContribution == 0.0f && other.m_fTotalContribution == 0.0f &&
				other.m_nMostRecentNonZeroContributionTimeStamp > m_nMostRecentNonZeroContributionTimeStamp )
				return true;

			int nMaxLines = max( m_nNumLinesCalculated, other.m_nNumLinesCalculated );
			int nMinLines = min( m_nNumLinesCalculated, other.m_nNumLinesCalculated );

			// if other is black, and ratios aren't extremely far off, keep this one
			if ( nMaxLines <= 16 * nMinLines )
			{
				if ( other.m_fTotalContribution == 0.0f && m_fTotalContribution > 0 )
					return false;
				if ( m_fTotalContribution == 0.0f && other.m_fTotalContribution > 0 )
					return true;
			}

			// if incremental states are close, do brightest
			if ( nMaxLines <= 2 * nMinLines )
				return PredictedContribution() < other.PredictedContribution();

			// else do least refined
			return ( m_nNumLinesCalculated > other.m_nNumLinesCalculated );
		}
		case INCR_STATE_PARTIAL_RESULTS + 16 * INCR_STATE_NO_RESULTS:
		{
			if ( other.m_fTotalContribution )
				return true;
			if ( m_fTotalContribution == 0.0f && other.m_fTotalContribution == 0.0f )
				return other.m_nMostRecentNonZeroContributionTimeStamp > m_nMostRecentNonZeroContributionTimeStamp;
			return PredictedContribution() < other.PredictedContribution();
		}
		case INCR_STATE_NO_RESULTS + 16 * INCR_STATE_PARTIAL_RESULTS:
		{
			if ( m_fTotalContribution )
				return false;
			if ( m_fTotalContribution == 0.0f && other.m_fTotalContribution == 0.0f )
				return other.m_nMostRecentNonZeroContributionTimeStamp > m_nMostRecentNonZeroContributionTimeStamp;
			return PredictedContribution() < other.PredictedContribution();
		}
		case INCR_STATE_NO_RESULTS * 16 + INCR_STATE_NO_RESULTS:
		{
			// if incrmental vs discarded, brightest or most recently bright wins
			if ( m_fTotalContribution == 0.0f && other.m_fTotalContribution == 0.0f )
				return other.m_nMostRecentNonZeroContributionTimeStamp > m_nMostRecentNonZeroContributionTimeStamp;
			return PredictedContribution() < other.PredictedContribution();
		}
	}
	return false;
}

void CLightingPreviewThread::HandleGeomMessage( MessageToLPreview& msg_in )
{
	if ( m_pRtEnv )
	{
		delete m_pRtEnv;
		m_pRtEnv = NULL;
	}
	CUtlVector<Vector>& tris = *msg_in.m_pShadowTriangleList;
	if ( !tris.IsEmpty() )
	{
		m_pRtEnv = new RayTracingEnvironment;
		for ( int i = 0; i < tris.Count(); i += 3 )
			m_pRtEnv->AddTriangle( i, tris[i], tris[1 + i], tris[2 + i], vec3_origin, RTE_FLAGS_DONT_STORE_TRIANGLE_COLORS | RTE_FLAGS_DONT_STORE_TRIANGLE_MATERIALS, 0 );
	}
	delete msg_in.m_pShadowTriangleList;
	m_bAccStructureBuilt = false;
}


float CIncrementalLightInfo::PredictedContribution() const
{
	if ( m_fTotalContribution == 0 || !m_nNumLinesCalculated )
		return 0.f;
	else
		return m_fTotalContribution * ( m_CalculatedContribution.NumRows() * ( 1.0f / m_nNumLinesCalculated ) );
}

void CLightingPreviewThread::CalculateSceneBounds()
{
	FourVectors minbound, maxbound;
	minbound.DuplicateVector( m_LastEyePosition );
	maxbound.DuplicateVector( m_LastEyePosition );
	for ( int y = 0; y < m_GBuffer.NumRows(); y++ )
	{
		FourVectors const* cptr = m_GBuffer.RowPtr<FourVectors>( GBUFFER_ATTR_POSITION, y );
		for ( int x = 0; x < m_GBuffer.NumQuadsPerRow(); x++ )
		{
			minbound.x = MinSIMD( cptr->x, minbound.x );
			minbound.y = MinSIMD( cptr->y, minbound.y );
			minbound.z = MinSIMD( cptr->z, minbound.z );

			maxbound.x = MaxSIMD( cptr->x, maxbound.x );
			maxbound.y = MaxSIMD( cptr->y, maxbound.y );
			maxbound.z = MaxSIMD( cptr->z, maxbound.z );
			cptr++;
		}
	}
	m_MinViewCoords = minbound.Vec( 0 );
	m_MaxViewCoords = maxbound.Vec( 0 );
	for ( int v = 1; v < 4; v++ )
	{
		m_MinViewCoords = m_MinViewCoords.Min( minbound.Vec( v ) );
		m_MaxViewCoords = m_MaxViewCoords.Max( maxbound.Vec( v ) );
	}
}


void CLightingPreviewThread::UpdateIncrementalForNewLightList()
{
	for ( CLightingPreviewLightDescription* l = m_LightList.Head(); l; l = l->m_pNext )
	{
		// see if we know about this light
		for ( CIncrementalLightInfo* i = m_pIncrementalLightInfoList; i; i = i->m_pNext )
		{
			if ( i->m_nObjectID == l->m_nObjectID )
			{
				// found it!
				l->m_pIncrementalInfo = i;
				i->m_pLight = l;
				break;
			}
		}
		if ( !l->m_pIncrementalInfo )
		{
			l->m_pIncrementalInfo = new CIncrementalLightInfo;
			l->m_pIncrementalInfo->m_nObjectID = l->m_nObjectID;
			l->m_pIncrementalInfo->m_pLight = l;

			// add to list
			l->m_pIncrementalInfo->m_pNext = m_pIncrementalLightInfoList;
			m_pIncrementalLightInfoList = l->m_pIncrementalInfo;
		}
	}
}


void CLightingPreviewThread::Run()
{
	bool should_quit = false;
	while ( !should_quit )
	{
		while ( !should_quit && ( !AnyUsefulWorkToDo() || g_HammerToLPreviewMsgQueue.MessageWaiting() ) )
			should_quit |= HandleAMessage();
		if ( !should_quit && AnyUsefulWorkToDo() )
			DoWork();
		if ( m_bResultChangedSinceLastSend )
		{
			float newtime = Plat_FloatTime();
			if ( newtime - m_fLastSendTime > 2.0f || !AnyUsefulWorkToDo() )
				SendResult();
		}

	}
}

bool CLightingPreviewThread::HandleAMessage()
{
	MessageToLPreview msg_in;
	g_HammerToLPreviewMsgQueue.WaitMessage( &msg_in );
	switch ( msg_in.m_MsgType )
	{
		case LPREVIEW_MSG_EXIT:
			return true;									// return from thread

		case LPREVIEW_MSG_LIGHT_DATA:
		{
			m_LightList.Purge();
			m_LightList = msg_in.m_LightList;
			m_LastEyePosition = msg_in.m_EyePosition;
			UpdateIncrementalForNewLightList();
			DiscardResults();
		}
		break;

		case LPREVIEW_MSG_GEOM_DATA:
			HandleGeomMessage( msg_in );
			DiscardResults();
			break;

		case LPREVIEW_MSG_G_BUFFERS:
			HandleGBuffersMessage( msg_in );
			DiscardResults();
			break;
	}
	return false;
}

bool CLightingPreviewThread::AnyUsefulWorkToDo()
{
	if ( m_GBuffer.NumRows() )
	{
		for ( CLightingPreviewLightDescription* l = m_LightList.Head(); l; l = l->m_pNext )
		{
			CIncrementalLightInfo* l_info = l->m_pIncrementalInfo;
			if ( l_info->HasWorkToDo() )
				return true;
		}
	}
	return false;
}

void CLightingPreviewThread::DoWork()
{
	if ( m_pRtEnv && !m_bAccStructureBuilt )
	{
		m_bAccStructureBuilt = true;
		m_pRtEnv->SetupAccelerationStructure();
	}
	CLightingPreviewLightDescription* pLightsToRun[64];
	int nNumLightJobs = 0;
	int nJobsToDo = s_nNumThreads + 1;
	if ( m_bFirstWork )
	{
		nJobsToDo *= 2;
		m_bFirstWork = false;
	}
#if LPREVIEW_MULTITHREAD == 0
	nJobsToDo = 1;
#endif

	nJobsToDo = Min( nJobsToDo, 64 );
	for ( int i = 0; i < nJobsToDo; i++ )
	{
		CLightingPreviewLightDescription* best_l = NULL;
		for ( CLightingPreviewLightDescription* l = m_LightList.Head(); l; l = l->m_pNext )
		{
			CIncrementalLightInfo* l_info = l->m_pIncrementalInfo;
			// check if light could influence scene
			if ( l_info->m_bDisabled )
				continue;								// this light can't effect the visible scene
			if ( l->m_Type != MATERIAL_LIGHT_DIRECTIONAL )
			{
				float lrad = l->DistanceAtWhichBrightnessIsLessThan( 1.0f / 500.0f );
				if ( !l->m_Position.WithinAABox( m_MinViewCoords - ReplicateToVector( lrad ), m_MaxViewCoords + ReplicateToVector( lrad ) ) )
					l_info->m_bDisabled = true;
			}
			if ( l_info->m_bDisabled )
				continue;								// this light can't effect the visible scene

			// check that we don't have it
			bool bHaveit = false;
			for ( int j = 0; j < nNumLightJobs; j++ )
				if ( pLightsToRun[j] == l )
					bHaveit = true;
			if ( !bHaveit && l_info->HasWorkToDo() )
			{
				if ( !best_l || best_l->m_pIncrementalInfo->IsLowerPriorityThan( this, *l_info ) )
					best_l = l;
			}
		}
		if ( best_l )
			pLightsToRun[nNumLightJobs++] = best_l;
	}
	// now, process in parallel
	if ( nNumLightJobs )
	{
#if LPREVIEW_MULTITHREAD == 1
		{
			CJobSetN<64> jobs;
			for ( int i = 0; i < nNumLightJobs; i++ )
				jobs += s_pThreadPool->QueueCall( this, &CLightingPreviewThread::CalculateForLight, pLightsToRun[i] );

			jobs.WaitForFinish( s_pThreadPool );
		}
#else
		for ( int i = 0; i < nNumLightJobs; i++ )
		{
			Warning( "process light %p lnum=%d contribution = %f predicted = %f\n", pLightsToRun[i], pLightsToRun[i]->m_pIncrementalInfo->m_nNumLinesCalculated, pLightsToRun[i]->m_pIncrementalInfo->m_fTotalContribution, pLightsToRun[i]->m_pIncrementalInfo->PredictedContribution() );
			CalculateForLight( pLightsToRun[i] );
		}
#endif
		// now, some lights may have created lights for indirect light. We must move these to the global list.
		// we could not do this while creating them, because of thread-safety.
		for ( int i = 0; i < nNumLightJobs; i++ )
		{
			if ( pLightsToRun[i]->m_pIncrementalInfo->m_flLastContribution )
				m_bResultChangedSinceLastSend = true;
			for ( int j = 0; j < pLightsToRun[i]->m_TempChildren.Count(); j++ )
			{
				CLightingPreviewLightDescription* pNew = pLightsToRun[i]->m_TempChildren[j];
				pNew->m_pIncrementalInfo = new CIncrementalLightInfo;
				pNew->m_pIncrementalInfo->m_nObjectID = pNew->m_nObjectID;
				pNew->m_pIncrementalInfo->m_pLight = pNew;
				pNew->m_pIncrementalInfo->m_pNext = m_pIncrementalLightInfoList;
				m_pIncrementalLightInfoList = pNew->m_pIncrementalInfo;
				m_LightList.AddToTail( pNew );
			}
			pLightsToRun[i]->m_TempChildren.Purge();
			pLightsToRun[i]->m_bDidIndirect = true;
		}
	}
}



void CLightingPreviewThread::HandleGBuffersMessage( MessageToLPreview& msg_in )
{
	//FPExceptionEnabler e;
	m_GBuffer.Purge();
	m_GBuffer.SetAttributeType( GBUFFER_ATTR_POSITION, ATTRDATATYPE_4V );
	m_GBuffer.SetAttributeType( GBUFFER_ATTR_ALBEDO, ATTRDATATYPE_4V );
	m_GBuffer.SetAttributeType( GBUFFER_ATTR_NORMAL, ATTRDATATYPE_4V );
	m_GBuffer.AllocateData( msg_in.m_pDefferedRenderingBMs[0]->NumCols(),
							msg_in.m_pDefferedRenderingBMs[0]->NumRows() );

#ifdef _DEBUG
	m_GBuffer.FillAttr( GBUFFER_ATTR_POSITION, vec3_origin );
	m_GBuffer.FillAttr( GBUFFER_ATTR_ALBEDO, vec3_origin );
	m_GBuffer.FillAttr( GBUFFER_ATTR_NORMAL, Vector( 1, 0, 0 ) );
#endif

	m_GBuffer.PackScalarAttributesToVectorAttribute( msg_in.m_pDefferedRenderingBMs[0], GBUFFER_ATTR_ALBEDO,
													 FBM_ATTR_RED, FBM_ATTR_GREEN, FBM_ATTR_BLUE );
	m_GBuffer.PackScalarAttributesToVectorAttribute( msg_in.m_pDefferedRenderingBMs[1], GBUFFER_ATTR_NORMAL,
													 FBM_ATTR_RED, FBM_ATTR_GREEN, FBM_ATTR_BLUE );
	m_GBuffer.PackScalarAttributesToVectorAttribute( msg_in.m_pDefferedRenderingBMs[2], GBUFFER_ATTR_POSITION,
													 FBM_ATTR_RED, FBM_ATTR_GREEN, FBM_ATTR_BLUE );

#ifdef _DEBUG
	for ( int y = 0; y < m_GBuffer.NumRows(); y++ )
	{
		float const* pPos = m_GBuffer.RowPtr<float>( GBUFFER_ATTR_POSITION, y );
		float const* pAlbedo = m_GBuffer.RowPtr<float>( GBUFFER_ATTR_ALBEDO, y );
		float const* pNormal = m_GBuffer.RowPtr<float>( GBUFFER_ATTR_NORMAL, y );
		for ( int x = 0; x < m_GBuffer.NumCols(); x++ )
		{
			Assert( !isnan( pPos[0] ) );
			Assert( !isnan( pPos[4] ) );
			Assert( !isnan( pPos[8] ) );
			Assert( !isnan( pAlbedo[0] ) );
			Assert( !isnan( pAlbedo[4] ) );
			Assert( !isnan( pAlbedo[8] ) );
			Assert( !isnan( pNormal[0] ) );
			Assert( !isnan( pNormal[4] ) );
			Assert( !isnan( pNormal[8] ) );
			pPos++;
			pAlbedo++;
			pNormal++;
			if ( ( x & 3 ) == 3 )
			{
				pPos += 8;
				pAlbedo += 8;
				pNormal += 8;
			}
		}
	}
#endif

	m_GBufferLowRes.Purge();
	m_GBufferLowRes.SetAttributeType( GBUFFER_ATTR_POSITION, ATTRDATATYPE_4V );
	m_GBufferLowRes.SetAttributeType( GBUFFER_ATTR_ALBEDO, ATTRDATATYPE_4V );
	m_GBufferLowRes.SetAttributeType( GBUFFER_ATTR_NORMAL, ATTRDATATYPE_4V );
	m_GBufferLowRes.AllocateData( msg_in.m_pDefferedRenderingBMs[0]->NumCols() / 4,
								  msg_in.m_pDefferedRenderingBMs[0]->NumRows() / 4 );

	// now, downsample
	m_GBufferLowRes.ResampleAttribute( m_GBuffer, GBUFFER_ATTR_POSITION );
	m_GBufferLowRes.ResampleAttribute( m_GBuffer, GBUFFER_ATTR_ALBEDO );
	m_GBufferLowRes.ResampleAttribute( m_GBuffer, GBUFFER_ATTR_NORMAL );


	m_LastEyePosition = msg_in.m_EyePosition;
	for ( uint i = 0;i < ARRAYSIZE( msg_in.m_pDefferedRenderingBMs ); i++ )
		delete msg_in.m_pDefferedRenderingBMs[i];
	n_gbufs_queued--;
	m_nBitmapGenerationCounter = msg_in.m_nBitmapGenerationCounter;
	CalculateSceneBounds();
}

#ifdef _DEBUG
static FORCEINLINE bool isnan( fltx4 n )
{
	return isnan( SubFloat( n, 0 ) ) || isnan( SubFloat( n, 1 ) ) || isnan( SubFloat( n, 2 ) ) || isnan( SubFloat( n, 3 ) );
}
#endif

#ifdef __clang__
__attribute__((optnone))
#else
#pragma optimize( "", off )
#endif

void CLightingPreviewThread::AccumulateOuput( int nLineMask, CSOAContainer* rslt, CSOAContainer* rslt1 ) const
{
	//FPExceptionEnabler e;
	for ( const CLightingPreviewLightDescription* l = m_LightList.Head(); l; l = l->m_pNext )
	{
		CSOAContainer* pRslt = rslt;
		const CSOAContainer* pGB = &m_GBuffer;
		if ( l->m_bLowRes )
		{
			pGB = &m_GBufferLowRes;
			pRslt = rslt1;
		}
		const CIncrementalLightInfo* l_info = l->m_pIncrementalInfo;
		if ( l_info->m_fTotalContribution > 0.0f && l_info->m_eIncrState >= INCR_STATE_PARTIAL_RESULTS )
		{
			// need to add partials, replicated to handle undone lines
			const CSOAContainer& src = l_info->m_CalculatedContribution;
			int nY0 = l_info->m_nFirstCalculatedLine;
			int nY1 = nY0;
			// scan forward to find the next calculated line, if any
			while ( nY1 < l_info->m_nMaxCalculatedLine )
			{
				nY1++;
				if ( l_info->m_nCalculationLevel[nY1] )
					break;
			}
			fltx4 fl4NormalFactorScale = ReplicateX4( 4.0f );
			fltx4 fl4NormalBias = ReplicateX4( 0.0f ); //1.01 );		// prevent 0.
			for ( int y = 0; y < pGB->NumRows(); y++ )
			{
				if ( nLineMask & ( 1 << ( y & 31 ) ) )
				{
					fltx4 fl4Weights[2];
					if ( y < nY0 || nY0 == nY1 )
					{
						fl4Weights[0] = Four_Ones;
						fl4Weights[1] = Four_Zeros;
					}
					else
					{
						fl4Weights[1] = ReplicateX4( ( y - nY0 ) * ( 1.0f / ( nY1 - nY0 ) ) );
						fl4Weights[0] = SubSIMD( Four_Ones, fl4Weights[1] );
					}
					FourVectors* pRslts[2];
					pRslts[0] = src.RowPtr<FourVectors>( RSLT_ATTR_DIFFUSE_RGB, nY0 );
					pRslts[1] = src.RowPtr<FourVectors>( RSLT_ATTR_DIFFUSE_RGB, nY1 );
					FourVectors* dest = pRslt->RowPtr<FourVectors>( RSLT_BUFFER_RSLT_RGB, y );
					FourVectors const* pNormal = pGB->RowPtr<FourVectors>( GBUFFER_ATTR_NORMAL, y );
					FourVectors const* pRsltNormals[2];
					pRsltNormals[0] = pGB->RowPtr<FourVectors>( GBUFFER_ATTR_NORMAL, nY0 );
					pRsltNormals[1] = pGB->RowPtr<FourVectors>( GBUFFER_ATTR_NORMAL, nY1 );

					FourVectors const* pCoord = pGB->RowPtr<FourVectors>( GBUFFER_ATTR_POSITION, y );
					FourVectors const* pCoords[2];
					pCoords[0] = pGB->RowPtr<FourVectors>( GBUFFER_ATTR_POSITION, nY0 );
					pCoords[1] = pGB->RowPtr<FourVectors>( GBUFFER_ATTR_POSITION, nY1 );

					fltx4 fl4DistanceScale = ReplicateX4( 1.0 / 36.0 );
					for ( int x = 0; x < pGB->NumQuadsPerRow(); x++ )
					{
						FourVectors l1 = *pRslts[1]++;
						Assert( !isnan( l1.x ) );
						Assert( !isnan( l1.y ) );
						Assert( !isnan( l1.z ) );
						fltx4 fl4Dot = *pRsltNormals[1]++ * *pNormal;
						Assert( !isnan( fl4Dot ) );
						fl4Dot = MaxSIMD( Four_Epsilons, MulSIMD( fl4NormalFactorScale, AddSIMD( fl4NormalBias, fl4Dot ) ) );

						FourVectors fl4Delta = *pCoords[1]++;
						Assert( !isnan( fl4Delta.x ) );
						Assert( !isnan( fl4Delta.y ) );
						Assert( !isnan( fl4Delta.z ) );
						fl4Delta -= *pCoord;
						fltx4 fl4Distance = fl4Delta.length();
						Assert( !isnan( fl4Distance ) );
						fl4Distance = ReciprocalSIMD( AddSIMD( Four_Ones, MulSIMD( fl4Distance, fl4DistanceScale ) ) );

						fltx4 fl4SumWeights = MulSIMD( fl4Distance, MulSIMD( fl4Weights[1], fl4Dot ) );
						Assert( !isnan( fl4SumWeights ) );
						l1 *= fl4SumWeights;

						fl4Dot = *pRsltNormals[0]++ * *pNormal++;
						Assert( !isnan( fl4Dot ) );
						fl4Dot = MaxSIMD( Four_Epsilons, MulSIMD( fl4NormalFactorScale, AddSIMD( fl4NormalBias, fl4Dot ) ) );
						fl4Delta = *pCoords[0]++;
						fl4Delta -= *pCoord;
						Assert( !isnan( fl4Delta.x ) );
						Assert( !isnan( fl4Delta.y ) );
						Assert( !isnan( fl4Delta.z ) );
						fl4Distance = fl4Delta.length();
						Assert( !isnan( fl4Distance ) );
						pCoord++;

						fl4Distance = ReciprocalSIMD( AddSIMD( Four_Ones, MulSIMD( fl4Distance, fl4DistanceScale ) ) );
						FourVectors l2 = *pRslts[0]++;
						fltx4 w0 = MulSIMD( fl4Distance, MulSIMD( fl4Dot, fl4Weights[0] ) );
						Assert( !isnan( w0 ) );
						l2 *= w0;
						l1 += l2;

						fl4SumWeights = AddSIMD( fl4SumWeights, w0 );
						Assert( !isnan( fl4SumWeights ) );
						l1 *= ReciprocalSIMD( fl4SumWeights );

						*dest++ += l1;
					}
				}
				// now, update line indices
				if ( y >= nY1 )
				{
					nY0 = nY1;
					while ( nY1 < l_info->m_nMaxCalculatedLine )
					{
						nY1++;
						if ( l_info->m_nCalculationLevel[nY1] )
							break;
					}
				}

			}
		}
	}
}

FORCEINLINE void RotateLeftDoubleSIMD( fltx4& a, fltx4& b )
{
	a = SetWSIMD( RotateLeft( a ), SplatXSIMD( b ) );
	b = RotateLeft( b );
}

#define GRAB4PIXELS( base )															\
		base##AAAA.x = SplatXSIMD( base##ShiftRegister0.x );						\
		base##AAAA.y = SplatXSIMD( base##ShiftRegister0.y );						\
		base##AAAA.z = SplatXSIMD( base##ShiftRegister0.z );						\
		base##BBBB.x = SplatYSIMD( base##ShiftRegister0.x );						\
		base##BBBB.y = SplatYSIMD( base##ShiftRegister0.y );						\
		base##BBBB.z = SplatYSIMD( base##ShiftRegister0.z );						\
		RotateLeftDoubleSIMD( base##ShiftRegister0.x, base##ShiftRegister0a.x );	\
		RotateLeftDoubleSIMD( base##ShiftRegister0.y, base##ShiftRegister0a.y );	\
		RotateLeftDoubleSIMD( base##ShiftRegister0.z, base##ShiftRegister0a.z );	\
		base##EEEE.x = SplatXSIMD( base##ShiftRegister1.x );						\
		base##EEEE.y = SplatXSIMD( base##ShiftRegister1.y );						\
		base##EEEE.z = SplatXSIMD( base##ShiftRegister1.z );						\
		base##FFFF.x = SplatYSIMD( base##ShiftRegister1.x );						\
		base##FFFF.y = SplatYSIMD( base##ShiftRegister1.y );						\
		base##FFFF.z = SplatYSIMD( base##ShiftRegister1.z );						\
		RotateLeftDoubleSIMD( base##ShiftRegister1.x, base##ShiftRegister1a.x );	\
		RotateLeftDoubleSIMD( base##ShiftRegister1.y, base##ShiftRegister1a.y );	\
		RotateLeftDoubleSIMD( base##ShiftRegister1.z, base##ShiftRegister1a.z )

void CLightingPreviewThread::AddLowresResultToHires( CSOAContainer& lores, CSOAContainer& hires )
{
	// we will bilaterally upsample lowres and add it to hires. This is coded for the specific case of a 4x4 downsamping
	fltx4 fl4NormalFactorScale = ReplicateX4( 4.0f );
	fltx4 fl4NormalBias = ReplicateX4( 0.0f ); //1.01 );		// prevent 0.
	fltx4 fl4DistanceScale = ReplicateX4( 1.0f / 36.0f );
	Assert( lores.NumRows() == m_GBufferLowRes.NumRows() );
	Assert( lores.NumCols() == m_GBufferLowRes.NumCols() );
	for ( int y = 0; y < hires.NumRows(); y++ )
	{
		int ysrc0 = min( ( y >> 2 ), lores.NumRows() -1 );
		int ysrc1 = min( ysrc0 + 1, lores.NumRows() - 1 );
		int nIterations = hires.NumQuadsPerRow();
		int numFetches = lores.NumQuadsPerRow();

		FourVectors* pSrc0 = lores.RowPtr<FourVectors>( RSLT_BUFFER_RSLT_RGB, ysrc0 );
		FourVectors* pSrc1 = lores.RowPtr<FourVectors>( RSLT_BUFFER_RSLT_RGB, ysrc1 );
		FourVectors rsltShiftRegister0 = *pSrc0++;
		FourVectors rsltShiftRegister0a;
		FourVectors rsltShiftRegister1 = *pSrc1++;
		FourVectors rsltShiftRegister1a;

		FourVectors* pSrcNormal0 = m_GBufferLowRes.RowPtr<FourVectors>( GBUFFER_ATTR_NORMAL, ysrc0 );
		FourVectors* pSrcNormal1 = m_GBufferLowRes.RowPtr<FourVectors>( GBUFFER_ATTR_NORMAL, ysrc1 );
		FourVectors normShiftRegister0 = *pSrcNormal0++;
		FourVectors normShiftRegister0a;
		FourVectors normShiftRegister1 = *pSrcNormal1++;
		FourVectors normShiftRegister1a;

		FourVectors* pSrcPos0 = m_GBufferLowRes.RowPtr<FourVectors>( GBUFFER_ATTR_POSITION, ysrc0 );
		FourVectors* pSrcPos1 = m_GBufferLowRes.RowPtr<FourVectors>( GBUFFER_ATTR_POSITION, ysrc1 );
		FourVectors posShiftRegister0 = *pSrcPos0++;
		FourVectors posShiftRegister0a;
		FourVectors posShiftRegister1 = *pSrcPos1++;
		FourVectors posShiftRegister1a;

		FourVectors* pDest = hires.RowPtr<FourVectors>( RSLT_BUFFER_RSLT_RGB, y );
		FourVectors* pDestNormal = m_GBuffer.RowPtr<FourVectors>( GBUFFER_ATTR_NORMAL, y );
		FourVectors* pDestPos = m_GBuffer.RowPtr<FourVectors>( GBUFFER_ATTR_POSITION, y );

		numFetches--;

		for ( int x = 0; x < nIterations; x++ )
		{
			if ( ( x & 3 ) == 0 && numFetches )		  // need to fetch new data every 4 outputs
			{
				numFetches--;
				rsltShiftRegister0a = *( pSrc0++ );
				rsltShiftRegister1a = *( pSrc1++ );

				normShiftRegister0a = *( pSrcNormal0++ );
				normShiftRegister1a = *( pSrcNormal1++ );

				posShiftRegister0a = *( pSrcPos0++ );
				posShiftRegister1a = *( pSrcPos1++ );
			}

			FourVectors rsltAAAA, rsltBBBB, rsltEEEE, rsltFFFF;
			GRAB4PIXELS( rslt );

			FourVectors normAAAA, normBBBB, normEEEE, normFFFF;
			GRAB4PIXELS( norm );

			FourVectors posAAAA, posBBBB, posEEEE, posFFFF;
			GRAB4PIXELS( pos );

			// we should now be ready to filter. We will take the 4 pixels we have, and produce 4 output pixels
			const FourVectors& dNorm = *pDestNormal++;
			fltx4 fl4ADot = MaxSIMD( Four_Epsilons, MulSIMD( fl4NormalFactorScale, AddSIMD( fl4NormalBias, dNorm * normAAAA ) ) );
			fltx4 fl4BDot = MaxSIMD( Four_Epsilons, MulSIMD( fl4NormalFactorScale, AddSIMD( fl4NormalBias, dNorm * normBBBB ) ) );
			fltx4 fl4EDot = MaxSIMD( Four_Epsilons, MulSIMD( fl4NormalFactorScale, AddSIMD( fl4NormalBias, dNorm * normEEEE ) ) );
			fltx4 fl4FDot = MaxSIMD( Four_Epsilons, MulSIMD( fl4NormalFactorScale, AddSIMD( fl4NormalBias, dNorm * normFFFF ) ) );

			const FourVectors& fl4Pos = *pDestPos++;
			FourVectors v4Delta = posAAAA;
			v4Delta -= fl4Pos;
			fltx4 fl4WA = MulSIMD( fl4ADot, ReciprocalSIMD( AddSIMD( Four_Ones, MulSIMD( v4Delta.length(), fl4DistanceScale ) ) ) );

			v4Delta = posBBBB;
			v4Delta -= fl4Pos;
			fltx4 fl4WB = MulSIMD( fl4BDot, ReciprocalSIMD( AddSIMD( Four_Ones, MulSIMD( v4Delta.length(), fl4DistanceScale ) ) ) );

			v4Delta = posEEEE;
			v4Delta -= fl4Pos;
			fltx4 fl4WE = MulSIMD( fl4EDot, ReciprocalSIMD( AddSIMD( Four_Ones, MulSIMD( v4Delta.length(), fl4DistanceScale ) ) ) );

			v4Delta = posFFFF;
			v4Delta -= fl4Pos;
			fltx4 fl4WF = MulSIMD( fl4FDot, ReciprocalSIMD( AddSIMD( Four_Ones, MulSIMD( v4Delta.length(), fl4DistanceScale ) ) ) );

			fltx4 fl4OOSumWeights = ReciprocalSIMD( AddSIMD( AddSIMD( fl4WA, fl4WB ), AddSIMD( fl4WE, fl4WF ) ) );

			// now, calculate the output color
			FourVectors out = rsltAAAA;
			out *= fl4WA;

			FourVectors out1 = rsltBBBB;
			out1 *= fl4WB;
			out += out1;

			out1 = rsltEEEE;
			out1 *= fl4WE;
			out += out1;

			out1 = rsltFFFF;
			out1 *= fl4WF;
			out += out1;

			out *= fl4OOSumWeights;

			*pDest++ += out;
		}
	}
}

void CLightingPreviewThread::SendResult()
{
	if ( m_GBuffer.NumRows() && m_GBuffer.NumCols() )
	{
		//FPExceptionEnabler e;
		//		Warning("send\n");
		CSOAContainer rsltBuffer;
		rsltBuffer.SetAttributeType( RSLT_BUFFER_RSLT_RGB, ATTRDATATYPE_4V );
		rsltBuffer.AllocateData( m_GBuffer.NumCols(), m_GBuffer.NumRows() );
		rsltBuffer.FillAttr( RSLT_BUFFER_RSLT_RGB, EstimatedUnshotAmbient() );

		bool bDidLoRes = false;
		for ( CLightingPreviewLightDescription* l = m_LightList.Head(); l; l = l->m_pNext )
			if ( l->m_bLowRes )
				bDidLoRes = true;

		CSOAContainer rsltBuffer1;
		if ( bDidLoRes )
		{
			rsltBuffer1.SetAttributeType( RSLT_BUFFER_RSLT_RGB, ATTRDATATYPE_4V );
			rsltBuffer1.AllocateData( m_GBufferLowRes.NumCols(), m_GBufferLowRes.NumRows() );
			rsltBuffer1.FillAttr( RSLT_BUFFER_RSLT_RGB, vec3_origin );
		}

		{
			CJobSetN<32> jobs;
			for ( int i = 0; i < 32; i++ )
				jobs += s_pThreadPool->QueueCall( this, &CLightingPreviewThread::AccumulateOuput, 1 << i, &rsltBuffer, &rsltBuffer1 );
			jobs.WaitForFinish( s_pThreadPool );
		}

		if ( bDidLoRes )
			AddLowresResultToHires( rsltBuffer1, rsltBuffer );

		// now, multiply by albedo
		rsltBuffer.MulAttr( m_GBuffer, GBUFFER_ATTR_ALBEDO, RSLT_BUFFER_RSLT_RGB );

		SendResultRendering( rsltBuffer );
		m_fLastSendTime = Plat_FloatTime();
		m_bResultChangedSinceLastSend = false;
	}
}

int InsideOut( int nTotal, int nCounter )
{
	int b = 0;
	for ( int m = nTotal, k = 1; k < nTotal; k <<= 1 )
	{
		if ( nCounter << 1 >= m )
		{
			b += k;
			nCounter -= ( m + 1 ) >> 1;
			m >>= 1;
		}
		else
		{
			m = ( m + 1 ) >> 1;
		}
	}
	Assert( b >= 0 && b < nTotal );
	return b;
}

static const FourVectors zero_vector( vec3_origin, vec3_origin, vec3_origin, vec3_origin );
void CLightingPreviewThread::CalculateForLightTask( int nLineStart, int nLineEnd, CLightingPreviewLightDescription* l, float* fContributionOut, CIncrementalLightInfo* pLInfo )
{
	//FPExceptionEnabler e;

	FourVectors total_light = zero_vector;

	CIncrementalLightInfo* l_info = l->m_pIncrementalInfo;
	CSOAContainer& rslt = l_info->m_CalculatedContribution;
	// figure out what lines to do
	fltx4 ThresholdBrightness = ReplicateX4( 0.1f / 1024.0f );
	FourVectors LastLinesTotalLight = zero_vector;

	// calculate jitter stuff
	fltx4 fl4RandRange = ReplicateX4( l->m_flJitterAmount );
	bool bJitter = l->m_flJitterAmount > 0.0f;
	int nCtx = GetSIMDRandContext();
	CSOAContainer* pGB = l->m_bLowRes ? &m_GBufferLowRes : &m_GBuffer;

	for ( int idx = nLineStart; idx <= nLineEnd; idx++ )
	{
		int y = InsideOut( rslt.NumRows(), idx );
		FourVectors ThisLinesTotalLight = zero_vector;
		FourVectors* pDataOut = rslt.RowPtr<FourVectors>( RSLT_ATTR_DIFFUSE_RGB, y );
		FourVectors* pAlbedo = pGB->RowPtr<FourVectors>( GBUFFER_ATTR_ALBEDO, y );
		FourVectors* pPos = pGB->RowPtr<FourVectors>( GBUFFER_ATTR_POSITION, y );
		FourVectors* pNormal = pGB->RowPtr<FourVectors>( GBUFFER_ATTR_NORMAL, y );
		for ( int x = 0; x < rslt.NumQuadsPerRow(); x++ )
		{
			// shadow check
			FourVectors pos = *pPos++;
			FourVectors normal = *pNormal++;
			Assert( !isnan( pos.x ) );
			Assert( !isnan( pos.y ) );
			Assert( !isnan( pos.z ) );
			Assert( !isnan( normal.x ) );
			Assert( !isnan( normal.y ) );
			Assert( !isnan( normal.z ) );

			FourVectors l_add = zero_vector;
			l->ComputeLightAtPoints( pos, normal, l_add, false );
			Assert( !isnan( l_add.x ) );
			Assert( !isnan( l_add.y ) );
			Assert( !isnan( l_add.z ) );
			fltx4 v_or = AndSIMD( CmpEqSIMD( l_add.x, _mm_setzero_ps() ), AndSIMD( CmpEqSIMD( l_add.y, _mm_setzero_ps() ), CmpEqSIMD( l_add.z, _mm_setzero_ps() ) ) );
			if ( TestSignSIMD( v_or ) != 0xF )
			{
				FourVectors lpos;
				lpos.DuplicateVector( l->m_Position );
				// jitter light position
				if ( bJitter )
				{
					lpos.x = AddSIMD( lpos.x, MulSIMD( fl4RandRange, SubSIMD( MulSIMD( Four_Twos, RandSIMD( nCtx ) ), Four_Ones ) ) );
					lpos.y = AddSIMD( lpos.y, MulSIMD( fl4RandRange, SubSIMD( MulSIMD( Four_Twos, RandSIMD( nCtx ) ), Four_Ones ) ) );
					lpos.z = AddSIMD( lpos.z, MulSIMD( fl4RandRange, SubSIMD( MulSIMD( Four_Twos, RandSIMD( nCtx ) ), Four_Ones ) ) );
				}
				Assert( !isnan( lpos.x ) );
				Assert( !isnan( lpos.y ) );
				Assert( !isnan( lpos.z ) );

				FourRays myray;
				myray.direction = lpos;
				myray.direction -= pos;
				fltx4 len = myray.direction.length();
				myray.direction *= ReciprocalSIMD( len );
				Assert( !isnan( myray.direction.x ) );
				Assert( !isnan( myray.direction.y ) );
				Assert( !isnan( myray.direction.z ) );

				// slide towards light to avoid self-intersection
				myray.origin = myray.direction;
				myray.origin *= 0.02f;
				myray.origin += pos;
				Assert( !isnan( myray.origin.x ) );
				Assert( !isnan( myray.origin.y ) );
				Assert( !isnan( myray.origin.z ) );

				RayTracingResult r_rslt;
				m_pRtEnv->Trace4Rays( myray, Four_Zeros, ReplicateX4( 1.0e9f ), &r_rslt, -1, nullptr );

				fltx4 mask = _mm_castsi128_ps( _mm_andnot_si128(
					_mm_cmplt_epi32( _mm_load_si128( reinterpret_cast<__m128i*>( r_rslt.HitIds ) ), _mm_setzero_si128() ),
					_mm_castps_si128( _mm_cmplt_ps( r_rslt.HitDistance, len ) ) ) );
				l_add.x = AndNotSIMD( mask, l_add.x );
				l_add.y = AndNotSIMD( mask, l_add.y );
				l_add.z = AndNotSIMD( mask, l_add.z );
				Assert( !isnan( l_add.x ) );
				Assert( !isnan( l_add.y ) );
				Assert( !isnan( l_add.z ) );

				*pDataOut = l_add;
				l_add *= *pAlbedo;
				Assert( !isnan( l_add.x ) );
				Assert( !isnan( l_add.y ) );
				Assert( !isnan( l_add.z ) );
				// now, supress brightness < threshold so as to not falsely think
				// far away lights are interesting
				l_add.x = AndSIMD( l_add.x, CmpGtSIMD( l_add.x, ThresholdBrightness ) );
				l_add.y = AndSIMD( l_add.y, CmpGtSIMD( l_add.y, ThresholdBrightness ) );
				l_add.z = AndSIMD( l_add.z, CmpGtSIMD( l_add.z, ThresholdBrightness ) );
				Assert( !isnan( l_add.x ) );
				Assert( !isnan( l_add.y ) );
				Assert( !isnan( l_add.z ) );
				total_light += l_add;
				Assert( !isnan( total_light.x ) );
				Assert( !isnan( total_light.y ) );
				Assert( !isnan( total_light.z ) );
			}
			else
				*pDataOut = zero_vector;
			Assert( !isnan( pDataOut->x ) );
			Assert( !isnan( pDataOut->y ) );
			Assert( !isnan( pDataOut->z ) );
			pDataOut++;
			pAlbedo++;
		}
		pLInfo->m_nCalculationLevel[y] = 1;
		pLInfo->m_nMaxCalculatedLine = max( y, pLInfo->m_nMaxCalculatedLine );
		pLInfo->m_nFirstCalculatedLine = min( y, pLInfo->m_nFirstCalculatedLine );
	}
	ReleaseSIMDRandContext( nCtx );
	Assert( !isnan( total_light.x ) );
	Assert( !isnan( total_light.y ) );
	Assert( !isnan( total_light.z ) );
	if ( IsAllZeros( total_light.x ) && IsAllZeros( total_light.y ) && IsAllZeros( total_light.z ) )
		*fContributionOut = 0.f;
	else
	{
		fltx4 lmag = total_light.length();
		*fContributionOut = SubFloat( lmag, 0 ) + SubFloat( lmag, 1 ) + SubFloat( lmag, 2 ) + SubFloat( lmag, 3 );
	}
}

#define N_FAKE_LIGHTS_FOR_INDIRECT 50

void CLightingPreviewThread::CalculateForLight( CLightingPreviewLightDescription* l )
{
	if ( !l->m_bDidIndirect )
	{
		// create a bunch of pseudo lights for this light
		float lrad = l->DistanceAtWhichBrightnessIsLessThan( 1.0f / 500.0f );
		RayTracingSingleResult rslts[N_FAKE_LIGHTS_FOR_INDIRECT];
		Vector rayDirs[N_FAKE_LIGHTS_FOR_INDIRECT];
		DirectionalSampler_t sampler;
		RayStream myStream;
		Vector rayStart = l->m_Position;
		for ( int i = 0; i < N_FAKE_LIGHTS_FOR_INDIRECT; i++ )
		{
			rayDirs[i] = sampler.NextValue();
			m_pRtEnv->AddToRayStream( myStream, rayStart, rayStart + lrad * rayDirs[i], rslts + i );
		}
		m_pRtEnv->FinishRayStream( myStream );
		// now, we have a bunch of raytracing results
		for ( int i = 0; i < N_FAKE_LIGHTS_FOR_INDIRECT; i++ )
		{
			if ( rslts[i].HitID != -1 )						// hit something
			{
				Vector vecHitPos = rayStart + rslts[i].HitDistance * rayDirs[i];
				FourVectors v4Pnt;
				v4Pnt.DuplicateVector( vecHitPos );
				FourVectors v4Normal;
				v4Normal.DuplicateVector( rslts[i].surface_normal );
				FourVectors v4Color;
				l->ComputeLightAtPoints( v4Pnt, v4Normal, v4Color );
				Vector vecColorToShoot = v4Color.Vec( 0 ) * 0.25f / N_FAKE_LIGHTS_FOR_INDIRECT;

				if ( vecColorToShoot.Length() > 1.0f / 255.0f )
				{
					CLightingPreviewLightDescription* pNew = new CLightingPreviewLightDescription;
					pNew->Init( 0xf0000000 );
					pNew->m_Position = vecHitPos + rslts[i].surface_normal * 2;
					pNew->m_Type = MATERIAL_LIGHT_SPOT;
					pNew->m_Color = vecColorToShoot;
					pNew->m_Direction = rslts[i].surface_normal;
					pNew->m_Theta = 0;
					pNew->m_Phi = M_PI;
					pNew->m_Falloff = 5.0f;
					pNew->m_Range = 0.0f;
					pNew->m_Attenuation0 = 0;
					pNew->m_Attenuation1 = 0;
					pNew->m_Attenuation2 = 1;
					pNew->m_bDidIndirect = true;
					pNew->m_bLowRes = l->m_bLowRes;
					pNew->RecalculateDerivedValues();
					l->m_TempChildren.AddToTail( pNew );
				}
			}
		}
		l->m_bDidIndirect = true;
	}

	CIncrementalLightInfo* l_info = l->m_pIncrementalInfo;
	CSOAContainer* pGB = &m_GBuffer;
	if ( l->m_bLowRes )
		pGB = &m_GBufferLowRes;

	l_info->SetContributionSize( pGB->NumCols(), pGB->NumRows() );

	// figure out which lines need to be calculated
	int nStartIteration = l_info->m_nNumLinesCalculated;
	int nEndIteration = min( l_info->m_nNumLinesCalculated + NUMBER_OF_LINES_TO_CALCULATE_PER_STEP, pGB->NumRows() - 1 );

	float total_light;
	CalculateForLightTask( nStartIteration, nEndIteration, l, &total_light, l_info );
	l_info->m_flLastContribution = total_light;
	l_info->m_fTotalContribution += total_light;

	// throw away light array if no contribution ?????
	if ( l_info->m_fTotalContribution == 0.0f )
		l_info->m_CalculatedContribution.Purge();
	else
	{
		l_info->m_nMostRecentNonZeroContributionTimeStamp = m_nContributionCounter;
	}
	l_info->m_nNumLinesCalculated = nEndIteration + 1;
	if ( nEndIteration == pGB->NumRows() - 1 )
		l_info->m_eIncrState = INCR_STATE_HAVE_FULL_RESULTS;
	else
		l_info->m_eIncrState = INCR_STATE_PARTIAL_RESULTS;
}

#ifndef __clang__
#pragma optimize( "", on )
#endif

void CLightingPreviewThread::SendResultRendering( CSOAContainer& rsltBuffer )
{
	Bitmap_t* ret_bm = new Bitmap_t;
	ret_bm->Init( rsltBuffer.NumCols(), rsltBuffer.NumRows(), IMAGE_FORMAT_RGBA8888 );
	// lets copy into the output bitmap
	for ( int y = 0; y < ret_bm->Height(); y++ )
	{
		float const* pRGBData = rsltBuffer.RowPtr<float>( RSLT_BUFFER_RSLT_RGB, y );
#ifdef _DEBUG
		OutputDebugStringA( CFmtStr( "%g %g %g\n", pRGBData[0], pRGBData[4], pRGBData[8] ) );
#endif
		for ( int x = 0; x < ret_bm->Width(); x++ )
		{
			Vector color( pRGBData[0], pRGBData[4], pRGBData[8] );
			unsigned char* pPixel = ret_bm->GetPixel( x, y );
			pPixel[0] = (uint8)min( 255.f, 255.0f * pow( color.z, 1 / 2.2f ) );
			pPixel[1] = (uint8)min( 255.f, 255.0f * pow( color.y, 1 / 2.2f ) );
			pPixel[2] = (uint8)min( 255.f, 255.0f * pow( color.x, 1 / 2.2f ) );
			pPixel[3] = 0;
			pRGBData++;
			if ( ( x & 3 ) == 3 )
				pRGBData += 8;
		}
	}
	MessageFromLPreview ret_msg( LPREVIEW_MSG_DISPLAY_RESULT );
	ret_msg.m_pBitmapToDisplay = ret_bm;
	ret_msg.m_nBitmapGenerationCounter = m_nBitmapGenerationCounter;
	g_LPreviewToHammerMsgQueue.QueueMessage( ret_msg );
}



// master side of lighting preview

unsigned LightingPreviewThreadFN( void* )
{
	CLightingPreviewThread LPreviewObject;
	ThreadSetPriority( -2 );								// low
	s_pThreadPool = CreateThreadPool();

	const CPUInformation* pCPUInfo = GetCPUInformation();
	ThreadPoolStartParams_t startParams;
	s_nNumThreads = startParams.nThreads = Clamp( pCPUInfo->m_nLogicalProcessors - 1, 1, TP_MAX_POOL_THREADS );
	startParams.nStackSize = 4 * 1024 * 1024;
	startParams.fDistribute = TRS_TRUE;
	startParams.iThreadPriority = -2;
	startParams.bUseAffinityTable = true;
	for ( int i = 0; i < s_nNumThreads; i++ )
		startParams.iAffinityTable[i] = 1 << ( i + 1 );
	s_pThreadPool->Start( startParams, "hammer_lighting" );
	CSOAContainer::SetThreadPool( s_pThreadPool );

	LPreviewObject.Run();

	CSOAContainer::SetThreadPool( nullptr );
	s_pThreadPool->Stop();
	DestroyThreadPool( s_pThreadPool );
	return 0;
}


void HandleLightingPreview()
{
	if ( GetMainWnd()->m_pLightingPreviewOutputWindow && !GetMainWnd()->m_bLightingPreviewOutputWindowShowing )
	{
		delete GetMainWnd()->m_pLightingPreviewOutputWindow;
		GetMainWnd()->m_pLightingPreviewOutputWindow = NULL;
	}

	// called during main loop
	while ( g_LPreviewToHammerMsgQueue.MessageWaiting() )
	{
		MessageFromLPreview msg;
		g_LPreviewToHammerMsgQueue.WaitMessage( &msg );
		switch ( msg.m_MsgType )
		{
			case LPREVIEW_MSG_DISPLAY_RESULT:
			{
				if ( !CMapDoc::GetActiveMapDoc() || !CMapDoc::GetActiveMapDoc()->HasAnyLPreview() )
					break;
				if ( g_pLPreviewOutputBitmap )
					delete g_pLPreviewOutputBitmap;
				g_pLPreviewOutputBitmap = NULL;
				if ( msg.m_nBitmapGenerationCounter == g_nBitmapGenerationCounter )
				{
					g_pLPreviewOutputBitmap = msg.m_pBitmapToDisplay;
					if ( g_pLPreviewOutputBitmap && g_pLPreviewOutputBitmap->Width() > 10 )
					{
						SignalUpdate( EVTYPE_BITMAP_RECEIVED_FROM_LPREVIEW );
						CLightingPreviewResultsWindow *w=GetMainWnd()->m_pLightingPreviewOutputWindow;
						if ( !GetMainWnd()->m_bLightingPreviewOutputWindowShowing )
						{
							w = new CLightingPreviewResultsWindow;
							GetMainWnd()->m_pLightingPreviewOutputWindow = w;
							w->Create( GetMainWnd() );
							GetMainWnd()->m_bLightingPreviewOutputWindowShowing = true;
						}
						if ( !w->IsWindowVisible() )
							w->ShowWindow( SW_SHOW );
						RECT existing_rect;
						w->GetClientRect( &existing_rect );
						if (existing_rect.right != g_pLPreviewOutputBitmap->Width() - 1 ||
							existing_rect.bottom != g_pLPreviewOutputBitmap->Height() - 1 )
						{
							CRect myRect;
							myRect.top = 0;
							myRect.left = 0;
							myRect.right = g_pLPreviewOutputBitmap->Width() - 1;
							myRect.bottom = g_pLPreviewOutputBitmap->Height() - 1;
							w->CalcWindowRect( &myRect );
							w->SetWindowPos(
								NULL, 0, 0,
								myRect.Width(), myRect.Height(),
								SWP_NOMOVE | SWP_NOZORDER );
						}

						w->Invalidate( false );
						w->UpdateWindow();
					}
				}
				else
					delete msg.m_pBitmapToDisplay;			// its old
			break;
			}
		}
	}
}