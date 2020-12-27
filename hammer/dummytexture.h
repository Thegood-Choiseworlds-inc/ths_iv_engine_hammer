//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of IEditorTexture interface for placeholder textures.
//			Placeholder textures are used for textures that are referenced in
//			the map file but not found in storage.
//
//=============================================================================//

#ifndef DUMMYTEXTURE_H
#define DUMMYTEXTURE_H
#ifdef _WIN32
#pragma once
#endif


#include "ieditortexture.h"

class CDummyTexture final : public IEditorTexture
{
public:
	CDummyTexture( const char* pszName );
	~CDummyTexture() override = default;

	static void DestroyDummyMaterial();

	const char* GetName() const override { return m_szName; }
	const char* GetFileName() const override { return m_szName; }
	int GetShortName( char* pszName ) const override;
	int GetKeywords( char* pszKeywords ) const override;

	void Draw( CDC* pDC, const RECT& rect, int iFontHeight, int iIconHeight, const DrawTexData_t& DrawTexData ) override;

	bool IsDummy() const override { return true; }

	int GetImageWidth() const override { return 256; }
	int GetImageHeight() const override { return 256; }
	int GetWidth() const override { return 256; }
	int GetHeight() const override { return 256; }
	float GetDecalScale() const override { return 1.0f; }

	bool HasData() const override { return true; }

	bool Load() override { return true; }
	void Reload( bool bFullReload ) override {}
	bool IsLoaded() const override { return true; }

	bool IsWater() const override { return false; }

	IMaterial* GetMaterial( bool bForceLoad = true ) override { return errorMaterial; }

	bool HasValidColorInformation() const override { return false; }
	uint ClosestColorDist( const color24& ) const override { return 0; }

protected:

	char m_szName[MAX_PATH];

	static IMaterial* errorMaterial;
};


#endif // DUMMYTEXTURE_H