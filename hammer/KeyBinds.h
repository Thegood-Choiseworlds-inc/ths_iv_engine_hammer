#pragma once

#include "keyboard.h"
#include "UtlStringMap.h"
#include "utlstring.h"
#include "utlvector.h"

#include "resource.h"
#include "VGuiWnd.h"

class KeyValues;
class CUtlSymbolTable;
class CKeyBoardEditorDialog; // not the one in vgui

#define BEGIN_KEYMAP(mapping) \
	CUtlStringMap<KeyMap_t> mappings; \
	if (g_pKeyBinds->LoadKeybinds(mapping, mappings))

#define _ADD_KEY(string, enumVal) \
	if (mappings.Defined(string)) \
	{ \
		KeyMap_t map = mappings[string]; \
		m_Keyboard.AddKeyMap(map.uChar, map.uModifierKeys, enumVal); \
	}

#define ADD_KEY(enumVal) _ADD_KEY(#enumVal, enumVal)

class KeyBinds
{
public:
	KeyBinds();
	~KeyBinds();

	bool Init();
	void Save();

	bool LoadKeybinds( const char* pMapping, CUtlStringMap<KeyMap_t>& map );

	bool GetAccelTableFor( const char* pMapping, HACCEL& out );

	bool EditingKeybinds() const { return m_bEditingKeybinds; }

	struct BindingInfo_t
	{
		CUtlString name;
		unsigned int keyCode;
		unsigned int modifiers;
		bool virt;
		CUtlString help;
	};

	KeyValues* EnumerateBindings( KeyValues* iter, CUtlVector<BindingInfo_t>& list );
	void UpdateBindings( const char* group, const CUtlVector<BindingInfo_t>& list );
	void GetDefaults( const char* group, CUtlVector<BindingInfo_t>& list );
private:
	KeyValues* m_pKvKeybinds;
	KeyValues* m_pKvDefaultKeybinds;
	bool m_bEditingKeybinds;

	friend class CKeybindEditor;
};

extern KeyBinds* g_pKeyBinds;


class CKeybindEditor : public CVguiDialog
{
	DECLARE_DYNAMIC(CKeybindEditor)

public:
	CKeybindEditor( CWnd* pParent = NULL );   // standard constructor
	~CKeybindEditor() override;

	// Dialog Data
	enum { IDD = IDD_KEYBIND_EDITOR };

protected:
	virtual void DoDataExchange( CDataExchange* pDX );    // DDX/DDV support
	virtual BOOL PreTranslateMessage( MSG* pMsg );

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnDestroy();
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );

	virtual BOOL OnInitDialog();

	void SaveLoadSettings( bool bSave );
	void Resize();

	CVGuiPanelWnd			m_VGuiWindow;

	CKeyBoardEditorDialog*	m_pDialog;

	void Show();
	void Hide();
};