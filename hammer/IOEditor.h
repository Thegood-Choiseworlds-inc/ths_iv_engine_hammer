#pragma once

#include "resource.h"
#include "VGuiWnd.h"

class CIOEditorDialog;

class CIOEditor : public CVguiDialog
{
	DECLARE_DYNAMIC(CIOEditor)

public:
	CIOEditor( CWnd* pParent = NULL );   // standard constructor
	~CIOEditor() override;

	// Dialog Data
	enum { IDD = IDD_IO_EDITOR };

protected:
	virtual void DoDataExchange( CDataExchange* pDX );    // DDX/DDV support
	virtual BOOL PreTranslateMessage( MSG* pMsg );

	DECLARE_MESSAGE_MAP()

	float flLastPaint;

public:
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnDestroy();
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );

	virtual BOOL OnInitDialog();

	void SaveLoadSettings( bool bSave );
	void Resize();

	CVGuiPanelWnd		m_VGuiWindow;

	CIOEditorDialog*	m_pDialog;

	void Show();
	void Hide();
};