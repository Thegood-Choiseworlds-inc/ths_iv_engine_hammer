//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Implementation of IEditorTexture interface for materials.
//
// $NoKeywords: $
//===========================================================================//

#ifndef MATERIAL_H
#define MATERIAL_H
#pragma once


#include "ieditortexture.h"
#include "materialsystem/imaterial.h"

class IMaterial;
class IMaterialSystem;
class IMaterialSystemHardwareConfig;
struct MaterialSystem_Config_t;
struct MaterialCacheEntry_t;


#define INCLUDE_MODEL_MATERIALS		0x01
#define INCLUDE_WORLD_MATERIALS		0x02
#define INCLUDE_ALL_MATERIALS		0xFFFFFFFF


//-----------------------------------------------------------------------------
// Inherit from this to enumerate materials
//-----------------------------------------------------------------------------
class IMaterialEnumerator
{
public:
	virtual bool EnumMaterial( const char* pMaterialName, int nContext ) = 0;
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CMaterial final : public IEditorTexture
{
public:
	static bool Initialize( HWND hwnd );
	static void ShutDown();
	static void EnumerateMaterials( IMaterialEnumerator* pEnum, const char* szRoot, int nContext, int nFlags = INCLUDE_ALL_MATERIALS );
	static CMaterial* CreateMaterial( const char* pszMaterialName, bool bLoadImmediately, bool* pFound = 0 );

	~CMaterial() override;

	void Draw( CDC* pDC, const RECT& rect, int iFontHeight, int iIconHeight, const DrawTexData_t& DrawTexData ) override;

	void FreeData();

	const char* GetName() const override { return m_szName; }
	const char* GetFileName() const override;
	int GetShortName( char* pszName ) const override;
	int GetKeywords( char* pszKeywords) const override;

	// Image dimensions
	int GetImageWidth() const override;
	int GetImageHeight() const override;
	int GetWidth() const override;
	int GetHeight() const override;
	float GetDecalScale() const override;

	bool HasData() const override { return m_nWidth != 0 && m_nHeight != 0; }
	bool IsDummy() const override { return false; }

	bool Load() override;
	void Reload( bool bFullReload ) override;
	bool IsLoaded() const override { return m_bLoaded; }

	bool IsWater() const override;

	IMaterial* GetMaterial( bool bForceLoad = true ) override;

	bool HasValidColorInformation() const override;
	uint ClosestColorDist( const color24& clr ) const override;

	void TryLoadColorData();

protected:
	// Used to draw the bitmap for the texture browser
	void DrawBitmap( CDC* pDC, const RECT& srcRect, const RECT& dstRect );
	void DrawBrowserIcons( CDC* pDC, RECT& dstRect, bool detectErrors );
	void DrawIcon( CDC* pDC, CMaterial* pIcon, RECT& dstRect );

	static bool ShouldSkipMaterial( const char* pszName, int nFlags );

	// Finds all .VMT files in a particular directory
	static bool LoadMaterialsInDirectory( char const* pDirectoryName, IMaterialEnumerator* pEnum, int nContext, int nFlags );

	// Discovers all .VMT files lying under a particular directory recursively
	static bool InitDirectoryRecursive( char const* pDirectoryName, IMaterialEnumerator* pEnum, int nContext, int nFlags );

	CMaterial();
	bool LoadMaterialHeader( IMaterial* material );
	bool LoadMaterialImage();
	void LoadColorInfo( byte* data, uint dataSize, uint hash, uint hashSize, color24 baseColor );

	static bool IsIgnoredMaterial( const char* pName );

	// Will actually load the material bits
	// We don't want to load them all at once because it takes way too long
	bool LoadMaterial();

	char m_szName[MAX_PATH];
	char m_szKeywords[MAX_PATH];

	int m_nWidth;				// Texture width in texels.
	int m_nHeight;				// Texture height in texels.
	bool m_TranslucentBaseTexture;
	bool m_bLoaded;				// We don't load these immediately; only when needed..

	void* m_pData;				// Loaded texel data (NULL if not loaded).

	ThreeState_t m_bIsWater;

	IMaterial* m_pMaterial;

	Vector m_baseColor;
	color24 m_computedColors[4];
	CInterlockedInt m_computedColorCount;

	// color comp cache
	mutable color24 m_computedColorValue;
	mutable uint m_computedColorDist;

	friend class CMaterialImageCache;
};


typedef CMaterial *CMaterialPtr;


//-----------------------------------------------------------------------------
// returns the material system interface + config
//-----------------------------------------------------------------------------

inline IMaterialSystem* MaterialSystemInterface()
{
	return materials;
}

inline MaterialSystem_Config_t& MaterialSystemConfig()
{
	extern MaterialSystem_Config_t g_materialSystemConfig;
	return g_materialSystemConfig;
}

inline IMaterialSystemHardwareConfig* MaterialSystemHardwareConfig()
{
	extern IMaterialSystemHardwareConfig* g_pMaterialSystemHardwareConfig;
	return g_pMaterialSystemHardwareConfig;
}

//--------------------------------------------------------------------------------
// call AllocateLightingPreviewtextures to make sure necessary rts are allocated
//--------------------------------------------------------------------------------
void AllocateLightingPreviewtextures();

#endif // MATERIAL_H