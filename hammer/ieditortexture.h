//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ====
//
// Purpose: Defines the interface a given texture for the 3D renderer. Current
//			implementations are for world textures (WADTexture.cpp) and sprite
//			textures (Texture.cpp).
//
//=============================================================================

#ifndef IEDITORTEXTURE_H
#define IEDITORTEXTURE_H
#pragma once


#include "utlvector.h"


class CDC;
class IMaterial;


//
// Set your texture ID to this in your implementation's constructor.
//
#define TEXTURE_ID_NONE	-1


//
// Flags for DrawTexData_t.
//
#define drawCaption			0x01
#define	drawResizeAlways	0x02
#define	drawIcons			0x04
#define	drawErrors			0x08
#define	drawUsageCount		0x10


struct DrawTexData_t
{
	int nFlags;
	int nUsageCount;
};


class IEditorTexture
{
public:

	virtual ~IEditorTexture() = default;

	//
	// dvs: remove one of these
	//
	virtual int GetImageWidth() const = 0;
	virtual int GetImageHeight() const = 0;

	virtual int GetWidth() const = 0;
	virtual int GetHeight() const = 0;

	virtual float GetDecalScale() const = 0;

	//
	// dvs: Try to remove as many of these as possible:
	//
	virtual const char* GetName() const = 0;
	virtual int GetShortName( char* szShortName ) const = 0;
	virtual int GetKeywords( char* szKeywords ) const = 0;
	virtual void Draw( CDC* pDC, const RECT& rect, int iFontHeight, int iIconHeight, const DrawTexData_t& DrawTexData ) = 0;
	virtual bool HasData() const = 0;
	virtual bool Load() = 0; // ensure that texture is loaded. could this be done internally?
	virtual void Reload( bool bFullReload ) = 0; // The texture changed. If bFullReload is true, then the material system reloads it too.
	virtual bool IsLoaded() const = 0;
	virtual const char* GetFileName() const = 0;

	virtual bool IsWater() const = 0;

	//-----------------------------------------------------------------------------
	// Purpose: Returns whether this texture is a dummy texture or not. Dummy textures
	//			serve as placeholders for textures that were found in the map, but
	//			not in the WAD (or the materials tree). The dummy texture enables us
	//			to bind the texture, find it by name, etc.
	//-----------------------------------------------------------------------------
	virtual bool IsDummy() const = 0; // dvs: perhaps not the best name?

	//-----------------------------------------------------------------------------
	// Returns the material system material associated with a texture
	//-----------------------------------------------------------------------------

	virtual IMaterial* GetMaterial( bool bForceLoad = true ) = 0;

	virtual bool HasValidColorInformation() const = 0;
	virtual uint ClosestColorDist( const color24& clr ) const = 0;
};

typedef CUtlVector<IEditorTexture*> EditorTextureList_t;

struct UsedTexture_t
{
	IEditorTexture* pTex;
	int nUsageCount;
};

class CUsedTextureList : public CUtlVector<UsedTexture_t>
{
public:
	inline int Find( IEditorTexture* pTex )
	{
		return FindMatch( [pTex]( const UsedTexture_t& tex ) { return tex.pTex == pTex; } );
	}
};


#endif // IEDITORTEXTURE_H