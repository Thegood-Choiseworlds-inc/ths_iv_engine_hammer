//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of IEditorTexture interface for materials.
//
//			Materials are kept in a directory tree containing pairs of VMT
//			and VTF files. Each pair of files represents a material.
//
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "mapdoc.h"
#include "material.h"
#include "globalfunctions.h"
#include "bspfile.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "materialsystem/ishaderapi.h"
#include "filesystem.h"
#include "tier1/strtools.h"
#include "tier1/fmtstr.h"
#include "tier1/utlhashtable.h"
#include "tier0/dbg.h"
#include "texturesystem.h"
#include "materialproxyfactory_wc.h"
#include "options.h"
#include "pixelwriter.h"
#include "vstdlib/jobthread.h"
#include "tier1/utlenvelope.h"
#include "vtffile.h"
#pragma push_macro( "_DEBUG" )
#pragma push_macro( "DEBUG" )
#undef _DEBUG
#undef DEBUG
#include "opencv2/imgproc.hpp"
#include "opencv2/core/utils/allocator_stats.hpp"
#include "opencv2/core/utils/allocator_stats.impl.hpp"
#pragma pop_macro( "_DEBUG" )
#pragma pop_macro( "DEBUG" )

#include <psapi.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_ASSERT(x) Assert(x)
#include "stb_image_resize.h"

#pragma warning(disable:4244)

// replace opencv's allocator with our "direct" aligned one
namespace cv
{
	static cv::utils::AllocatorStatistics allocator_stats;

	cv::utils::AllocatorStatisticsInterface& getAllocatorStatistics()
	{
		return allocator_stats;
	}

	void* fastMalloc( size_t size )
	{
		return MemAlloc_AllocAligned( size, 64 );
	}

	void fastFree( void* ptr )
	{
		MemAlloc_FreeAligned( ptr );
	}
}

extern "C" void* cvAlloc( size_t size )
{
	return cv::fastMalloc( size );
}

extern "C" void cvFree_( void* ptr )
{
	cv::fastFree( ptr );
}


static IThreadPool* s_MaterialThreadPool;

MaterialSystem_Config_t g_materialSystemConfig;


//-----------------------------------------------------------------------------
// Purpose:
// This class speeds up the call to IMaterial::GetPreviewImageProperties because
// we call it thousands of times per level load when there are detail props.
//-----------------------------------------------------------------------------
class CPreviewImagePropertiesCache
{
public:
	//-----------------------------------------------------------------------------
	// Purpose: Anyone can call this instead of IMaterial::GetPreviewImageProperties
	// and it'll be a lot faster if there are redundant calls to it.
	//-----------------------------------------------------------------------------
	static PreviewImageRetVal_t GetPreviewImageProperties( IMaterial *pMaterial, int *width, int *height, ImageFormat *imageFormat, bool* isTranslucent )
	{
		int i = s_PreviewImagePropertiesCache.Find( pMaterial );
		if ( i == s_PreviewImagePropertiesCache.InvalidIndex() )
		{
			// Add an entry to the cache.
			CPreviewImagePropertiesCache::CEntry entry;
			entry.m_RetVal = GetPreviewImagePropertiesInternal( pMaterial, entry.m_Width, entry.m_Height, entry.m_ImageFormat, entry.m_bIsTranslucent );
			i = s_PreviewImagePropertiesCache.Insert( pMaterial, entry );
		}

		const CPreviewImagePropertiesCache::CEntry& entry = s_PreviewImagePropertiesCache[i];
		*width = entry.m_Width;
		*height = entry.m_Height;
		*imageFormat = entry.m_ImageFormat;
		*isTranslucent = entry.m_bIsTranslucent && pMaterial->GetMaterialVarFlag( MATERIAL_VAR_TRANSLUCENT );

		return entry.m_RetVal;
	}

	static void InvalidateMaterial( IMaterial* pMaterial )
	{
		int i = s_PreviewImagePropertiesCache.Find( pMaterial );
		if ( i != s_PreviewImagePropertiesCache.InvalidIndex() )
			s_PreviewImagePropertiesCache.RemoveAt( i );
	}

	static PreviewImageRetVal_t GetPreviewImage( IMaterial* pMaterial, unsigned char* pData, int width, int height, ImageFormat imageFormat )
	{
		const auto name = GetPreviewImageFileName( pMaterial );
		if ( name.IsEmpty() )
			return MATERIAL_NO_PREVIEW_IMAGE;
		FileHandle_t file;
		if ( file = g_pFullFileSystem->Open( name, "rb" ); file == nullptr )
			return MATERIAL_PREVIEW_IMAGE_BAD;

		CVTFFile tex;
		if ( !tex.Load( file, false ) )
			return MATERIAL_PREVIEW_IMAGE_BAD;

		if ( width != tex.GetWidth() || height != tex.GetHeight() )
		{
#ifdef DEBUG
			CFastTimer timer;
			timer.Start();
#endif

			const auto needTransparent = CVTFFile::GetImageFormatInfo( imageFormat ).uiAlphaBitsPerPixel > 0;
			const auto fmt = needTransparent ? IMAGE_FORMAT_RGBA8888 : IMAGE_FORMAT_RGB888;
			const auto oldSize = CVTFFile::ComputeImageSize( tex.GetWidth(), tex.GetHeight(), 1, fmt );
			auto tmpSrc = fmt == tex.GetFormat() ? tex.GetData() : new byte[oldSize];
			if ( fmt != tex.GetFormat() )
				CVTFFile::Convert( tex.GetData(), tmpSrc, tex.GetWidth(), tex.GetHeight(), tex.GetFormat(), fmt );
			const auto size = CVTFFile::ComputeImageSize( width, height, 1, fmt );
			auto tmpBuf = fmt == imageFormat ? pData : new byte[size];
			stbir_resize( tmpSrc, tex.GetWidth(), tex.GetHeight(), tex.GetWidth() * ( needTransparent ? 4 : 3 ),
						  tmpBuf, width, height, width * ( needTransparent ? 4 : 3 ), STBIR_TYPE_UINT8, needTransparent ? 4 : 3,
						  -1, 0, STBIR_EDGE_CLAMP, STBIR_EDGE_CLAMP, STBIR_FILTER_MITCHELL, STBIR_FILTER_MITCHELL, STBIR_COLORSPACE_LINEAR, nullptr );
			if ( fmt != imageFormat )
			{
				CVTFFile::Convert( tmpBuf, pData, width, height, fmt, imageFormat );
				delete[] tmpBuf;
			}
			if ( fmt != tex.GetFormat() )
				delete[] tmpSrc;
#ifdef DEBUG
			timer.End();
			OutputDebugString( CFmtStr( "resize %s: %g ms\n", pMaterial->GetName(), timer.GetDuration().GetMillisecondsF() ) );
#endif
			return MATERIAL_PREVIEW_IMAGE_OK;
		}

		CVTFFile::Convert( tex.GetData(), pData, width, height, tex.GetFormat(), imageFormat );
		return MATERIAL_PREVIEW_IMAGE_OK;
	}

	static CUtlString GetPreviewImageFileName( IMaterial* pMaterial )
	{
		auto name = GetPreviewImageName( pMaterial );
		if ( !name )
			return {};
		return CFmtStrN<MAX_PATH>( "materials/%s.vtf", name ).Get();
	}

private:
	static const char* GetPreviewImageName( IMaterial* pMaterial )
	{
		bool found;
		if ( pMaterial->FindVar( "%noToolTexture", &found, false ) && found )
			return nullptr;
		if ( auto tex = pMaterial->FindVar( "%toolTexture", &found, false ); found )
		{
			if ( tex->GetType() == MATERIAL_VAR_TYPE_STRING )
				return tex->GetStringValue();
			if ( tex->GetType() == MATERIAL_VAR_TYPE_TEXTURE )
				return tex->GetTextureValue()->GetName();
		}
		if ( auto tex = pMaterial->FindVar( "$baseTexture", &found, false ); found )
		{
			if ( tex->GetType() == MATERIAL_VAR_TYPE_STRING )
				return tex->GetStringValue();
			if ( tex->GetType() == MATERIAL_VAR_TYPE_TEXTURE )
				return tex->GetTextureValue()->GetName();
		}
		return pMaterial->GetName();
	}

	static PreviewImageRetVal_t GetPreviewImagePropertiesInternal( IMaterial* pMaterial, int& width, int& height, ImageFormat& imageFormat, bool& isTranslucent )
	{
		const auto name = GetPreviewImageFileName( pMaterial );
		if ( name.IsEmpty() )
		{
			width = height = 0;
			imageFormat = IMAGE_FORMAT_RGB888;
			isTranslucent = false;
			return MATERIAL_NO_PREVIEW_IMAGE;
		}

		FileHandle_t file;
		if ( file = g_pFullFileSystem->Open( name, "rb" ); file == nullptr )
			return MATERIAL_PREVIEW_IMAGE_BAD;

		CVTFFile tex;
		if ( !tex.Load( file, true ) )
			return MATERIAL_PREVIEW_IMAGE_BAD;

		width = tex.GetWidth();
		height = tex.GetHeight();
		imageFormat = tex.GetFormat();
		isTranslucent = ( tex.GetFlags() & ( TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA ) ) != 0;
		return MATERIAL_PREVIEW_IMAGE_OK;
	}

	class CEntry
	{
	public:
		int m_Width;
		int m_Height;
		ImageFormat m_ImageFormat;
		bool m_bIsTranslucent;
		PreviewImageRetVal_t m_RetVal;
	};

	static bool PreviewImageLessFunc( IMaterial* const& a, IMaterial* const& b )
	{
		return a < b;
	}

	static CUtlMap<IMaterial*, CPreviewImagePropertiesCache::CEntry> s_PreviewImagePropertiesCache;
};
CUtlMap<IMaterial*, CPreviewImagePropertiesCache::CEntry> CPreviewImagePropertiesCache::s_PreviewImagePropertiesCache( 64, 64, &CPreviewImagePropertiesCache::PreviewImageLessFunc );


//-----------------------------------------------------------------------------
// Purpose: stuff for caching textures in memory.
//-----------------------------------------------------------------------------
class CMaterialImageCache
{
public:
	CMaterialImageCache( int maxNumGraphicsLoaded );
	~CMaterialImageCache();
	void EnCache( CMaterial* pMaterial );

protected:

	CMaterial** pool;
	int cacheSize;
	int currentID;  // next one to get killed.
};


//-----------------------------------------------------------------------------
// Purpose: Constructor. Allocates a pool of material pointers.
// Input  : maxNumGraphicsLoaded -
//-----------------------------------------------------------------------------
CMaterialImageCache::CMaterialImageCache( int maxNumGraphicsLoaded )
{
	cacheSize = maxNumGraphicsLoaded;
	pool = new CMaterialPtr[cacheSize];
	memset( pool, 0, sizeof( CMaterialPtr ) * cacheSize );
	currentID = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees the pool memory.
//-----------------------------------------------------------------------------
CMaterialImageCache::~CMaterialImageCache()
{
	delete[] pool;
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pMaterial -
//-----------------------------------------------------------------------------
void CMaterialImageCache::EnCache( CMaterial* pMaterial )
{
	if ( pMaterial->m_pData != NULL )
		// Already cached.
		return;

	// kill currentID
	if ( pool[currentID] && pool[currentID]->HasData() )
		pool[currentID]->FreeData();

	pool[currentID] = pMaterial;
	pMaterial->LoadMaterialImage();
	currentID = ( currentID + 1 ) % cacheSize;

#if 0
	OutputDebugString( "CMaterialCache::Encache: " );
	OutputDebugString( pMaterial->m_szName );
	OutputDebugString( "\n" );
#endif
}


static CMaterialImageCache* g_pMaterialImageCache = NULL;


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//-----------------------------------------------------------------------------
CMaterial::CMaterial()
{
	memset( m_szName, 0, sizeof( m_szName ) );
	memset( m_szKeywords, 0, sizeof( m_szKeywords ) );

	m_nWidth = 0;
	m_nHeight = 0;
	m_pData = NULL;
	m_bLoaded = false;
	m_pMaterial = NULL;
	m_TranslucentBaseTexture = false;
	m_bIsWater = TRS_NONE;
	m_baseColor.Init( 1, 1, 1 );
	memset( m_computedColors, 0, sizeof( m_computedColors ) );
	m_computedColorCount = 0;
	memset( &m_computedColorValue, 0, sizeof( m_computedColorValue ) );
	m_computedColorDist = UINT32_MAX;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees texture image data and palette.
//-----------------------------------------------------------------------------
CMaterial::~CMaterial()
{
	//
	// Free image data.
	//
	if (m_pData != NULL)
	{
		free( m_pData );
		m_pData = NULL;
	}

	/* FIXME: Texture manager shuts down after the material system
	if (m_pMaterial)
	{
		m_pMaterial->DecrementReferenceCount();
		m_pMaterial = NULL;
	}
	*/
}


#define MATERIAL_PREFIX_LEN	10
//-----------------------------------------------------------------------------
// Finds all .VMT files in a particular directory
//-----------------------------------------------------------------------------
bool CMaterial::LoadMaterialsInDirectory( char const* pDirectoryName, IMaterialEnumerator* pEnum, int nContext, int nFlags )
{
	CFmtStrN<MAX_PATH> pWildCard( "%s/*.vmt", pDirectoryName );
	if ( !g_pFullFileSystem )
		return false;

	FileFindHandle_t findHandle;
	const char* pFileName = g_pFullFileSystem->FindFirstEx( pWildCard, "GAME", &findHandle );
	while ( pFileName )
	{
		if ( IsIgnoredMaterial( pFileName ) )
		{
			pFileName = g_pFullFileSystem->FindNext( findHandle );
			continue;
		}

		if ( !g_pFullFileSystem->FindIsDirectory( findHandle ) )
		{
			// Strip off the 'materials/' part of the material name.
			CFmtStrN<MAX_PATH> pFileNameWithPath( "%s/%s", &pDirectoryName[MATERIAL_PREFIX_LEN], pFileName );
			V_strnlwr( pFileNameWithPath.Access(), pFileNameWithPath.Length() );

			// Strip off the extension...
			if ( char* pExt = V_strrchr( pFileNameWithPath.Access(), '.' ) )
				*pExt = 0;

			if ( !pEnum->EnumMaterial( pFileNameWithPath, nContext ) )
				return false;
		}
		pFileName = g_pFullFileSystem->FindNext( findHandle );
	}
	g_pFullFileSystem->FindClose( findHandle );
	return true;
}


//-----------------------------------------------------------------------------
// Discovers all .VMT files lying under a particular directory
// It only finds their names so we can generate shell materials for them
// that we can load up at a later time
//-----------------------------------------------------------------------------
bool CMaterial::InitDirectoryRecursive( char const* pDirectoryName, IMaterialEnumerator* pEnum, int nContext, int nFlags )
{
	// Make sure this is an ok directory, otherwise don't bother
	if ( ShouldSkipMaterial( pDirectoryName + MATERIAL_PREFIX_LEN, nFlags ) )
		return true;

	if ( !LoadMaterialsInDirectory( pDirectoryName, pEnum, nContext, nFlags ) )
		return false;

	FileFindHandle_t findHandle;
	CFmtStrN<MAX_PATH> pWildCard( "%s/*.*", pDirectoryName );
	const char* pFileName = g_pFullFileSystem->FindFirstEx( pWildCard, "GAME", &findHandle );
	while ( pFileName )
	{
		if ( IsIgnoredMaterial( pFileName ) )
		{
			pFileName = g_pFullFileSystem->FindNext( findHandle );
			continue;
		}

		if ( pFileName[0] != '.' || ( pFileName[1] != '.' && pFileName[1] != 0 ) )
		{
			if ( g_pFullFileSystem->FindIsDirectory( findHandle ) )
			{
				CFmtStrN<MAX_PATH> pFileNameWithPath( "%s/%s", pDirectoryName, pFileName );
				if ( !InitDirectoryRecursive( pFileNameWithPath, pEnum, nContext, nFlags ) )
					return false;
			}
		}

		pFileName = g_pFullFileSystem->FindNext( findHandle );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Discovers all .VMT files lying under a particular directory
// It only finds their names so we can generate shell materials for them
// that we can load up at a later time
//-----------------------------------------------------------------------------
void CMaterial::EnumerateMaterials( IMaterialEnumerator* pEnum, const char* szRoot, int nContext, int nFlags )
{
	InitDirectoryRecursive( szRoot, pEnum, nContext, nFlags );
}


//-----------------------------------------------------------------------------
// Purpose: Called from GetFirst/GetNextMaterialName to skip unwanted materials.
// Input  : pszName - Name of material to evaluate.
//			nFlags - One or more of the following:
//				INCLUDE_ALL_MATERIALS
//				INCLUDE_WORLD_MATERIALS
//				INCLUDE_MODEL_MATERIALS
// Output : Returns true to skip, false to not skip this material.
//-----------------------------------------------------------------------------
bool CMaterial::ShouldSkipMaterial( const char* pszName, int nFlags )
{
	//static char szStrippedName[MAX_PATH];

	// if NULL skip it
	if ( !pszName )
		return true;

	//
	// check against the list of exclusion directories
	//
	for ( int i = 0; i < g_pGameConfig->m_MaterialExclusions.Count(); i++ )
	{
		// This will guarantee the match is at the start of the string
		const char* pMatchFound = V_stristr( pszName, g_pGameConfig->m_MaterialExclusions[i].szDirectory );
		if ( pMatchFound == pszName )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Factory. Creates a material by name.
// Input  : pszMaterialName - Name of material, ie "brick/brickfloor01".
// Output : Returns a pointer to the new material object, NULL if the given
//			material did not exist.
//-----------------------------------------------------------------------------
CMaterial* CMaterial::CreateMaterial( const char* pszMaterialName, bool bLoadImmediately, bool* pFound )
{
	Assert( pszMaterialName );
	CMaterial* pMaterial = new CMaterial;

	// Store off the material name so we can load it later if we need to
	V_sprintf_safe( pMaterial->m_szName, pszMaterialName );

	//
	// Find the material by name and load it.
	//
	if ( bLoadImmediately )
	{
		// Returns if the material was found or not
		if ( bool bFound = pMaterial->LoadMaterial(); pFound )
			*pFound = bFound;
	}

	return pMaterial;
}

bool CMaterial::IsIgnoredMaterial( const char* pName )
{
	// TODO: make this a customizable user option?
	if ( !V_strnicmp( pName, ".svn", 4 ) || V_strstr( pName, ".svn" ) ||
		!V_strnicmp( pName, "models", 6 ) || V_strstr( pName, "models" ) ||
		!V_strnicmp( pName, "backpack", 8 ) || V_strstr( pName, "backpack" ) )
		return true;

	return false;
}
//-----------------------------------------------------------------------------
// Will actually load the material bits
// We don't want to load them all at once because it takes way too long
//-----------------------------------------------------------------------------
bool CMaterial::LoadMaterial()
{
	bool bFound = true;
	if ( !m_bLoaded )
	{
		if ( IsIgnoredMaterial( m_szName ) )
			return false;

		m_bLoaded = true;

		IMaterial* pMat = materials->FindMaterial( m_szName, TEXTURE_GROUP_OTHER );
		if ( IsErrorMaterial( pMat ) )
			bFound = false;

		Assert( pMat );

		if ( !pMat )
			return false;

		if ( !LoadMaterialHeader( pMat ) )
		{
			bFound = false;
			if ( ( pMat = materials->FindMaterial( "debug/debugempty", TEXTURE_GROUP_OTHER ) ) != nullptr )
				LoadMaterialHeader( pMat );
		}
	}

	return bFound;
}


//-----------------------------------------------------------------------------
// Reloads owing to a material change
//-----------------------------------------------------------------------------
void CMaterial::Reload( bool bFullReload )
{
	// Don't bother if we're not loaded yet
	if ( !m_bLoaded )
		return;

	FreeData();

	if ( m_pMaterial )
	{
		CPreviewImagePropertiesCache::InvalidateMaterial( m_pMaterial );
		m_pMaterial->DecrementReferenceCount();
	}
	m_pMaterial = materials->FindMaterial( m_szName, TEXTURE_GROUP_OTHER );
	Assert( m_pMaterial );

	if ( bFullReload )
		m_pMaterial->Refresh();

	bool translucentBaseTexture;
	ImageFormat eImageFormat;
	int width, height;
	PreviewImageRetVal_t retVal = CPreviewImagePropertiesCache::GetPreviewImageProperties( m_pMaterial, &width, &height, &eImageFormat, &translucentBaseTexture );
	if ( retVal == MATERIAL_PREVIEW_IMAGE_OK )
	{
		m_nWidth = width;
		m_nHeight = height;
		m_TranslucentBaseTexture = translucentBaseTexture;
	}

	m_bIsWater = TRS_NONE;

	// Find the keywords for this material from the vmt file.
	bool bFound;
	IMaterialVar* pVar = m_pMaterial->FindVar( "%keywords", &bFound, false );
	if ( bFound )
	{
		V_strcpy_safe( m_szKeywords, pVar->GetStringValue() );

		// Register the keywords
		g_Textures.RegisterTextureKeywords( this );
	}

	// Make sure to bump the refcount again. Not sure why this wasn't always done (check for leaks).
	if ( m_pMaterial )
		m_pMaterial->IncrementReferenceCount();
}


//-----------------------------------------------------------------------------
// Returns the material
//-----------------------------------------------------------------------------
IMaterial* CMaterial::GetMaterial( bool bForceLoad )
{
	if ( bForceLoad )
		LoadMaterial();

	return m_pMaterial;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMaterial::DrawIcon( CDC* pDC, CMaterial* pIcon, RECT& dstRect )
{
	if ( !pIcon )
		return;

	g_pMaterialImageCache->EnCache( pIcon );

	RECT rect, dst;
	rect.left = 0; rect.right = pIcon->GetWidth();

	// FIXME: Workaround the fact that materials must be power of 2, I want 12 bite
	rect.top = 2; rect.bottom = pIcon->GetHeight() - 2;

	dst = dstRect;
	float dstHeight = dstRect.bottom - dstRect.top;
	float srcAspect = (float)(rect.right - rect.left) / (float)(rect.bottom - rect.top);
	dst.right = dst.left + (dstHeight * srcAspect);
	pIcon->DrawBitmap( pDC, rect, dst );

	dstRect.left += dst.right - dst.left;
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : pDC -
//			dstRect -
//			detectErrors -
//-----------------------------------------------------------------------------
void CMaterial::DrawBrowserIcons( CDC* pDC, RECT& dstRect, bool detectErrors )
{
	static CMaterial* pTranslucentIcon = nullptr;
	static CMaterial* pOpaqueIcon = nullptr;
	static CMaterial* pSelfIllumIcon = nullptr;
	static CMaterial* pBaseAlphaEnvMapMaskIcon = nullptr;
	static CMaterial* pErrorIcon = nullptr;

	if ( !pTranslucentIcon )
	{
		if ( ( pTranslucentIcon = CreateMaterial( "editor/translucenticon", true ) ) != nullptr )
			pTranslucentIcon->m_TranslucentBaseTexture = false;
		if ( ( pOpaqueIcon = CreateMaterial( "editor/opaqueicon", true ) ) != nullptr )
			pOpaqueIcon->m_TranslucentBaseTexture = false;
		if ( ( pSelfIllumIcon = CreateMaterial( "editor/selfillumicon", true ) ) != nullptr )
			pSelfIllumIcon->m_TranslucentBaseTexture = false;
		if ( ( pBaseAlphaEnvMapMaskIcon = CreateMaterial( "editor/basealphaenvmapmaskicon", true ) ) != nullptr )
			pBaseAlphaEnvMapMaskIcon->m_TranslucentBaseTexture = false;
		if ( ( pErrorIcon = CreateMaterial( "editor/erroricon", true ) ) != nullptr )
			pErrorIcon->m_TranslucentBaseTexture = false;

		Assert( pTranslucentIcon && pOpaqueIcon && pSelfIllumIcon && pBaseAlphaEnvMapMaskIcon && pErrorIcon );
	}

	bool error = false;
	IMaterial* pMaterial = GetMaterial();
	if ( pMaterial->GetMaterialVarFlag( MATERIAL_VAR_TRANSLUCENT ) )
	{
		DrawIcon( pDC, pTranslucentIcon, dstRect );
		if ( detectErrors )
			error = error || !m_TranslucentBaseTexture;
	}
	else
		DrawIcon( pDC, pOpaqueIcon, dstRect );

	if ( pMaterial->GetMaterialVarFlag( MATERIAL_VAR_SELFILLUM ))
	{
		DrawIcon( pDC, pSelfIllumIcon, dstRect );
		if ( detectErrors )
			error = error || !m_TranslucentBaseTexture;
	}

	if ( pMaterial->GetMaterialVarFlag( MATERIAL_VAR_BASEALPHAENVMAPMASK ) )
	{
		DrawIcon( pDC, pBaseAlphaEnvMapMaskIcon, dstRect );
		if ( detectErrors )
			error = error || !m_TranslucentBaseTexture;
	}

	if ( error )
		DrawIcon( pDC, pErrorIcon, dstRect );
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : pDC -
//			srcRect -
//			dstRect -
//-----------------------------------------------------------------------------
void CMaterial::DrawBitmap( CDC* pDC, const RECT& srcRect, const RECT& dstRect )
{
	int srcWidth = srcRect.right - srcRect.left;
	int srcHeight = srcRect.bottom - srcRect.top;

	BITMAPINFO bmi;
	memset( &bmi, 0, sizeof( bmi ) );
	BITMAPINFOHEADER& bmih = bmi.bmiHeader;
	bmih.biSize = sizeof( BITMAPINFOHEADER );
	bmih.biWidth = srcWidth;
	bmih.biHeight = -srcHeight;
	bmih.biCompression = BI_RGB;
	bmih.biPlanes = 1;

	const auto maxWdith = Min( m_nWidth, 512 );
	const auto maxHeight = Min( m_nHeight, 512 );
	bmih.biBitCount = m_TranslucentBaseTexture ? 32 : 24;
	bmih.biSizeImage = maxWdith * maxHeight * ( m_TranslucentBaseTexture ? 4 : 3 );

	int dest_width = dstRect.right - dstRect.left;
	int dest_height = dstRect.bottom - dstRect.top;

	if ( m_TranslucentBaseTexture )
	{
		void* data;
		CDC hdc;
		hdc.CreateCompatibleDC( pDC );

		bmih.biWidth = dest_width;
		bmih.biHeight = -dest_height;

		auto bitmap = CreateDIBSection( hdc, &bmi, DIB_RGB_COLORS, &data, NULL, 0x0 );
		CPixelWriter writer;
		writer.SetPixelMemory( IMAGE_FORMAT_BGRA8888, data, dest_width * 4 );

		constexpr int boxSize = 8;
		for ( int y = 0; y < dest_height; ++y )
		{
			writer.Seek( 0, y );
			for ( int x = 0; x < dest_width; ++x )
			{
				if ( ( x & boxSize ) ^ ( y & boxSize ) )
					writer.WritePixel( 102, 102, 102, 255 );
				else
					writer.WritePixel( 153, 153, 153, 255 );
			}
		}

		hdc.SelectObject( bitmap );
		pDC->BitBlt( dstRect.left, dstRect.top, dest_width, dest_height, &hdc, srcRect.left, -srcRect.top, SRCCOPY );
		DeleteObject( bitmap );

		bitmap = CreateBitmap( srcWidth, srcHeight, 1, 32, m_pData );
		hdc.SelectObject( bitmap );

		constexpr BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
		if ( !pDC->AlphaBlend( dstRect.left, dstRect.top, dest_width, dest_height, &hdc, srcRect.left, -srcRect.top, srcWidth, srcHeight, bf ) )
			Msg( mwError, "CMaterial::Draw(): AlphaBlend failed." );

		DeleteObject( bitmap );
		return;
	}

	// ** bits **
	pDC->SetStretchBltMode( COLORONCOLOR );
	if ( StretchDIBits( pDC->m_hDC, dstRect.left, dstRect.top, dest_width, dest_height,
						srcRect.left, -srcRect.top, srcWidth, srcHeight, m_pData, &bmi, DIB_RGB_COLORS, SRCCOPY ) == (int)GDI_ERROR )
		Msg( mwError, "CMaterial::Draw(): StretchDIBits failed." );
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pDC -
//			rect -
//			iFontHeight -
//			dwFlags -
//-----------------------------------------------------------------------------
void CMaterial::Draw( CDC* pDC, const RECT& rect, int iFontHeight, int iIconHeight, const DrawTexData_t& DrawTexData )
{
	g_pMaterialImageCache->EnCache( this );
	if ( !HasData() )
	{
NoData:
		// draw "no data"
		CFont* pOldFont = (CFont*)pDC->SelectStockObject( ANSI_VAR_FONT );
		COLORREF cr = pDC->SetTextColor( RGB( 0xff, 0xff, 0xff ) );
		COLORREF cr2 = pDC->SetBkColor( RGB( 0, 0, 0 ) );

		// draw black rect first
		pDC->FillRect( &rect, CBrush::FromHandle( HBRUSH( GetStockObject( BLACK_BRUSH ) ) ) );

		// then text
		pDC->TextOut( rect.left + 2, rect.top + 2, "No Image", 8 );
		pDC->SelectObject( pOldFont );
		pDC->SetTextColor( cr );
		pDC->SetBkColor( cr2 );
		return;
	}

	// no data -
	if ( !m_pData && !Load() )
	{
		// can't load -
		goto NoData;
	}

	const auto maxWdith = Min( m_nWidth, 512 );
	const auto maxHeight = Min( m_nHeight, 512 );

	// Draw the material image
	RECT srcRect, dstRect;
	srcRect.left = 0;
	srcRect.top = 0;
	srcRect.right = maxWdith;
	srcRect.bottom = maxHeight;
	dstRect = rect;

	if ( DrawTexData.nFlags & drawCaption )
		dstRect.bottom -= iFontHeight + 4;
	if ( DrawTexData.nFlags & drawIcons )
		dstRect.bottom -= iIconHeight;

	if ( !( DrawTexData.nFlags & drawResizeAlways ) )
	{
		if ( maxWdith < dstRect.right - dstRect.left )
			dstRect.right = dstRect.left + maxWdith;

		if ( maxHeight < dstRect.bottom - dstRect.top )
			dstRect.bottom = dstRect.top + maxHeight;
	}
	DrawBitmap( pDC, srcRect, dstRect );

	// Draw the icons
	if ( DrawTexData.nFlags & drawIcons )
	{
		dstRect = rect;
		if ( DrawTexData.nFlags & drawCaption )
			dstRect.bottom -= iFontHeight + 5;
		dstRect.top = dstRect.bottom - iIconHeight;
		DrawBrowserIcons( pDC, dstRect, ( DrawTexData.nFlags & drawErrors ) != 0 );
	}

	// computed color visualization
#ifdef DEBUG
	if ( HasValidColorInformation() )
	{
		const auto width = m_computedColorCount * 16 + 1;
		const auto height = 18;

		BITMAPINFO bmi;
		memset( &bmi, 0, sizeof( bmi ) );
		BITMAPINFOHEADER& bmih = bmi.bmiHeader;
		bmih.biSize = sizeof( BITMAPINFOHEADER );
		bmih.biWidth = width;
		bmih.biHeight = height;
		bmih.biCompression = BI_RGB;
		bmih.biPlanes = 1;

		bmih.biBitCount = 32;
		bmih.biSizeImage = width * height * 4;

		void* data;
		CDC hdc;
		hdc.CreateCompatibleDC( pDC );

		auto bitmap = CreateDIBSection( hdc, &bmi, DIB_RGB_COLORS, &data, NULL, 0x0 );
		CPixelWriter writer;
		writer.SetPixelMemory( IMAGE_FORMAT_BGRA8888, data, width * 4 );

		writer.Seek( 0, 0 );
		for ( int x = 0; x < width; ++x )
			writer.WritePixel( 0, 0, 0, 255 );
		for ( int y = 1; y < height - 1; ++y )
		{
			writer.Seek( 0, y );
			for ( int x = 0; x < width; ++x )
			{
				if ( ( x % 16 ) == 0 )
				{
					writer.WritePixel( 0, 0, 0, 255 );
					continue;
				}

				const color24& rgb = m_computedColors[x / 16];
				writer.WritePixel( rgb.r, rgb.g, rgb.b, 255 );
			}
		}
		writer.Seek( 0, height - 1 );
		for ( int x = 0; x < width; ++x )
			writer.WritePixel( 0, 0, 0, 255 );

		hdc.SelectObject( bitmap );
		pDC->BitBlt( rect.left, rect.top, width, height, &hdc, 0, 0, SRCCOPY );
		DeleteObject( bitmap );
	}
#endif

	// ** caption **
	if ( DrawTexData.nFlags & drawCaption )
	{
		static int iCharWidth = -1;
		// first paint?
		if ( iCharWidth == -1 )
			pDC->GetCharWidth( 'A', 'A', &iCharWidth );

		// draw background for name
		CBrush brCaption( RGB( 0, 0, 255 ) );
		CRect  rcCaption( rect );

		rcCaption.top = rcCaption.bottom - ( iFontHeight + 5 );
		pDC->FillRect( rcCaption, &brCaption );

		const int maxChars = rcCaption.Width() / iCharWidth;
		// draw name
		char szShortName[MAX_PATH];
		int iLen = GetShortName( szShortName );
		const char* name = szShortName;
		if ( iLen > maxChars )
		{
			name += iLen - maxChars;
			iLen = maxChars;
		}
		pDC->ExtTextOut( rect.left, rect.bottom - ( iFontHeight + 4 ), ETO_CLIPPED, rcCaption, name, iLen, nullptr );

		// draw usage count
		if ( DrawTexData.nFlags & drawUsageCount )
		{
			CString str;
			str.Format( "%d", DrawTexData.nUsageCount );
			CSize size = pDC->GetTextExtent( str );
			pDC->TextOut( rect.right - size.cx, rect.bottom - ( iFontHeight + 4 ), str );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMaterial::FreeData()
{
	free( m_pData );
	m_pData = NULL;
	m_computedColorCount = 0; // force color calc on next load
}


//-----------------------------------------------------------------------------
// Purpose: Returns a string of comma delimited keywords associated with this
//			material.
// Input  : pszKeywords - Buffer to receive keywords, NULL to query string length.
// Output : Returns the number of characters in the keyword string.
//-----------------------------------------------------------------------------
int CMaterial::GetKeywords( char* pszKeywords ) const
{
	// To access keywords, we have to have the header loaded
	const_cast<CMaterial*>( this )->Load();
	if ( pszKeywords == NULL )
		return V_strlen( m_szKeywords );

	V_strcpy( pszKeywords, m_szKeywords );
	return V_strlen( m_szKeywords );
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pszName -
// Output : int
//-----------------------------------------------------------------------------
int CMaterial::GetShortName( char* pszName ) const
{
	if ( pszName == NULL )
		return V_strlen( m_szName );

	V_strcpy( pszName, m_szName );
	return V_strlen( m_szName );
}


abstract_class IMaterialInternal : public IMaterial
{
public:
	virtual int GetReferenceCount() const = 0;
	virtual void SetEnumerationID( int id ) = 0;
	virtual void SetNeedsWhiteLightmap( bool val ) = 0;
	virtual bool GetNeedsWhiteLightmap() const = 0;
	virtual void Uncache( bool bPreserveVars ) = 0;
	virtual void Precache() = 0;
	virtual bool PrecacheVars( KeyValues* pKeyValues, KeyValues* pPatchKeyValues, CUtlVector<FileNameHandle_t>* pIncludes, int nFindContext ) = 0;
	virtual void ReloadTextures() = 0;
	virtual void SetMinLightmapPageID( int pageID ) = 0;
	virtual void SetMaxLightmapPageID( int pageID ) = 0;;
	virtual int GetMinLightmapPageID() const = 0;
	virtual int GetMaxLightmapPageID() const = 0;
	virtual IShader* GetShader() const = 0;
	virtual bool IsPrecached() const = 0;
	virtual bool IsPrecachedVars() const = 0;
	virtual void DrawMesh( VertexCompressionType_t vertexCompression ) = 0;
	virtual VertexFormat_t GetVertexFormat() const = 0;
	virtual VertexFormat_t GetVertexUsage() const = 0;
	virtual bool PerformDebugTrace() const = 0;
	virtual bool NoDebugOverride() const = 0;
	virtual void ToggleSuppression() = 0;
	virtual bool IsSuppressed() const = 0;
	virtual void ToggleDebugTrace() = 0;
	virtual bool UseFog() const = 0;
	virtual void AddMaterialVar( IMaterialVar* pMaterialVar ) = 0;
	virtual void* GetRenderState() = 0;
	virtual bool IsManuallyCreated() const = 0;
	virtual bool NeedsFixedFunctionFlashlight() const = 0;
	virtual bool IsUsingVertexID() const = 0;
	virtual void MarkAsPreloaded( bool bSet ) = 0;
	virtual bool IsPreloaded() const = 0;
	virtual void ArtificialAddRef() = 0;
	virtual void ArtificialRelease() = 0;
	virtual void ReportVarChanged( IMaterialVar* pVar ) = 0;
	virtual uint32 GetChangeID() const = 0;
	virtual bool IsTranslucentInternal( float fAlphaModulation ) const = 0;
	virtual bool IsRealTimeVersion() const = 0;
	virtual void ClearContextData() {}
	virtual IMaterialInternal* GetRealTimeVersion() = 0;
	virtual IMaterialInternal* GetQueueFriendlyVersion() = 0;
	virtual void PrecacheMappingDimensions() = 0;
	virtual void FindRepresentativeTexture() = 0;
	virtual void DecideShouldReloadFromWhitelist( IFileList* pFileList ) = 0;
	virtual void ReloadFromWhitelistIfMarked() = 0;
};

//-----------------------------------------------------------------------------
// Purpose:
// Input  : material -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMaterial::LoadMaterialHeader( IMaterial* pMat )
{
	PreviewImageRetVal_t retVal;
	bool translucentBaseTexture;
	ImageFormat eImageFormat;
	int width, height;
	retVal = CPreviewImagePropertiesCache::GetPreviewImageProperties( pMat, &width, &height, &eImageFormat, &translucentBaseTexture);
	if ( retVal == MATERIAL_PREVIEW_IMAGE_BAD )
		return false;

	m_pMaterial = pMat;
	m_pMaterial->IncrementReferenceCount();

	m_nWidth = width;
	m_nHeight = height;
	m_TranslucentBaseTexture = translucentBaseTexture;

	// Find the keywords for this material from the vmt file.
	bool bFound;
	IMaterialVar* pVar = pMat->FindVar( "%keywords", &bFound, false );
	if ( bFound )
	{
		V_strcpy_safe( m_szKeywords, pVar->GetStringValue() );

		// Register the keywords
		g_Textures.RegisterTextureKeywords( this );
	}

	Vector clr;
	dynamic_cast<IMaterialInternal*>( m_pMaterial )->GetRealTimeVersion()->GetColorModulation( &clr.x, &clr.y, &clr.z ); // HACK: queued version returns -nan's
	if ( IS_NAN( clr.x ) || IS_NAN( clr.y ) || IS_NAN( clr.z ) )
		clr.Init( 1.f, 1.f, 1.f );
	if ( clr.x > 1.0f || clr.y > 1.0f || clr.z > 1.0f )
		clr.NormalizeInPlace();
	m_baseColor = clr;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the full path of the file from which this material was loaded.
//-----------------------------------------------------------------------------
const char* CMaterial::GetFileName() const
{
	return m_szName;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMaterial::IsWater() const
{
	if ( m_bIsWater != TRS_NONE )
		return static_cast<bool>( m_bIsWater );
	bool bFound;
	IMaterialVar* pVar = m_pMaterial->FindVar( "$surfaceprop", &bFound, false );
	if ( bFound )
	{
		if ( !V_stricmp( "water", pVar->GetStringValue() ) )
		{
			const_cast<CMaterial*>( this )->m_bIsWater = TRS_TRUE;
			return true;
		}
	}

	const_cast<CMaterial*>( this )->m_bIsWater = TRS_FALSE;
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Loads this material's image from disk if it is not already loaded.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMaterial::Load()
{
	LoadMaterial();
	return true;
}


//-----------------------------------------------------------------------------
// cache in the image size only when we need to
//-----------------------------------------------------------------------------
int CMaterial::GetImageWidth() const
{
	const_cast<CMaterial*>( this )->Load();
	return m_nWidth;
}

int CMaterial::GetImageHeight() const
{
	const_cast<CMaterial*>( this )->Load();
	return m_nHeight;
}

int CMaterial::GetWidth() const
{
	const_cast<CMaterial*>( this )->Load();
	return m_nWidth;
}

int CMaterial::GetHeight() const
{
	const_cast<CMaterial*>( this )->Load();
	return m_nHeight;
}

float CMaterial::GetDecalScale() const
{
	const_cast<CMaterial*>( this )->Load();

	bool found;
	IMaterialVar* decalScaleVar = m_pMaterial->FindVar( "$decalScale", &found, false );
	return !found ? 1.0f : decalScaleVar->GetFloatValue();
}

static class CColorCache
{
	static constexpr uint HEADER = '1LOC';
public:
	CColorCache() : m_hashes( 8, []() { int f[4] {}; CpuIdEx( f, 1, 0 ); return ( f[2] & 0x100000 ) != 0; }() ? HashColorKeySSE4 : HashColorKey ) {}
	~CColorCache() = default;

	void Save() const
	{
		CUtlBuffer buf;
		buf.PutUnsignedInt( HEADER );
		FOR_EACH_HASHTABLE( m_hashes, i )
		{
			buf.Put( &m_hashes.Key( i ), sizeof( ColorHashKey ) );
			buf.Put( &m_hashes.Element( i ), sizeof( ColorHashValue ) );
		}
		g_pFullFileSystem->WriteFile( "colorCache.dat", "HAMMER", buf );
	}

	void Load()
	{
		CUtlBuffer buf;
		if ( !g_pFullFileSystem->ReadFile( "colorCache.dat", "HAMMER", buf ) )
			return;
		Assert( ( ( buf.TellMaxPut() - sizeof( HEADER ) ) % int( sizeof( ColorHashKey ) + sizeof( ColorHashValue ) ) ) == 0 );
		if ( ( buf.TellMaxPut() - sizeof( HEADER ) ) % int( sizeof( ColorHashKey ) + sizeof( ColorHashValue ) ) )
			return Msg( "Color cache has invalid size! Dropping.\n" );
		if ( buf.GetUnsignedInt() != HEADER )
			return Msg( "Color cache header has invalid signature. Dropping.\n" );
		m_lock.LockForWrite();
		ColorHashKey key;
		ColorHashValue value;
#ifdef DEBUG
		uint count = 0;
#endif
		while ( buf.GetBytesRemaining() > 0 )
		{
			buf.Get( &key, sizeof( ColorHashKey ) );
			buf.Get( &value, sizeof( ColorHashValue ) );
			m_hashes.Insert( key, value );
#ifdef DEBUG
			++count;
#endif
		}
		Assert( count == m_hashes.Count() );
		m_lock.UnlockWrite();
	}

	bool GetData( uint hash, uint hashSize, color24 baseColor, color24 ( &data )[4], int& validCount ) const
	{
		m_lock.LockForRead();
		const ColorHashKey key{ hash, hashSize, baseColor };
		auto index = m_hashes.Find( key );
		if ( !m_hashes.IsValidHandle( index ) )
		{
			m_lock.UnlockRead();
			return false;
		}
		const auto& cache = m_hashes.Element( index );
		for ( size_t i = 0; i < ARRAYSIZE( data ); i++ )
			data[i] = cache.colors[i];
		validCount = cache.validCount;
		m_lock.UnlockRead();
		return true;
	}

	void CacheData( uint hash, uint hashSize, color24 baseColor, const color24 ( &data )[4], byte validCount )
	{
		m_lock.LockForWrite();
		const ColorHashKey key{ hash, hashSize, baseColor };
		ColorHashValue value{};
		for ( size_t i = 0; i < ARRAYSIZE( data ); i++ )
			value.colors[i] = data[i];
		value.validCount = validCount;
		m_hashes.Insert( key, value ); // if we calculate same data for different materials (with same $basetexture)
		m_lock.UnlockWrite();
	}

private:
	struct ColorHashKey
	{
		uint hash;
		uint hashSize;
		color24 color;

		bool operator==( const ColorHashKey& other ) const
		{
			return hash == other.hash && hashSize == other.hashSize && color == other.color;
		}
	};

	struct ColorHashValue
	{
		color24 colors[4];
		byte validCount;
	};

	template <size_t len>
	static uint32 Hash( const char* key )
	{
		uint32 hash = 0;
		for ( uint i = 0; i < len; ++i )
		{
			hash += key[i];
			hash += ( hash << 10 );
			hash ^= ( hash >> 6 );
		}
		hash += ( hash << 3 );
		hash ^= ( hash >> 11 );
		hash += ( hash << 15 );
		return hash;
	}

	template <size_t len>
	static uint32 HashSSE4( const char* key )
	{
		uint32 hash = UINT32_MAX;
		for ( uint i = 0; i < len; ++i )
			hash = _mm_crc32_u8( hash, key[i] );
		return hash ^ UINT32_MAX;
	}

	static uint32 HashColorKey( const ColorHashKey& key )
	{
		return Hash<sizeof( ColorHashKey )>( reinterpret_cast<const char*>( &key ) );
	}

	static uint32 HashColorKeySSE4( const ColorHashKey& key )
	{
		return HashSSE4<sizeof( ColorHashKey )>( reinterpret_cast<const char*>( &key ) );
	}

	using HashTable = CUtlHashtable<ColorHashKey, ColorHashValue, uint32(*)( const ColorHashKey& )>;
	HashTable m_hashes;
	CThreadSpinRWLock m_lock;
} s_colorCache;

void CMaterial::LoadColorInfo( byte* data, uint dataSize, uint hash, uint hashSize, color24 baseColor ) // data is freed by CUtlEnvelope
{
#ifdef DEBUG
	CFastTimer timer;
	timer.Start();
#endif

	int colorCount = 0;
	if ( s_colorCache.GetData( hash, hashSize, baseColor, m_computedColors, colorCount ) )
	{
		m_computedColorCount = colorCount;
#ifdef DEBUG
		timer.End();
		OutputDebugString( CFmtStr( "%s: %g ms (cache hit)\n", m_szName, timer.GetDuration().GetMillisecondsF() ) );
#endif
		return;
	}

	cv::Mat cvData;
	cv::Mat( dataSize / 3, 3, CV_8U, data ).convertTo( cvData, CV_32F, 1.0 / 255.0 );
	if ( m_baseColor.LengthSqr() > 0.f && ( m_baseColor.x < 1.f || m_baseColor.y < 1.f || m_baseColor.z < 1.f ) )
		for ( int i = 0; i < cvData.rows; i++ )
			reinterpret_cast<Vector&>( cvData.at<cv::Vec3f>( i, 0 ) ) *= m_baseColor;

#ifdef DEBUG
	try
	{
#endif
		colorCount = ARRAYSIZE( m_computedColors );
		if ( colorCount >= cvData.rows )
		{
			colorCount = cvData.rows;
			for ( int i = 0; i < colorCount; ++i )
			{
				const auto& px = reinterpret_cast<const Vector&>( cvData.at<cv::Vec3f>( i, 0 ) );
				auto& v = m_computedColors[i];
				v.r = static_cast<int>( px.x * 255 + 0.5f );
				v.g = static_cast<int>( px.y * 255 + 0.5f );
				v.b = static_cast<int>( px.z * 255 + 0.5f );
			}
		}
		else
		{
			cv::Mat centers, labels;
			cv::kmeans( cvData, ARRAYSIZE( m_computedColors ), labels, cv::TermCriteria( cv::TermCriteria::MAX_ITER, 10, 1.0 ), 3, cv::KmeansFlags::KMEANS_PP_CENTERS, centers );
			cv::Mat cent = centers.reshape( 3, centers.rows );

			for ( size_t i = 0; i < ARRAYSIZE( m_computedColors ); ++i )
			{
				const auto& px = reinterpret_cast<const Vector&>( cent.at<cv::Vec3f>( i, 0 ) );
				auto& v = m_computedColors[i];
				v.r = static_cast<int>( px.x * 255 + 0.5f );
				v.g = static_cast<int>( px.y * 255 + 0.5f );
				v.b = static_cast<int>( px.z * 255 + 0.5f );
			}
		}
#ifdef DEBUG
	}
	catch ( const cv::Exception& e )
	{
		OutputDebugString( e.what() );
	}
#endif

	s_colorCache.CacheData( hash, hashSize, baseColor, m_computedColors, colorCount );
	m_computedColorCount = colorCount; // set last, signalizes data is valid

#ifdef DEBUG
	timer.End();
	OutputDebugString( CFmtStr( "%s: %g ms\n", m_szName, timer.GetDuration().GetMillisecondsF() ) );
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMaterial::LoadMaterialImage()
{
	Load();

	if ( !m_nWidth || !m_nHeight )
		return false;

	const auto maxWdith = Min( m_nWidth, 512 );
	const auto maxHeight = Min( m_nHeight, 512 );
	const auto size = maxWdith * size_t( maxHeight ) * ( m_TranslucentBaseTexture ? 4 : 3 );
	m_pData = malloc( size );
	Assert( m_pData );
	memset( m_pData, 0, size );

	ImageFormat imageFormat = m_TranslucentBaseTexture ? IMAGE_FORMAT_BGRA8888 : IMAGE_FORMAT_BGR888;

	PreviewImageRetVal_t retVal = CPreviewImagePropertiesCache::GetPreviewImage( m_pMaterial, (unsigned char*)m_pData, maxWdith, maxHeight, imageFormat );
	if ( retVal == MATERIAL_PREVIEW_IMAGE_OK && m_TranslucentBaseTexture ) // premultiply color by alpha
	{
		auto data = reinterpret_cast<unsigned char*>( m_pData );
		for ( size_t i = 0; i < size; i += 4 )
		{
			auto a = data[i + 3];
			data[i + 0] = ( data[i + 0] * a ) / 255;
			data[i + 1] = ( data[i + 1] * a ) / 255;
			data[i + 2] = ( data[i + 2] * a ) / 255;
		}
	}

	if ( retVal == MATERIAL_PREVIEW_IMAGE_OK ) // apply color tint
	{
		const Vector clr = m_baseColor;
		if ( clr.x != 1.0f || clr.y != 1.0f || clr.z != 1.0f )
		{
			const int incr = m_TranslucentBaseTexture ? 4 : 3;
			auto data = reinterpret_cast<unsigned char*>( m_pData );
			for ( size_t i = 0; i < size; i += incr )
			{
				data[i + 0] *= clr.z;
				data[i + 1] *= clr.y;
				data[i + 2] *= clr.x;
			}
		}
	}

	TryLoadColorData();

	return retVal != MATERIAL_PREVIEW_IMAGE_BAD;
}

void CMaterial::TryLoadColorData()
{
	if ( m_computedColorCount > 0 )
		return;

	const auto inMainThread = ThreadInMainThread();
	if ( inMainThread ) // try to regulate memory usage to ~2GB
	{
		constexpr const size_t GB = 1024 * 1024 * 1024;
		PROCESS_MEMORY_COUNTERS counter{};
		if ( GetProcessMemoryInfo( GetCurrentProcess(), &counter, sizeof( counter ) ) && counter.WorkingSetSize / GB >= 2 )
		{
			s_MaterialThreadPool->QueueCall( this, &CMaterial::TryLoadColorData )->Release(); // try again later
			return;
		}
	}

	const auto maxWdith = Min( m_nWidth, 512 );
	const auto maxHeight = Min( m_nHeight, 512 );
	const uint size = maxWdith * maxHeight * 3;
	byte* data = new byte[size];
	memset( data, 0, size );
	if ( CPreviewImagePropertiesCache::GetPreviewImage( m_pMaterial, data, maxWdith, maxHeight, IMAGE_FORMAT_RGB888 ) != MATERIAL_PREVIEW_IMAGE_OK )
	{
		delete[] data;
		return;
	}

	color24 baseColor{ static_cast<byte>( m_baseColor.x * 255 + 0.5f ), static_cast<byte>( m_baseColor.y * 255 + 0.5f ), static_cast<byte>( m_baseColor.z * 255 + 0.5f ) };
	uint hash;
	uint hashSize;
	{
		extern uint32 MurmurHash3_32( const void* key, size_t len, uint32 seed, bool bCaselessStringVariant = false );

		CUtlBuffer buf;
		g_pFullFileSystem->ReadFile( CPreviewImagePropertiesCache::GetPreviewImageFileName( m_pMaterial ), nullptr, buf ); // HACK: need to cache the file itself, or else we miss the cache :(
		hash = MurmurHash3_32( buf.Base(), buf.TellMaxPut(), 1047 );
		hashSize = buf.TellMaxPut();
	}

	int colorCount;
	if ( s_colorCache.GetData( hash, hashSize, baseColor, m_computedColors, colorCount ) ) // if cache hit failed
	{
		m_computedColorCount = colorCount;
		delete[] data;
		return;
	}

	if ( inMainThread ) // queue if in main thread
		s_MaterialThreadPool->QueueCall( this, &CMaterial::LoadColorInfo, CUtlEnvelope( data, size, true ), size, hash, hashSize, baseColor )->Release();
	else
	{
		LoadColorInfo( data, size, hash, hashSize, baseColor );
		delete[] data;
	}
}

bool CMaterial::HasValidColorInformation() const
{
	return m_computedColorCount > 0;
}

uint CMaterial::ClosestColorDist( const color24& clr ) const
{
	if ( clr == m_computedColorValue && m_computedColorDist != UINT32_MAX )
		return m_computedColorDist;

	Vector hsv;
	RGBtoHSV( { clr.r / 255.f, clr.g / 255.f, clr.b / 255.f }, hsv );
	hsv.y *= 255;
	hsv.z *= 255;
	uint dist = UINT_MAX;
	for ( int i = 0; i < m_computedColorCount; i++ )
	{
		const auto& c = m_computedColors[i];
		Vector hsv2;
		RGBtoHSV( { c.r / 255.f, c.g / 255.f, c.b / 255.f }, hsv2 );
		hsv2.y *= 255;
		hsv2.z *= 255;
		const auto d1 = Sqr( c.r - clr.r ) + Sqr( c.g - clr.g ) + Sqr( c.b - clr.b );
		const auto d2 = ( hsv - hsv2 ).LengthSqr();
		if ( const auto cur = d1 + static_cast<uint>( d2 * 2 ); cur < dist )
			dist = cur;
	}

	m_computedColorValue = clr;
	m_computedColorDist = dist;
	return dist;
}

static void InitMaterialSystemConfig( MaterialSystem_Config_t& pConfig )
{
	pConfig.bEditMode = true;
	pConfig.m_nAASamples = 0;
	pConfig.SetFlag( MATSYS_VIDCFG_FLAGS_DISABLE_BUMPMAP, true);
	// When I do this the model browser layout is horked...
	// pConfig->SetFlag( MATSYS_VIDCFG_FLAGS_USING_MULTIPLE_WINDOWS, true );
}


static constexpr const char* const s_rt_names[] = { "_rt_albedo", "_rt_normal", "_rt_position", "_rt_accbuf" };
constexpr const ImageFormat s_rt_formats[]={ IMAGE_FORMAT_RGBA32323232F, IMAGE_FORMAT_RGBA32323232F,
											 IMAGE_FORMAT_RGBA32323232F, IMAGE_FORMAT_RGBA16161616F };

static CTextureReference sg_ExtraFP16Targets[NELEMS( s_rt_names )];

void AllocateLightingPreviewtextures()
{
	constexpr int RT_SIZE = 1024;
	static bool bHaveAllocated=false;
	if ( !bHaveAllocated )
	{
		bHaveAllocated = true;
		MaterialSystemInterface()->BeginRenderTargetAllocation();
		for ( uint idx = 0; idx < NELEMS( sg_ExtraFP16Targets ); idx++ )
			sg_ExtraFP16Targets[idx].Init(
				materials->CreateNamedRenderTargetTextureEx2(
					s_rt_names[idx],
					RT_SIZE, RT_SIZE, RT_SIZE_LITERAL,
					s_rt_formats[idx], idx % 3 ? MATERIAL_RT_DEPTH_NONE : MATERIAL_RT_DEPTH_SEPARATE,
					TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
					CREATERENDERTARGETFLAGS_HDR )
				);

		// End block in which all render targets should be allocated (kicking off an Alt-Tab type
		// behavior)
		MaterialSystemInterface()->EndRenderTargetAllocation();
	}
}

abstract_class IShaderSystem
{
public:
	virtual ShaderAPITextureHandle_t GetShaderAPITextureBindHandle( ITexture *pTexture, int nFrameVar, int nTextureChannel = 0 ) =0;

	// Binds a texture
	virtual void BindTexture( Sampler_t sampler1, ITexture *pTexture, int nFrameVar = 0 ) = 0;
	virtual void BindTexture( Sampler_t sampler1, Sampler_t sampler2, ITexture *pTexture, int nFrameVar = 0 ) = 0;

	// Takes a snapshot
	virtual void TakeSnapshot( ) = 0;

	// Draws a snapshot
	virtual void DrawSnapshot( bool bMakeActualDrawCall = true ) = 0;

	// Are we using graphics?
	virtual bool IsUsingGraphics() const = 0;

	// Are we using graphics?
	virtual bool CanUseEditorMaterials() const = 0;
};

abstract_class IShaderSystemInternal : public IShaderInit, public IShaderSystem
{
public:
	// Initialization, shutdown
	virtual void		Init() = 0;
	virtual void		Shutdown() = 0;
	virtual void		ModInit() = 0;
	virtual void		ModShutdown() = 0;

	// Methods related to reading in shader DLLs
	virtual bool		LoadShaderDLL( const char *pFullPath ) = 0;
	virtual void		UnloadShaderDLL( const char* pFullPath ) = 0;

	// ...
};

abstract_class ILoadShader
{
public:
	virtual void LoadShaderDll( const char* fullDllPath ) = 0;
};


//-----------------------------------------------------------------------------
// Purpose:
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMaterial::Initialize( HWND hwnd )
{
	{
		auto shaderSystem = dynamic_cast<IShaderSystemInternal*>( static_cast<IShaderSystem*>( materials->QueryInterface( "ShaderSystem002" ) ) );
		char szGameDir[_MAX_PATH], szFullGameDir[_MAX_PATH], szBaseDir[_MAX_PATH];
		APP()->GetDirectory( DIR_MOD, szGameDir );

		char* gameDir = szGameDir;
		if ( !V_IsAbsolutePath(szGameDir) )
		{
			V_MakeAbsolutePath( szFullGameDir, _MAX_PATH, szGameDir );
			gameDir = szFullGameDir;
		}

		V_strcat( gameDir, "\\bin", _MAX_PATH );
		V_FixSlashes( gameDir );
		V_FixDoubleSlashes( gameDir );
		V_strcpy_safe( szBaseDir, gameDir );
		V_strcat( gameDir, "\\*.dll", _MAX_PATH );

		FileFindHandle_t find = 0;

		CSysModule* module = nullptr;
		ILoadShader* load = nullptr;
		Sys_LoadInterface( "hammer/bin/hammer_shader_dx9.dll", "ILoadShaderDll001", &module, reinterpret_cast<void**>( &load ) );

		for ( const char* name = g_pFullFileSystem->FindFirstEx( gameDir, nullptr, &find ); name; name = g_pFullFileSystem->FindNext( find ) )
		{
			if ( !V_stristr( name, "shader" ) )
				continue;

			CFmtStrN<MAX_PATH> path( "%s\\%s", szBaseDir, name );

			if ( Sys_LoadModule( path, SYS_NOLOAD ) )
				continue;

			load->LoadShaderDll( path );
		}

		g_pFullFileSystem->FindClose( find );

		g_pFullFileSystem->AddSearchPath( "hammer", "GAME" );
		shaderSystem->LoadShaderDLL( "hammer/bin/hammer_shader_dx9.dll" );

		Sys_UnloadModule( module ); // decrement ref
	}

	// NOTE: This gets set to true later upon creating a 3d view.
	g_materialSystemConfig = materials->GetCurrentConfigForVideoCard();
	InitMaterialSystemConfig( g_materialSystemConfig );

	// Create a cache for material images (for browsing and uploading to the driver).
	if (g_pMaterialImageCache == NULL)
	{
		g_pMaterialImageCache = new CMaterialImageCache(1500);
		if (g_pMaterialImageCache == NULL)
			return false;
	}

	materials->OverrideConfig( g_materialSystemConfig, false );

	// Set the mode
	// When setting the mode, we need to grab the parent window
	// since that's going to enclose all our little render windows
	g_materialSystemConfig.m_VideoMode.m_Width = g_materialSystemConfig.m_VideoMode.m_Height = 0;
	g_materialSystemConfig.m_VideoMode.m_Format = IMAGE_FORMAT_BGRA8888;
	g_materialSystemConfig.m_VideoMode.m_RefreshRate = 0;
	g_materialSystemConfig.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, true );
	g_materialSystemConfig.SetFlag( MATSYS_VIDCFG_FLAGS_RESIZING, true );
	g_materialSystemConfig.SetFlag(	MATSYS_VIDCFG_FLAGS_STENCIL, true );
	g_materialSystemConfig.SetFlag(	MATSYS_VIDCFG_FLAGS_USING_MULTIPLE_WINDOWS, true );

	if ( Options.general.bMaterialProxies )
		materials->SetMaterialProxyFactory( GetHammerMaterialProxyFactory() );

	bool res = materials->SetMode( hwnd, g_materialSystemConfig );

	materials->ReloadMaterials();

	s_MaterialThreadPool = CreateThreadPool();

	const CPUInformation* pCPUInfo = GetCPUInformation();
	ThreadPoolStartParams_t startParams;
	startParams.nThreadsMax = startParams.nThreads = Clamp( pCPUInfo->m_nLogicalProcessors / 2, 1, TP_MAX_POOL_THREADS );
	startParams.nStackSize = 4 * 1024 * 1024;
	startParams.fDistribute = TRS_FALSE;
	startParams.iThreadPriority = -2;
	startParams.bUseAffinityTable = true;
	startParams.bExecOnThreadPoolThreadsOnly = true;
	for ( int i = 0; i < startParams.nThreads; i++ )
		startParams.iAffinityTable[i] = 1 << ( i + 1 );
	s_MaterialThreadPool->Start( startParams, "hammer_materials" );

	s_MaterialThreadPool->QueueCall( &s_colorCache, &CColorCache::Load ); // 1st try to load cached data

	return res;
}



//-----------------------------------------------------------------------------
// Purpose: Restores the material system to an uninitialized state.
//-----------------------------------------------------------------------------
void CMaterial::ShutDown()
{
	for ( uint i = 0; i < NELEMS( sg_ExtraFP16Targets ); ++i )
		sg_ExtraFP16Targets[i].Shutdown();

	if ( materials != NULL )
		materials->UncacheAllMaterials();

	delete g_pMaterialImageCache;
	g_pMaterialImageCache = NULL;

	s_MaterialThreadPool->Stop(); // wait for all threads to finish
	DestroyThreadPool( s_MaterialThreadPool );

	s_colorCache.Save(); // but save on main thread
}