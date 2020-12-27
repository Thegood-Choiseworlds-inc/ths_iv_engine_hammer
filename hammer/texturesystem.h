//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Texture management functions. Exposes a list of available textures,
//			texture groups, and Most Recently Used textures.
//
//=============================================================================//

#ifndef TEXTURESYSTEM_H
#define TEXTURESYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "ieditortexture.h"
#include "material.h"
#include "utlvector.h"
#include "utldict.h"
#include "FileChangeWatcher.h"


class CGameConfig;
class CTextureSystem;


//-----------------------------------------------------------------------------
// Purpose: Defines the interface to a set of textures of a given texture format.
//			The textures are stored as an index into the global array of textures.
//-----------------------------------------------------------------------------
class CTextureGroup
{
public:
	CTextureGroup( const char* pszName );

	const char* GetName() const { return m_szName; }
	int GetCount() const { return m_Textures.Count(); }

	void AddTexture( IEditorTexture* pTexture );
	void Sort( const color24* color );

	IEditorTexture* GetTexture( int nIndex );
	IEditorTexture* GetTexture( char const* pName );

	// Fast find texture..
	IEditorTexture* FindTextureByName( const char* pName, int* piIndex );

	// Used to lazily load in all the textures
	void LazyLoadTextures();

	void CalcColorInfoForAllMaterials();

protected:

	char m_szName[MAX_PATH];
	CUtlVector<IEditorTexture*> m_Textures;
	CUtlDict<int, int> m_TextureNameMap;	// Maps the texture name to an index into m_Textures (the key is IEditorTexture::GetName).

	// Used to lazily load the textures in the group
	int	m_nTextureToLoad;
};


typedef CUtlVector<CTextureGroup*> TextureGroupList_t;


//
// When the user switches game configs, all the textures and materials are switched.
// This structure holds all the context necessary to accomplish this.
//
struct TextureContext_t
{
	CGameConfig*		pConfig;	// The game config that this texture context corresponds to.
	CTextureGroup*		pAllGroup;
	TextureGroupList_t	Groups;
	EditorTextureList_t	MRU;		// List of Most Recently Used textures, first is the most recent.
	EditorTextureList_t	Dummies;	// List of Dummy textures - textures that were created to hold the place of missing textures.
};


class CMaterialFileChangeWatcher final : private CFileChangeWatcher::ICallbacks
{
public:
	void Init( CTextureSystem* pSystem, int context );
	void Update();	// Call this periodically to update.

private:
	// CFileChangeWatcher::ICallbacks..
	void OnFileChange( const char* pRelativeFilename, const char* pFullFilename ) override;

private:
	CFileChangeWatcher m_Watcher;
	CTextureSystem* m_pTextureSystem;
	int m_Context;
};


class CTextureSystem final : public IMaterialEnumerator
{
public:
	friend class CMaterialFileChangeWatcher;

	CTextureSystem();
	~CTextureSystem() = default;

	bool Initialize( HWND hwnd );
	void ShutDown();

	void SetActiveConfig( CGameConfig* pConfig );

	//
	// Exposes a list of texture groups (sets of textures of a given format).
	//
	void SetActiveGroup( const char* pcszName );
	int GroupsGetCount() const;
	CTextureGroup* GroupsGet( int nIndex ) const;

	//
	// Exposes a list of active textures based on the currently active texture group.
	//
	int GetActiveTextureCount() const;
	IEditorTexture* GetActiveTexture( int nIndex ) const;
	IEditorTexture* EnumActiveTextures( int& piIndex ) const;
	IEditorTexture* FindActiveTexture( LPCSTR pszName, int* piIndex = NULL, BOOL bDummy = TRUE );
	bool HasTexturesForConfig( CGameConfig* pConfig );

	//
	// Exposes a list of Most Recently Used textures.
	//
	void AddMRU( IEditorTexture* pTex );
	int MRUGetCount() const;
	IEditorTexture* MRUGet( int nIndex ) const;

	//
	// Exposes a list of all unique keywords found in the master texture list.
	//
	int GetNumKeywords() const;
	const char* GetKeyword( int index ) const;

	//
	// Holds a list of placeholder textures used when a map refers to missing textures.
	//
	IEditorTexture* AddDummy( LPCTSTR pszName );

	//
	// Load graphics files from options list.
	//
	void LoadAllGraphicsFiles();

	// IMaterialEnumerator interface, Used to add all the world materials into the material list.
	bool EnumMaterial( const char* pMaterialName, int nContext ) override;

	// Used to lazily load in all the textures during app idle.
	void LazyLoadTextures();

	// Registers the keywords as existing in a particular material.
	void RegisterTextureKeywords( IEditorTexture* pTexture );

	// Opens the source file associated with a material.
	void OpenSource( const char* pMaterialName );

	// Opens explorer dialog and selects the source file
	void ExploreToSource( const char* pMaterialName );

	// Reload individual textures.
	void ReloadTextures( const char* pFilterName );

	// bind local cubemap again
	void RebindDefaultCubeMap();

	void UpdateFileChangeWatchers();

	// Gets tools/toolsnodraw
	IEditorTexture* GetNoDrawTexture() const { return m_pNoDrawTexture; }

	void ReSort( const color24* color );

	void CalcColorInfoForAllMaterials();

protected:

// CMaterialFileChangeWatcher stuff - watches for changes to VMTs or VTFs and handles them.

	enum EFileType
	{
		k_eFileTypeVMT,
		k_eFileTypeVTF
	};
	void OnFileChange( const char* pFilename, int context, EFileType eFileType );
	void ReloadMaterialsUsingTexture( ITexture* pTestTexture );

	static bool GetFileTypeFromFilename( const char* pFilename, CTextureSystem::EFileType& pFileType );

	CUtlVector<CMaterialFileChangeWatcher*> m_ChangeWatchers;

// Internal stuff.

	void FreeAllTextures();

	TextureContext_t* AddTextureContext();
	TextureContext_t* FindTextureContextForConfig( CGameConfig* pConfig );

	int AddTexture( IEditorTexture* pTexture );

	void LoadMaterials( CGameConfig* pConfig );

	//
	// Master array of textures.
	//
	CUtlVector<IEditorTexture*> m_Textures;

	IEditorTexture* m_pLastTex;
	int m_nLastIndex;

	//
	// List of groups (sets of textures of a given texture format). Only one
	// group can be active at a time, based on the game configuration.
	//
	CUtlVector<TextureContext_t> m_TextureContexts;		// One per game config.
	TextureContext_t* m_pActiveContext;					// Points to the active entry in m_TextureContexts.
	CTextureGroup* m_pActiveGroup;						// Points to the active entry in m_TextureContexts.

	//
	// List of keywords found in all textures.
	//
	CUtlVector<const char*> m_Keywords;

	// default cubemap
	ITexture* m_pCubemapTexture;

	// tools/toolsnodraw
	IEditorTexture* m_pNoDrawTexture;
};


//-----------------------------------------------------------------------------
// Purpose: Returns the number of textures in the active group.
//-----------------------------------------------------------------------------
inline int CTextureSystem::GetActiveTextureCount() const
{
	return m_pActiveGroup ? m_pActiveGroup->GetCount() : 0;
}


inline IEditorTexture* CTextureSystem::GetActiveTexture( int nIndex ) const
{
	return m_pActiveGroup ? m_pActiveGroup->GetTexture( nIndex ) : nullptr;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline int CTextureSystem::GroupsGetCount() const
{
	return m_pActiveContext ? m_pActiveContext->Groups.Count() : 0;
}

inline CTextureGroup* CTextureSystem::GroupsGet( int nIndex ) const
{
	return m_pActiveContext ? m_pActiveContext->Groups.Element( nIndex ) : nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Initiates an iteration of the MRU list.
//-----------------------------------------------------------------------------
inline int CTextureSystem::MRUGetCount() const
{
	return m_pActiveContext ? m_pActiveContext->MRU.Count() : 0;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the next texture in the MRU of the given format.
// Input  : pos - Iterator.
//			eDesiredFormat - Texture format to return.
// Output : Pointer to the texture.
//-----------------------------------------------------------------------------
inline IEditorTexture* CTextureSystem::MRUGet( int nIndex ) const
{
	return m_pActiveContext ? m_pActiveContext->MRU.Element( nIndex ) : nullptr;
}


extern CTextureSystem g_Textures;


#endif // TEXTURESYSTEM_H