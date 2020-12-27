#include "stdafx.h"
#include "KeyBinds.h"
#include "resource.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "fmtstr.h"
#include "mapview3d.h"

#undef GetCurrentTime

#include "utlcommon.h"

#undef PropertySheet
#undef MessageBox

#include "vgui_controls/Button.h"
#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/SectionedListPanel.h"
#include "vgui_controls/TextEntry.h"

#include "vgui/IInput.h"
#include "vgui/IVGui.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"

#include "HammerVGui.h"
#include "inputsystem/iinputsystem.h"

#include "tier0/memdbgon.h"

#define KEYBINDS_CONFIG_FILE "HammerKeyConfig.cfg"
#define KEYBINDS_CONFIG_DEFAULT_FILE "HammerKeyConfig_default.cfg"

static KeyBinds s_KeyBinds;
KeyBinds *g_pKeyBinds = &s_KeyBinds;

struct KeyNames_t
{
	WORD		code;
	char const	*string;
	char const	*displaystring;
};

static constexpr KeyNames_t g_KeyNames[] =
{
	{'0', "0", "0"},
	{'1', "1", "1"},
	{'2', "2", "2"},
	{'3', "3", "3"},
	{'4', "4", "4"},
	{'5', "5", "5"},
	{'6', "6", "6"},
	{'7', "7", "7"},
	{'8', "8", "8"},
	{'9', "9", "9"},
	{'A', "A", "A"},
	{'B', "B", "B"},
	{'C', "C", "C"},
	{'D', "D", "7"},
	{'E', "E", "E"},
	{'F', "F", "F"},
	{'G', "G", "G"},
	{'H', "H", "H"},
	{'I', "I", "I"},
	{'J', "J", "J"},
	{'K', "K", "K"},
	{'L', "L", "L"},
	{'M', "M", "M"},
	{'N', "N", "N"},
	{'O', "O", "O"},
	{'P', "P", "P"},
	{'Q', "Q", "Q"},
	{'R', "R", "R"},
	{'S', "S", "S"},
	{'T', "T", "T"},
	{'U', "U", "U"},
	{'V', "V", "V"},
	{'W', "W", "W"},
	{'X', "X", "X"},
	{'Y', "Y", "Y"},
	{'Z', "Z", "Z"},
	{VK_NUMPAD0, "VK_NUMPAD0", "Key Pad 0"},
	{VK_NUMPAD1, "VK_NUMPAD1", "Key Pad 1"},
	{VK_NUMPAD2, "VK_NUMPAD2", "Key Pad 2"},
	{VK_NUMPAD3, "VK_NUMPAD3", "Key Pad 3"},
	{VK_NUMPAD4, "VK_NUMPAD4", "Key Pad 4"},
	{VK_NUMPAD5, "VK_NUMPAD5", "Key Pad 5"},
	{VK_NUMPAD6, "VK_NUMPAD6", "Key Pad 6"},
	{VK_NUMPAD7, "VK_NUMPAD7", "Key Pad 7"},
	{VK_NUMPAD8, "VK_NUMPAD8", "Key Pad 8"},
	{VK_NUMPAD9, "VK_NUMPAD9", "Key Pad 9"},
	{VK_DIVIDE, "VK_DIVIDE", "Key Pad /"},
	{VK_MULTIPLY, "VK_MULTIPLY", "Key Pad *"},
	{VK_OEM_MINUS, "VK_OEM_MINUS", "Key Pad -"},
	{VK_OEM_PLUS, "VK_OEM_PLUS", "Key Pad +"},
	{VK_RETURN, "VK_RETURN", "Key Pad Enter"},
	{VK_DECIMAL, "VK_DECIMAL", "Key Pad ."},
	{VK_OEM_6, "VK_OEM_6", "["},
	{']', "]", "]"},
	{VK_OEM_1, "VK_OEM_1", ";"},
	{VK_OEM_7, "VK_OEM_7", "'"},
	{VK_OEM_3, "VK_OEM_3", "`"},
	{VK_OEM_COMMA, "VK_OEM_COMMA", ","},
	{VK_DECIMAL, "VK_DECIMAL", "."},
	{VK_OEM_2, "VK_OEM_2", "/"},
	{VK_OEM_5, "VK_OEM_5", "\\"},
	{VK_OEM_MINUS, "VK_OEM_MINUS", "-"},
	{VK_OEM_PLUS, "VK_OEM_PLUS", "="},
	{VK_RETURN, "VK_RETURN", "Enter"},
	{VK_SPACE, "VK_SPACE", "Space"},
	{VK_BACK, "VK_BACK", "Backspace"},
	{VK_TAB, "VK_TAB", "Tab"},
	{VK_CAPITAL, "VK_CAPITAL", "Caps Lock"},
	{VK_NUMLOCK, "VK_NUMLOCK", "Num Lock"},
	{VK_ESCAPE, "VK_ESCAPE", "Escape"},
	{VK_SCROLL, "VK_SCROLL", "Scroll Lock"},
	{VK_INSERT, "VK_INSERT", "Ins"},
	{VK_DELETE, "VK_DELETE", "Del"},
	{VK_HOME, "VK_HOME", "Home"},
	{VK_END, "VK_END", "End"},
	{VK_PRIOR, "VK_PRIOR", "PgUp"},
	{VK_NEXT, "VK_NEXT", "PgDn"},
	{VK_PAUSE, "VK_PAUSE", "Break"},
	{VK_LSHIFT, "VK_LSHIFT", "Shift"},
	{VK_RSHIFT, "VK_RSHIFT", "Shift"},
	{VK_MENU, "VK_MENU", "Alt"},
	{VK_LCONTROL, "VK_LCONTROL", "Ctrl"},
	{VK_RCONTROL, "VK_RCONTROL", "Ctrl"},
	{VK_LWIN, "VK_LWIN", "Windows"},
	{VK_RWIN, "VK_RWIN", "Windows"},
	{VK_APPS, "VK_APPS", "App"},
	{VK_UP, "VK_UP", "Up"},
	{VK_LEFT, "VK_LEFT", "Left"},
	{VK_DOWN, "VK_DOWN", "Down"},
	{VK_RIGHT, "VK_RIGHT", "Right"},
	{VK_F1, "VK_F1", "F1"},
	{VK_F2, "VK_F2", "F2"},
	{VK_F3, "VK_F3", "F3"},
	{VK_F4, "VK_F4", "F4"},
	{VK_F5, "VK_F5", "F5"},
	{VK_F6, "VK_F6", "F6"},
	{VK_F7, "VK_F7", "F7"},
	{VK_F8, "VK_F8", "F8"},
	{VK_F9, "VK_F9", "F9"},
	{VK_F10, "VK_F10", "F10"},
	{VK_F11, "VK_F11", "F11"},
	{VK_F12, "VK_F12", "F12"}
};


WORD GetKeyForStr(const char *pStr)
{
	for (const auto& t : g_KeyNames)
	{
		if (!Q_stricmp(pStr, t.displaystring) || !Q_stricmp(pStr, t.string))
			return t.code;
	}

	return 0;
}

const char *GetStrForKey(WORD key)
{
	for (const auto& t : g_KeyNames)
	{
		if (t.code == key)
			return t.string;
	}

	return nullptr;
}



struct CommandNames_t
{
	const char *pKeyName;
	int iCommand;
};

#define COM(x) {#x, x}

// IDR_MAINFRAME
static constexpr CommandNames_t s_CommandNamesMain[] =
{
	COM( ID_CONTEXT_HELP ),
	COM( ID_EDIT_CUT ),
	COM( ID_EDIT_PASTE ),
	COM( ID_EDIT_UNDO ),
	COM( ID_FILE_NEW ),
	COM( ID_FILE_OPEN ),
	COM( ID_FILE_PRINT ),
	COM( ID_FILE_SAVE ),
	COM( ID_HELP_FINDER ),
	COM( ID_NEXT_PANE ),
	COM( ID_PREV_PANE ),
	COM( ID_VIEW_2DXY ),
	COM( ID_VIEW_2DXZ ),
	COM( ID_VIEW_2DYZ ),
	COM( ID_VIEW_3DTEXTURED ),
	COM( ID_VIEW_3DTEXTURED_SHADED ),
};

// IDR_MAPDOC
static constexpr CommandNames_t s_CommandNamesDoc[] =
{
	COM( ID_CREATEOBJECT ),
	COM( ID_EDIT_APPLYTEXTURE ),
	COM( ID_EDIT_CLEARSELECTION ),
	COM( ID_EDIT_COPYWC ),
	COM( ID_EDIT_CUTWC ),
	COM( ID_EDIT_FINDENTITIES ),
	COM( ID_EDIT_PASTESPECIAL ),
	COM( ID_EDIT_PASTEWC ),
	COM( ID_EDIT_PROPERTIES ),
	COM( ID_EDIT_REDO ),
	COM( ID_EDIT_REPLACE ),
	COM( ID_EDIT_TOENTITY ),
	COM( ID_EDIT_TOWORLD ),
	COM( ID_FILE_EXPORTAGAIN ),
	COM( ID_FILE_RUNMAP ),
	COM( ID_FLIP_HORIZONTAL ),
	COM( ID_FLIP_VERTICAL ),
	COM( ID_GOTO_BRUSH ),
	COM( ID_HELP_TOPICS ),
	COM( ID_INSERTPREFAB_ORIGINAL ),
	COM( ID_MAP_CHECK ),
	COM( ID_MAP_GRIDHIGHER ),
	COM( ID_MAP_GRIDLOWER ),
	COM( ID_MAP_SNAPTOGRID ),
	COM( ID_MODE_APPLICATOR ),
	COM( ID_TEST ),
	COM( ID_TOGGLE_GROUPIGNORE ),
	COM( ID_TOOLS_APPLYDECALS ),
	COM( ID_TOOLS_BLOCK ),
	COM( ID_TOOLS_CAMERA ),
	COM( ID_TOOLS_CLIPPER ),
	COM( ID_TOOLS_CREATEPREFAB ),
	COM( ID_TOOLS_DISPLACE ),
	COM( ID_TOOLS_ENTITY ),
	COM( ID_TOOLS_GROUP ),
	COM( ID_TOOLS_HIDE_ENTITY_NAMES ),
	COM( ID_TOOLS_MAGNIFY ),
	COM( ID_TOOLS_MORPH ),
	COM( ID_TOOLS_OVERLAY ),
	COM( ID_TOOLS_PATH ),
	COM( ID_TOOLS_POINTER ),
	COM( ID_TOOLS_SNAP_SELECTED_TO_GRID_INDIVIDUALLY ),
	COM( ID_TOOLS_SNAPSELECTEDTOGRID ),
	COM( ID_TOOLS_SOUND_BROWSER ),
	COM( ID_TOOLS_SPLITFACE ),
	COM( ID_TOOLS_SUBTRACTSELECTION ),
	COM( ID_TOOLS_TOGGLETEXLOCK ),
	COM( ID_TOOLS_TRANSFORM ),
	COM( ID_TOOLS_UNGROUP ),
	COM( ID_VIEW_QUICKHIDE ),
	COM( ID_VIEW_QUICKHIDEUNSELECTED ),
	COM( ID_VIEW_QUICKHIDEVISGROUP ),
	COM( ID_VIEW_QUICKUNHIDE ),
	COM( ID_VIEW3D_BRIGHTER ),
	COM( ID_VIEW3D_DARKER ),
	COM( ID_VIEW_AUTOSIZE4 ),
	COM( ID_VIEW_CENTER3DVIEWSONSELECTION ),
	COM( ID_VIEW_CENTERONSELECTION ),
	COM( ID_VIEW_GRID ),
	COM( ID_VIEW_MAXIMIZERESTOREACTIVEVIEW ),
	COM( ID_VIEW_SHOWMODELSIN2D ),
	COM( ID_VIEW_TEXTUREBROWSER ),
	COM( ID_VSCALE_TOGGLE ),
	COM( ID_TOOLS_SPRINKLE ),
	COM( ID_TOOLS_MODEL_BROWSER ),
};

int GetIDForCommandStr(const char *pName)
{
	for (const auto& t : s_CommandNamesMain)
	{
		if (!Q_stricmp(pName, t.pKeyName))
			return t.iCommand;
	}

	for (const auto& t : s_CommandNamesDoc)
	{
		if (!Q_stricmp(pName, t.pKeyName))
			return t.iCommand;
	}

	return 0;
}

const char *GetStrForCommandID(int ID)
{
	for (const auto& t : s_CommandNamesMain)
	{
		if (t.iCommand == ID)
			return t.pKeyName;
	}

	for (const auto& t : s_CommandNamesDoc)
	{
		if (t.iCommand == ID)
			return t.pKeyName;
	}

	return nullptr;
}

KeyBinds::KeyBinds() : m_pKvKeybinds( nullptr ), m_pKvDefaultKeybinds( nullptr ), m_bEditingKeybinds( false )
{
}

KeyBinds::~KeyBinds()
{
	if (m_pKvKeybinds)
	{
		m_pKvKeybinds->deleteThis();
		m_pKvKeybinds = nullptr;
	}
	if (m_pKvDefaultKeybinds)
	{
		m_pKvDefaultKeybinds->deleteThis();
		m_pKvDefaultKeybinds = nullptr;
	}
}

bool KeyBinds::Init()
{
	m_pKvKeybinds = new KeyValues("KeyBindings");
	m_pKvDefaultKeybinds = new KeyValues( "KeyBindings" );
	if ( !m_pKvDefaultKeybinds->LoadFromFile( g_pFullFileSystem, KEYBINDS_CONFIG_DEFAULT_FILE, "hammer_cfg" ) )
	{
		// If we weren't even able to load the default, we're screwed
		return false;
	}
	if ( !m_pKvKeybinds->LoadFromFile( g_pFullFileSystem, KEYBINDS_CONFIG_FILE, "hammer_cfg" ) )
	{
		// Save them out for the first time
		m_pKvDefaultKeybinds->CopySubkeys( m_pKvKeybinds );

		// remove all help infos
		FOR_EACH_SUBKEY( m_pKvKeybinds, group )
		{
			FOR_EACH_SUBKEY( group, bind )
			{
				KeyValues* info = bind->FindKey( "info" );
				if ( info )
				{
					bind->RemoveSubKey( info );
					info->deleteThis();
				}
			}
		}
		Save();
	}

	return true;
}

void KeyBinds::Save()
{
	// Write out to file if we're valid
	if ( m_pKvKeybinds )
		m_pKvKeybinds->SaveToFile( g_pFullFileSystem, KEYBINDS_CONFIG_FILE, "hammer_cfg" );
}

bool KeyBinds::LoadKeybinds( const char* pMapping, CUtlStringMap<KeyMap_t>& map )
{
	if (pMapping)
	{
		KeyValues* pKvKeybinds = m_pKvKeybinds->FindKey( pMapping );
		if ( pKvKeybinds )
		{
			bool bMappedAKey = false; // Did we at least map a key?
			FOR_EACH_TRUE_SUBKEY( pKvKeybinds, pKvKeyMap )
			{
				// Each key map has a command as its name, and the key, and modifiers
				KeyMap_t mapping;

				const char* pKey = pKvKeyMap->GetString( "key", nullptr );
				// If there's no/empty key, keep moving forward
				if ( !pKey || !pKey[0] )
					continue;

				bool bVirt = pKvKeyMap->GetBool( "virtkey" );

				unsigned potentialKey = bVirt ? GetKeyForStr( pKey ) : pKey[0];

				if ( potentialKey )
				{
					// If it doesn't have any modifiers it'll still work
					KeyValues* pKvModifiers = pKvKeyMap->FindKey( "modifiers" );
					if ( pKvModifiers )
					{
						mapping.uModifierKeys = ( pKvModifiers->GetBool( "shift" ) * KEY_MOD_SHIFT ) |
												( pKvModifiers->GetBool( "ctrl" ) * KEY_MOD_CONTROL ) |
												( pKvModifiers->GetBool( "alt" ) * KEY_MOD_ALT );
					}
					else
						mapping.uModifierKeys = 0;

					// We're writing virtual here for the accel tables
					mapping.uModifierKeys |= bVirt * KEY_MOD_VIRT;

					mapping.uChar = potentialKey;
					mapping.uLogicalKey = 0; // This gets defined in the method that calls this

					map[pKvKeyMap->GetName()] = mapping;
					bMappedAKey = true;
				}
			}

			return bMappedAKey;
		}
	}

	return false;
}

bool KeyBinds::GetAccelTableFor( const char* pMapping, HACCEL& out )
{
	KeyValues* pKvKeybinds = m_pKvKeybinds->FindKey( pMapping );
	if ( pKvKeybinds )
	{
		CUtlVector<ACCEL> accelerators;
		FOR_EACH_TRUE_SUBKEY( pKvKeybinds, pKvKeybind )
		{
			const char* pKey = pKvKeybind->GetString( "key", nullptr );
			if ( !pKey || !pKey[0] )
				Error( "Invalid key for bind %s", pKvKeybind->GetName() );

			bool bVirt = pKvKeybind->GetBool( "virtkey" );

			WORD potentialKey = bVirt ? GetKeyForStr( pKey ) : tolower( pKey[0] );
			WORD potentialID = GetIDForCommandStr( pKvKeybind->GetName() );

			if ( potentialKey && potentialID )
			{
				ACCEL acc;

				acc.fVirt = FNOINVERT | // NOINVERT needed so nothing causes the menu to show up
							( bVirt * FVIRTKEY );

				// If it doesn't have any modifiers it'll still work
				KeyValues* pKvModifiers = pKvKeybind->FindKey( "modifiers" );
				if ( pKvModifiers )
				{
					acc.fVirt |= ( pKvModifiers->GetBool( "shift" ) * FSHIFT ) |
								( pKvModifiers->GetBool( "ctrl" ) * FCONTROL ) |
								( pKvModifiers->GetBool( "alt" ) * FALT );
				}

				acc.cmd = potentialID;
				acc.key = potentialKey;
				accelerators.AddToTail( acc );
			}
			else
				Error( "Failed for key, %i %i for command %s", potentialKey, potentialID, pKvKeybind->GetName() );
		}

		if ( !accelerators.IsEmpty() )
		{
			if ( out )
				DestroyAcceleratorTable( out );
			out = CreateAcceleratorTable( accelerators.Base(), accelerators.Count() );
			return true;
		}
	}

	AssertMsg( 0, "Failed to get accel table for %s", pMapping );

	return false;
}

KeyValues* KeyBinds::EnumerateBindings( KeyValues* iter, CUtlVector<BindingInfo_t>& list )
{
	list.Purge();

	if ( !iter )
		iter = m_pKvDefaultKeybinds->GetFirstSubKey();
	else
		iter = iter->GetNextKey();
	if ( !iter )
		return nullptr;

	const CommandNames_t* curMap = nullptr;
	int mapSize = 0;
	if ( !V_stricmp( iter->GetName(), "IDR_MAINFRAME" ) )
	{
		curMap = s_CommandNamesMain;
		mapSize = ARRAYSIZE( s_CommandNamesMain );
	}
	else if ( !V_stricmp( iter->GetName(), "IDR_MAPDOC" ) )
	{
		curMap = s_CommandNamesDoc;
		mapSize = ARRAYSIZE( s_CommandNamesDoc );
	}

	KeyValues* own = m_pKvKeybinds->FindKey( iter->GetName() );
	if ( curMap )
	{
		for ( int i = 0; i < mapSize; i++ )
		{
			KeyValues* ownBind = own ? own->FindKey( curMap[i].pKeyName ) : nullptr;
			KeyValues* bind = iter->FindKey( curMap[i].pKeyName );
			if ( ownBind )
			{
				// Each key map has a command as its name, and the key, and modifiers
				const char* pKey = ownBind->GetString( "key", nullptr );

				// If there's no/empty key, keep moving forward
				if ( !pKey || !pKey[0] )
					goto noBind;

				bool bVirt = ownBind->GetBool( "virtkey" );

				unsigned potentialKey = bVirt ? GetKeyForStr( pKey ) : pKey[0];

				if ( potentialKey )
				{
					unsigned mod = 0;
					// If it doesn't have any modifiers it'll still work
					KeyValues* pKvModifiers = ownBind->FindKey( "modifiers" );
					if ( pKvModifiers )
					{
						mod = ( pKvModifiers->GetBool( "shift" ) * vgui::MODIFIER_SHIFT ) |
							( pKvModifiers->GetBool( "ctrl" ) * vgui::MODIFIER_CONTROL ) |
							( pKvModifiers->GetBool( "alt" ) * vgui::MODIFIER_ALT );
					}

					list.AddToTail( BindingInfo_t{ curMap[i].pKeyName, potentialKey, mod, bVirt, bind ? bind->GetString( "info", nullptr ) : nullptr } );
					continue;
				}
				goto noBind;
			}
			else
			{
			noBind:
				list.AddToTail( BindingInfo_t{ curMap[i].pKeyName, 0U, 0U, false, bind ? bind->GetString( "info", nullptr ) : nullptr } );
			}
		}
	}
	else
	{
		FOR_EACH_TRUE_SUBKEY( iter, pKvKeyMap )
		{
			KeyValues* ownBind = own ? own->FindKey( pKvKeyMap->GetName() ) : nullptr;
			if ( !ownBind )
			{
				list.AddToTail( BindingInfo_t{ pKvKeyMap->GetName(), 0U, 0U, false, pKvKeyMap->GetString( "info", nullptr ) } );
				continue;
			}

			// Each key map has a command as its name, and the key, and modifiers
			const char* pKey = ownBind->GetString( "key", nullptr );

			// If there's no/empty key, keep moving forward
			if ( !pKey || !pKey[0] )
				continue;

			bool bVirt = ownBind->GetBool( "virtkey" );

			unsigned potentialKey = bVirt ? GetKeyForStr( pKey ) : pKey[0];

			if ( potentialKey )
			{
				unsigned mod = 0;
				// If it doesn't have any modifiers it'll still work
				KeyValues* pKvModifiers = ownBind->FindKey( "modifiers" );
				if ( pKvModifiers )
				{
					mod = ( pKvModifiers->GetBool( "shift" ) * vgui::MODIFIER_SHIFT ) |
						( pKvModifiers->GetBool( "ctrl" ) * vgui::MODIFIER_CONTROL ) |
						( pKvModifiers->GetBool( "alt" ) * vgui::MODIFIER_ALT );
				}

				list.AddToTail( BindingInfo_t{ ownBind->GetName(), potentialKey, mod, bVirt, pKvKeyMap->GetString( "info", nullptr ) } );
			}
		}
	}

	return iter;
}

void KeyBinds::UpdateBindings( const char* group, const CUtlVector<BindingInfo_t>& list )
{
	KeyValues* bindGroup = m_pKvKeybinds->FindKey( group );
	Assert( bindGroup );
	if ( !bindGroup )
		return;

	for ( const BindingInfo_t& b : list )
	{
		KeyValues* bind = bindGroup->FindKey( b.name, !!b.keyCode );
		if ( bind && !b.keyCode )
		{
			bindGroup->RemoveSubKey( bind );
			bind->deleteThis();
			continue;
		}
		else if ( !b.keyCode )
			continue;

		if ( b.virt )
			bind->SetString( "key", GetStrForKey( b.keyCode ) );
		else
		{
			char key[] = { (char)b.keyCode, 0 };
			bind->SetString( "key", key );
		}

		if ( b.virt )
			bind->SetBool( "virtkey", true );
		else if ( KeyValues* m = bind->FindKey( "virtkey" ) )
		{
			bind->RemoveSubKey( m );
			m->deleteThis();
		}

		KeyValues* mod = bind->FindKey( "modifiers", !!b.modifiers );
		if ( !b.modifiers && mod )
		{
			bind->RemoveSubKey( mod );
			mod->deleteThis();
		}
		else if ( b.modifiers )
		{
			if ( b.modifiers & vgui::MODIFIER_SHIFT )
				mod->SetBool( "shift", true );
			else if ( KeyValues* m = mod->FindKey( "shift" ) )
			{
				mod->RemoveSubKey( m );
				m->deleteThis();
			}
			if ( b.modifiers & vgui::MODIFIER_CONTROL )
				mod->SetBool( "ctrl", true );
			else if ( KeyValues* m = mod->FindKey( "ctrl" ) )
			{
				mod->RemoveSubKey( m );
				m->deleteThis();
			}
			if ( b.modifiers & vgui::MODIFIER_ALT )
				mod->SetBool( "alt", true );
			else if ( KeyValues* m = mod->FindKey( "alt" ) )
			{
				mod->RemoveSubKey( m );
				m->deleteThis();
			}
		}
	}
}

void KeyBinds::GetDefaults( const char* group, CUtlVector<BindingInfo_t>& list )
{
	KeyValues* bindGroup = m_pKvDefaultKeybinds->FindKey( group );
	if ( !bindGroup )
		return;

	for ( BindingInfo_t& b : list )
	{
		KeyValues* bind = bindGroup->FindKey( b.name );
		if ( !bind )
			continue;

		// Each key map has a command as its name, and the key, and modifiers
		const char* pKey = bind->GetString( "key", nullptr );

		// If there's no/empty key, keep moving forward
		if ( !pKey || !pKey[0] )
			continue;

		bool bVirt = bind->GetBool( "virtkey" );

		unsigned potentialKey = bVirt ? GetKeyForStr( pKey ) : pKey[0];

		if ( potentialKey )
		{
			unsigned mod = 0;
			// If it doesn't have any modifiers it'll still work
			KeyValues* pKvModifiers = bind->FindKey( "modifiers" );
			if ( pKvModifiers )
			{
				mod = ( pKvModifiers->GetBool( "shift" ) * vgui::MODIFIER_SHIFT ) |
					( pKvModifiers->GetBool( "ctrl" ) * vgui::MODIFIER_CONTROL ) |
					( pKvModifiers->GetBool( "alt" ) * vgui::MODIFIER_ALT );
			}

			b.keyCode = potentialKey;
			b.modifiers = mod;
			b.help = bind->GetString( "info", nullptr );
		}
	}
}

class CInlineEditPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CInlineEditPanel, vgui::Panel );
public:
	CInlineEditPanel() : vgui::Panel( NULL, "InlineEditPanel" ) {}

	void Paint() override
	{
		int wide, tall;
		GetSize( wide, tall );

		// Draw a white rectangle around that cell
		vgui::surface()->DrawSetColor( 63, 63, 63, 255 );
		vgui::surface()->DrawFilledRect( 0, 0, wide, tall );

		vgui::surface()->DrawSetColor( 0, 255, 0, 255 );
		vgui::surface()->DrawOutlinedRect( 0, 0, wide, tall );
	}

	void OnKeyCodeTyped( vgui::KeyCode code ) override
	{
		// forward up
		if ( GetParent() )
			GetParent()->OnKeyCodeTyped(code);
	}

	void ApplySchemeSettings( vgui::IScheme* pScheme ) override
	{
		Panel::ApplySchemeSettings( pScheme );
		SetBorder( pScheme->GetBorder( "DepressedButtonBorder" ) );
	}

	void OnMousePressed( vgui::MouseCode code ) override
	{
		// forward up mouse pressed messages to be handled by the key options
		if ( GetParent() )
			GetParent()->OnMousePressed( code );
	}
};

class VControlsListPanel : public vgui::ListPanel
{
	DECLARE_CLASS_SIMPLE( VControlsListPanel, vgui::ListPanel );
public:
	VControlsListPanel( vgui::Panel* parent, const char* listName )	: BaseClass( parent, listName )
	{
		m_bCaptureMode	= false;
		m_nClickRow		= 0;
		m_pInlineEditPanel = new CInlineEditPanel();
		m_hFont = vgui::INVALID_FONT;
	}

	~VControlsListPanel() override
	{
		m_pInlineEditPanel->MarkForDeletion();
	}

	// Start/end capturing
	void StartCaptureMode( vgui::HCursor hCursor = NULL )
	{
		m_bCaptureMode = true;
		EnterEditMode( m_nClickRow, 1, m_pInlineEditPanel );
		vgui::input()->SetMouseFocus( m_pInlineEditPanel->GetVPanel() );
		vgui::input()->SetMouseCapture( m_pInlineEditPanel->GetVPanel() );

		if ( hCursor )
		{
			m_pInlineEditPanel->SetCursor( hCursor );

			// save off the cursor position so we can restore it
			vgui::input()->GetCursorPos( m_iMouseX, m_iMouseY );
		}
	}

	void EndCaptureMode( vgui::HCursor hCursor = NULL )
	{
		m_bCaptureMode = false;
		vgui::input()->SetMouseCapture( NULL );
		LeaveEditMode();
		RequestFocus();
		vgui::input()->SetMouseFocus( GetVPanel() );
		if ( hCursor )
		{
			m_pInlineEditPanel->SetCursor( hCursor );
			vgui::surface()->SetCursor( hCursor );
			if ( hCursor != vgui::dc_none )
				vgui::input()->SetCursorPos( m_iMouseX, m_iMouseY );
		}
	}

	bool IsCapturing() const { return m_bCaptureMode; }

	// Set which item should be associated with the prompt
	void SetItemOfInterest( int itemID ) { m_nClickRow = itemID; }
	int GetItemOfInterest() const { return m_nClickRow; }

	void OnMousePressed( vgui::MouseCode code ) override
	{
		if ( IsCapturing() )
		{
			// forward up mouse pressed messages to be handled by the key options
			if ( GetParent() )
				GetParent()->OnMousePressed( code );
		}
		else
			BaseClass::OnMousePressed( code );
	}

	void OnMouseDoublePressed( vgui::MouseCode code ) override
	{
		int c = GetSelectedItemsCount();
		if ( c > 0 )
			// enter capture mode
			OnKeyCodeTyped( KEY_ENTER );
		else
			BaseClass::OnMouseDoublePressed( code );
	}

	KEYBINDING_FUNC( clearbinding, KEY_DELETE, 0, OnClearBinding, 0, 0 )
	{
		if ( m_bCaptureMode )
			return;

		if ( GetItemOfInterest() < 0 )
			return;

		PostMessage( GetParent()->GetVPanel(), new KeyValues( "ClearBinding", "item", GetItemOfInterest() ) );
	}

private:
	void ApplySchemeSettings( vgui::IScheme* pScheme ) override
	{
		BaseClass::ApplySchemeSettings( pScheme );
		m_hFont = pScheme->GetFont( "DefaultVerySmall", IsProportional() );
	}

	// Are we showing the prompt?
	bool				m_bCaptureMode;
	// If so, where?
	int					m_nClickRow;
	// Font to use for showing the prompt
	vgui::HFont			m_hFont;
	// panel used to edit
	CInlineEditPanel*	m_pInlineEditPanel;
	int m_iMouseX, m_iMouseY;
};

static ButtonCode_t FromVirtualKey( int key, bool virt )
{
	if ( !virt )
	{
		const char c[] = { (char)key, 0 };
		auto code = vgui::Panel::DisplayStringToKeyCode( c );
		if ( code != KEY_NONE )
			return code;
	}
	return g_pInputSystem->VirtualKeyToButtonCode( key );
}

static int ToVirtualKey( ButtonCode_t key, bool hasMod, bool& didVirt )
{
	didVirt = false;
	if ( !hasMod && ( ( key >= KEY_0 && key <= KEY_Z ) || ( key >= KEY_LBRACKET && key <= KEY_EQUAL ) ) )
	{
		auto code = vgui::Panel::KeyCodeToDisplayStringShort( key );
		if ( *code )
			return *code;
	}
	didVirt = true;
	return g_pInputSystem->ButtonCodeToVirtualKey( key );
}

class CKeyBoardEditorPage : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CKeyBoardEditorPage, vgui::EditablePanel );
public:
	CKeyBoardEditorPage( vgui::Panel* parent, const CUtlVector<KeyBinds::BindingInfo_t>& bindings ) : BaseClass( parent, "KeyBoardEditorPage" )
	{
		m_pList = new VControlsListPanel( this, "KeyBindings" );
		m_pList->SetIgnoreDoubleClick( true );
		m_pList->AddColumnHeader( 0, "Action", "Binding name", 175, 175, 500, vgui::ListPanel::COLUMN_UNHIDABLE );
		m_pList->AddColumnHeader( 1, "Binding", "Binding", 150, 150, 500, vgui::ListPanel::COLUMN_UNHIDABLE );
		m_pList->AddColumnHeader( 2, "Description", "Description", 300, vgui::ListPanel::COLUMN_RESIZEWITHWINDOW | vgui::ListPanel::COLUMN_UNHIDABLE );

		auto s = new vgui::CBoxSizer( vgui::ESLD_VERTICAL );
		s->AddPanel( m_pList, vgui::SizerAddArgs_t().Expand( 1.0f ) );
		SetSizer( s );

		SaveMappings( bindings );
	}

	void OnKeyCodeTyped( vgui::KeyCode code ) override
	{
		switch ( code )
		{
		case KEY_ENTER:
			if ( !m_pList->IsCapturing() )
				OnCommand( "ChangeKey" );
			else
				BindKey( code );
			break;
		case KEY_LSHIFT:
		case KEY_RSHIFT:
		case KEY_LALT:
		case KEY_RALT:
		case KEY_LCONTROL:
		case KEY_RCONTROL:
			// Swallow these
			break;
		default:
			if ( m_pList->IsCapturing() )
				BindKey( code );
			else
				BaseClass::OnKeyCodeTyped( code );
		}
	}

	void ApplySchemeSettings( vgui::IScheme* scheme ) override
	{
		BaseClass::ApplySchemeSettings( scheme );
		PopulateList();
	}

	void OnRevert()
	{
		RestoreMappings();
		PopulateList();
	}

	void OnUseDefaults()
	{
		RestoreMappings();
		PopulateList();
	}

protected:
	MESSAGE_FUNC( OnPageHide, "PageHide" )
	{
		if ( m_pList->IsCapturing() )
			// Cancel capturing
			m_pList->EndCaptureMode( vgui::dc_arrow );
	}

	void OnCommand( char const* cmd )
	{
		if ( !m_pList->IsCapturing() && !Q_stricmp( cmd, "ChangeKey" ) )
			m_pList->StartCaptureMode( vgui::dc_blank );
		else
			BaseClass::OnCommand( cmd );
	}

	void PopulateList()
	{
		m_pList->DeleteAllItems();

		CUtlRBTree<KeyValues*, int> sorted( 0, 0, BindingLessFunc );

		for ( int i = 0; i < m_current.Count(); ++i )
		{
			auto& b = m_current[i];
			auto& m = m_original[i];

			// Create a new: blank item
			KeyValues* item = new KeyValues( "Item" );

			item->SetString( "Action", m.name );
			item->SetWString( "Binding", b.uChar != KEY_NONE ? Panel::KeyCodeModifiersToDisplayString( static_cast<vgui::KeyCode>( b.uChar ), b.uModifierKeys ) : L"" );

			// Find the binding
			if ( !m.help.IsEmpty() )
				item->SetString( "Description", m.help );

			item->SetPtr( "Item", &b );
			item->SetPtr( "Item2", &m );

			sorted.Insert( item );
		}

		for ( int j = sorted.FirstInorder() ; j != sorted.InvalidIndex(); j = sorted.NextInorder( j ) )
		{
			KeyValues* item = sorted[j];

			// Add to list
			m_pList->AddItem( item, 0, false, false );

			item->deleteThis();
		}

		sorted.RemoveAll();
	}

	void BindKey( vgui::KeyCode code )
	{
		using namespace vgui;
		bool shift = input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );
		bool ctrl = input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL );
		bool alt = input()->IsKeyDown( KEY_LALT ) || input()->IsKeyDown( KEY_RALT );

		int modifiers = 0;
		if ( shift )
			modifiers |= MODIFIER_SHIFT;
		if ( ctrl )
			modifiers |= MODIFIER_CONTROL;
		if ( alt )
			modifiers |= MODIFIER_ALT;

		int r = m_pList->GetItemOfInterest();

		// Retrieve clicked row and column
		m_pList->EndCaptureMode( dc_arrow );

		// Find item for this row
		KeyValues* item = m_pList->GetItem( r );
		if ( item )
		{
			auto kbMap = reinterpret_cast<KeyMap_t*>( item->GetPtr( "Item", nullptr ) );
			auto info = reinterpret_cast<KeyBinds::BindingInfo_t*>( item->GetPtr( "Item2", nullptr ) );
			if ( kbMap )
			{
				auto binding = LookupBindingByKeyCode( code, modifiers );
				if ( binding && info->name != binding->name )
				{
					// Key is already rebound!!!
					CUtlString s;
					s.Format( "Can't bind to '%S', key is already bound to '%s'\n", Panel::KeyCodeModifiersToDisplayString( code, modifiers ), binding->name.Get() );
					auto warn = new vgui::MessageBox( "Warning!", s, this );
					warn->DoModal();
					return;
				}

				kbMap->uChar			= code;
				kbMap->uModifierKeys	= modifiers;

				PopulateList();
			}
		}
	}

	// Trap row selection message
	MESSAGE_FUNC( ItemSelected, "ItemSelected" )
	{
		int c = m_pList->GetSelectedItemsCount();
		if ( c > 0 )
			m_pList->SetItemOfInterest( m_pList->GetSelectedItem( 0 ) );
	}

	MESSAGE_FUNC_INT( OnClearBinding, "ClearBinding", item )
	{
		// Find item for this row
		KeyValues* kv = m_pList->GetItem( item );
		if ( !kv )
			return;

		auto kbMap = reinterpret_cast<KeyMap_t*>( kv->GetPtr( "Item", nullptr ) );
		if ( !kbMap )
			return;

		kbMap->uChar			= KEY_NONE;
		kbMap->uModifierKeys	= 0;

		PopulateList();
	}

	void SaveMappings( const CUtlVector<KeyBinds::BindingInfo_t>& bindings )
	{
		Assert( m_original.Count() == 0 );

		m_original = bindings;
		m_current.SetSize( bindings.Count() );
		RestoreMappings();
	}

	void RestoreMappings()
	{
		for ( int i = 0; i < m_current.Count(); i++ )
		{
			const auto& orig = m_original[i];
			auto& cur = m_current[i];
			if ( orig.keyCode )
				cur.uChar = FromVirtualKey( orig.keyCode, orig.virt );
			else
				cur.uChar = KEY_NONE;
			cur.uModifierKeys = orig.modifiers;
		}
	}

protected:
	const KeyBinds::BindingInfo_t* LookupBindingByKeyCode( /*vgui::KeyCode*/unsigned code, unsigned modifiers ) const
	{
		for ( int i = 0; i < m_current.Count(); i++ )
		{
			auto& cur = m_current[i];
			auto& ori = m_original[i];
			if ( cur.uChar == code && cur.uModifierKeys == modifiers )
				return &ori;
		}

		return nullptr;
	}

	static bool BindingLessFunc( KeyValues* const& lhs, KeyValues* const& rhs )
	{
		KeyValues* p1 = const_cast<KeyValues*>( lhs );
		KeyValues* p2 = const_cast<KeyValues*>( rhs );
		return Q_stricmp( p1->GetString( "Action" ), p2->GetString( "Action" ) ) < 0;
	}

	VControlsListPanel*					m_pList;

	CUtlVector<KeyMap_t>				m_current;
	CUtlVector<KeyBinds::BindingInfo_t>	m_original;

	friend class CKeyBoardEditorSheet;
};

//-----------------------------------------------------------------------------
// Purpose: Dialog for use in editing keybindings
//-----------------------------------------------------------------------------
class CKeyBoardEditorSheet : public vgui::PropertySheet
{
	DECLARE_CLASS_SIMPLE( CKeyBoardEditorSheet, vgui::PropertySheet );

public:
	CKeyBoardEditorSheet( vgui::Panel* parent ) : BaseClass( parent, "KeyBoardEditorSheet" )
	{
		SetSmallTabs( false );

		// Create this sheet and add the subcontrols
		CUtlVector<KeyBinds::BindingInfo_t> bindList;
		for ( KeyValues* iter = nullptr; ( iter = g_pKeyBinds->EnumerateBindings( iter, bindList ) ) != nullptr; )
		{
			CKeyBoardEditorPage* newPage = new CKeyBoardEditorPage( this, bindList );
			AddPage( newPage, iter->GetName() );
		}
	}

	void OnSaveChanges()
	{
		int c = GetNumPages();
		for ( int i = 0 ; i < c; ++i )
		{
			CKeyBoardEditorPage* page = static_cast<CKeyBoardEditorPage*>( GetPage( i ) );
			const auto& cur = page->m_current;
			const auto& orig = page->m_original;

			CUtlVector<KeyBinds::BindingInfo_t> outBindings;
			outBindings.SetCount( cur.Count() );
			for ( int j = 0; j < outBindings.Count(); ++j )
			{
				auto& out = outBindings[j];
				auto& c = cur[j];
				auto& o = orig[j];

				out.name = o.name;
				if ( c.uChar == (unsigned)KEY_NONE )
				{
					out.virt = false;
					out.keyCode = 0U;
				}
				else
				{
					out.keyCode = ToVirtualKey( (ButtonCode_t)c.uChar, !!c.uModifierKeys, out.virt );
					out.modifiers = c.uModifierKeys;
				}
			}

			char name[100];
			GetTabTitle( i, name, 100 );
			g_pKeyBinds->UpdateBindings( name, outBindings );
		}
		g_pKeyBinds->Save();
	}

	void OnRevert()
	{
		int c = GetNumPages();
		for ( int i = 0 ; i < c; ++i )
		{
			CKeyBoardEditorPage* page = static_cast<CKeyBoardEditorPage*>( GetPage( i ) );
			page->OnRevert();
		}
	}

	void OnUseDefaults()
	{
		int c = GetNumPages();
		for ( int i = 0 ; i < c; ++i )
		{
			CKeyBoardEditorPage* page = static_cast<CKeyBoardEditorPage*>( GetPage( i ) );
			char name[100];
			GetTabTitle( i, name, 100 );
			g_pKeyBinds->GetDefaults( name, page->m_original );
			page->OnUseDefaults();
		}
	}
};

class CKeyBoardEditorDialog : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CKeyBoardEditorDialog, vgui::EditablePanel );
public:
	CKeyBoardEditorDialog( vgui::Panel* parent ) : BaseClass( parent, "KeyBoardEditorDialog" )
	{
		m_pSave = new vgui::Button( this, "Save", "Save", this, "save" );
		m_pCancel = new vgui::Button( this, "Cancel", "Cancel", this, "cancel" );
		m_pRevert = new vgui::Button( this, "Revert", "Revert", this, "revert" );
		m_pUseDefaults = new vgui::Button( this, "Defaults", "Use defaults", this, "defaults" );

		m_pKBEditor = new CKeyBoardEditorSheet( this );

		auto s = new vgui::CBoxSizer( vgui::ESLD_VERTICAL );
		s->AddPanel( m_pKBEditor, vgui::SizerAddArgs_t().Expand( 1.0f ) );

		auto s2 = new vgui::CBoxSizer( vgui::ESLD_HORIZONTAL );
		s2->AddSpacer( vgui::SizerAddArgs_t().Expand( 0.5f ).Padding( 4 ) );

		s2->AddPanel( m_pUseDefaults, vgui::SizerAddArgs_t().Padding( 0 ) );
		s2->AddSpacer( vgui::SizerAddArgs_t().Padding( 4 ) );
		s2->AddPanel( m_pRevert, vgui::SizerAddArgs_t().Padding( 0 ) );
		s2->AddSpacer( vgui::SizerAddArgs_t().Padding( 4 ) );
		s2->AddPanel( m_pSave, vgui::SizerAddArgs_t().Padding( 0 ) );
		s2->AddSpacer( vgui::SizerAddArgs_t().Padding( 4 ) );
		s2->AddPanel( m_pCancel, vgui::SizerAddArgs_t().Padding( 0 ) );

		s->AddSizer( s2, vgui::SizerAddArgs_t().Padding( 5 ) );
		s->AddSpacer( vgui::SizerAddArgs_t().Padding( 3 ) );

		SetSizer( s );

		SetMinimumSize( 640, 200 );

		SetVisible( true );
	}

	void OnCommand( char const* cmd ) override
	{
		if ( !Q_stricmp( cmd, "save" ) )
		{
			m_pKBEditor->OnSaveChanges();
			GetParent()->OnCommand( "OK" );
		}
		else if ( !Q_stricmp( cmd, "revert" ) )
		{
			m_pKBEditor->OnRevert();
		}
		else if ( !Q_stricmp( cmd, "defaults" ) )
		{
			m_pKBEditor->OnUseDefaults();
		}
		else if ( !Q_stricmp( cmd, "cancel" ) )
		{
			GetParent()->OnCommand( "cancel" );
		}
		else
		{
			BaseClass::OnCommand( cmd );
		}
	}

private:
	CKeyBoardEditorSheet*		m_pKBEditor;

	vgui::Button*				m_pSave;
	vgui::Button*				m_pCancel;
	vgui::Button*				m_pRevert;
	vgui::Button*				m_pUseDefaults;
};

static constexpr LPCTSTR pszIniSection = "Keybind Editor";

// CKeybindEditor dialog

class CKeybindEditorPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CKeybindEditorPanel, vgui::EditablePanel );
public:
	CKeybindEditorPanel( CKeybindEditor* pBrowser, const char* panelName, vgui::HScheme hScheme ) :
		vgui::EditablePanel( NULL, panelName, hScheme )
	{
		m_pBrowser = pBrowser;
	}

	void OnSizeChanged( int newWide, int newTall ) override
	{
		// call Panel and not EditablePanel OnSizeChanged.
		Panel::OnSizeChanged( newWide, newTall );
	}

	void OnCommand( const char* pCommand ) override
	{
		if ( Q_stricmp( pCommand, "OK" ) == 0 )
		{
			m_pBrowser->EndDialog( IDOK );
		}
		else if ( Q_stricmp( pCommand, "Cancel" ) == 0 || Q_stricmp( pCommand, "Close" ) == 0 )
		{
			m_pBrowser->EndDialog( IDCANCEL );
		}
	}

	void OnKeyCodeTyped( vgui::KeyCode code ) override
	{
		BaseClass::OnKeyCodeTyped( code );

		if ( code == KEY_ESCAPE )
			m_pBrowser->EndDialog( IDCANCEL );
	}

private:
	CKeybindEditor* m_pBrowser;
};

IMPLEMENT_DYNAMIC( CKeybindEditor, CVguiDialog )
CKeybindEditor::CKeybindEditor( CWnd* pParent /*=NULL*/ )
	: CVguiDialog( CKeybindEditor::IDD, pParent )
{
	g_pKeyBinds->m_bEditingKeybinds = true;
	m_pDialog = new CKeyBoardEditorDialog( NULL );
}

CKeybindEditor::~CKeybindEditor()
{
	// CDialog isn't going to clean up its vgui children
	delete m_pDialog;
	g_pKeyBinds->m_bEditingKeybinds = false;
}

void CKeybindEditor::SaveLoadSettings( bool bSave )
{
	CString str;
	CRect rect;
	CWinApp* pApp = AfxGetApp();

	if ( bSave )
	{
		GetWindowRect( rect );
		str.Format( "%ld %ld %ld %ld", rect.left, rect.top, rect.right, rect.bottom );
		pApp->WriteProfileString( pszIniSection, "Position", str );
	}
	else
	{
		str = pApp->GetProfileString( pszIniSection, "Position" );
		if ( !str.IsEmpty() )
		{
			const CRect screenRect{ POINT{ GetSystemMetrics( SM_XVIRTUALSCREEN ), GetSystemMetrics( SM_YVIRTUALSCREEN ) }, SIZE{ GetSystemMetrics( SM_CXVIRTUALSCREEN ), GetSystemMetrics( SM_CYVIRTUALSCREEN ) } };
			sscanf( str, "%ld %ld %ld %ld", &rect.left, &rect.top, &rect.right, &rect.bottom );
			const auto size = rect.Size();
			rect &= screenRect;
			if ( rect.Width() == 0 || rect.Height() == 0 )
				rect.InflateRect( 0, 0, size.cx, size.cy );
			MoveWindow( rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE );
			Resize();
		}
	}
}

void CKeybindEditor::DoDataExchange( CDataExchange* pDX )
{
	CDialog::DoDataExchange( pDX );
}

void CKeybindEditor::Resize()
{
	// reposition controls
	CRect rect;
	GetClientRect( &rect );

	m_VGuiWindow.MoveWindow( rect );

	m_pDialog->SetBounds( 0, 0, rect.Width(), rect.Height() );
}

void CKeybindEditor::OnSize(UINT nType, int cx, int cy)
{
	if ( nType == SIZE_MINIMIZED || !IsWindow( m_VGuiWindow.m_hWnd ) )
		return CDialog::OnSize( nType, cx, cy );

	Resize();

	CDialog::OnSize(nType, cx, cy);
}

BOOL CKeybindEditor::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

BEGIN_MESSAGE_MAP(CKeybindEditor, CDialog)
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL CKeybindEditor::PreTranslateMessage( MSG* pMsg )
{
	// don't filter dialog message
	return CWnd::PreTranslateMessage( pMsg );
}

BOOL CKeybindEditor::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_VGuiWindow.Create( NULL, _T("KeybindEditor"), WS_VISIBLE|WS_CHILD, CRect(0,0,100,100), this, IDD_KEYBIND_EDITOR );

	vgui::EditablePanel* pMainPanel = new CKeybindEditorPanel( this, "KeybindEditorPanel", HammerVGui()->GetHammerScheme() );

	m_VGuiWindow.SetParentWindow( &m_VGuiWindow );
	m_VGuiWindow.SetMainPanel( pMainPanel );
	pMainPanel->MakePopup( false, false );
	m_VGuiWindow.SetRepaintInterval( 30 );

	m_pDialog->SetParent( pMainPanel );
	m_pDialog->AddActionSignalTarget( pMainPanel );
	pMainPanel->InvalidateLayout( true );

	SaveLoadSettings( false ); // load

	//m_pPicker->Activate();

	return TRUE;
}

void CKeybindEditor::OnDestroy()
{
	SaveLoadSettings( true ); // save

	CDialog::OnDestroy();
}

void CKeybindEditor::Show()
{
	if ( m_pDialog )
		m_pDialog->SetVisible( true );

}
void CKeybindEditor::Hide()
{
	if ( m_pDialog )
		m_pDialog->SetVisible( false );
}