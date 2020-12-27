//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef OBJECTPAGE_H
#define OBJECTPAGE_H
#ifdef _WIN32
#pragma once
#endif

#include "mapclass.h"
#include "afxcmn.h"
enum SaveData_Reason_t : char;

// this should remain binary compatible with CMFCHeaderCtrl
class CHeaderCtrlEx : public CHeaderCtrl
{
	DECLARE_DYNAMIC(CHeaderCtrlEx)

	// Construction
public:
	CHeaderCtrlEx();

	// Attributes
public:
	int GetSortColumn() const;
	BOOL IsAscending() const;
	int GetColumnState(int iColumn) const; // Returns: 0 - not not sorted, -1 - descending, 1 - ascending

	BOOL IsMultipleSort() const { return m_bMultipleSort; }
	BOOL IsDialogControl() const { return m_bIsDlgControl; }

protected:
	CMap<int,int,int,int> m_mapColumnsStatus; // -1, 1, 0
	BOOL  m_bIsMousePressed;
	BOOL  m_bMultipleSort;
	BOOL  m_bAscending;
	BOOL  m_bTracked;
	BOOL  m_bIsDlgControl;
	int   m_nHighlightedItem;
	HFONT m_hFont;

	// Operations
public:
	void SetSortColumn(int iColumn, BOOL bAscending = TRUE, BOOL bAdd = FALSE);
	void RemoveSortColumn(int iColumn);
	void EnableMultipleSort(BOOL bEnable = TRUE);

	// Overrides
protected:
	virtual void PreSubclassWindow();
	virtual void OnDrawItem(CDC* pDC, int iItem, CRect rect, BOOL bIsPressed, BOOL bIsHighlighted);
	virtual void OnFillBackground(CDC* pDC);
	virtual void OnDrawSortArrow(CDC* pDC, CRect rectArrow);

	// Implementation
public:
	virtual ~CHeaderCtrlEx();

protected:
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnCancelMode();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnMouseLeave();
	afx_msg void OnSetFont(CFont* pFont, BOOL bRedraw);
	DECLARE_MESSAGE_MAP()

	void CommonInit();
	CFont* SelectFont(CDC *pDC);
};

class CListCtrlEx : public CListCtrl
{
	DECLARE_DYNAMIC( CListCtrlEx )

	// Construction
public:
	CListCtrlEx();

	// Attributes
public:
	virtual CHeaderCtrlEx& GetHeaderCtrl() { return m_wndHeader; }

	// Mark sorted column by background color
	void EnableMarkSortedColumn(BOOL bMark = TRUE, BOOL bRedraw = TRUE);

protected:
	CHeaderCtrlEx m_wndHeader;
	COLORREF	m_clrSortedColumn;
	int			m_iSortedColumn;
	BOOL		m_bAscending;
	BOOL		m_bMarkSortedColumn;
	HFONT		m_hOldFont;

	// Operations
public:
	// Sorting operations:
	virtual void Sort(int iColumn, BOOL bAscending = TRUE, BOOL bAdd = FALSE);
	void SetSortColumn(int iColumn, BOOL bAscending = TRUE, BOOL bAdd = FALSE);
	void RemoveSortColumn(int iColumn);
	void EnableMultipleSort(BOOL bEnable = TRUE);
	BOOL IsMultipleSort() const;

	// Overrides
	virtual int OnCompareItems(LPARAM lParam1, LPARAM lParam2, int iColumn);

	// Support for individual cells text/background colors:
	virtual COLORREF OnGetCellTextColor(int /*nRow*/, int /*nColum*/) { return GetTextColor(); }
	virtual COLORREF OnGetCellBkColor(int /*nRow*/, int /*nColum*/) { return GetBkColor(); }
	virtual HFONT OnGetCellFont(int /*nRow*/, int /*nColum*/, DWORD /*dwData*/ = 0) { return NULL; }

protected:
	virtual void PreSubclassWindow();

	// Implementation
public:
	virtual ~CListCtrlEx();

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSysColorChange();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnStyleChanged(int nStyleType, LPSTYLESTRUCT lpStyleStruct);

	DECLARE_MESSAGE_MAP()

	static int CALLBACK CompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
	BOOL InitList();
	void InitColors();

	virtual void InitHeader();
};



class CObjectPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CObjectPage)

public:

	CObjectPage(void)
	{
		m_bMultiEdit = false;
		m_bFirstTimeActive = true;
		m_bHasUpdatedData = false;
	}

	CObjectPage(UINT nResourceID) : CPropertyPage(nResourceID)
	{
		m_bMultiEdit = false;
		m_bFirstTimeActive = false;
	}

	~CObjectPage() {}

	virtual void MarkDataDirty() {}

	enum
	{
		LoadFirstData,
		LoadData,
		LoadFinished,
	};

	inline void SetObjectList(const CMapObjectList *pObjectList);

	// Called by the sheet to update the selected objects. pData points to the object being added to the selection.
	virtual void UpdateData( int Mode, PVOID pData, bool bCanEdit );

	// Called by the sheet to store this page's data into the objects being edited.
	virtual bool SaveData( SaveData_Reason_t reason ) { return(true); }

	// Called by the sheet to let the dialog remember its state before a refresh of the data.
	virtual void RememberState(void) {}

	virtual void SetMultiEdit(bool b) { m_bMultiEdit = b; }
	virtual void OnShowPropertySheet(BOOL bShow, UINT nStatus) {}

	void SetTitle( const char* titleText )
	{
		auto& psp = GetPSP();
		psp.pszTitle = titleText;
		psp.dwFlags |= PSP_USETITLE;
	}

	bool IsMultiEdit() { return m_bMultiEdit; }

	CRuntimeClass * GetEditObjectRuntimeClass(void) { return m_pEditObjectRuntimeClass; }
	PVOID GetEditObject();

	BOOL OnSetActive(void);
	BOOL OnApply(void) { return(TRUE); }

	bool m_bFirstTimeActive;					// Used to detect the first time this page becomes active.
	bool m_bHasUpdatedData;						// Used to prevent SaveData() called on pages that haven't had loaded the data yet.

	// Set while we are changing the page layout.
	static BOOL s_bRESTRUCTURING;

protected:

	const CMapObjectList *m_pObjectList;				// The list of objects that we are editing.
	bool m_bMultiEdit;							// Set to true if we are editing more than one object.
	bool m_bCanEdit;							// Set to true if this page allows for editing

	CRuntimeClass *m_pEditObjectRuntimeClass;	// The type of object that this page can edit.

	static const char *VALUE_DIFFERENT_STRING;
};


//-----------------------------------------------------------------------------
// Purpose: Sets the list of objects that this dialog should reflect.
// Input  : pObjectList - List of objects (typically the selection list).
//-----------------------------------------------------------------------------
void CObjectPage::SetObjectList(const CMapObjectList *pObjectList)
{
	Assert(pObjectList != NULL);
	m_pObjectList = pObjectList;
}


#endif // OBJECTPAGE_H
