//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef GLOBALFUNCTIONS_H
#define GLOBALFUNCTIONS_H
#ifdef _WIN32
#pragma once
#endif

#include "mapclass.h"	// For CMapObjectList


class CMapSolid;
class CMainFrame;
class CMapWorld;
class CMapDoc;
class IWorldEditDispMgr;
class CSubdivMesh;


CMapWorld *GetActiveWorld(void);

IWorldEditDispMgr *GetActiveWorldEditDispManager( void );

//
// misc.cpp:
//
void randomize();
DWORD random();

void NotifyDuplicates(CMapSolid *pSolid);
void NotifyDuplicates(const CMapObjectList *pList);

bool IsWorldObject(CMapAtom *pObject);

BOOL AddLeavesToListCallback(CMapClass *pObject, CMapObjectList *pList);

void WriteDebug(const char *pszStr);
LPCTSTR GetDefaultTextureName();
void SetDefaultTextureName( const char *szTexName );
int mychdir(LPCTSTR pszDir);

//
// Message window interface.
//
class CMessageWnd;

enum MWMSGTYPE
{	mwStatus,
	mwError,
	mwWarning
};

extern CMessageWnd *g_pwndMessage;

void Msg(int type, PRINTF_FORMAT_STRING const char *fmt, ...);

// noise function
float PerlinNoise2D( float x, float y, float rockiness );
float PerlinNoise2DScaled( float x, float y, float rockiness );

void DBG(PRINTF_FORMAT_STRING const char *fmt, ...);

#endif // GLOBALFUNCTIONS_H
