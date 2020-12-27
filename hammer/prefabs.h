//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PREFABS_H
#define PREFABS_H
#pragma once


#include <afxtempl.h>

class BoundBox;
class CMapClass;
class CPrefab;
class CPrefabLibrary;


const POSITION ENUM_START = POSITION(1);
const int MAX_NOTES = 501;


enum
{
	pt3D,
};


enum LibraryType_t
{
	LibType_None,
	LibType_HalfLife,
	LibType_HalfLife2,
};


typedef CTypedPtrList<CPtrList, CPrefab*> CPrefabList;
typedef CTypedPtrList<CPtrList, CPrefabLibrary*> CPrefabLibraryList;


class CPrefab
{
public:

	CPrefab(void);
	virtual ~CPrefab(void);

	virtual int Save(LPCTSTR pszFilename, DWORD = 0) = 0;
	virtual int Load(DWORD = 0) = 0;

	// set info:
	void SetName(LPCTSTR pszName)
	{ strcpy(szName, pszName); }
	void SetNotes(LPCTSTR pszNotes)
	{ strcpy(szNotes, pszNotes); }

	// get info:
	LPCTSTR GetName() { return szName; }
	LPCTSTR GetNotes() { return szNotes; }

	// unique id assigned at creation time:
	DWORD GetID() { return dwID; }

	DWORD GetLibraryID() { return dwLibID; }

	// common interface:
	virtual CMapClass *CreateInBox(BoundBox *pBox) = 0;
	virtual int GetType() = 0;
	virtual void FreeData() = 0;
	virtual bool IsLoaded() = 0;

	// static misc stuff:
	static CPrefab* FindID(DWORD dwID);

	// caching:
	static void FreeAllData();	// free ALL objects' data

protected:

	char szName[31];
	char szNotes[MAX_NOTES];
	DWORD dwID;
	DWORD dwLibID;	// library id
	
	DWORD dwFileOffset;
	DWORD dwFileSize;	// size in file - for copying purposes

	static CPrefabList PrefabList;

friend class CPrefabLibrary;
friend class CPrefabLibraryVMF;
};


//
// A collection of prefabs.
//
class CPrefabLibrary
{
public:
	CPrefabLibrary();
	virtual ~CPrefabLibrary();

	virtual int Load(LPCTSTR pszFilename) = 0;
	virtual bool IsFile(const char *szFile) = 0;

	void SetNameFromFilename(LPCTSTR pszFilename);
	virtual int SetName(const char *pszName) = 0;
	void SetNotes(LPCTSTR pszNotes)
	{
		strcpy(szNotes, pszNotes);
	}

	// get info:
	LPCTSTR GetName() { return m_szName; }
	LPCTSTR GetNotes() { return szNotes; }

	// unique id assigned at creation time:
	DWORD GetID() { return dwID; }

	CPrefab * EnumPrefabs(POSITION& p);
	void Add(CPrefab *pPrefab);
	void Remove(CPrefab *pPrefab);

	static CPrefabLibrary *FindID(DWORD dwID);
	static CPrefabLibrary *EnumLibraries(POSITION &p);
	static void LoadAllLibraries(void);
	static void FreeAllLibraries(void);
	static CPrefabLibrary *FindOpenLibrary(LPCTSTR pszFilename);

protected:

	void FreePrefabs();

	static CPrefabLibraryList PrefabLibraryList;

	CPrefabList Prefabs;
	char m_szName[31];
	char szNotes[MAX_NOTES];
	DWORD dwID;

friend class CPrefab;
friend class CPrefabVMF;
};

class CPrefabLibraryVMF : public CPrefabLibrary
{
public:
	CPrefabLibraryVMF();
	~CPrefabLibraryVMF();

	bool IsFile(const char *szFile);
	int Load(LPCTSTR pszFilename);
	int Save(LPCTSTR pszFilename = NULL, BOOL bIndexOnly = FALSE);
	int SetName(const char *pszName);

protected:

	char m_szFolderName[MAX_PATH];

friend class CPrefab;
};


#endif // PREFABS_H
