//===== Copyright © 2005-2013, Valve Corporation, All rights reserved. ======//
//
// Purpose: A higher level link library for general use in the game and tools.
//
//===========================================================================//


#ifndef MDLUTILS_2_H
#define MDLUTILS_2_H

#if defined( _WIN32 )
#pragma once
#endif

#include "tier3/mdlutils.h"

struct MDLData_t
{
	MDLData_t();

	CMDL		m_MDL;
	matrix3x4_t	m_MDLToWorld;
	bool		m_bRequestBoneMergeTakeover;
};

class CMergedMDL
{
public:
	// constructor, destructor
	CMergedMDL();
	virtual ~CMergedMDL();

	// Sets the current mdl
	void SetMDL( MDLHandle_t handle, void *pProxyData = NULL );
	void SetMDL( const char *pMDLName, void *pProxyData = NULL );
	CMDL *GetMDL() { return &m_RootMDL.m_MDL; }

	// Sets the current sequence
	void SetSequence( int nSequence );

	// Set the pose parameters
	void SetPoseParameters( const float *pPoseParameters, int nCount );

	// Set the overlay sequence layers
	void SetSequenceLayers( const MDLSquenceLayer_t *pSequenceLayers, int nCount );

	void SetSkin( int nSkin );

	// Bounds.
	bool GetBoundingBox( Vector &vecBoundsMin, Vector &vecBoundsMax );

	void SetModelAnglesAndPosition( const QAngle &angRot, const Vector &vecPos );

	// Attached models.
	void	SetMergeMDL( MDLHandle_t handle, void *pProxyData = NULL, bool bRequestBonemergeTakeover = false );
	MDLHandle_t SetMergeMDL( const char *pMDLName, void *pProxyData = NULL, bool bRequestBonemergeTakeover = false );
	int		GetMergeMDLIndex( MDLHandle_t handle );
	CMDL	*GetMergeMDL(MDLHandle_t handle );
	void	ClearMergeMDLs( void );

	void Draw();

	void SetupBonesForAttachmentQueries( void );

	// from IRenderToRTHelperObject
	void Draw( const matrix3x4_t &rootToWorld );
	bool GetBoundingSphere( Vector &vecCenter, float &flRadius );

protected:
	virtual void OnPostSetUpBonesPreDraw() {}
	virtual void OnModelDrawPassStart( int iPass, CStudioHdr *pStudioHdr, int &nFlags ) {}
	virtual void OnModelDrawPassFinished( int iPass, CStudioHdr *pStudioHdr, int &nFlags ) {}

protected:

	MDLData_t				m_RootMDL;
	CUtlVector<MDLData_t>	m_aMergeMDLs;

	float	m_PoseParameters[ MAXSTUDIOPOSEPARAM ];

private:

	static const int MAX_SEQUENCE_LAYERS = 8;
	int					m_nNumSequenceLayers;
	MDLSquenceLayer_t	m_SequenceLayers[ MAX_SEQUENCE_LAYERS ];
};

//-----------------------------------------------------------------------------
// Returns the bounding box for the model
//-----------------------------------------------------------------------------
void GetMDLBoundingBox( Vector *pMins, Vector *pMaxs, MDLHandle_t h, int nSequence );

//-----------------------------------------------------------------------------
// Returns the radius of the model as measured from the origin
//-----------------------------------------------------------------------------
float GetMDLRadius( MDLHandle_t h, int nSequence );

//-----------------------------------------------------------------------------
// Returns a more accurate bounding sphere
//-----------------------------------------------------------------------------
void GetMDLBoundingSphere( Vector *pVecCenter, float *pRadius, MDLHandle_t h, int nSequence );

//-----------------------------------------------------------------------------
// Determines which pose parameters are used by the specified sequence
//-----------------------------------------------------------------------------
void FindSequencePoseParameters( CStudioHdr &hdr, int nSequence, bool *pPoseParameters, int nCount );


#endif // MDLUTILS_2_H