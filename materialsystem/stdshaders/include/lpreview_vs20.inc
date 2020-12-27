// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH

#ifndef LPREVIEW_VS20_H
#define LPREVIEW_VS20_H

#include "shaderapi/ishaderapi.h"
#include "shaderapi/ishadershadow.h"
#include "materialsystem/imaterialvar.h"

class lpreview_vs20_Static_Index
{
public:
	lpreview_vs20_Static_Index( IShaderShadow* pShaderShadow, IMaterialVar** params )
	{
	}

	int GetIndex()
	{
		return 0;
	}
};

#define shaderStaticTest_lpreview_vs20 1


class lpreview_vs20_Dynamic_Index
{
public:
	lpreview_vs20_Dynamic_Index( IShaderDynamicAPI* pShaderAPI )
	{
	}

	int GetIndex()
	{
		return 0;
	}
};

#define shaderDynamicTest_lpreview_vs20 1


#endif	// LPREVIEW_VS20_H