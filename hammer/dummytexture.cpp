//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of IEditorTexture interface for placeholder textures.
//			Placeholder textures are used for textures that are referenced in
//			the map file but not found in storage.
//
//=============================================================================//

#include "stdafx.h"
#include "dummytexture.h"
#include "pixelwriter.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "tier1/KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

IMaterial* CDummyTexture::errorMaterial = nullptr;

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CDummyTexture::CDummyTexture( const char* pszName )
{
	if ( pszName != NULL )
		strcpy( m_szName, pszName );
	else
		strcpy( m_szName, "Missing texture" );

	if ( !errorMaterial )
	{
		errorMaterial = materials->CreateMaterial( "__editor_error", new KeyValues( "UnlitGeneric", "$basetexture", "error" ) );
		errorMaterial->AddRef();
	}
}

void CDummyTexture::DestroyDummyMaterial()
{
	if ( errorMaterial )
	{
		errorMaterial->DecrementReferenceCount();
		errorMaterial->DeleteIfUnreferenced();
		errorMaterial = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns a string of comma delimited keywords associated with this
//			material.
// Input  : pszKeywords - Buffer to receive keywords, NULL to query string length.
// Output : Returns the number of characters in the keyword string.
//-----------------------------------------------------------------------------
int CDummyTexture::GetKeywords( char* pszKeywords ) const
{
	if ( pszKeywords != NULL )
		*pszKeywords = '\0';

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pszName -
// Output :
//-----------------------------------------------------------------------------
int CDummyTexture::GetShortName( char* pszName ) const
{
	if ( pszName == NULL )
		return V_strlen( m_szName );

	V_strcpy( pszName, m_szName );
	return V_strlen( pszName );
}


//-----------------------------------------------------------------------------
// Purpose: Renders "No Image" into a device context as a placeholder for the
//			missing texture.
// Input  : pDC -
//			rect -
//			iFontHeight -
//			dwFlags -
//-----------------------------------------------------------------------------
void CDummyTexture::Draw( CDC* pDC, const RECT& rect, int iFontHeight, int iIconHeight, const DrawTexData_t& DrawTexData )
{
	CFont* pOldFont = (CFont*)pDC->SelectStockObject( ANSI_VAR_FONT );
	COLORREF crText = pDC->SetTextColor( RGB( 0xff, 0xff, 0xff ) );
	COLORREF crBack = pDC->SetBkColor( RGB( 0, 0, 0 ) );

	auto width = rect.right - rect.left;
	auto height = rect.bottom - rect.top;

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
	writer.SetPixelMemory( IMAGE_FORMAT_RGBA8888, data, width * 4 );

	constexpr int boxSize = 16;
	for ( int y = 0; y < height; ++y )
	{
		writer.Seek( 0, y );
		for ( int x = 0; x < width; ++x )
		{
			if ( ( x & boxSize ) ^ ( y & boxSize ) )
				writer.WritePixel( 0, 0, 0, 255 );
			else
				writer.WritePixel( 255, 0, 255, 255 );
		}
	}

	hdc.SelectObject( bitmap );
	pDC->BitBlt( rect.left, rect.top, width, height, &hdc, 0, 0, SRCCOPY );
	DeleteObject( bitmap );

	pDC->TextOut( rect.left + 2, rect.top + 2, "No Image", 8 );

	pDC->SelectObject( pOldFont );
	pDC->SetTextColor( crText );
	pDC->SetBkColor( crBack );
}