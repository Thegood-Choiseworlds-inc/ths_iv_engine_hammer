//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "objectpage.h"
#include "globalfunctions.h"
#include "objectproperties.h"
#include "afxvisualmanager.h"
#include "afxglobals.h"
#include "afxdrawmanager.h"

#pragma region CHeaderCtrlEx


IMPLEMENT_DYNAMIC(CHeaderCtrlEx, CHeaderCtrl)

/////////////////////////////////////////////////////////////////////////////
// CHeaderCtrlEx

CHeaderCtrlEx::CHeaderCtrlEx()
{
	m_bIsMousePressed = FALSE;
	m_bMultipleSort = FALSE;
	m_bAscending = TRUE;
	m_nHighlightedItem = -1;
	m_bTracked = FALSE;
	m_bIsDlgControl = FALSE;
	m_hFont = NULL;
}

CHeaderCtrlEx::~CHeaderCtrlEx()
{
}

BEGIN_MESSAGE_MAP(CHeaderCtrlEx, CHeaderCtrl)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_CANCELMODE()
	ON_WM_CREATE()
	ON_WM_MOUSELEAVE()
	ON_WM_SETFONT()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CHeaderCtrlEx message handlers

void CHeaderCtrlEx::OnDrawItem(CDC* pDC, int iItem, CRect rect, BOOL bIsPressed, BOOL bIsHighlighted)
{
	ASSERT_VALID(this);
	ASSERT_VALID(pDC);

	const int nTextMargin = 5;

	// Draw border:
	CMFCVisualManager::GetInstance()->OnDrawHeaderCtrlBorder( reinterpret_cast<CMFCHeaderCtrl*>( this ), pDC, rect, bIsPressed, bIsHighlighted);

	if (iItem < 0)
	{
		return;
	}

	int nSortVal = 0;
	if (m_mapColumnsStatus.Lookup(iItem, nSortVal) && nSortVal != 0)
	{
		// Draw sort arrow:
		CRect rectArrow = rect;
		rectArrow.DeflateRect(5, 5);
		rectArrow.left = rectArrow.right - rectArrow.Height();

		if (bIsPressed)
		{
			rectArrow.right++;
			rectArrow.bottom++;
		}

		rect.right = rectArrow.left - 1;

		int dy2 = (int)(.134 * rectArrow.Width());
		rectArrow.DeflateRect(0, dy2);

		m_bAscending = nSortVal > 0;
		OnDrawSortArrow(pDC, rectArrow);
	}

	HD_ITEM hdItem;
	memset(&hdItem, 0, sizeof(hdItem));
	hdItem.mask = HDI_FORMAT | HDI_BITMAP | HDI_TEXT | HDI_IMAGE;

	TCHAR szText [256];
	hdItem.pszText = szText;
	hdItem.cchTextMax = 255;

	if (!GetItem(iItem, &hdItem))
	{
		return;
	}

	// Draw bitmap and image:
	if ((hdItem.fmt & HDF_IMAGE) && hdItem.iImage >= 0)
	{
		// By Max Khiszinsky:

		// The column has a image from imagelist:
		CImageList* pImageList = GetImageList();
		if (pImageList != NULL)
		{
			int cx = 0;
			int cy = 0;

			VERIFY(::ImageList_GetIconSize(*pImageList, &cx, &cy));

			CPoint pt = rect.TopLeft();
			pt.x ++;
			pt.y = (rect.top + rect.bottom - cy) / 2;

			VERIFY(pImageList->Draw(pDC, hdItem.iImage, pt, ILD_NORMAL));

			rect.left += cx;
		}
	}

	if ((hdItem.fmt &(HDF_BITMAP | HDF_BITMAP_ON_RIGHT)) && hdItem.hbm != NULL)
	{
		CBitmap* pBmp = CBitmap::FromHandle(hdItem.hbm);
		ASSERT_VALID(pBmp);

		BITMAP bmp;
		pBmp->GetBitmap(&bmp);

		CRect rectBitmap = rect;
		if (hdItem.fmt & HDF_BITMAP_ON_RIGHT)
		{
			rectBitmap.right--;
			rect.right = rectBitmap.left = rectBitmap.right - bmp.bmWidth;
		}
		else
		{
			rectBitmap.left++;
			rect.left = rectBitmap.right = rectBitmap.left + bmp.bmWidth;
		}

		rectBitmap.top += max(0L, (rectBitmap.Height() - bmp.bmHeight) / 2);
		rectBitmap.bottom = rectBitmap.top + bmp.bmHeight;

		pDC->DrawState(rectBitmap.TopLeft(), rectBitmap.Size(), pBmp, DSS_NORMAL);
	}

	// Draw text:
	if ((hdItem.fmt & HDF_STRING) && hdItem.pszText != NULL)
	{
		CRect rectLabel = rect;
		rectLabel.DeflateRect(nTextMargin, 0);

		CString strLabel = hdItem.pszText;

		UINT uiTextFlags = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
		if (hdItem.fmt & HDF_CENTER)
		{
			uiTextFlags |= DT_CENTER;
		}
		else if (hdItem.fmt & HDF_RIGHT)
		{
			uiTextFlags |= DT_RIGHT;
		}

		pDC->DrawText(strLabel, rectLabel, uiTextFlags);
	}
}

void CHeaderCtrlEx::SetSortColumn(int iColumn, BOOL bAscending, BOOL bAdd)
{
	ASSERT_VALID(this);

	if (iColumn < 0)
	{
		m_mapColumnsStatus.RemoveAll();
		return;
	}

	if (bAdd)
	{
		if (!m_bMultipleSort)
		{
			ASSERT(FALSE);
			bAdd = FALSE;
		}
	}

	if (!bAdd)
	{
		m_mapColumnsStatus.RemoveAll();
	}

	m_mapColumnsStatus.SetAt(iColumn, bAscending ? 1 : -1);
	RedrawWindow();
}

void CHeaderCtrlEx::RemoveSortColumn(int iColumn)
{
	ASSERT_VALID(this);
	m_mapColumnsStatus.RemoveKey(iColumn);
	RedrawWindow();
}

BOOL CHeaderCtrlEx::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;
}

void CHeaderCtrlEx::OnPaint()
{
	if (GetStyle() & HDS_FILTERBAR)
	{
		Default();
		return;
	}

	CPaintDC dc(this); // device context for painting
	CMemDC memDC(dc, this);
	CDC* pDC = &memDC.GetDC();

	CRect rectClip;
	dc.GetClipBox(rectClip);

	CRect rectClient;
	GetClientRect(rectClient);

	OnFillBackground(pDC);

	CFont* pOldFont = SelectFont(pDC);
	ASSERT_VALID(pOldFont);

	pDC->SetTextColor(GetGlobalData()->clrBtnText);
	pDC->SetBkMode(TRANSPARENT);

	CRect rect;
	GetClientRect(rect);

	CRect rectItem;
	int nCount = GetItemCount();

	int xMax = 0;

	for (int i = 0; i < nCount; i++)
	{
		// Is item pressed?
		CPoint ptCursor;
		::GetCursorPos(&ptCursor);
		ScreenToClient(&ptCursor);

		HDHITTESTINFO hdHitTestInfo;
		hdHitTestInfo.pt = ptCursor;

		int iHit = (int) SendMessage(HDM_HITTEST, 0, (LPARAM) &hdHitTestInfo);

		BOOL bIsHighlighted = iHit == i &&(hdHitTestInfo.flags & HHT_ONHEADER);
		BOOL bIsPressed = m_bIsMousePressed && bIsHighlighted;

		GetItemRect(i, rectItem);

		CRgn rgnClip;
		rectItem.left += 1;
		rectItem.right += 1;
		rgnClip.CreateRectRgnIndirect(&rectItem);
		pDC->SelectClipRgn(&rgnClip);

		// Draw item:
		OnDrawItem(pDC, i, rectItem, bIsPressed, m_nHighlightedItem == i);

		pDC->SelectClipRgn(NULL);

		xMax = max(xMax, (int)rectItem.right);
	}

	// Draw "tail border":
	if (nCount == 0)
	{
		rectItem = rect;
		rectItem.right++;
	}
	else
	{
		rectItem.left = xMax;
		rectItem.right = rect.right + 1;
	}

	OnDrawItem(pDC, -1, rectItem, FALSE, FALSE);

	pDC->SelectObject(pOldFont);
}

void CHeaderCtrlEx::OnFillBackground(CDC* pDC)
{
	ASSERT_VALID(this);
	ASSERT_VALID(pDC);

	CRect rectClient;
	GetClientRect(rectClient);

	CMFCVisualManager::GetInstance()->OnFillHeaderCtrlBackground(reinterpret_cast<CMFCHeaderCtrl*>( this ), pDC, rectClient);
}

CFont* CHeaderCtrlEx::SelectFont(CDC *pDC)
{
	ASSERT_VALID(this);
	ASSERT_VALID(pDC);

	CFont* pOldFont = NULL;

	if (m_hFont != NULL)
	{
		pOldFont = pDC->SelectObject(CFont::FromHandle(m_hFont));
	}
	else
	{
		pOldFont = m_bIsDlgControl ? (CFont*) pDC->SelectStockObject(DEFAULT_GUI_FONT) : pDC->SelectObject(&(GetGlobalData()->fontRegular));
	}

	return pOldFont;
}

void CHeaderCtrlEx::OnLButtonDown(UINT nFlags, CPoint point)
{
	m_bIsMousePressed = TRUE;
	CHeaderCtrl::OnLButtonDown(nFlags, point);
}

void CHeaderCtrlEx::OnLButtonUp(UINT nFlags, CPoint point)
{
	m_bIsMousePressed = FALSE;
	CHeaderCtrl::OnLButtonUp(nFlags, point);
}

void CHeaderCtrlEx::OnDrawSortArrow(CDC* pDC, CRect rectArrow)
{
	ASSERT_VALID(pDC);
	ASSERT_VALID(this);

	CMFCVisualManager::GetInstance()->OnDrawHeaderCtrlSortArrow(reinterpret_cast<CMFCHeaderCtrl*>( this ), pDC, rectArrow, m_bAscending);
}

void CHeaderCtrlEx::EnableMultipleSort(BOOL bEnable)
{
	ASSERT_VALID(this);

	if (m_bMultipleSort == bEnable)
	{
		return;
	}

	m_bMultipleSort = bEnable;

	if (!m_bMultipleSort)
	{
		m_mapColumnsStatus.RemoveAll();

		if (GetSafeHwnd() != NULL)
		{
			RedrawWindow();
		}
	}
}

int CHeaderCtrlEx::GetSortColumn() const
{
	ASSERT_VALID(this);

	if (m_bMultipleSort)
	{
		TRACE0("Call CHeaderCtrlEx::GetColumnState for muliple sort\n");
		ASSERT(FALSE);
		return -1;
	}

	int nCount = GetItemCount();
	for (int i = 0; i < nCount; i++)
	{
		int nSortVal = 0;
		if (m_mapColumnsStatus.Lookup(i, nSortVal) && nSortVal != 0)
		{
			return i;
		}
	}

	return -1;
}

BOOL CHeaderCtrlEx::IsAscending() const
{
	ASSERT_VALID(this);

	if (m_bMultipleSort)
	{
		TRACE0("Call CHeaderCtrlEx::GetColumnState for muliple sort\n");
		ASSERT(FALSE);
		return FALSE;
	}

	int nCount = GetItemCount();

	for (int i = 0; i < nCount; i++)
	{
		int nSortVal = 0;
		if (m_mapColumnsStatus.Lookup(i, nSortVal) && nSortVal != 0)
		{
			return nSortVal > 0;
		}
	}

	return FALSE;
}

int CHeaderCtrlEx::GetColumnState(int iColumn) const
{
	int nSortVal = 0;
	m_mapColumnsStatus.Lookup(iColumn, nSortVal);

	return nSortVal;
}

void CHeaderCtrlEx::OnMouseMove(UINT nFlags, CPoint point)
{
	if ((nFlags & MK_LBUTTON) == 0)
	{
		HDHITTESTINFO hdHitTestInfo;
		hdHitTestInfo.pt = point;

		int nPrevHighlightedItem = m_nHighlightedItem;
		m_nHighlightedItem = (int) SendMessage(HDM_HITTEST, 0, (LPARAM) &hdHitTestInfo);

		if ((hdHitTestInfo.flags & HHT_ONHEADER) == 0)
		{
			m_nHighlightedItem = -1;
		}

		if (!m_bTracked)
		{
			m_bTracked = TRUE;

			TRACKMOUSEEVENT trackmouseevent;
			trackmouseevent.cbSize = sizeof(trackmouseevent);
			trackmouseevent.dwFlags = TME_LEAVE;
			trackmouseevent.hwndTrack = GetSafeHwnd();
			TrackMouseEvent(&trackmouseevent);
		}

		if (nPrevHighlightedItem != m_nHighlightedItem)
		{
			RedrawWindow();
		}
	}

	CHeaderCtrl::OnMouseMove(nFlags, point);
}

void CHeaderCtrlEx::OnMouseLeave()
{
	m_bTracked = FALSE;

	if (m_nHighlightedItem >= 0)
	{
		m_nHighlightedItem = -1;
		RedrawWindow();
	}
}

void CHeaderCtrlEx::OnCancelMode()
{
	CHeaderCtrl::OnCancelMode();

	if (m_nHighlightedItem >= 0)
	{
		m_nHighlightedItem = -1;
		RedrawWindow();
	}
}

int CHeaderCtrlEx::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CHeaderCtrl::OnCreate(lpCreateStruct) == -1)
		return -1;

	CommonInit();

	return 0;
}

void CHeaderCtrlEx::PreSubclassWindow()
{
	CommonInit();
	CHeaderCtrl::PreSubclassWindow();
}

void CHeaderCtrlEx::CommonInit()
{
	ASSERT_VALID(this);

	for (CWnd* pParentWnd = GetParent(); pParentWnd != NULL;
		pParentWnd = pParentWnd->GetParent())
	{
		if (pParentWnd->IsKindOf(RUNTIME_CLASS(CDialog)))
		{
			m_bIsDlgControl = TRUE;
			break;
		}
	}
}

void CHeaderCtrlEx::OnSetFont(CFont* pFont, BOOL bRedraw)
{
	m_hFont = (HFONT)pFont->GetSafeHandle();

	if (bRedraw)
	{
		Invalidate();
		UpdateWindow();
	}
}

IMPLEMENT_DYNAMIC(CListCtrlEx, CListCtrl)

CListCtrlEx::CListCtrlEx()
{
	m_iSortedColumn = -1;
	m_bAscending = TRUE;
	m_bMarkSortedColumn = FALSE;
	m_clrSortedColumn = (COLORREF)-1;
	m_hOldFont = NULL;
}

CListCtrlEx::~CListCtrlEx()
{
}

BEGIN_MESSAGE_MAP(CListCtrlEx, CListCtrl)
	ON_WM_CREATE()
	ON_WM_ERASEBKGND()
	ON_WM_SYSCOLORCHANGE()
	ON_WM_SIZE()
	ON_WM_STYLECHANGED()
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CListCtrlEx::OnCustomDraw)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, &CListCtrlEx::OnColumnClick)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CListCtrlEx message handlers

BOOL CListCtrlEx::InitList()
{
	InitHeader();
	InitColors();
	return TRUE;
}

void CListCtrlEx::InitHeader()
{
	// Initialize header control:
	GetHeaderCtrl().SubclassDlgItem(0, this);
}

void CListCtrlEx::PreSubclassWindow()
{
	CListCtrl::PreSubclassWindow();

	_AFX_THREAD_STATE* pThreadState = AfxGetThreadState();
	if (pThreadState->m_pWndInit == NULL)
	{
		if (!InitList())
		{
			ASSERT(FALSE);
		}
	}
}

int CListCtrlEx::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CListCtrl::OnCreate(lpCreateStruct) == -1)
		return -1;

	if (!InitList())
	{
		return -1;
	}

	return 0;
}

void CListCtrlEx::OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	ENSURE(pNMListView != NULL);

	int iColumn = pNMListView->iSubItem;
	BOOL bShiftIsPressed = (::GetAsyncKeyState(VK_SHIFT) & 0x8000);
	int nColumnState = GetHeaderCtrl().GetColumnState(iColumn);
	BOOL bAscending = TRUE;

	if (nColumnState != 0)
	{
		bAscending = nColumnState <= 0;
	}

	Sort(iColumn, bAscending, bShiftIsPressed && IsMultipleSort());
	*pResult = 0;
}

void CListCtrlEx::Sort(int iColumn, BOOL bAscending, BOOL bAdd)
{
	CWaitCursor wait;

	GetHeaderCtrl().SetSortColumn(iColumn, bAscending, bAdd);

	m_iSortedColumn = iColumn;
	m_bAscending = bAscending;

	SortItems(CompareProc, (LPARAM) this);
}

void CListCtrlEx::SetSortColumn(int iColumn, BOOL bAscending, BOOL bAdd)
{
	GetHeaderCtrl().SetSortColumn(iColumn, bAscending, bAdd);
}

void CListCtrlEx::RemoveSortColumn(int iColumn)
{
	GetHeaderCtrl().RemoveSortColumn(iColumn);
}

void CListCtrlEx::EnableMultipleSort(BOOL bEnable)
{
	GetHeaderCtrl().EnableMultipleSort(bEnable);
}

BOOL CListCtrlEx::IsMultipleSort() const
{
	return((CListCtrlEx*) this)->GetHeaderCtrl().IsMultipleSort();
}

int CListCtrlEx::OnCompareItems(LPARAM /*lParam1*/, LPARAM /*lParam2*/, int /*iColumn*/)
{
	return 0;
}

int CALLBACK CListCtrlEx::CompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	CListCtrlEx* pList = (CListCtrlEx*) lParamSort;
	ASSERT_VALID(pList);

	int nRes = pList->OnCompareItems(lParam1, lParam2, pList->m_iSortedColumn);
	nRes = pList->m_bAscending ? nRes : -nRes;

	return nRes;
}

void CListCtrlEx::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	ENSURE(pNMHDR != NULL);
	LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)pNMHDR;

	switch (lplvcd->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		*pResult = CDRF_NOTIFYITEMDRAW;
		break;

	case CDDS_ITEMPREPAINT:
		*pResult = CDRF_NOTIFYSUBITEMDRAW;
		break;

	case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
	{
		int iColumn = lplvcd->iSubItem;
		int iRow = (int) lplvcd->nmcd.dwItemSpec;

		lplvcd->clrTextBk = OnGetCellBkColor(iRow, iColumn);
		lplvcd->clrText = OnGetCellTextColor(iRow, iColumn);

		if (iColumn == m_iSortedColumn && m_bMarkSortedColumn && lplvcd->clrTextBk == GetBkColor())
		{
			lplvcd->clrTextBk = m_clrSortedColumn;
		}

		HFONT hFont = OnGetCellFont( iRow, iColumn, (DWORD) lplvcd->nmcd.lItemlParam);
		if (hFont != NULL)
		{
			m_hOldFont = (HFONT) SelectObject(lplvcd->nmcd.hdc, hFont);
			ENSURE(m_hOldFont != NULL);

			*pResult = CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT;
		}
		else
		{
			*pResult = CDRF_DODEFAULT;
		}
	}
	break;

	case CDDS_ITEMPOSTPAINT | CDDS_SUBITEM:
		if (m_hOldFont != NULL)
		{
			SelectObject(lplvcd->nmcd.hdc, m_hOldFont);
			m_hOldFont = NULL;
		}

		*pResult = CDRF_DODEFAULT;
		break;
	}
}

void CListCtrlEx::EnableMarkSortedColumn(BOOL bMark/* = TRUE*/, BOOL bRedraw/* = TRUE */)
{
	m_bMarkSortedColumn = bMark;

	if (GetSafeHwnd() != NULL && bRedraw)
	{
		RedrawWindow();
	}
}

BOOL CListCtrlEx::OnEraseBkgnd(CDC* pDC)
{
	BOOL bRes = CListCtrl::OnEraseBkgnd(pDC);

	if (m_iSortedColumn >= 0 && m_bMarkSortedColumn)
	{
		CRect rectClient;
		GetClientRect(&rectClient);

		CRect rectHeader;
		GetHeaderCtrl().GetItemRect(m_iSortedColumn, &rectHeader);
		GetHeaderCtrl().MapWindowPoints(this, rectHeader);

		CRect rectColumn = rectClient;
		rectColumn.left = rectHeader.left;
		rectColumn.right = rectHeader.right;

		CBrush br(m_clrSortedColumn);
		pDC->FillRect(rectColumn, &br);
	}

	return bRes;
}

void CListCtrlEx::OnSysColorChange()
{
	CListCtrl::OnSysColorChange();

	InitColors();
	RedrawWindow();
}

void CListCtrlEx::InitColors()
{
	m_clrSortedColumn = CDrawingManager::PixelAlpha(GetBkColor(), .97, .97, .97);
}

void CListCtrlEx::OnStyleChanged(int nStyleType, LPSTYLESTRUCT lpStyleStruct)
{
	CListCtrl::OnStyleChanged(nStyleType, lpStyleStruct);

	if ((lpStyleStruct->styleNew & LVS_REPORT) && (lpStyleStruct->styleOld & LVS_REPORT) == 0)
	{
		if (GetHeaderCtrl().GetSafeHwnd() == NULL)
		{
			InitHeader();
		}
	}
}

void CListCtrlEx::OnSize(UINT nType, int cx, int cy)
{
	CListCtrl::OnSize(nType, cx, cy);

	if (GetHeaderCtrl().GetSafeHwnd() != NULL)
	{
		GetHeaderCtrl().RedrawWindow();
	}
}


#pragma endregion


//
// Used to indicate multiselect of entities with different keyvalues.
//
const char *CObjectPage::VALUE_DIFFERENT_STRING = "(different)";

//
// Set while we are changing the page layout.
//
BOOL CObjectPage::s_bRESTRUCTURING = FALSE;


IMPLEMENT_DYNCREATE(CObjectPage, CPropertyPage)


//-----------------------------------------------------------------------------
// Purpose: stores whether or not this page can be updated
// Input  : Mode - unused
//			pData - unused
//			bCanEdit - the edit state
//-----------------------------------------------------------------------------
void CObjectPage::UpdateData( int Mode, PVOID pData, bool bCanEdit )
{
	m_bCanEdit = bCanEdit;
}


//-----------------------------------------------------------------------------
// Purpose: Called when we become the active page.
//-----------------------------------------------------------------------------
BOOL CObjectPage::OnSetActive(void)
{
	//VPROF_BUDGET( "CObjectPage::OnSetActive", "Object Properties" );

	CObjectProperties *pParent = (CObjectProperties *)GetParent();

	if (CObjectPage::s_bRESTRUCTURING || !GetActiveWorld())
	{
		auto res = CPropertyPage::OnSetActive();

		if ( pParent->GetDynamicLayout() )
			pParent->GetDynamicLayout()->AddItem( GetSafeHwnd(), CMFCDynamicLayout::MoveNone(), CMFCDynamicLayout::SizeHorizontalAndVertical( 100, 100 ) );

		return res;
	}

	if (m_bFirstTimeActive)
	{
		m_bFirstTimeActive = false;
		pParent->LoadDataForPages(pParent->GetPageIndex(this));
	}

	auto res = CPropertyPage::OnSetActive();

	pParent->GetDynamicLayout()->AddItem( GetSafeHwnd(), CMFCDynamicLayout::MoveNone(), CMFCDynamicLayout::SizeHorizontalAndVertical( 100, 100 ) );

	return res;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
PVOID CObjectPage::GetEditObject()
{
	//VPROF_BUDGET( "CObjectPage::GetEditObject", "Object Properties" );
	return ((CObjectProperties*) GetParent())->GetEditObject(GetEditObjectRuntimeClass());
}


