//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "objectproperties.h"
#include "objectpage.h"
#include "op_flags.h"
#include "op_groups.h"
#include "op_entity.h"
#include "op_output.h"
#include "OP_Model.h"
#include "op_input.h"
#include "mapdoc.h"
#include "mapview.h"
#include "mapentity.h"
#include "mapgroup.h"
#include "MapInstance.h"
#include "mapsolid.h"
#include "mapstudiomodel.h"
#include "mapworld.h"
#include "history.h"
#include "globalfunctions.h"
#include "Selection.h"
#include "custommessages.h"
#include "camera.h"
#include "manifest.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//
// Layout types for remembering the last layout of the dialog. We could
// also remember this as an array of booleans for which pages were visible.
//
constexpr unsigned NO_PAGE   = 0b0000;
constexpr unsigned ENTITY_PAGE = 0b0001;
constexpr unsigned GROUPS_PAGE = 0b0010;
constexpr unsigned FLAGS_PAGE  = 0b0100;
constexpr unsigned MODEL_PAGE  = 0b1000;

constexpr unsigned LAYOUT_NONE = NO_PAGE;
constexpr unsigned LAYOUT_SOLID_or_MULTI = GROUPS_PAGE;
constexpr unsigned LAYOUT_ENTITY_or_MULTI_ENT = ENTITY_PAGE | FLAGS_PAGE | GROUPS_PAGE;
constexpr unsigned LAYOUT_WORLD = ENTITY_PAGE;
constexpr unsigned LAYOUT_MODEL = ENTITY_PAGE | FLAGS_PAGE | GROUPS_PAGE | MODEL_PAGE;


IMPLEMENT_DYNAMIC(CObjectProperties, CPropertySheet)


BEGIN_MESSAGE_MAP(CObjectProperties, CPropertySheet)
	//{{AFX_MSG_MAP(CObjectProperties)
	ON_WM_KILLFOCUS()
	ON_WM_ACTIVATE()
	ON_WM_CLOSE()
	ON_WM_PAINT()
	ON_WM_SHOWWINDOW()
	ON_WM_CREATE()
	ON_COMMAND(IDOK, &ThisClass::OnApply )
	ON_COMMAND(ID_APPLY_NOW, &ThisClass::OnApply )
	ON_COMMAND(IDCANCEL, &ThisClass::OnCancel)
	ON_COMMAND(IDI_INPUT, &ThisClass::OnInputs)
	ON_COMMAND(IDI_OUTPUT, &ThisClass::OnOutputs)
	ON_COMMAND(IDD_EDIT_INSTANCE, &ThisClass::OnEditInstance)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

IMPLEMENT_DYNAMIC(editCMapClass, CObject);
IMPLEMENT_DYNAMIC(editCEditGameClass, CObject);


static editCMapClass e_CMapClass;
static editCEditGameClass e_CEditGameClass;


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CObjectProperties::CObjectProperties(void) :
	CPropertySheet()
{
	m_bDummy = false;
	m_pDummy = NULL;
	m_pInputButton = NULL;
	m_pOutputButton = NULL;
	m_pInstanceButton = NULL;
	m_pOrgObjects = NULL;
	m_bDataDirty = false;
	m_bCanEdit = false;
	memset( m_customFlags, 0, sizeof( m_customFlags ) );

	CreatePages();
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
// Input  : nIDCaption -
//			pParentWnd -
//			iSelectPage -
//-----------------------------------------------------------------------------
CObjectProperties::CObjectProperties(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
	m_bDummy = false;
	m_pDummy = NULL;
	m_pInputButton = NULL;
	m_pOutputButton = NULL;
	m_pInstanceButton = NULL;
	m_bCanEdit = false;
	memset( m_customFlags, 0, sizeof( m_customFlags ) );

	CreatePages();
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
// Input  : pszCaption -
//			pParentWnd -
//			iSelectPage -
//-----------------------------------------------------------------------------
CObjectProperties::CObjectProperties(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
	m_bDummy = false;
	m_pDummy = NULL;
	m_pInputButton = NULL;
	m_pOutputButton = NULL;
	m_pInstanceButton = NULL;
	m_bCanEdit = false;
	memset( m_customFlags, 0, sizeof( m_customFlags ) );

	CreatePages();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CObjectProperties::~CObjectProperties()
{
	delete m_pDummy;

	delete m_pEntity;
	delete m_pFlags;
	delete m_pGroups;
	delete m_pOutput;
	delete m_pInput;
	delete m_pModel;

	for ( COP_Flags* flag : m_customFlags )
		delete flag;

	delete m_pInputButton;
	delete m_pOutputButton;
	delete m_pInstanceButton;

	delete[] m_ppPages;
}


//-----------------------------------------------------------------------------
// Purpose: Creates all possible pages and attaches our object list to them.
//			Not all will be used depending on the types of objects being edited.
//-----------------------------------------------------------------------------
void CObjectProperties::CreatePages(void)
{
	//VPROF_BUDGET( "CObjectProperties::CreatePages", "Object Properties" );

	m_pEntity = new COP_Entity;
	m_pEntity->SetObjectList(&m_DstObjects);

	m_pFlags = new COP_Flags;
	m_pFlags->SetFlagName( "spawnflags" );
	m_pFlags->SetObjectList(&m_DstObjects);

	// There are some dependencies between the entity and flags tabs since
	// they both edit the spawnflags property.
	m_pEntity->SetSpawnFlagsPage( m_pFlags );
	m_pFlags->SetEntityPage( m_pEntity );

	m_pGroups = new COP_Groups;
	m_pGroups->SetObjectList(&m_DstObjects);

	m_pOutput = new COP_Output;
	m_pOutput->SetObjectList(&m_DstObjects);

	m_pInput = new COP_Input;
	m_pInput->SetObjectList(&m_DstObjects);

	m_pModel = new COP_Model;
	m_pModel->SetObjectList(&m_DstObjects);

	m_pDummy = new CObjectPage(IDD_OBJPAGE_DUMMY);

	m_ppPages = NULL;
	m_nPages = 0;

	m_pLastActivePage = NULL;
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : pType -
//-----------------------------------------------------------------------------
PVOID CObjectProperties::GetEditObject(CRuntimeClass *pType)
{
	//VPROF_BUDGET( "CObjectProperties::GetEditObject", "Object Properties" );

	if (pType == RUNTIME_CLASS(editCMapClass))
	{
		return PVOID((CMapClass*)&e_CMapClass);
	}
	else if (pType == RUNTIME_CLASS(editCEditGameClass))
	{
		return PVOID((CEditGameClass*)&e_CEditGameClass);
	}

	Assert(0);
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : pobj -
//			pType -
//-----------------------------------------------------------------------------
PVOID CObjectProperties::GetEditObjectFromMapObject(CMapClass *pobj, CRuntimeClass *pType)
{
	//VPROF_BUDGET( "CObjectProperties::GetEditObjectFromMapObject", "Object Properties" );

	if (pType == RUNTIME_CLASS(editCMapClass))
	{
		return PVOID(pobj);
	}
	else if (pType == RUNTIME_CLASS(editCEditGameClass))
	{
		if (pobj->IsMapClass(MAPCLASS_TYPE(CMapEntity)))
		{
			return PVOID((CEditGameClass*)((CMapEntity*)pobj));
		}

		if (pobj->IsMapClass(MAPCLASS_TYPE(CMapWorld)))
		{
			return PVOID((CEditGameClass*)((CMapWorld*)pobj));
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pobj -
//-----------------------------------------------------------------------------
void CObjectProperties::CopyDataToEditObjects(CMapClass *pobj)
{
	//VPROF_BUDGET( "CObjectProperties::CopyDataToEditObjects", "Object Properties" );

	//
	// All copies here are done without updating object dependencies, because
	// we're copying to a place that is outside of the world.
	//
	e_CMapClass.CopyFrom(pobj, false);

	if (pobj->IsMapClass(MAPCLASS_TYPE(CMapEntity)))
	{
		e_CEditGameClass.CopyFrom((CEditGameClass *)((CMapEntity *)pobj));
	}
	else if (pobj->IsMapClass(MAPCLASS_TYPE(CMapWorld)))
	{
		e_CEditGameClass.CopyFrom((CEditGameClass *)((CMapWorld *)pobj));
	}
}

//------------------------------------------------------------------------------
// Purpose:
// Input  : nState -
//------------------------------------------------------------------------------
void CObjectProperties::SetOutputButtonState(int nState)
{
	//VPROF_BUDGET( "CObjectProperties::SetOutputButtonState", "Object Properties" );

	if (nState == CONNECTION_GOOD)
	{
		m_pOutputButton->SetIcon(m_hIconOutputGood);
		m_pOutputButton->ShowWindow(SW_SHOW);
		m_pOutputButton->Invalidate();
		m_pOutputButton->UpdateWindow();
	}
	else if (nState == CONNECTION_BAD)
	{
		m_pOutputButton->SetIcon(m_hIconOutputBad);
		m_pOutputButton->ShowWindow(SW_SHOW);
		m_pOutputButton->Invalidate();
		m_pOutputButton->UpdateWindow();
	}
	else
	{
		m_pOutputButton->ShowWindow(SW_HIDE);
		m_pOutputButton->Invalidate();
		m_pOutputButton->UpdateWindow();
	}
}


//------------------------------------------------------------------------------
// Purpose:
// Input  : nState -
//------------------------------------------------------------------------------
void CObjectProperties::SetInputButtonState(int nState)
{
	//VPROF_BUDGET( "CObjectProperties::SetInputButtonState", "Object Properties" );

	if (nState == CONNECTION_GOOD)
	{
		m_pInputButton->SetIcon(m_hIconInputGood);
		m_pInputButton->ShowWindow(SW_SHOW);
		m_pInputButton->Invalidate();
		m_pInputButton->UpdateWindow();
	}
	else if (nState == CONNECTION_BAD)
	{
		m_pInputButton->SetIcon(m_hIconInputBad);
		m_pInputButton->ShowWindow(SW_SHOW);
		m_pInputButton->Invalidate();
		m_pInputButton->UpdateWindow();
	}
	else
	{
		m_pInputButton->ShowWindow(SW_HIDE);
		m_pInputButton->Invalidate();
		m_pInputButton->UpdateWindow();
	}
}


//------------------------------------------------------------------------------
// Purpose: Set icon being displayed on output button.
//------------------------------------------------------------------------------
void CObjectProperties::UpdateOutputButton(void)
{
	//VPROF_BUDGET( "CObjectProperties::UpdateOutputButton", "Object Properties" );

	if (!m_pOutputButton)
	{
		return;
	}

	bool bHaveConnection = false;
	bool bIgnoreHiddenTargets = false;
	if ( m_pOutput )
		bIgnoreHiddenTargets = !m_pOutput->ShouldShowHiddenTargets();

	FOR_EACH_OBJ( m_DstObjects, pos )
	{
		CMapClass *pObject = m_DstObjects.Element(pos);

		if ((pObject != NULL) && (pObject->IsMapClass(MAPCLASS_TYPE(CMapEntity))))
		{
			CMapEntity *pEntity = (CMapEntity *)pObject;
			int nStatus = CEntityConnection::ValidateOutputConnections( pEntity, true, bIgnoreHiddenTargets, true );
			if (nStatus == CONNECTION_BAD)
			{
				SetOutputButtonState(CONNECTION_BAD);
				return;
			}
			else if (nStatus == CONNECTION_GOOD)
			{
				bHaveConnection = true;
			}
		}
	}
	if (bHaveConnection)
	{
		SetOutputButtonState(CONNECTION_GOOD);
	}
	else
	{
		SetOutputButtonState(CONNECTION_NONE);
	}
}


//------------------------------------------------------------------------------
// Purpose: Set icon being displayed on input button.
//------------------------------------------------------------------------------
void CObjectProperties::UpdateInputButton()
{
	//VPROF_BUDGET( "CObjectProperties::UpdateInputButton", "Object Properties" );

	if (!m_pInputButton)
	{
		return;
	}

	bool bHaveConnection = false;

	FOR_EACH_OBJ( m_DstObjects, pos )
	{
		CMapClass *pObject = m_DstObjects.Element(pos);

		if ((pObject != NULL) && (pObject->IsMapClass(MAPCLASS_TYPE(CMapEntity))))
		{
			CMapEntity *pEntity = (CMapEntity *)pObject;
			int nStatus = CEntityConnection::ValidateInputConnections(pEntity, false);
			if (nStatus == CONNECTION_BAD)
			{
				SetInputButtonState(CONNECTION_BAD);
				return;
			}
			else if (nStatus == CONNECTION_GOOD)
			{
				bHaveConnection = true;
			}
		}
	}
	if (bHaveConnection)
	{
		SetInputButtonState(CONNECTION_GOOD);
	}
	else
	{
		SetInputButtonState(CONNECTION_NONE);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Finds/Creates the buttons.
//-----------------------------------------------------------------------------
void CObjectProperties::CreateButtons(void)
{
	//VPROF_BUDGET( "CObjectProperties::CreateButtons", "Object Properties" );

	// Grab, enable and DONT show the OK button
	// (Because <enter> only accelerates IDOK)
	// and we dont want "OK" (apply+close) functionality
	CButton	*pOKButton = reinterpret_cast<CButton *>(GetDlgItem(IDOK));
	pOKButton->EnableWindow();
	// Dont show the window, just make it active to forward <enter> -> IDOK -> OnApply

	CRect modRect;

	// Create buttons to display connection status icons
	CRect rect;
	GetWindowRect(&rect);
	rect.InflateRect(0, 0, 0, 32);
	MoveWindow(&rect, FALSE);
	GetClientRect(&rect);

	// Grab and enable & show the hidden Apply button too
	CButton *pApplyButton = reinterpret_cast<CButton *>(GetDlgItem(ID_APPLY_NOW));
	pApplyButton->SetButtonStyle( pApplyButton->GetButtonStyle() | BS_DEFPUSHBUTTON );
	pApplyButton->EnableWindow();
	pApplyButton->GetWindowRect( &modRect );
	ScreenToClient( &modRect );
	modRect.top -= 4;
	modRect.bottom -= 4;
	int width = modRect.right - modRect.left;
	modRect.right = rect.right - 8;
	modRect.left = modRect.right - width;
	pApplyButton->MoveWindow( &modRect, FALSE );
	pApplyButton->ShowWindow(SW_SHOWNA);

	// Grab and enable & show the hidden Cancel button too
	CButton *pCancelButton = reinterpret_cast<CButton *>(GetDlgItem(IDCANCEL));
	pCancelButton->EnableWindow();
	modRect.right = modRect.left - 4;
	modRect.left = modRect.right - width;
	pCancelButton->MoveWindow( &modRect, FALSE );
	pCancelButton->ShowWindow(SW_SHOWNA);

	//
	// Load Icons
	//
	CWinApp *pApp = AfxGetApp();
	const int s = 16;
	m_hIconOutputGood = (HICON)LoadImage(pApp->m_hInstance, MAKEINTRESOURCE(IDI_OUTPUT), IMAGE_ICON, s, s, LR_DEFAULTCOLOR);
	m_hIconOutputBad  = (HICON)LoadImage(pApp->m_hInstance, MAKEINTRESOURCE(IDI_OUTPUTBAD), IMAGE_ICON, s, s, LR_DEFAULTCOLOR);
	m_hIconInputGood  = (HICON)LoadImage(pApp->m_hInstance, MAKEINTRESOURCE(IDI_INPUT), IMAGE_ICON, s, s, LR_DEFAULTCOLOR);
	m_hIconInputBad   = (HICON)LoadImage(pApp->m_hInstance, MAKEINTRESOURCE(IDI_INPUTBAD), IMAGE_ICON, s, s, LR_DEFAULTCOLOR);

	m_pInputButton = new CButton;
	m_pInputButton->Create(_T("My button"), WS_CHILD|BS_ICON|BS_FLAT, CRect(6,rect.bottom - 30,32,rect.bottom - 4), this, IDI_INPUT);

	m_pOutputButton = new CButton;
	m_pOutputButton->Create(_T("My button"), WS_CHILD|BS_ICON|BS_FLAT, CRect(40,rect.bottom - 30,66,rect.bottom - 4), this, IDI_OUTPUT);

	m_pInstanceButton = new CButton;
	m_pInstanceButton->Create( _T( "Edit Instance" ), WS_CHILD|BS_TEXT, CRect( modRect.left - 140, modRect.top, modRect.left - 6, modRect.bottom ), this, IDD_EDIT_INSTANCE );

	auto layout = GetDynamicLayout();
	layout->AddItem( m_pInputButton->GetSafeHwnd(), CMFCDynamicLayout::MoveVertical( 100 ), CMFCDynamicLayout::SizeNone() );
	layout->AddItem( m_pOutputButton->GetSafeHwnd(), CMFCDynamicLayout::MoveVertical( 100 ), CMFCDynamicLayout::SizeNone() );
	layout->AddItem( m_pInstanceButton->GetSafeHwnd(), CMFCDynamicLayout::MoveHorizontalAndVertical( 100, 100 ), CMFCDynamicLayout::SizeNone() );
	layout->AddItem( ID_APPLY_NOW, CMFCDynamicLayout::MoveHorizontalAndVertical( 100, 100 ), CMFCDynamicLayout::SizeNone() );
	layout->AddItem( IDCANCEL, CMFCDynamicLayout::MoveHorizontalAndVertical( 100, 100 ), CMFCDynamicLayout::SizeNone() );
}


//-----------------------------------------------------------------------------
// Purpose: Returns the appropriate page layout for the current object list.
//-----------------------------------------------------------------------------
void CObjectProperties::GetTabsForLayout(unsigned eLayoutType, bool &bEntity, bool &bGroups, bool &bFlags, bool &bModel)
{
	bEntity = eLayoutType & ENTITY_PAGE;
	bGroups = eLayoutType & GROUPS_PAGE;
	bFlags = eLayoutType & FLAGS_PAGE;
	bModel = eLayoutType & MODEL_PAGE;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the appropriate page layout for the current object list.
//-----------------------------------------------------------------------------
unsigned CObjectProperties::GetTabLayout()
{
	//VPROF_BUDGET( "CObjectProperties::GetLayout", "Object Properties" );

	unsigned eLayoutType = LAYOUT_NONE;

	if ( !m_DstObjects.IsEmpty() && CMapDoc::GetActiveMapDoc() )
	{
		//
		// Figure out which layout to use based on the objects being edited.
		//
		bool bFirst = true;
		MAPCLASSTYPE PrevType = MAPCLASS_TYPE( CMapEntity );

		FOR_EACH_OBJ( m_DstObjects, pos )
		{
			CMapClass* pObject = m_DstObjects.Element( pos );
			MAPCLASSTYPE ThisType = pObject->GetType();

			if ( bFirst )
			{
				bFirst = false;

				if ( ThisType == MAPCLASS_TYPE( CMapEntity ) )
				{
					CMapEntity* pEntity = static_cast<CMapEntity*>( pObject );

					//
					// Only show the model tab when we have a single entity selected that
					// has a model helper.
					//
					if ( m_DstObjects.Count() == 1 )
					{
						if ( pEntity->GetChildOfType<CMapStudioModel>() )
							eLayoutType = LAYOUT_MODEL;
						else
							eLayoutType = LAYOUT_ENTITY_or_MULTI_ENT;
					}
					else
						eLayoutType = LAYOUT_ENTITY_or_MULTI_ENT;
				}
				else if ( ThisType == MAPCLASS_TYPE( CMapSolid ) || ThisType == MAPCLASS_TYPE( CMapGroup ) )
					eLayoutType = LAYOUT_SOLID_or_MULTI;
				else if ( ThisType == MAPCLASS_TYPE( CMapWorld ) )
					eLayoutType = LAYOUT_WORLD;
			}
			else if ( ThisType != PrevType )
				return LAYOUT_SOLID_or_MULTI;

			PrevType = ThisType;
		}
	}

	if ( eLayoutType == LAYOUT_MODEL || eLayoutType == LAYOUT_ENTITY_or_MULTI_ENT )
	{
		extern GameData* pGD;
		GDclass* pClass = pGD->ClassForName( dynamic_cast<CEditGameClass*>( m_DstObjects.Element( 0 ) )->GetClassName() );
		if ( !pClass )
			return eLayoutType;

		int nCurCustom = 0;
		bool changed = false;
		int nCount = pClass->GetVariableCount();
		for ( int i = 0; i < nCount; ++i )
		{
			if ( nCurCustom == MAX_CUSTOM_FLAGS )
				break;
			GDinputvariable* pVar = pClass->GetVariableAt( i );
			if ( pVar->GetType() != ivFlags || !stricmp( pVar->GetName(), "spawnflags" ) )
				continue;
			++nCurCustom;

			auto& page = m_customFlags[nCurCustom - 1];
			if ( !page )
			{
				page = new COP_Flags;
				page->SetObjectList( &m_DstObjects );
				page->SetEntityPage( m_pEntity );
				page->SetFlagName( pVar->GetName() );
				page->SetTitle( pVar->GetLongName() );
				m_pEntity->SetFlagsPage( pVar->GetName(), page );
				continue;
			}

			if ( !stricmp( page->GetFlagName(), pVar->GetName() ) )
				continue;

			page->SetTitle( pVar->GetLongName() );
			m_pEntity->SetFlagsPage( page->GetFlagName(), nullptr );
			page->SetFlagName( pVar->GetName() );
			m_pEntity->SetFlagsPage( pVar->GetName(), page );
			changed = true;
		}

		if ( nCurCustom )
			eLayoutType |= ( ( 1 << nCurCustom ) - 1 ) << 4;
		if ( changed )
			eLayoutType |= ( 1 << 31 );
	}

	return eLayoutType;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CObjectProperties::RestoreActivePage(void)
{
	//VPROF_BUDGET( "CObjectProperties::RestoreActivePage", "Object Properties" );

	//
	// Try to restore the previously active page. If it is not in the page list
	// just activate page zero.
	//
	bool bPageSet = false;
	for (int i = 0; i < m_nPages; i++)
	{
		if (m_ppPages[i] == m_pLastActivePage)
		{
			SetActivePage(m_pLastActivePage);
			bPageSet = true;
			break;
		}
	}

	if (!bPageSet)
	{
		SetActivePage(0);
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CObjectProperties::SaveActivePage(void)
{
	//VPROF_BUDGET( "CObjectProperties::SaveActivePage", "Object Properties" );

	CObjectPage *pPage = (CObjectPage *)GetActivePage();
	if (pPage != NULL)
	{
		m_pLastActivePage = pPage;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets up pages to display based on "m_DstObjects".
// Output : Returns TRUE if the page structure changed, FALSE if not.
//-----------------------------------------------------------------------------
BOOL CObjectProperties::SetupPages(void)
{
	//VPROF_BUDGET( "CObjectProperties::SetupPages", "Object Properties" );

	static bool bFirstTime = true;
	static unsigned eLastLayoutType = 1U << 31U;
	static unsigned eLastValidLayoutType = LAYOUT_NONE;

	//
	// Save the current active page.
	//
	if ( eLastLayoutType != LAYOUT_NONE && eLastLayoutType != 1U << 31U )
		SaveActivePage();

	//
	// Determine the appropriate layout for the current object list.
	//
	unsigned eLayoutType = GetTabLayout();

	bool bEntity;
	bool bGroups;
	bool bFlags;
	bool bModel;
	GetTabsForLayout(eLayoutType, bEntity, bGroups, bFlags, bModel);

	//
	// If the layout has not changed, we're done. All the pages are already set up.
	//
	if (eLayoutType == eLastLayoutType)
	{
		//
		// Try to restore the previously active page. If it has been deleted just
		// activate page zero.
		//
		RestoreActivePage();
		return FALSE;
	}

	//
	// Forget the last active page when the layout changes from one
	// valid layout to another (such as from entity to solid).
	// Don't reset when switching between model entities and non-model entities,
	// because it's annoying to be switched away from the Outputs tab.
	//
	if ((eLayoutType != LAYOUT_NONE) && (eLayoutType != eLastValidLayoutType) &&
		!((eLayoutType & LAYOUT_ENTITY_or_MULTI_ENT) == LAYOUT_ENTITY_or_MULTI_ENT && (eLastValidLayoutType & LAYOUT_MODEL) == LAYOUT_MODEL) &&
		!((eLayoutType & LAYOUT_MODEL) == LAYOUT_MODEL && (eLastValidLayoutType & LAYOUT_ENTITY_or_MULTI_ENT) == LAYOUT_ENTITY_or_MULTI_ENT))
	{
		m_pLastActivePage = NULL;
		eLastValidLayoutType = eLayoutType & ~( 1 << 31 );
	}

	eLastLayoutType = eLayoutType & ~( 1 << 31 );

	CObjectPage::s_bRESTRUCTURING = TRUE;

	bool bIO = bEntity && eLayoutType != LAYOUT_WORLD;
	UINT nAddPages = bEntity + bGroups + bFlags + bModel + bIO;

	// don't want to change focus .. just pages!
	CWnd *pActiveWnd = GetActiveWindow();

	bool bDisabledraw = false;
	if (::IsWindow(m_hWnd) && IsWindowVisible())
	{
		SetRedraw(FALSE);
		bDisabledraw = true;
	}

	if (!m_bDummy && (nAddPages == 0))
	{
		AddPage(m_pDummy);
		m_bDummy = true;
	}
	else if (m_bDummy && (nAddPages > 0))
	{
		RemovePage(m_pDummy);
		m_bDummy = false;
	}

#define CUSTOM_PAGE(p) {false, (eLayoutType & (1U << (p + 3U))) != 0, m_customFlags[p-1]}

	struct
	{
		bool m_bIsVisible;
		bool m_bWantVisible;
		CObjectPage *m_pPage;
	} pages[] =
	{
		{false, bEntity, m_pEntity},
		{false, bIO, m_pOutput},
		{false, bIO, m_pInput},
		{false, bModel, m_pModel},
		{false, bFlags, m_pFlags},
		CUSTOM_PAGE(1),
		CUSTOM_PAGE(2),
		CUSTOM_PAGE(3),
		CUSTOM_PAGE(4),
		CUSTOM_PAGE(5),
		CUSTOM_PAGE(6),
		CUSTOM_PAGE(7),
		CUSTOM_PAGE(8),
		CUSTOM_PAGE(9),
		CUSTOM_PAGE(10),
		CUSTOM_PAGE(11),
		CUSTOM_PAGE(12),
		CUSTOM_PAGE(13),
		CUSTOM_PAGE(14),
		CUSTOM_PAGE(15),
		CUSTOM_PAGE(16),
		CUSTOM_PAGE(17),
		CUSTOM_PAGE(18),
		CUSTOM_PAGE(19),
		CUSTOM_PAGE(20),
		CUSTOM_PAGE(21),
		CUSTOM_PAGE(22),
		CUSTOM_PAGE(23),
		CUSTOM_PAGE(24),
		CUSTOM_PAGE(25),
		CUSTOM_PAGE(26),
		CUSTOM_PAGE(27),
		{false, bGroups, m_pGroups}
	};

#undef CUSTOM_PAGE

	// First, remove pages that we don't want visible.
	// Also store if they're visible.
	for ( uint i=0; i < ARRAYSIZE( pages ); i++ )
	{
		auto& page = pages[i];
		page.m_bIsVisible = page.m_pPage && GetPageIndex( page.m_pPage ) != -1;
		if ( page.m_bIsVisible && !page.m_bWantVisible )
		{
			// It's visible but they don't want it there.
			RemovePage( page.m_pPage );
			page.m_bIsVisible = false;
		}
	}

	// We're about to add pages, but it'll only add them to the right of what's already there,
	// so we must get rid of anything to the right of our leftmost addition.
	for ( uint i=0; i < ARRAYSIZE( pages ); i++ )
	{
		if ( !pages[i].m_bIsVisible && pages[i].m_bWantVisible )
		{
			// Ok, page i needs to be on, so nuke everything to the right of it.
			for ( uint j=i+1; j < ARRAYSIZE( pages ); j++ )
			{
				auto& page = pages[j];
				if ( page.m_bIsVisible )
				{
					RemovePage( page.m_pPage );
					page.m_bIsVisible = false;
				}
			}
			break;
		}
	}

	for ( uint i=0; i < ARRAYSIZE( pages ); i++ )
	{
		if ( !pages[i].m_bIsVisible && pages[i].m_bWantVisible )
			AddPage( pages[i].m_pPage );
	}

	//
	// Store active pages in our array.
	//
	if (!m_bDummy)
	{
		delete[] m_ppPages;
		m_nPages = GetPageCount();
		m_ppPages = new CObjectPage*[m_nPages];

		for (int i = 0; i < m_nPages; i++)
		{
			m_ppPages[i] = (CObjectPage *)GetPage(i);
			m_ppPages[i]->m_bFirstTimeActive = true;
			m_ppPages[i]->m_bHasUpdatedData = false;
		}
	}

	CObjectPage::s_bRESTRUCTURING = FALSE;

	//VPROF_BUDGET( "CObjectProperties::RestoreActivePage", "Object Properties" );
	RestoreActivePage();

	//
	// Enable redraws if they were disabled above.
	//
	if (bDisabledraw)
	{
		SetRedraw(TRUE);
		Invalidate(FALSE);
	}

	// Set button status
	UpdateOutputButton();
	UpdateInputButton();

	if (pActiveWnd != NULL)
	{
		pActiveWnd->SetActiveWindow();
	}

	bFirstTime = false;

	return TRUE;	// pages changed - return true
}


//------------------------------------------------------------------------------
// Purpose: Set object properties dialogue to the Output tab and highlight
//			the given item
// Input  : pConnection -
//------------------------------------------------------------------------------
void CObjectProperties::SetPageToOutput(CEntityConnection *pConnection)
{
	if ( m_bDataDirty )
		ReloadData();

	SetActivePage(m_pOutput);
	m_pOutput->SetSelectedConnection(pConnection);
}

void CObjectProperties::SetPageToInput(CEntityConnection *pConnection)
{
	if ( m_bDataDirty )
		ReloadData();

	SetActivePage(m_pInput);

	m_pInput->SetSelectedConnection(pConnection);
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CObjectProperties::SaveData( SaveData_Reason_t reason )
{
	//VPROF_BUDGET( "CObjectProperties::SaveData", "Object Properties" );

	//
	// Make sure window is visible - don't want to save otherwise.
	//
	if (!IsWindowVisible())
	{
		return;
	}

	// we should never save in a dirty state
	if ( m_bDataDirty )
		return;

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (!pDoc || !m_DstObjects.Count() || m_bDummy)
	{
		return;
	}

	//
	// Transfer all page data to the objects being edited.
	//
	GetHistory()->MarkUndoPosition( pDoc->GetSelection()->GetList(), "Change Properties");

	// Don't keep all the world's children when we're editing the world, because
	// that's really slow (and pointless since all we're changing is keyvalues).
	bool bKeptWorld = false;
	if (m_DstObjects.Count() == 1)
	{
		CMapClass *pObject = m_DstObjects.Element( 0 );
		if ( IsWorldObject(pObject) )
		{
			GetHistory()->KeepNoChildren(pObject);
			bKeptWorld = true;
		}
	}

	if (!bKeptWorld)
	{
		GetHistory()->Keep(&m_DstObjects);
	}

	for (int i = 0; i < m_nPages; i++)
	{
		//
		// Pages that have never been shown have no hwnd.
		//
		if (IsWindow(m_ppPages[i]->m_hWnd) && m_ppPages[i]->m_bHasUpdatedData )
		{
			m_ppPages[i]->SaveData( reason );
		}
	}

	// Objects may have changed. Update the views.

	pDoc->SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: Submits the objects to be edited to the property pages so they can
//			update their controls.
// Input  : iPage - Page index or -1 to update all pages.
//-----------------------------------------------------------------------------
void CObjectProperties::LoadDataForPages(int iPage)
{
	//VPROF_BUDGET( "CObjectProperties::LoadDataForPages", "Object Properties" );

	if (m_bDummy)
	{
		return;
	}

	//
	// Determine whether we are editing multiple objects or not.
	//
	bool bMultiEdit = (m_DstObjects.Count() > 1);
	m_bCanEdit = true;

	//
	// Submit the edit objects to each page one at a time.
	//
	int nMode = CObjectPage::LoadFirstData;

	FOR_EACH_OBJ( m_DstObjects, pos )
	{
		CMapClass *pobj = m_DstObjects.Element(pos);

		if ( pobj->IsEditable() == false )
		{
			m_bCanEdit = false;
		}

		if (iPage != -1)
		{
			//
			// Specific page.
			//
			m_ppPages[iPage]->SetMultiEdit(bMultiEdit);

			void *pObject = GetEditObjectFromMapObject(pobj, m_ppPages[iPage]->GetEditObjectRuntimeClass());
			if (pObject != NULL)
			{
				m_ppPages[iPage]->UpdateData(nMode, pObject, m_bCanEdit);
				m_ppPages[iPage]->m_bHasUpdatedData = true;
			}
		}
		else for (int i = 0; i < m_nPages; i++)
		{
			//
			// All pages.
			//
			m_ppPages[i]->SetMultiEdit(bMultiEdit);

			// This page hasn't even been shown yet. Don't bother updating its data.
			if (m_ppPages[i]->m_bFirstTimeActive)
				continue;

			void *pObject = GetEditObjectFromMapObject(pobj, m_ppPages[i]->GetEditObjectRuntimeClass());
			if (pObject != NULL)
			{
				m_ppPages[i]->UpdateData(nMode, pObject, m_bCanEdit);
				m_ppPages[i]->m_bHasUpdatedData = true;
			}
		}

		nMode = CObjectPage::LoadData;
	}

	CButton *pApplyButton = reinterpret_cast<CButton *>(GetDlgItem(ID_APPLY_NOW));
	pApplyButton->EnableWindow( ( m_bCanEdit ? TRUE : FALSE ) );

	//
	// Tell the pages that we are done submitting data.
	//
	if (iPage != -1)
	{
		//
		// Specific page.
		//
		m_ppPages[iPage]->UpdateData(CObjectPage::LoadFinished, NULL, m_bCanEdit);
	}
	else for (int i = 0; i < m_nPages; i++)
	{
		//
		// All pages.
		//

		// This page hasn't even been shown yet. Don't bother updating its data.
		if (m_ppPages[i]->m_bFirstTimeActive)
			continue;

		m_ppPages[i]->UpdateData(CObjectPage::LoadFinished, NULL, m_bCanEdit);
	}

	//
	// Update the input/output icons based on the new data.
	//
	UpdateOutputButton();
	UpdateInputButton();
}


//-----------------------------------------------------------------------------
// Purpose: Adds the object to m_DstObjects unless it is a group, in which case
//			it is expanded (recursively) to its children.
//-----------------------------------------------------------------------------
void CObjectProperties::AddObjectExpandGroups(CMapClass *pObject)
{
	//VPROF_BUDGET( "CObjectProperties::AddObjectExpandGroups", "Object Properties" );

	if (pObject->IsGroup())
	{
		const CMapObjectList *pChildren = pObject->GetChildren();

		FOR_EACH_OBJ( *pChildren, pos )
		{
			AddObjectExpandGroups( pChildren->Element(pos) );
		}
	}
	else
	{
		m_DstObjects.AddToTail(pObject);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Updates the property page data when the selection contents change.
// Input  : pObjects - List of currently selected objects.
//-----------------------------------------------------------------------------
void CObjectProperties::ReloadData()
{
	//VPROF_BUDGET( "CObjectProperties::LoadData", "Object Properties" );

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

	//
	// Disable window so it does not gain focus during this operation.
	//
	EnableWindow(FALSE);


	//
	// Transfer the objects from pObjects to m_DstObjects, expanding
	// groups to their member children.
	//
	m_DstObjects.RemoveAll();
	if ( m_pOrgObjects )
	{
		FOR_EACH_OBJ( (*m_pOrgObjects), pos )
		{
			AddObjectExpandGroups( m_pOrgObjects->Element(pos) );
		}
	}

	m_pInstanceButton->ShowWindow( SW_HIDE );

	//
	// If there is only one object selected, copy its data to our temporary
	// edit objects.
	//
	if (m_DstObjects.Count() == 1)
	{
		//
		// Copy the single destination object's data to our temporary
		// edit objects.
		//
		CMapClass *pobj = m_DstObjects.Element(0);
		CopyDataToEditObjects( pobj );

		//
		// Set the window title to include the object's description.
		//
		char szTitle[MAX_PATH];
		sprintf(szTitle, "Object Properties: %s", pobj->GetDescription());
		SetWindowText(szTitle);

		CManifestInstance	*pManifestInstance = dynamic_cast< CManifestInstance * >( pobj );
		if ( pManifestInstance )
		{
			CManifest *pManifest = CMapDoc::GetManifest();

			if ( pManifest )
			{
				ShowWindow( SW_HIDE );
				if ( pDoc )
				{
					pDoc->UpdateAllViews( MAPVIEW_UPDATE_SELECTION | MAPVIEW_UPDATE_TOOL | MAPVIEW_RENDER_NOW );
				}
				pManifest->SetPrimaryMap( pManifestInstance->GetManifestMap() );
				return;
			}
		}

		CMapEntity	*pEntity = dynamic_cast< CMapEntity * >( pobj );
		if ( pEntity )
		{
			if ( strcmpi( pEntity->GetClassName(), "func_instance" ) == 0 )
			{
				pDoc->PopulateInstance( pEntity );
				CMapInstance	*pMapInstance = pEntity->GetChildOfType<CMapInstance>();
				if ( pMapInstance && pMapInstance->GetInstancedMap() )
				{
					m_pInstanceButton->ShowWindow( SW_SHOW );
				}
			}
			else if ( strcmpi( pEntity->GetClassName(), "func_instance_parms" ) == 0 )
			{
				if ( pDoc )
				{
					pDoc->PopulateInstanceParms( pEntity );
				}
			}
		}

	}
	else if (m_DstObjects.Count() > 1)
	{
		if ( m_DstObjects.FindMatch( []( CMapClass* i ) { return dynamic_cast<CMapSolid*>( i ) == nullptr; } ) != -1 )
			SetWindowText( "Object Properties: multiple objects" );
		else
			SetWindowText( "Object Properties: multiple solids" );
	}
	else
	{
		SetWindowText("Object Properties");
	}

	SetupPages();
	LoadDataForPages();

	EnableWindow(TRUE);

	m_bDataDirty = false;
}


BOOL CObjectProperties::OnInitDialog()
{
	BOOL b = CPropertySheet::OnInitDialog();
	SetWindowText("Object Properties");

	if ( !GetDynamicLayout() )
	{
		EnableDynamicLayout();
		GetDynamicLayout()->Create( this );
	}
	CreateButtons();

	return b;
}


//-----------------------------------------------------------------------------
// Purpose: Closes the object properties dialog, saving changes.
//-----------------------------------------------------------------------------
void CObjectProperties::OnClose(void)
{
	//VPROF_BUDGET( "CObjectProperties::OnClose", "Object Properties" );
	ApplyChanges( true );

	ShowWindow(SW_HIDE);
}

void CObjectProperties::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	if ( m_bDataDirty )
		ReloadData();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : bShow -
//			nStatus -
//-----------------------------------------------------------------------------
void CObjectProperties::OnShowWindow(BOOL bShow, UINT nStatus)
{
	//VPROF_BUDGET( "CObjectProperties::OnShowWindow", "Object Properties" );

	// Forget the last active page when the window is hidden or shown.
	// FIXME: SetupPages calls SaveActivePage, so we must switch to page 0 here
	SetActivePage(0);
	m_pLastActivePage = NULL;

	CPropertySheet::OnShowWindow(bShow, nStatus);

	for (int i = 0; i < m_nPages; i++)
	{
		m_ppPages[i]->OnShowPropertySheet(bShow, nStatus);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles the Apply button.
//-----------------------------------------------------------------------------
void CObjectProperties::ApplyChanges( bool bCalledOnClose )
{
	//VPROF_BUDGET( "CObjectProperties::OnApply", "Object Properties" );

	if ( !m_bCanEdit )
	{
		return;
	}

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
		return;

	//We lock visgroup updates here because activities in the object properties dialog can
	//change visgroups which, if updated, will change the object properties, causing problems.
	//All visgroup updates will occur at the end of this apply operation.
	bool bLocked = pDoc->VisGroups_LockUpdates( true );

	for (int i = 0; i < m_nPages; i++)
	{
		if (!m_ppPages[i]->OnApply())
		{
			return;
		}
	}

	//
	// Save and reload the data so the GUI updates.
	//

	SaveData_Reason_t reason = ( bCalledOnClose ) ? SAVEDATA_CLOSE : SAVEDATA_APPLY;
	SaveData( reason );

	ReloadData();

	// Pass along the apply message to the entities.
	FOR_EACH_OBJ( m_DstObjects, pos )
	{
		CMapClass *pObject = m_DstObjects.Element( pos );
		if ( pObject )
		{
			pObject->OnApply();
		}
	}

	if ( bLocked )
	{
		pDoc->VisGroups_LockUpdates( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles the Apply button.
//-----------------------------------------------------------------------------
void CObjectProperties::OnApply(void)
{
	ApplyChanges( false );
}

//-----------------------------------------------------------------------------
// Purpose: Handles <return> keys sent to OK -> apply instead
//-----------------------------------------------------------------------------
void CObjectProperties::OnOK(void)
{
	//VPROF_BUDGET( "CObjectProperties::OnClose", "Object Properties" );
	ApplyChanges( false );
}

//-----------------------------------------------------------------------------
// Purpose: Handles the Apply button.
//-----------------------------------------------------------------------------
void CObjectProperties::OnCancel(void)
{
	ShowWindow(SW_HIDE);

	// reload original data and overwrite any changes made prio
	ReloadData();
}


//-----------------------------------------------------------------------------
// Purpose: Handles the input icon button.
//-----------------------------------------------------------------------------
void CObjectProperties::OnInputs(void)
{
	SetActivePage(m_pInput);
}


//-----------------------------------------------------------------------------
// Purpose: Handles the output icon button.
//-----------------------------------------------------------------------------
void CObjectProperties::OnOutputs(void)
{
	SetActivePage(m_pOutput);
}


//-----------------------------------------------------------------------------
// Purpose: handle the pushing of the Edit Instance button.  Will attempt to
//			switch to the map document containing the instance.
// Input  : none
// Output : none
//-----------------------------------------------------------------------------
void CObjectProperties::OnEditInstance(void)
{
	if (m_DstObjects.Count() == 1)
	{
		CMapClass	*pObj = m_DstObjects.Element( 0 );
		CMapEntity	*pEntity = dynamic_cast< CMapEntity * >( pObj );

		if ( pEntity )
		{
			EnumChildrenPos_t	pos;
			CMapClass *pChild = pEntity->GetFirstDescendent( pos );
			while ( pChild != NULL )
			{
				CMapInstance *pMapInstance = dynamic_cast< CMapInstance * >( pChild );
				if ( pMapInstance != NULL )
				{
					OnClose();

					pMapInstance->SwitchTo();
				}

				pChild = pEntity->GetNextDescendent( pos );
			}
		}
	}

}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CObjectProperties::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	//VPROF_BUDGET( "CObjectProperties::OnCreate", "Object Properties" );

	lpCreateStruct->dwExStyle |= WS_EX_TOOLWINDOW;

	if (CPropertySheet::OnCreate(lpCreateStruct) == -1)
	{
		return -1;
	}

	return 0;
}

void CObjectProperties::SetObjectList(const CMapObjectList *pObjectList)
{
	m_pOrgObjects = pObjectList;
	MarkDataDirty();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CObjectProperties::MarkDataDirty()
{
	//VPROF_BUDGET( "CObjectProperties::RefreshData", "Object Properties" );

	// if flag already set, dont touch anything
	if ( m_bDataDirty )
		return;

	for (int i = 0; i < m_nPages; i++)
	{
		if (m_ppPages[i]->m_hWnd)
		{
			m_ppPages[i]->RememberState();
			m_ppPages[i]->MarkDataDirty();
		}
	}

	Invalidate( false );

	m_DstObjects.RemoveAll();

	m_bDataDirty = true;
}

