// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH

#ifndef SCREENSPACEEFFFECT_VS20_H
#define SCREENSPACEEFFFECT_VS20_H

#include "shaderapi/ishaderapi.h"
#include "shaderapi/ishadershadow.h"
#include "materialsystem/imaterialvar.h"

class screenspaceefffect_vs20_Static_Index
{
	int m_nVERTEXCOLOR : 3;
	int m_nTRANSFORMVERTS : 3;
public:
	void SetVERTEXCOLOR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVERTEXCOLOR = i;
	}

	void SetTRANSFORMVERTS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nTRANSFORMVERTS = i;
	}

	screenspaceefffect_vs20_Static_Index( IShaderShadow* pShaderShadow, IMaterialVar** params )
	{
		m_nVERTEXCOLOR = 0;
		m_nTRANSFORMVERTS = 0;
	}

	int GetIndex()
	{
		return ( 1 * m_nVERTEXCOLOR ) + ( 2 * m_nTRANSFORMVERTS ) + 0;
	}
};

#define shaderStaticTest_screenspaceefffect_vs20 1


class screenspaceefffect_vs20_Dynamic_Index
{
public:
	screenspaceefffect_vs20_Dynamic_Index( IShaderDynamicAPI* pShaderAPI )
	{
	}

	int GetIndex()
	{
		return 0;
	}
};

#define shaderDynamicTest_screenspaceefffect_vs20 1


#endif	// SCREENSPACEEFFFECT_VS20_H