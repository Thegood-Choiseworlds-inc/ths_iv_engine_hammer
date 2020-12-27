
#include "../stdafx.h"
#undef GetUserName

#include "vnodeview.h"

#include "editorcommon.h"
#include "vbasecontainer.h"
#include "vjack.h"
#include "vbridge.h"

#include "node_entity.h"
#include "node_addoutput.h"
#include "node_general_out.h"

#include "vgui/IPanel.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"

#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"

#include "vgui_controls/Menu.h"
#include "vgui_controls/MenuItem.h"
#include "vgui_controls/AnimationController.h"
#include "tier1/ilocalize.h"
#include "cmodel.h"
#include "trace.h"
#include "collisionutils.h"

#include "../mapdoc.h"

#include "tier0/memdbgon.h"

extern float vguiFrameTime;

class CSmartTooltip : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CSmartTooltip, vgui::Panel );

public:
	enum
	{
		STTIPMODE_NORMAL = 0,
		STTIPMODE_FUNCTIONPARAMS
	};

	CSmartTooltip( Panel* parent, const char* pElementname )
	{
		m_iParamHighlight = 0;
		m_iMode = STTIPMODE_NORMAL;
		if ( m_iFont == 0 )
		{
			m_iFont = vgui::surface()->CreateFont();
			vgui::surface()->SetFontGlyphSet( m_iFont, "Consolas", 14, 0, 0, 0, vgui::ISurface::FONTFLAG_CUSTOM | vgui::ISurface::FONTFLAG_ANTIALIAS );
		}

		SetVisible( true );
		MakePopup( false );

		SetMouseInputEnabled( false );
		SetKeyBoardInputEnabled( false );
	}

	void SetActiveParam( int iP )
	{
		m_iParamHighlight = iP;
	}

	void Paint() override
	{
		BaseClass::Paint();

		/*if ( !m_pCurObject )
			return;

		const char *pStr[] = {
			m_pCurObject->m_pszDefinition,
			m_pCurObject->m_pszHelptext,
			m_pCurObject->m_pszSourceFile,
		};

		int iFontTall = vgui::surface()->GetFontTall( m_iFont );
		int iCWidth = vgui::surface()->GetCharacterWidth( m_iFont, ' ' );
		int screenx, screeny;
		surface()->GetScreenSize( screenx, screeny );

		int cX, cY;
		int sX, sY;
		GetSize( sX, sY );
		sX -= iCWidth;

		const bool bFuncDraw = m_iMode == STTIPMODE_FUNCTIONPARAMS;

		Color colDef( 10, 10, 11, 255 );
		Color colHighlight( 0, 0, 0, 255 );

		if ( bFuncDraw )
			colDef = Color( 70, 70, 70, 255 );

		surface()->DrawSetTextFont( m_iFont );
		surface()->DrawSetTextColor( colDef );

		cY = 0;

		int iCurParam = 1;
		bool bHitBracket = false;

		for ( int i = 0; i < 3; i++ )
		{
			if ( !pStr[i] || !*pStr[i] )
				continue;

			cX = 0;

			const char *pWalk = pStr[i];
			bool bLastLinebreak = false;
			while ( *pWalk )
			{
				const bool bLineBreak = *pWalk == '\n';
				const bool bTab = *pWalk == '\t';
				const bool bComma = *pWalk == ',';
				const bool bBracket = *pWalk == '(';

				surface()->DrawSetTextPos( cX, cY );

				if ( bFuncDraw && bHitBracket )
					surface()->DrawSetTextColor( (!bComma && iCurParam == m_iParamHighlight) ? colHighlight : colDef );

				if ( !bLineBreak )
					surface()->DrawUnicodeChar( bTab ? L' ' : *pWalk );

				if ( bFuncDraw )
				{
					if ( bComma )
						iCurParam++;
					if ( bBracket )
						bHitBracket = true;
				}

				int curWidth = iCWidth; //surface()->GetCharacterWidth( m_iFont, *pWalk );

				if ( cX + (curWidth) < sX && !bLineBreak )
				{
					cX += curWidth;
					bLastLinebreak = false;
				}
				else if ( *( pWalk + 1 ) || bLineBreak && !bLastLinebreak )
				{
					cX = 0;
					cY += iFontTall;
					bLastLinebreak = bLineBreak;
				}

				pWalk++;
			}

			cY += iFontTall * 2;
		}*/
	}

	void ApplySchemeSettings( vgui::IScheme* pScheme ) override
	{
		BaseClass::ApplySchemeSettings( pScheme );

		SetBorder( pScheme->GetBorder("ButtonBorder") );

		SetBgColor( Color( 150, 170, 220, 160 ) );
	}

	bool IsFunctionParams();

private:
	int GetNumRowsForString( const char* pStr, int* max_x = NULL );

	static vgui::HFont m_iFont;

	int m_iMode;
	int m_iParamHighlight;
};

vgui::HFont CSmartTooltip::m_iFont = 0;

vgui::HFont CNodeView::_fonts[CNodeView::MAX_ZOOM_FONTS]{ 0 };

void SetupVguiTex( int& var, const char* tex )
{
	var = vgui::surface()->DrawGetTextureId( tex );
	if ( var <= 0 )
	{
		var = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( var, tex, true, false );
	}
}

Color Lerp( float perc, const Color& a, const Color& b )
{
	return Color( a[0] + (b[0] - a[0]) * perc,
		a[1] + (b[1] - a[1]) * perc,
		a[2] + (b[2] - a[2]) * perc,
		a[3] + (b[3] - a[3]) * perc );
}

constexpr float VIEWSPEED_PAN = 30;
constexpr float VIEWSPEED_ZOOM = 30;

constexpr float BRIDGE_DRAG_TOLERANCE = 10;

constexpr float VIEWZOOM_SPEED = 0.65f;

constexpr float GRID_CELL_SIZE = 100.0f;

CNodeView::CNodeView( Panel* parent, const char* pElementName )
	: BaseClass( parent, pElementName )
{
	InitColors();

	SetProportional( false );

	m_flZoom = 8;
	m_flZoomGoal = 8;
	m_vecPosition.Init();
	m_vecPositionGoal.Init();

	m_lTime_MouseLastMoved = 0.0f;
	m_flErrorTime = 0;

	SetupVguiTex( m_iTex_Darken, "editor/darken" );

	pJackLast = nullptr;
	m_pCurBridge = nullptr;
	m_pTooltip = nullptr;

	m_StackIndex = 0;

	m_iCursorLast = vgui::dc_user;

	if ( !_fonts[0] )
	{
		constexpr int flags = vgui::ISurface::FONTFLAG_CUSTOM | vgui::ISurface::FONTFLAG_DROPSHADOW | vgui::ISurface::FONTFLAG_ANTIALIAS;
		for ( int i = 0; i < MAX_ZOOM_FONTS; i++ )
		{
			_fonts[i] = vgui::surface()->CreateFont();
			vgui::surface()->SetFontGlyphSet( _fonts[i], "Tahoma", 1 + i, 1, 0, 0, flags );
		}
	}

	Init();
}

CNodeView::~CNodeView()
{
	PurgeCanvas();

	KillDragBridge();

	DestroyTooltip();
}

void CNodeView::OnContainerRemoved( CBaseContainerNode* container )
{
	int idx = m_hContainerNodes.Find( container );
	if ( !m_hContainerNodes.IsValidIndex( idx ) )
		return;
	m_hContainerNodes.Remove( idx );

	Assert( !m_hContainerNodes.IsValidIndex( m_hContainerNodes.Find( container ) ) );
}

void CNodeView::OnContainerAdded( CBaseContainerNode* container )
{
	Assert( !m_hContainerNodes.IsValidIndex( m_hContainerNodes.Find( container ) ) );
	m_hContainerNodes.AddToTail( container );
}

void CNodeView::ListContainerAtPos( Vector2D& pos, CUtlVector<CBaseContainerNode*>& hList )
{
	hList.Purge();
	for ( int i = 0; i < m_hContainerNodes.Count(); i++ )
	{
		if ( m_hContainerNodes[i]->IsInContainerBounds( pos ) )
			hList.AddToTail( m_hContainerNodes[i] );
	}
}

void CNodeView::PurgeCanvas()
{
	if ( m_pCurBridge )
	{
		delete m_pCurBridge;
		m_pCurBridge = nullptr;
	}
	SafeDeleteVector( m_hBridgeList );
	for ( int i = 0; i < m_hNodeList.Count(); i++ )
		m_hNodeList[i]->MarkForDeletion();
	SafeDeleteVector( m_hNodeList );
	m_hNodesInMove.Purge();
	m_hTmpNodeIndex.Purge();

	m_hContainerNodes.Purge();
}

void CNodeView::InitCanvas()
{
	PurgeCanvas();

	CBaseNode* n = SpawnNode( IONODE_ENTITY );
	n->SetPosition( Vector2D( 0, 0 ) );
}

void CNodeView::Init()
{
	ResetView();
	PurgeCanvas();
}

void CNodeView::OnBridgeRemoved( CBridge* b )
{
	if ( b == m_pCurBridge )
		m_pCurBridge = nullptr;
	m_hBridgeList.FindAndRemove( b );
}

void CNodeView::OnJackRemoved( const CJack* j )
{
	if ( pJackLast == j )
		pJackLast = nullptr;
}

bool CNodeView::IsMouseOver() const
{
	return const_cast<ThisClass*>( this )->GetVPanel() == vgui::input()->GetMouseOver();
}

void CNodeView::ResetView()
{
	m_vecMenuCreationPosition.Init();

	ResetView_User();

	m_vecPosition.Init();
	m_flZoom = m_flZoomGoal;

	StopDrag( iArmDrag == DRAG_NODES );
	iArmDrag = DRAG_NONE;

	UpdateMousePos();
	MxOld = Mx;
	MyOld = My;
	MxDelta = MyDelta = 0;

	if ( pJackLast )
		pJackLast->SetFocus( false );
	pJackLast = nullptr;
	bOverrideNodeIndices = false;
}

void CNodeView::ResetView_User( bool bInitial )
{
	if ( bInitial )
	{
		int x, y;
		GetSize( x, y );
		float relative = 592.0f / y;
		m_flZoomGoal = clamp( relative * 1.0f, VIEWZOOM_IN_MAX, VIEWZOOM_OUT_MAX );
		m_vecPositionGoal.Init();

		m_flZoom = VIEWZOOM_OUT_MAX;
		return;
	}

	int nsx, nsy;
	GetSize( nsx, nsy );
	Vector4D bounds;
	GetGraphBoundaries( bounds );

	Assert( bounds.z > bounds.x );
	Assert( bounds.w > bounds.y );

	m_vecPositionGoal.Init( -1.0f * bounds.x, bounds.y );
	m_vecPositionGoal.x -= ( bounds.z - bounds.x ) * 0.5f;
	m_vecPositionGoal.y += ( bounds.w - bounds.y ) * 0.5f;

	float delta_horizontal = ( bounds.z - bounds.x ) / nsx;
	float delta_vertical = ( bounds.w - bounds.y ) / nsy;
	m_flZoomGoal = max( delta_horizontal, delta_vertical ) + 0.2f;
	m_flZoomGoal = clamp( m_flZoomGoal, VIEWZOOM_IN_MAX, VIEWZOOM_OUT_MAX );
}

bool CNodeView::IsCursorOutOfBounds( Vector2D* delta )
{
	UpdateMousePos();

	int w, t;
	GetSize( w, t );

	if ( delta )
	{
		delta->Init();

		Vector2D mid( w * 0.5f, t * 0.5f );
		Vector2D cur( Mx, My );

		Vector2D _min( 0, t );
		Vector2D _max( w, 0 );

		ToNodeSpace( mid );
		ToNodeSpace( cur );
		ToNodeSpace( _min );
		ToNodeSpace( _max );

		Vector rstart( mid.x, mid.y, 0 );
		Vector rend( cur.x, cur.y, 0 );
		Ray_t ray;
		ray.Init( rstart, rend );

		Vector bmin( _min.x, _min.y, -10 );
		Vector bmax( _max.x, _max.y, 10 );

		float out = 0;
		CBaseTrace tr;
		if ( IntersectRayWithBox( ray, bmin, bmax, 1.0f, &tr, &out ) )
		{
			rend -= rstart + ( rend - rstart ) * out;
			delta->Init( rend.x, rend.y );
		}
	}

	if ( Mx < 0 || My < 0 )
		return true;
	if ( Mx > w || My > t )
		return true;

	if ( delta )
		delta->Init();

	return false;
}

float CNodeView::GetZoomScalar() const
{
	return 1.0f / max( 0.001f, m_flZoom );
}

void CNodeView::SetZoomScalar( float zoom )
{
	m_flZoom = zoom;
}

void CNodeView::ToNodeSpace( Vector2D& pos ) const
{
	auto t = const_cast<ThisClass*>( this );
	Vector2D panelMid( t->GetWide() * 0.5f, t->GetTall() * 0.5f );
	Vector2D origin = panelMid + m_vecPosition;

	pos -= origin;

	Vector2D panelMidNodeSpace = panelMid - origin;
	Vector2D delta( pos - panelMidNodeSpace );

	delta *= m_flZoom;

	pos = panelMidNodeSpace + delta;
	pos.y *= -1.0f;
}

void CNodeView::ToPanelSpace( Vector2D& pos ) const
{
#if 1
	auto t = const_cast<ThisClass*>( this );
	Vector2D panelMid( t->GetWide(), t->GetTall() );
	panelMid *= 0.5f * m_flZoom;
	Vector2D origin = panelMid + m_vecPosition;

	Vector2D MidPanelSpace = panelMid;
	//ToNodeSpace( MidPanelSpace );

	Vector2D delta( pos - MidPanelSpace );
	//delta *= 1.0f / max( 0.001f, m_flZoom );
	pos = MidPanelSpace + delta;

	pos.x += origin.x;
	pos.y -= origin.y;
	pos.y *= -1.0f;
	pos *= 1.0f / max( 0.001f, m_flZoom );
#else
	Vector2D panelMid( 0.5f * GetWide(), 0.5f * GetTall() );
	Vector2D origin = panelMid + m_vecPosition;

	Vector2D MidPanelSpace = panelMid;
	ToNodeSpace( MidPanelSpace );

	Vector2D delta( pos - MidPanelSpace );
	delta *= 1.0f / max( 0.001f, m_flZoom );
	pos = MidPanelSpace + delta;

	pos.x += origin.x;
	pos.y -= origin.y;
	pos.y *= -1.0f;
#endif
}

void CNodeView::GetGraphBoundaries( Vector4D& out ) const
{
	if ( !GetNumNodes() )
		out.Init( -100, -100, 100, 100 );
	else
	{
		CBaseNode* node = GetNode( 0 );
		Vector2D local_min = node->GetBoundsMinNodeSpace();
		Vector2D local_max = node->GetBoundsMaxNodeSpace();
		out.Init( local_min.x, local_min.y, local_max.x, local_max.y );
	}

	for ( int i = 1; i < GetNumNodes(); i++ )
	{
		CBaseNode* node = GetNode( i );

		Vector2D local_min = node->GetBoundsMinNodeSpace();
		Vector2D local_max = node->GetBoundsMaxNodeSpace();

		out.x = min( out.x, local_min.x );
		out.y = min( out.y, local_min.y );
		out.z = max( out.z, local_max.x );
		out.w = max( out.w, local_max.y );
	}
}

Vector2D CNodeView::GetMousePosInNodeSpace()
{
	UpdateMousePos();
	Vector2D mN( Mx, My );
	ToNodeSpace( mN );
	return mN;
}

void CNodeView::OnThink()
{
	UpdateMousePos();
	AccumulateMouseMove();

	BaseClass::OnThink();

	Think_HighlightJacks();
	Think_UpdateCursorIcon();
	Think_SmoothView();
	Think_Drag();
	Think_ArrowMove();

	Think_CreateTooltip();
	SaveMousePos();
}

void CNodeView::Think_SmoothView()
{
	for ( int i = 0; i < 2; i++ )
	{
		float delta = ( m_vecPositionGoal[i] - m_vecPosition[i] );
		float move = delta * vguiFrameTime * VIEWSPEED_PAN;
		if ( abs( move ) > abs( delta ) )
			move = delta;
		m_vecPosition[i] += move;
	}

	float delta = ( m_flZoomGoal - m_flZoom );
	float move = delta * vguiFrameTime * VIEWSPEED_ZOOM;
	if ( abs( move ) > abs( delta ) )
		move = delta;
	m_flZoom += move;
}

void CNodeView::Think_Drag()
{
	if ( !IsInDrag() )
		return;

	bool bNodeDrag = iArmDrag == DRAG_NODES;
	bool bSBoxDrag = iArmDrag == DRAG_SBOX;
	bool bBridgeDrag = iArmDrag == DRAG_BRDIGE;

	if ( bBridgeDrag && m_pCurBridge )
	{
		Vector2D target = GetMousePosInNodeSpace();
		CJack* curJack = GetBridgeSnapJack();
		if ( curJack )
		{
			target = curJack->GetCenter();
			if ( !curJack->IsInput() )
				target.x = curJack->GetBoundsMax().x;
			else
				target.x = curJack->GetBoundsMin().x;
		}

		m_pCurBridge->SetTemporaryTarget( target );
	}

	if ( bSBoxDrag || bBridgeDrag )
	{
		Think_PullView();
		return;
	}

	Vector2D curpos( Mx, My );
	Vector2D oldpos( MxOld, MyOld );

	ToNodeSpace( curpos );
	ToNodeSpace( oldpos );

	Vector2D delta = curpos - oldpos;

	if ( bNodeDrag )
	{
		Vector2D d = Think_PullView();
		DragSelection( delta + d );
		return;
	}

	delta.y *= -1.0f;

	m_vecPositionGoal += delta;
}

Vector2D CNodeView::Think_PullView()
{
	Vector2D delta;
	if ( !IsCursorOutOfBounds( &delta ) )
		return Vector2D( 0, 0 );

	float len = delta.NormalizeInPlace();
	len = min( 20 / GetZoomScalar(), len );
	delta *= len;

	delta *= vguiFrameTime * 75;

	m_vecPositionGoal -= Vector2D( delta.x, -delta.y );
	return delta;
}

void CNodeView::Think_ArrowMove()
{

}

void CNodeView::Think_HighlightJacks()
{
	CJack* cur = nullptr;
	if ( !IsInDrag() || iArmDrag != DRAG_BRDIGE )
	{
		float t = BRIDGE_DRAG_TOLERANCE;
		cur = GetJackUnderCursor( &t );
	}
	else
		cur = GetBridgeSnapJack();

	if ( IsInDrag() && iArmDrag != DRAG_BRDIGE )
		cur = nullptr;

	for ( int i = 0; cur != nullptr && i < cur->GetNumBridges(); i++ )
		cur->GetBridge( i )->SetTemporaryColor( CBridge::TMPCOL_TRANS );

	if ( cur == pJackLast )
		return;

	if ( pJackLast )
		pJackLast->SetFocus( false );

	if ( cur )
		cur->SetFocus( true );

	pJackLast = cur;
}

void CNodeView::Think_UpdateCursorIcon()
{
	int hoverCursor = 0;
	int dcTarget = vgui::dc_user;

	Vector2D mPos( Mx, My );
	ToNodeSpace( mPos );

	if ( !pJackLast && IsMouseOver() )
	{
		if ( IsInDrag() && iArmDrag == DRAG_NODES )
		{
			dcTarget = m_iCursorLast;
		}
		else
		{
			for ( int i = 0; i < m_hContainerNodes.Count(); i++ )
			{
				hoverCursor = m_hContainerNodes[i]->IsInBorderBounds( mPos );
				if ( hoverCursor )
					break;
			}
		}
	}

	switch ( hoverCursor )
	{
	default:
		break;
	case CBORDER_TOP_LEFT:
	case CBORDER_BOTTOM_RIGHT:
		dcTarget = vgui::dc_sizenwse;
		break;
	case CBORDER_TOP_RIGHT:
	case CBORDER_BOTTOM_LEFT:
		dcTarget = vgui::dc_sizenesw;
		break;
	case CBORDER_TOP:
	case CBORDER_BOTTOM:
		dcTarget = vgui::dc_sizens;
		break;
	case CBORDER_RIGHT:
	case CBORDER_LEFT:
		dcTarget = vgui::dc_sizewe;
		break;
	}
	if ( m_iCursorLast == dcTarget )
		return;

	m_iCursorLast = dcTarget;
	vgui::input()->SetCursorOveride( dcTarget );
}

void CNodeView::Think_CreateTooltip()
{
	if ( m_pTooltip )
		return;

	if ( m_ContextMenu && m_ContextMenu->IsVisible() )
		return;

	if ( m_lTime_MouseLastMoved <= 0 )
		return;

	long curTime = vgui::system()->GetTimeMillis();
	if ( curTime - m_lTime_MouseLastMoved > 1000 )
	{
		CBaseNode* pNode = GetNodeUnderCursor();
		if ( pNode )
			CreateTooltip( pNode->GetNodeType() );
	}
}

void CNodeView::UpdateMousePos()
{
	vgui::input()->GetCursorPosition( Mx, My );
	ScreenToLocal( Mx, My );
}

void CNodeView::AccumulateMouseMove()
{
	MxDelta = Mx - MxOld;
	MyDelta = My - MyOld;
}
void CNodeView::SaveMousePos()
{
	MxOld = Mx;
	MyOld = My;
}

void CNodeView::ForceFocus()
{
	vgui::input()->SetMouseFocus( GetVPanel() );
	if ( m_ContextMenu.Get() )
		m_ContextMenu->OnKillFocus();
}

void CNodeView::OnKeyCodeTyped( vgui::KeyCode code )
{
	DestroyTooltip();

	OnParentKeyCodeTyped( code );
}

void CNodeView::OnKeyCodePressed( vgui::KeyCode code )
{
	OnParentKeyCodePressed( code );
}

void CNodeView::OnKeyCodeReleased( vgui::KeyCode code )
{
	OnParentKeyCodeReleased( code );
}

void CNodeView::OnParentKeyCodeTyped( vgui::KeyCode code )
{
	if ( !AllowKeyInput() ) return;
}

void CNodeView::OnParentKeyCodePressed( vgui::KeyCode code )
{
	if ( !AllowKeyInput() ) return;

	if ( vgui::input()->IsKeyDown( KEY_LCONTROL ) || vgui::input()->IsKeyDown( KEY_RCONTROL ) )
	{
		if ( code == KEY_Z )
		{
			const bool bShiftDown = vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT );
			CMapDoc::GetActiveMapDoc()->UndoRedo( bShiftDown );
		}
		switch ( code )
		{
		case KEY_PAD_PLUS:
		case KEY_PAD_MINUS:
			{
				const bool bShiftDown = vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT );
				const int i = bShiftDown ? 10 : 5;
				OnMouseWheeled( code == KEY_PAD_MINUS ? -i : i );
				break;
			}
		}
		return;
	}

	switch ( code )
	{
	case KEY_PAD_PLUS:
	case KEY_PAD_MINUS:
		OnMouseWheeled( code == KEY_PAD_MINUS ? -1 : 1 );
		break;
	case KEY_DELETE:
		DeleteSelection();
		break;
	}
}

void CNodeView::OnParentKeyCodeReleased( vgui::KeyCode code )
{
	if ( !AllowKeyInput() ) return;
}

void CNodeView::OnMousePressed( vgui::MouseCode code )
{
	DestroyTooltip();
	ForceFocus();

	BaseClass::OnMousePressed( code );

	if ( code == MOUSE_MIDDLE )
	{
		iArmDrag = DRAG_VIEW;
	}
	else if ( code == MOUSE_LEFT )
	{
		CBaseNode* cur = GetNodeUnderCursor();
		float t = BRIDGE_DRAG_TOLERANCE;
		CJack* curJack = GetJackUnderCursor( &t );

		HandleSelectionInputPressed( cur );

		if ( curJack && ( !cur || cur == curJack->GetParentNode() ) )
		{
			CBaseNode* n = curJack->GetParentNode();
			if ( n )
			{
				bool bReArrange = curJack->IsInput() && curJack->GetNumBridges();
				iArmDrag = DRAG_BRDIGE;

				if ( !bReArrange )
					CreateDragBridge( curJack );
				else
					UpdateDragBridge( curJack, n );
			}
		}
		else if ( cur )
		{
			if ( cur->IsSelected() )
			{
				iArmDrag = DRAG_NODES;
				Vector2D mPos( Mx, My );
				ToNodeSpace( mPos );
				cur->OnLeftClick( mPos );
			}
		}
		else
		{
			m_vecSelectionBoxStart = GetMousePosInNodeSpace();
			iArmDrag = DRAG_SBOX;
		}
	}
}

void CNodeView::OnMouseDoublePressed( vgui::MouseCode code )
{
	ForceFocus();

	BaseClass::OnMouseDoublePressed( code );

	if ( code == MOUSE_LEFT )
	{
		CBaseNode* cur = GetNodeUnderCursor();
		if ( cur )
		{
			CreatePropertyDialog( cur );
		}
	}
}

void CNodeView::OnMouseReleased( vgui::MouseCode code )
{
	BaseClass::OnMouseReleased( code );

	const bool bNodeDrag = iArmDrag == DRAG_NODES;
	const bool bSBoxDrag = iArmDrag == DRAG_SBOX;
	const bool bBridgeDrag = iArmDrag == DRAG_BRDIGE;

	iArmDrag = DRAG_NONE;

	if ( IsInDrag() )
	{
		if ( bSBoxDrag )
		{
			HandleSelectionInputReleasedBox();
		}
		else if ( bBridgeDrag && m_pCurBridge )
		{
			CJack* tJ = GetBridgeSnapJack();

			if ( tJ && tJ->GetParentNode() )
			{
				if ( tJ->IsInput() && tJ->GetNumBridges() )
				{
					Assert( tJ->GetNumBridges() == 1 );
					tJ->DisconnectBridges();
				}
				FinishDragBridge( tJ );
			}
			else
				KillDragBridge();
		}

		StopDrag( bNodeDrag );
		return;
	}
	else if ( m_pCurBridge )
	{
		KillDragBridge();
	}

	if ( code == MOUSE_RIGHT )
	{
		CBaseNode* cur = GetNodeUnderCursor();
		float t = BRIDGE_DRAG_TOLERANCE;
		CJack* curJack = GetJackUnderCursor( &t );

		if ( curJack && ( !cur || cur == curJack->GetParentNode() ) )
		{
			curJack->PurgeBridges();
			return;
		}
		else if ( cur )
		{
			if ( !cur->IsSelected() )
			{
				DeselectAllNodes( cur );
				cur->SetSelected( true );
			}
		}
		else
			DeselectAllNodes();

		CreateContextMenu( GetFirstSelectedNode() );
	}
	if ( code == MOUSE_LEFT )
	{
		CBaseNode* cur = GetNodeUnderCursor();

		HandleSelectionInputReleased( cur );
	}
}

void CNodeView::OnCursorMoved( int x, int y )
{
	BaseClass::OnCursorMoved( x, y );

	DestroyTooltip();

	m_lTime_MouseLastMoved = vgui::system()->GetTimeMillis();

	if ( iArmDrag && ( x || y ) && !IsInDrag() )
	{
		bool bNodeDrag = iArmDrag == DRAG_NODES;
		BeginDrag( bNodeDrag );
	}
}

void CNodeView::OnMouseWheeled( int delta )
{
	if ( !IsMouseOver() )
		return;

	DestroyTooltip();

	BaseClass::OnMouseWheeled( delta );

	if ( IsInDrag() && iArmDrag == DRAG_NODES )
		return;

	Vector2D oldPos = GetMousePosInNodeSpace();

	float flZoomDelta = delta * VIEWZOOM_SPEED * Bias( RemapValClamped( m_flZoom, 0, VIEWZOOM_OUT_MAX, 0, 1 ), 0.6f );
	float flZoomOld = m_flZoom;

	m_flZoom -= flZoomDelta;
	m_flZoomGoal -= flZoomDelta;
	m_flZoom = clamp( m_flZoom, VIEWZOOM_IN_MAX, VIEWZOOM_OUT_MAX );
	m_flZoomGoal = clamp( m_flZoomGoal, VIEWZOOM_IN_MAX, VIEWZOOM_OUT_MAX );

	Vector2D newPos = GetMousePosInNodeSpace();

	m_flZoom = flZoomOld;

	Vector2D deltaPos = newPos - oldPos;

	if ( delta < 0 )
		deltaPos *= -1.0f;

	deltaPos.y *= -1.0f;

	m_vecPositionGoal += deltaPos;
}

void CNodeView::OnCursorEntered()
{
	m_lTime_MouseLastMoved = 0;
	DestroyTooltip();
}

void CNodeView::OnCursorExited()
{
	m_lTime_MouseLastMoved = 0;
	DestroyTooltip();
}

void CNodeView::CreateTooltip( int iNodeIndex )
{
	DestroyTooltip();

	m_pTooltip = new CSmartTooltip( this, "nodetooltip" );
	m_pTooltip->InvalidateLayout( true );

	int mx, my, sx, sy, tsx, tsy;
	vgui::input()->GetCursorPosition( mx, my );
	mx += 16;

	GetSize( sx, sy );
	m_pTooltip->GetSize( tsx, tsy );

	mx -= max( 0, tsx + mx - sx );
	my -= max( 0, tsy + my - sy );

	//ScreenToLocal( mx, my );
	m_pTooltip->SetPos( mx, my );
}

void CNodeView::DestroyTooltip()
{
	if ( !m_pTooltip )
		return;

	m_pTooltip->MarkForDeletion();
	m_pTooltip = nullptr;

	m_lTime_MouseLastMoved = 0;
}

void CNodeView::BeginDrag( bool bNodes )
{
	bInDrag = true;
	vgui::input()->SetMouseCapture( GetVPanel() );

	Assert( !m_hNodesInMove.Count() );
	if ( bNodes )
	{
		m_hNodesInMove.Purge();
		Vector2D mouseInNodeSpace( MxOld, MyOld );
		ToNodeSpace( mouseInNodeSpace );
		CBaseNode* cur = GetNodeUnderPos( mouseInNodeSpace );

		for ( int i = 0; i < m_hNodeList.Count(); i++ )
			if ( m_hNodeList[i]->IsSelected() )
				m_hNodesInMove.AddToTail( m_hNodeList[i] );
		for ( int i = 0; i < m_hNodesInMove.Count(); i++ )
			if ( m_hNodesInMove[i]->MustDragAlone() )
			{
				if ( cur == m_hNodesInMove[i] )
				{
					CBaseNode* SingleNode = m_hNodesInMove[i];
					DeselectAllNodes( SingleNode );
					m_hNodesInMove.Purge();
					m_hNodesInMove.AddToTail( SingleNode );
					break;
				}
				else
				{
					m_hNodesInMove[i]->SetSelected( false );
					m_hNodesInMove.Remove( i );
					i--;
				}
			}
		for ( int i = 0; i < m_hNodesInMove.Count(); i++ )
			m_hNodesInMove[i]->OnDragStart();
	}
}

bool CNodeView::IsInDrag()
{
	return bInDrag;
}

void CNodeView::StopDrag( bool bNodes )
{
	bInDrag = false;
	vgui::input()->SetMouseCapture( NULL );

	if ( bNodes )
	{
		for ( int i = 0; i < m_hNodesInMove.Count(); i++ )
			m_hNodesInMove[i]->OnDragEnd();
		m_hNodesInMove.Purge();
	}
}

void CNodeView::HandleSelectionInputPressed( CBaseNode* pNode )
{
	bool bToggle = vgui::input()->IsKeyDown( KEY_LCONTROL ) || vgui::input()->IsKeyDown( KEY_RCONTROL );
	bool bAdd = vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT );
	if ( bAdd )
		bToggle = false;
	bool bAccum = bAdd || bToggle;

	if ( pNode )
	{
		CBaseContainerNode* pContainer = pNode->GetAsContainer();

		bool bWasSelected = pNode->IsSelected();
		if ( !bWasSelected && !bAccum )
			DeselectAllNodes( pNode );

		if ( pContainer != nullptr )
		{
			pContainer->UpdateOnMove();

			if ( pContainer->ShouldSelectChildrenOnClick() )
				pContainer->SelectAllInBounds();
		}

		if ( bToggle )
			pNode->ToggleSelection();
		else
			pNode->SetSelected( true );

		MoveNodeToFront( pNode );
	}
}
void CNodeView::HandleSelectionInputReleased( CBaseNode* pNode )
{
	bool bToggle = vgui::input()->IsKeyDown( KEY_LCONTROL ) || vgui::input()->IsKeyDown( KEY_RCONTROL );
	bool bAdd = vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT );
	if ( bAdd )
		bToggle = false;
	bool bAccum = bAdd || bToggle;

	if ( pNode )
	{
		CBaseContainerNode* pContainer = pNode->GetAsContainer();

		if ( !bAccum )
			DeselectAllNodes( pNode );

		if ( pContainer != nullptr )
		{
			pContainer->UpdateOnMove();

			if ( pContainer->ShouldSelectChildrenOnClick() )
				pContainer->SelectAllInBounds();
		}
	}
	else if ( !bAccum )
		DeselectAllNodes();
}
void CNodeView::HandleSelectionInputReleasedBox()
{
	bool bToggle = vgui::input()->IsKeyDown( KEY_LCONTROL ) || vgui::input()->IsKeyDown( KEY_RCONTROL );
	bool bAdd = vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT );
	if ( bAdd )
		bToggle = false;
	bool bAccum = bAdd || bToggle;

	Vector2D sstart = m_vecSelectionBoxStart;
	Vector2D ssend = GetMousePosInNodeSpace();
	Vector smin( min( sstart.x, ssend.x ),
				 min( sstart.y, ssend.y ), -10 );
	Vector smax( max( sstart.x, ssend.x ),
				 max( sstart.y, ssend.y ), 10 );

	CUtlVector<CBaseNode*> m_hSelection;
	for ( int i = 0; i < m_hNodeList.Count(); i++ )
	{
		CBaseNode* n = m_hNodeList[i];

		Vector2D nmin = n->GetSelectionBoundsMinNodeSpace(); //GetBoundsMinNodeSpace();
		Vector2D nmax = n->GetSelectionBoundsMaxNodeSpace(); //GetBoundsMaxNodeSpace();
		Vector n1( nmin.x, nmin.y, -10 );
		Vector n2( nmax.x, nmax.y, 10 );

		if ( IsBoxIntersectingBox( smin, smax, n1, n2 ) )
		{
			m_hSelection.AddToTail( n );
		}
	}

	if ( !bAccum )
		DeselectAllNodes();
	for ( int i = 0; i < m_hSelection.Count(); i++ )
	{
		CBaseNode* n = m_hSelection[i];
		if ( bToggle )
			n->ToggleSelection();
		else
			n->SetSelected( true );
	}

	m_hSelection.Purge();
}

CBaseNode* CNodeView::GetNodeUnderCursor()
{
	UpdateMousePos();
	Vector2D mouseInNodeSpace( Mx, My );
	ToNodeSpace( mouseInNodeSpace );
	return GetNodeUnderPos( mouseInNodeSpace );
}

CBaseNode* CNodeView::GetNodeUnderPos( const Vector2D& pos ) const
{
	CBaseNode* p = nullptr;

	for ( int i = m_hNodeList.Count() - 1; i >= 0; i-- )
	{
		CBaseNode* n = m_hNodeList[i];
		if ( n->IsWithinBounds_Base( pos ) )
		{
			p = n;
			break;
		}
	}

	return p;
}

CJack* CNodeView::GetJackUnderCursor( float* tolerance, bool bInputs, bool bOutputs )
{
	UpdateMousePos();
	Vector2D mouseInNodeSpace( Mx, My );
	ToNodeSpace( mouseInNodeSpace );
	return GetJackUnderPos( mouseInNodeSpace, tolerance, bInputs, bOutputs );
}

CJack* CNodeView::GetJackUnderPos( const Vector2D& pos, float* tolerance, bool bInputs, bool bOutputs ) const
{
	if ( !IsMouseOver() )
		return nullptr;

	CJack* j = nullptr;
	for ( int i = m_hNodeList.Count() - 1; i >= 0; i-- )
	{
		CBaseNode* n = m_hNodeList[i];
		float bestdist = 99999;
		CJack* bestJack = nullptr;

		if ( bOutputs )
		{
			for ( int x = 0; x < n->GetNumJacks_Out(); x++ )
			{
				CJack* tmp = n->GetJack_Out( x );
				if ( tmp->IsWithinBounds_Base( pos ) )
				{
					j = tmp;
					break;
				}
				if ( tolerance )
				{
					Vector2D c = tmp->GetCenter();
					c -= pos;
					float dist = c.Length();
					if ( dist < *tolerance && dist < bestdist )
					{
						bestdist = dist;
						bestJack = tmp;
					}
				}
			}
		}
		if ( bInputs )
		{
			for ( int x = 0; x < n->GetNumJacks_In(); x++ )
			{
				CJack* tmp = n->GetJack_In( x );
				if ( tmp->IsWithinBounds_Base( pos ) )
				{
					j = tmp;
					break;
				}
				if ( tolerance )
				{
					Vector2D c = tmp->GetCenter();
					c -= pos;
					float dist = c.Length();
					if ( dist < *tolerance && dist < bestdist )
					{
						bestdist = dist;
						bestJack = tmp;
					}
				}
			}
		}

		if ( bestJack && !j )
			j = bestJack;

		if ( j )
			break;
	}
	return j;
}

void CNodeView::DeselectAllNodes( CBaseNode* pIgnore )
{
	for ( int i = 0; i < m_hNodeList.Count(); i++ )
	{
		CBaseNode* n = m_hNodeList[i];
		if ( n == pIgnore )
			continue;
		n->SetSelected( false );
	}
}

void CNodeView::SelectNodes( CUtlVector<CBaseNode*>& m_hNodes, bool bAdd )
{
	if ( !bAdd )
		DeselectAllNodes();
	for ( int i = 0; i < m_hNodeList.Count(); i++ )
	{
		CBaseNode* n = m_hNodeList[i];
		if ( m_hNodes.Find( n ) != m_hNodes.InvalidIndex() )
			n->SetSelected( true );
	}
}

bool CNodeView::IsSelectionBiggerThanOne()
{
	int num = 0;
	for ( int i = 0; i < m_hNodeList.Count(); i++ )
	{
		CBaseNode* n = m_hNodeList[i];
		if ( n->IsSelected() )
			num++;
	}
	return num > 1;
}

CBaseNode* CNodeView::GetFirstSelectedNode() const
{
	for ( int i = m_hNodeList.Count() - 1; i >= 0; i-- )
	{
		CBaseNode* n = m_hNodeList[i];
		if ( n->IsSelected() )
			return n;
	}
	return nullptr;
}

void CNodeView::DragSelection( const Vector2D& delta )
{
	//CUtlVector< CBaseNode* >hSelection;
	//for ( int i = 0; i < m_hNodeList.Count(); i++ )
	//	if ( m_hNodeList[i] )
	//		hSelection.AddToTail( m_hNodeList[i] );

	//for ( int i = 0; i < hSelection.Count(); i++ )
	//	if ( hSelection[i]->MustDragAlone() )
	//	{
	//		CBaseNode *SingleNode = hSelection[i];
	//		DeselectAllNodes( SingleNode );
	//		hSelection.Purge();
	//		hSelection.AddToTail( SingleNode );
	//		break;
	//	}

	//for ( int i = 0; i < hSelection.Count(); i++ )
	for ( int i = 0; i < m_hNodesInMove.Count(); i++ )
	{
		//CBaseNode *n = m_hNodeList[i];
		CBaseNode* n = m_hNodesInMove[i];
		//if ( !n->IsSelected() )
		//	continue;

		n->OnDrag( delta );

		//Vector2D pos = n->GetPosition();
		//pos += delta;
		//n->SetPosition( pos );
	}
	//hSelection.Purge();
}

void CNodeView::MoveNodeToFront( CBaseNode* p )
{
	if ( p == m_hNodeList.Tail() )
		return;

	int idx = m_hNodeList.Find( p );
	m_hNodeList.Remove( idx );
	m_hNodeList.AddToTail( p );
}

void CNodeView::DeleteSelection()
{
	if ( !AllowKeyInput() ) return;

	CUtlVector<CBaseNode*> m_hDeleteThese;
	for ( int i = 0; i < m_hNodeList.Count(); i++ )
	{
		CBaseNode* n = m_hNodeList[i];
		if ( !n->IsSelected() )
			continue;
		if ( n->IsNodeCrucial() )
			continue;

		m_hNodesInMove.FindAndRemove( m_hNodeList[i] );
		m_hNodeList.Remove( i );
		n->MarkForDeletion();
		m_hDeleteThese.AddToTail( n );
		i--;
	}
	SafeDeleteVector( m_hDeleteThese );
}

void CNodeView::DisconnectSelection()
{
	for ( int i = 0; i < m_hNodeList.Count(); i++ )
	{
		CBaseNode* n = m_hNodeList[i];
		if ( !n->IsSelected() )
			continue;

		n->PurgeBridges();
	}
}

bool CNodeView::AllowKeyInput()
{
	if ( vgui::input()->GetAppModalSurface() )
		return false;
	if ( m_Properties.Get() )
		return false;

	return true;
}

void CNodeView::StartOverrideIndices( CUtlVector<CBaseNode*>& m_hNodes )
{
	bOverrideNodeIndices = true;
	m_hTmpNodeIndex.Purge();
	m_hTmpNodeIndex.AddVectorToTail( m_hNodes );
}

void CNodeView::FinishOverrideIndices()
{
	bOverrideNodeIndices = false;
	m_hTmpNodeIndex.Purge();
}

void CNodeView::MoveNodes( const Vector2D& offset, CBaseNode* n )
{
	Vector2D pos = n->GetPosition();
	pos += offset;
	n->SetPosition( pos );
}

void CNodeView::MoveNodes( const Vector2D& offset, CUtlVector<CBaseNode*>& m_hList )
{
	for ( int i = 0; i < m_hList.Count(); i++ )
		MoveNodes( offset, m_hList[i] );
}

int CNodeView::GetNodeIndex( CBaseNode* n ) const
{
	if ( bOverrideNodeIndices )
		return m_hTmpNodeIndex.Find( n );
	return m_hNodeList.Find( n );
}

CBaseNode* CNodeView::GetNodeFromIndex( int idx ) const
{
	if ( bOverrideNodeIndices )
	{
		if ( !m_hTmpNodeIndex.IsValidIndex( idx ) )
			return nullptr;
		return m_hTmpNodeIndex[idx];
	}

	if ( !m_hNodeList.IsValidIndex( idx ) )
		return nullptr;
	return m_hNodeList[idx];
}

CBaseNode* CNodeView::GetNodeFromType( int type ) const
{
	for ( int i = 0; i < m_hNodeList.Count(); i++ )
	{
		if ( m_hNodeList[i]->GetNodeType() == type )
			return m_hNodeList[i];
	}
	return nullptr;
}

void CNodeView::CreateDragBridge( CJack* j )
{
	KillDragBridge();
	CBridge* b = j->BridgeBeginBuild();

	m_pCurBridge = b;
	m_pCurBridge->SetTemporaryTarget( GetMousePosInNodeSpace() );
}

void CNodeView::UpdateDragBridge( CJack* j, CBaseNode* n )
{
	KillDragBridge();
	m_pCurBridge = j->GetBridge( 0 );
	m_hBridgeList.FindAndRemove( m_pCurBridge );

	if ( !j->IsInput() )
		m_pCurBridge->DisconnectSource();
	else
		m_pCurBridge->DisconnectDestination();

	BeginDrag( false );

	m_pCurBridge->SetTemporaryTarget( GetMousePosInNodeSpace() );
}

void CNodeView::FinishDragBridge( CJack* j )
{
	if ( !m_pCurBridge )
		return;

	j->BridgeEndBuild( m_pCurBridge );

	m_pCurBridge = nullptr;
}

void CNodeView::AddBridgeToList( CBridge* b )
{
	m_hBridgeList.AddToTail( b );
}

void CNodeView::KillDragBridge()
{
	if ( m_pCurBridge )
	{
		delete m_pCurBridge;
		m_pCurBridge = nullptr;
	}
}

CJack* CNodeView::GetBridgeSnapJack()
{
	if ( !m_pCurBridge )
		return nullptr;

	bool bHasInputDefined = !!m_pCurBridge->GetInputJack();
	float t = BRIDGE_DRAG_TOLERANCE;
	CJack* curJack = GetJackUnderCursor( &t, bHasInputDefined, !bHasInputDefined );
	CBaseNode* n = curJack ? curJack->GetParentNode() : nullptr;

	const bool bHasVolatileBridges = curJack != nullptr && curJack->IsInput() && curJack->GetNumBridges();
	bool bSameNode = n && ( m_pCurBridge->GetInputNode() == n || m_pCurBridge->GetDestinationNode() == n ) &&
					 !bHasVolatileBridges;

	if ( curJack && n &&
		 ( curJack->IsInput() == bHasInputDefined ) &&
		 !bSameNode )
	{
		//if ( curJack->IsInput() && curJack->GetNumBridges() )
		//	return nullptr;

		CJack* potentialInput = m_pCurBridge->GetInputJack() ? m_pCurBridge->GetInputJack() : curJack;
		CJack* potentialDest = m_pCurBridge->GetDestinationJack() ? m_pCurBridge->GetDestinationJack() : curJack;
		for ( int i = 0; i < m_hBridgeList.Count(); i++ )
		{
			CBridge* b = m_hBridgeList[i];

			if ( potentialInput == b->GetInputJack() &&
				 potentialDest == b->GetDestinationJack() )
				return nullptr;
		}

		CBaseNode* origNode = bHasInputDefined ? m_pCurBridge->GetInputNode() : m_pCurBridge->GetDestinationNode();
		if ( n->RecursiveFindNode( origNode, !curJack->IsInput() ) )
			return nullptr;

		if ( n == origNode )
			return nullptr;

		if ( bHasVolatileBridges )
			curJack->GetBridge( 0 )->SetTemporaryColor( CBridge::TMPCOL_ORANGE );

		return curJack;
	}
	return nullptr;
}

bool CNodeView::ShouldDraw_Datatypes() const
{
	return true;
}

bool CNodeView::ShouldDraw_Shadows() const
{
	return m_flZoom < 4.f;
}

bool CNodeView::ShouldDraw_Nodes() const
{
	return true;
}

bool CNodeView::ShouldDraw_Jacks() const
{
	return true;
}

bool CNodeView::ShouldDraw_Bridges() const
{
	return true;
}

void CNodeView::Paint()
{
	BaseClass::Paint();

	int wide, tall;
	GetSize( wide, tall );

	Paint_Grid();
	if ( ShouldDraw_Shadows() )
	{
		if ( ShouldDraw_Nodes() )
			Paint_Nodes( true );
		if ( ShouldDraw_Bridges() )
			Paint_Bridges( true );
	}
	if ( ShouldDraw_Nodes() )
		Paint_Nodes( false );
	if ( ShouldDraw_Bridges() )
		Paint_Bridges( false );

	Paint_SelectionBox();

	constexpr int inset = 8;

	vgui::surface()->DrawSetTexture( m_iTex_Darken );
	vgui::surface()->DrawSetColor( Color( 0, 0, 0, 96 ) );
	vgui::surface()->DrawTexturedSubRect( 0, 0, inset, tall, 0, 0, 1, 1 );
	vgui::surface()->DrawTexturedSubRect( wide - inset, 0, wide, tall, 1, 0, 0, 1 );

	vgui::Vertex_t points[4];
	points[0].Init( Vector2D( 0, 0 ), Vector2D( 0, 1 ) );
	points[1].Init( Vector2D( wide, 0 ), Vector2D( 0, 0 ) );
	points[2].Init( Vector2D( wide, inset ), Vector2D( 1, 0 ) );
	points[3].Init( Vector2D( 0, inset ), Vector2D( 1, 1 ) );
	vgui::surface()->DrawTexturedPolygon( 4, points );

	points[0].Init( Vector2D( 0, tall - inset ), Vector2D( 1, 0 ) );
	points[1].Init( Vector2D( wide, tall - inset ), Vector2D( 1, 1 ) );
	points[2].Init( Vector2D( wide, tall ), Vector2D( 0, 1 ) );
	points[3].Init( Vector2D( 0, tall ), Vector2D( 0, 0 ) );
	vgui::surface()->DrawTexturedPolygon( 4, points );

	Paint_StatusBar();
}

void CNodeView::Paint_StatusBar()
{
	{
		int sx, sy;
		GetSize( sx, sy );

		int fontWide, fontTall;
		float z = 1.5f;
		bool bDraw;
		vgui::HFont font = GetFontScaled( 9, bDraw, &z );
		vgui::surface()->DrawSetTextColor( 200, 200, 200, 255 );
		vgui::surface()->DrawSetTextFont( font );
		wchar_t szconverted[256];

		auto len = V_swprintf_safe( szconverted, L"Position: %.0f %.0f", m_vecPosition.x, m_vecPosition.y );
		vgui::surface()->GetTextSize( font, szconverted, fontWide, fontTall );
		vgui::surface()->DrawSetTextPos( sx - fontWide - 4, 8 );
		vgui::surface()->DrawPrintText( szconverted, len );

		len = V_swprintf_safe( szconverted, L"Zoom: %.3f", m_flZoom );
		vgui::surface()->DrawSetTextPos( sx - fontWide - 4, 10 + fontTall );
		vgui::surface()->DrawPrintText( szconverted, len );
	}

	if ( !m_flErrorTime )
		return;

	Vector4D col( _col_OutOfDate[0] / 255.0f,
				  _col_OutOfDate[1] / 255.0f,
				  _col_OutOfDate[2] / 255.0f,
				  _col_OutOfDate[3] / 255.0f );

	/*bool bOutofDate = GetStackIndex() != GetCompiledStackIndex();
	if ( !bOutofDate )
		col.w = 0;*/

	if ( m_flErrorTime )
	{
		col.x = _col_Error[0] / 255.0f;
		col.y = _col_Error[1] / 255.0f;
		col.z = _col_Error[2] / 255.0f;
		col.w = abs( sin( m_flErrorTime * M_PI * 4 ) ) * m_flErrorTime;
		m_flErrorTime -= vguiFrameTime;
		if ( m_flErrorTime < 0 )
			m_flErrorTime = 0;
	}

	if ( col.w < 0.025f )
		return;

	constexpr int ERRORBORDER_EXTRUDE = 5;

	int sx, sy;
	GetSize( sx, sy );

	vgui::surface()->DrawSetColor( Color( col.x * 255, col.y * 255, col.z * 255, col.w * 255 ) );

	vgui::surface()->DrawFilledRect( 0, 0, ERRORBORDER_EXTRUDE, sy );
	vgui::surface()->DrawFilledRect( sx - ERRORBORDER_EXTRUDE, 0, sx, sy );
	vgui::surface()->DrawFilledRect( ERRORBORDER_EXTRUDE, 0, sx - ERRORBORDER_EXTRUDE, ERRORBORDER_EXTRUDE );
	vgui::surface()->DrawFilledRect( ERRORBORDER_EXTRUDE, sy - ERRORBORDER_EXTRUDE, sx - ERRORBORDER_EXTRUDE, sy );
}

void CNodeView::Paint_Grid()
{
	float alpha = Bias( RemapValClamped( m_flZoom, 0.5f, VIEWZOOM_OUT_MAX - 2, 1.0f, 0 ), 0.4f );
	Color bg = GetBgColor();
	Color col = Lerp( alpha, bg, _col_Grid );
	vgui::surface()->DrawSetColor( col );

	int sx, sy;
	GetSize( sx, sy );

	Vector2D pos_min( 0, 0 );
	Vector2D pos_max( sx, sy );
	Vector2D tmp1, tmp2;

	ToNodeSpace( pos_min );
	ToNodeSpace( pos_max );

	Vector2D delta = pos_max - pos_min;
	delta.y *= -1.0f;

	int num_vertical = max( 1.f, ceil( abs( delta.x ) / GRID_CELL_SIZE ) ) + 1;
	int num_horizontal = max( 1.f, ceil( abs( delta.y ) / GRID_CELL_SIZE ) ) + 1;

	float startx = ceil( pos_min.x / GRID_CELL_SIZE ) * GRID_CELL_SIZE;
	float starty = floor( pos_min.y / GRID_CELL_SIZE ) * GRID_CELL_SIZE;

	tmp1.Init( startx, starty );
	tmp2.Init( startx + GRID_CELL_SIZE, starty - GRID_CELL_SIZE );

	ToPanelSpace( tmp1 );
	ToPanelSpace( tmp2 );

	tmp2 = tmp2 - tmp1;

	Vector2D orig( 0, 0 );
	Vector2D thickness( 2.5f, 0 );
	ToPanelSpace( orig );
	ToPanelSpace( thickness );
	thickness.x = max( 1.f, thickness.x - orig.x ) * 0.5f;

	tmp1 -= tmp2;

	for ( int i = 0; i < num_vertical; i++ )
	{
		vgui::surface()->DrawFilledRect( tmp1.x + tmp2.x * i - thickness.x, 0,
											tmp1.x + tmp2.x * i + thickness.x, sy );
	}
	for ( int i = 0; i < num_horizontal; i++ )
	{
		vgui::surface()->DrawFilledRect( 0, tmp1.y + tmp2.y * i - thickness.x,
											sx, tmp1.y + tmp2.y * i + thickness.x );
	}
}

void CNodeView::Paint_Nodes( bool bShadow )
{
	for ( int i = 0; i < m_hNodeList.Count(); i++ )
	{
		CBaseNode* p = m_hNodeList[i];
		p->VguiDraw( bShadow );
	}
}

void CNodeView::Paint_Bridges( bool bShadow )
{
	for ( int i = 0; i < m_hBridgeList.Count(); i++ )
	{
		CBridge* p = m_hBridgeList[i];
		p->VguiDraw( bShadow );
	}

	if ( m_pCurBridge )
	{
		m_pCurBridge->VguiDraw( bShadow );
	}
}

void CNodeView::Paint_SelectionBox()
{
	if ( !IsInDrag() )
		return;
	if ( iArmDrag != DRAG_SBOX )
		return;

	Vector2D curpos = GetMousePosInNodeSpace();
	Vector2D oldpos = m_vecSelectionBoxStart;

	ToPanelSpace( curpos );
	ToPanelSpace( oldpos );
	vgui::surface()->DrawSetColor( 128, 128, 164, 128 );

	Vector2D _min = curpos.Min( oldpos );
	Vector2D _max = curpos.Max( oldpos );

	vgui::surface()->DrawFilledRect( _min.x, _min.y, _max.x, _max.y );

	vgui::surface()->DrawSetColor( Color( 48, 48, 96, 196 ) );
	vgui::surface()->DrawLine( _min.x - 1, _min.y - 1, _max.x, _min.y - 1 );
	vgui::surface()->DrawLine( _min.x - 1, _max.y, _max.x, _max.y );
	vgui::surface()->DrawLine( _min.x - 1, _min.y - 1, _min.x - 1, _max.y );
	vgui::surface()->DrawLine( _max.x, _min.y - 1, _max.x, _max.y );
}

void CNodeView::InitColors()
{
	_col_Grid.SetColor( 148, 148, 148, 255 );
	_col_Vignette.SetColor( 0, 0, 0, 96 );

	_col_OutOfDate.SetColor( 50, 50, 62, 0 );
	_col_Error.SetColor( 180, 50, 62, 0 );
}

void CNodeView::ApplySchemeSettings( vgui::IScheme* pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetPaintBackgroundEnabled( true );
	SetPaintBorderEnabled( true );

	SetPaintBackgroundType( 0 );
	//SetBorder(pScheme->GetBorder("BaseBorder"));

	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );

	SetBgColor( pScheme->GetColor( "NodeView.Bg", GetBgColor() ) );

	InitColors();
	_col_Grid = pScheme->GetColor( "NodeView.Grid", _col_Grid );
	_col_Vignette = pScheme->GetColor( "NodeView.Vignette", _col_Vignette );

	_col_OutOfDate = pScheme->GetColor( "EditorRoot.OutOfDate", _col_OutOfDate );
	_col_Error = pScheme->GetColor( "EditorRoot.Error", _col_Error );
}
void CNodeView::PerformLayout()
{
	BaseClass::PerformLayout();

	int w, t;
	GetParent()->GetSize( w, t );
	SetPos( 0, 0 );
	SetSize( w, t );
}

vgui::HFont CNodeView::GetFontScaled( float Scale, bool& bVis, float* zoom ) const
{
	constexpr int maxFonts = MAX_ZOOM_FONTS - 1;
	Scale *= zoom ? *zoom : GetZoomScalar();
	bVis = Scale > 2.0f;
	return _fonts[clamp( ( (int)Scale ) - 1, 0, maxFonts )];
}

void CNodeView::OnCommand( const char* command )
{
	BaseClass::OnCommand( command );

	if ( !Q_stricmp( command, "node_delete" ) )
		DeleteSelection();
	if ( !Q_stricmp( command, "node_disconnect" ) )
		DisconnectSelection();
}

CBaseNode* CNodeView::SpawnNode( int type )
{
	CBaseNode* pNode = nullptr;

	switch ( type )
	{
	case IONODE_ENTITY:
		pNode = new CEntityNode( this );
		break;
	case IONODE_ADDOUTPUT:
		pNode = new CAddoutputNode( this );
		break;
	case IONODE_GENERAL_OUT:
		pNode = new COutNode( this );
		break;
	default:
		break;
	}

	if ( !pNode )
		return nullptr;

	pNode->Spawn();

	m_hNodeList.AddToTail( pNode );
	return pNode;
}

void CNodeView::OnSpawnNode( int type )
{
	CBaseNode* pNode = SpawnNode( type );
	if ( !pNode )
		return;

	pNode->SetPosition( m_vecMenuCreationPosition, true );
}

void CNodeView::OnMenuClosed( KeyValues* pData )
{
	Panel* pMenu = ( (Panel*)pData->GetPtr( "panel" ) );
	if ( pMenu == m_ContextMenu.Get() )
	{
		m_ContextMenu.Get()->MarkForDeletion();
		m_ContextMenu = nullptr;
	}
}

void CNodeView::CreateContextMenu( CBaseNode* pMouseOverNode )
{
	if ( m_ContextMenu.Get() )
	{
		delete m_ContextMenu.Get();
		m_ContextMenu = nullptr;
	}

	UpdateMousePos();
	m_vecMenuCreationPosition.Init( Mx, My );
	ToNodeSpace( m_vecMenuCreationPosition );

	bool bMouseOverNode = pMouseOverNode != nullptr;
	bool bOtherNodesSelected = IsSelectionBiggerThanOne();
	bool bAnyNodesSelected = bMouseOverNode || bOtherNodesSelected;

	bool bCrucialNode = bMouseOverNode && pMouseOverNode->IsNodeCrucial();
	bool bBlockOnSingleCrucialNode = bCrucialNode && !bOtherNodesSelected;

	vgui::Panel* pMenuParent = this;

	vgui::Menu* m = new vgui::Menu( pMenuParent, "contextmenu" );
	vgui::Menu* padd = new vgui::Menu( m, "addmenu" );

	AddNodesToContextMenu( padd );

	m->AddCascadingMenuItem( "New node", this, padd );

	int tmp;
	tmp = m->AddMenuItem( "Delete", "node_delete", this );
	if ( !bAnyNodesSelected || bBlockOnSingleCrucialNode )
		m->GetMenuItem( tmp )->SetEnabled( false );

	tmp = m->AddMenuItem( "Disconnect", "node_disconnect", this );
	if ( !bAnyNodesSelected )
		m->GetMenuItem( tmp )->SetEnabled( false );

	KeyValues* pKV_P = new KeyValues( "OpenProperties" );
	pKV_P->SetPtr( "pNode", pMouseOverNode );
	tmp = m->AddMenuItem( "Properties", pKV_P, this );
	if ( ( bOtherNodesSelected || !bAnyNodesSelected ) || !bMouseOverNode ) m->GetMenuItem( tmp )->SetEnabled( false );

	m_ContextMenu = m;
	//input()->SetMouseFocus( m_ContextMenu->GetVPanel() );

	vgui::Menu::PlaceContextMenu( pMenuParent, m_ContextMenu );
}

void CNodeView::AddNodesToContextMenu( vgui::Menu* pNodeMenu )
{
	vgui::Menu* padd_misc = new vgui::Menu( pNodeMenu, "addmenu_misc" );

	padd_misc->AddMenuItem( "Group", new KeyValues( "spawnNode", "type", IONODE_ENTITY ), this );
	pNodeMenu->AddCascadingMenuItem( "Misc", this, padd_misc );
}

void CNodeView::OnOpenProperties( KeyValues* data )
{
	CBaseNode* n = static_cast<CBaseNode*>( data->GetPtr( "pNode" ) );
	if ( !n )
		return;
	CreatePropertyDialog( n );
}

void CNodeView::CreatePropertyDialog( CBaseNode* pNode )
{
	/*m_Properties = new CNodePropertySheet( pNode, this, "" );
	m_Properties->MakeReadyForUse();
	PropertySheet* pSheet = m_Properties->GetPropertySheet();
	pSheet->MakeReadyForUse();
	pSheet->InvalidateLayout( true, false );
	KeyValues* pKV = m_Properties->GetPropertyContainer();

	int type = pNode->GetNodeType();
	CUtlVector<CSheet_Base*> hSheets;

	switch ( type )
	{
		// SEMANTICS
	case HLSLNODE_VS_IN:
		hSheets.AddToTail( new CSheet_VSInput( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_VS_OUT:
	case HLSLNODE_PS_IN:
		hSheets.AddToTail( new CSheet_PSInVSOut( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_PS_OUT:
		hSheets.AddToTail( new CSheet_PSOutput( pNode, this, pKV, pSheet ) );
		break;

		// MATH
	case HLSLNODE_MATH_MULTIPLY:
		hSheets.AddToTail( new CSheet_Multiply( pNode, this, pKV, pSheet ) );
		break;

		// VECTORS
	case HLSLNODE_MATH_SWIZZLE:
		hSheets.AddToTail( new CSheet_Swizzle( pNode, this, pKV, pSheet ) );
		break;

		// CONSTANTS
	case HLSLNODE_CONSTANT_LOCAL:
		hSheets.AddToTail( new CSheet_Constant( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_CONSTANT_CALLBACK:
		hSheets.AddToTail( new CSheet_Callback( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_CONSTANT_VP_MUTABLE:
		hSheets.AddToTail( new CSheet_VParam_Mutable( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_CONSTANT_VP_STATIC:
		hSheets.AddToTail( new CSheet_VParam_Static( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_CONSTANT_LIGHTSCALE:
		hSheets.AddToTail( new CSheet_Lightscale( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_CONSTANT_RANDOM:
		hSheets.AddToTail( new CSheet_Random( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_CONSTANT_ARRAY:
		hSheets.AddToTail( new CSheet_Array( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_CONSTANT_FB_PIXELSIZE:
		hSheets.AddToTail( new CSheet_EnvCTexelsize( pNode, this, pKV, pSheet ) );
		break;

		// TEXTURES
	case HLSLNODE_TEXTURE_SAMPLER:
		hSheets.AddToTail( new CSheet_TextureSample( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_TEXTURE_TRANSFORM:
		hSheets.AddToTail( new CSheet_TexTransform( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_TEXTURE_SAMPLEROBJECT:
		{
			CSheet_TextureSample* pAddSheet = new CSheet_TextureSample( pNode, this, pKV, pSheet );
			pAddSheet->MakeSamplerOnly();
			hSheets.AddToTail( pAddSheet );
		}
		break;
	case HLSLNODE_TEXTURE_PARALLAX:
		hSheets.AddToTail( new CSheet_Parallax( pNode, this, pKV, pSheet ) );
		break;

		// FLOW CONTROL
	case HLSLNODE_CONTROLFLOW_LOOP:
		hSheets.AddToTail( new CSheet_Loop( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_CONTROLFLOW_CONDITION:
		hSheets.AddToTail( new CSheet_Condition( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_CONTROLFLOW_COMBO:
		hSheets.AddToTail( new CSheet_Combo( pNode, this, pKV, pSheet ) );
		break;

		// UTILITY
	case HLSLNODE_STUDIO_VERTEXLIGHTING:
		hSheets.AddToTail( new CSheet_Std_VLight( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_STUDIO_PIXELSHADER_LIGHTING:
		hSheets.AddToTail( new CSheet_Std_PLight( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_STUDIO_PIXELSHADER_SPECLIGHTING:
		hSheets.AddToTail( new CSheet_Std_PSpecLight( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_STUDIO_SKINNING:
		hSheets.AddToTail( new CSheet_Std_Skinning( pNode, this, pKV, pSheet ) );
		hSheets.Tail()->SetDynamicTitle( "Skinning" );
		break;
	case HLSLNODE_STUDIO_MORPH:
		hSheets.AddToTail( new CSheet_Std_Skinning( pNode, this, pKV, pSheet ) );
		hSheets.Tail()->SetDynamicTitle( "Morphing" );
		break;
	case HLSLNODE_STUDIO_VCOMPRESSION:
		hSheets.AddToTail( new CSheet_VCompression( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_UTILITY_FINAL:
		hSheets.AddToTail( new CSheet_Final( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_UTILITY_FLASHLIGHT:
		hSheets.AddToTail( new CSheet_Flashlight( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_UTILITY_CUSTOMCODE:
		{
			CSheet_Custom_IO* pIO = new CSheet_Custom_IO( pNode, this, pKV, pSheet );
			CSheet_Custom* pCode = new CSheet_Custom( pNode, this, pKV, pSheet );
			pCode->SetIOPage( pIO );
			hSheets.AddToTail( pCode );
			hSheets.AddToTail( pIO );
		}
		break;

		// MATRICES
	case HLSLNODE_MATRIX_COMPOSE:
		hSheets.AddToTail( new CSheet_MComp( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_MATRIX_CUSTOM:
		hSheets.AddToTail( new CSheet_CMatrix( pNode, this, pKV, pSheet ) );
		break;

		// POST PROCESS
	case HLSLNODE_POSTPROCESSING_CREATE_RT:
		hSheets.AddToTail( new CSheet_PP_RT( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_POSTPROCESSING_CREATE_MAT:
		hSheets.AddToTail( new CSheet_PP_Mat( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_POSTPROCESSING_CLEAR_BUFFERS:
		hSheets.AddToTail( new CSheet_PP_ClearBuff( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_POSTPROCESSING_DRAW_MATERIAL:
		hSheets.AddToTail( new CSheet_PP_DrawMat( pNode, this, pKV, pSheet ) );
		break;
	case HLSLNODE_POSTPROCESSING_RENDER_VIEW:
		hSheets.AddToTail( new CSheet_PP_RenderView( pNode, this, pKV, pSheet ) );
		break;

		// MISC
	case HLSLNODE_OTHER_COMMENT:
		hSheets.AddToTail( new CSheet_Comment( pNode, this, pKV, pSheet ) );
		break;
	}

	if ( !pNode->IsNodeCrucial() )
	{
		hSheets.AddToTail( new CSheet_General( pNode, this, pKV, pSheet ) );
	}

	for ( int i = 0; i < hSheets.Count(); i++ )
	{
		m_Properties->AddPage( hSheets[i], hSheets[i]->GetSheetTitle() );
	}

	hSheets.Purge();
	m_Properties->DoModal();
	m_Properties->SetDeleteSelfOnClose( true );*/
}