
#include "vbasecontainer.h"
#include "editorcommon.h"
#include "vnodeview.h"
#include "vjack.h"

#include "vgui/ISurface.h"

#include "tier1/ilocalize.h"

#include "tier0/memdbgon.h"

CBaseContainerNode::CBaseContainerNode( const char* opName, CNodeView* p )
	: BaseClass( opName, p )
{
	p->OnContainerAdded( this );
	m_vecContainerExtents.Init( 128, 384, -128 );

	m_iActiveScalingMode = CBORDER_INVALID;
	m_vecMouseStartPos.Init();
	Q_memset( m_vecAccumulatedExtent, 0, sizeof( m_vecAccumulatedExtent ) );

	m_flMinSizeX = 50;
	m_flMinSizeY = 30;

	SetupVguiTex( m_iGrabIcon, "editor/grabicon" );
}

CBaseContainerNode::~CBaseContainerNode()
{
	RemoveAllChildren();
	pNodeView->OnContainerRemoved( this );
}

void CBaseContainerNode::Spawn()
{
	BaseClass::Spawn();
	UpdateOnMove();
}

void CBaseContainerNode::UpdateContainerBounds()
{
	m_vecBounds.x = m_vecPosition.x - 30;
	m_vecBounds.w = GetContainerMins().y - 30;
	Vector2D maxs = GetContainerMaxs();
	m_vecBounds.z = maxs.x + 30;
	m_vecBounds.y = maxs.y + 30;
}

void CBaseContainerNode::UpdateSize()
{
	int maxJacks = max( m_hInputs.Count(), m_hOutputs.Count() );
	float sizeMin = ( JACK_SIZE_Y + JACK_DELTA_Y ) * maxJacks + JACK_SIZE_Y;
	sizeMin = max( m_flMinSizeY, sizeMin );
	m_vecBorderInfo.Init();
	m_vecSize.Init( max( m_flMinSizeX, 0.f ), -sizeMin );
	TouchJacks();
	//UpdateSimpleObjectBounds( m_vecPosition, m_vecSize, m_vecBounds );
	UpdateContainerBounds();
}

void CBaseContainerNode::SetPosition( Vector2D vec, bool bCenter )
{
	BaseClass::SetPosition( vec, bCenter );
	UpdateContainerBounds();
	UpdateOnMove();
}

void CBaseContainerNode::OnLeftClick( const Vector2D& pos )
{
	m_iActiveScalingMode = IsInBorderBounds( pos );
	m_vecMouseStartPos = pos;
	Q_memset( m_vecAccumulatedExtent, 0, sizeof( m_vecAccumulatedExtent ) );

	//if ( input()->IsKeyDown( KEY_RALT ) || input()->IsKeyDown( KEY_LALT ) )
	//{
	//	for ( int i = 0; i < m_hChildren.Count(); i++ )
	//		m_hChildren[i]->SetSelected( true );
	//}
}

void CBaseContainerNode::OnDrag( const Vector2D& delta )
{
	if ( m_iActiveScalingMode == CBORDER_INVALID )
		return BaseClass::OnDrag( delta );

	//Vector2D pos = GetPosition();

	Vector4D delta_NESW;
	Vector2D pos_delta;
	delta_NESW.Init();
	pos_delta.Init();

	Vector2D half = delta * 0.5f;

	switch ( m_iActiveScalingMode )
	{
	default:
		Assert( 0 );
	case CBORDER_TOP_LEFT:
		delta_NESW.x += half.y;
		delta_NESW.z -= half.y;
		delta_NESW.y -= delta.x;
		pos_delta.x += delta.x;
		pos_delta.y += half.y;
		break;
	case CBORDER_TOP:
		delta_NESW.x += half.y;
		delta_NESW.z -= half.y;
		pos_delta.y += half.y;
		break;
	case CBORDER_TOP_RIGHT:
		delta_NESW.x += half.y;
		delta_NESW.z -= half.y;
		delta_NESW.y += delta.x;
		pos_delta.y += half.y;
		break;
	case CBORDER_RIGHT:
		delta_NESW.y += delta.x;
		break;
	case CBORDER_BOTTOM_RIGHT:
		delta_NESW.x -= half.y;
		delta_NESW.z += half.y;
		delta_NESW.y += delta.x;
		pos_delta.y += half.y;
		break;
	case CBORDER_BOTTOM:
		delta_NESW.x -= half.y;
		delta_NESW.z += half.y;
		pos_delta.y += half.y;
		break;
	case CBORDER_BOTTOM_LEFT:
		delta_NESW.x -= half.y;
		delta_NESW.z += half.y;
		delta_NESW.y -= delta.x;
		pos_delta.x += delta.x;
		pos_delta.y += half.y;
		break;
	case CBORDER_LEFT:
		delta_NESW.y -= delta.x;
		pos_delta.x += delta.x;
		break;
	}

	for ( int i = 0; i < 5; i++ )
	{
		if ( m_vecAccumulatedExtent[i] )
		{
			float* target = nullptr;
			if ( i >= 3 )
				target = &pos_delta[i - 3];
			else
				target = &delta_NESW[i];

			const bool bWasNeg = m_vecAccumulatedExtent[i] < 0;
			m_vecAccumulatedExtent[i] += *target;
			const bool bIsNeg = m_vecAccumulatedExtent[i] < 0;
			if ( bWasNeg && bIsNeg || !bWasNeg && !bIsNeg )
				*target = 0;
			else
			{
				*target = m_vecAccumulatedExtent[i] * 1;
				m_vecAccumulatedExtent[i] = 0;
			}
		}
	}

	Vector oldExtents = m_vecContainerExtents;
	for ( int i = 0; i < 3; i++ )
		m_vecContainerExtents[i] += delta_NESW[i];

	if ( m_vecContainerExtents[0] < m_flMinSizeY )
	{
		m_vecAccumulatedExtent[0] += min( m_vecContainerExtents[0] - m_flMinSizeY, 0.f );
		m_vecAccumulatedExtent[4] -= min( m_vecContainerExtents[0] - m_flMinSizeY, 0.f ) * Sign( pos_delta.y );

		float _save = oldExtents.x - m_flMinSizeY;
		_save = max( _save, 0.f );
		pos_delta.y = _save * Sign( pos_delta.y );
		m_vecContainerExtents[0] = m_flMinSizeY;
	}
	if ( m_vecContainerExtents[2] > -m_flMinSizeY )
	{
		m_vecAccumulatedExtent[2] -= min( abs( m_vecContainerExtents[2] ) - ( m_flMinSizeY ), 0.f );
		m_vecContainerExtents[2] = -m_flMinSizeY;
	}
	if ( m_vecContainerExtents[1] < m_flMinSizeX )
	{
		if ( pos_delta.x > 0 )
		{
			float _save = oldExtents.y - m_flMinSizeX;
			_save = max( _save, 0.f );
			pos_delta.x = _save;
		}
		m_vecAccumulatedExtent[3] -= min( m_vecContainerExtents[1] - m_flMinSizeX, 0.f );
		m_vecAccumulatedExtent[1] += min( m_vecContainerExtents[1] - m_flMinSizeX, 0.f );
		m_vecContainerExtents[1] = m_flMinSizeX;
	}

	m_vecContainerExtents[2] = -m_vecContainerExtents[0];

	Vector2D posOld = GetPosition();
	SetPosition( posOld + pos_delta );
}

void CBaseContainerNode::OnDragEnd()
{
	BaseClass::OnDragEnd();

	m_iActiveScalingMode = CBORDER_INVALID;
}

void CBaseContainerNode::SelectAllInBounds( bool bSelected, CUtlVector<CBaseNode*>* hNodes )
{
	CUtlVector<CBaseNode*> local;
	CUtlVector<CBaseNode*>* nodes = hNodes ? hNodes : &local;

	Assert( nodes );

	int numNodes = pNodeView->GetNumNodes();
	for ( int i = 0; i < numNodes; i++ )
	{
		CBaseNode* n = pNodeView->GetNode( i );
		if ( n == this )
			continue;
		Vector2D nodePos = n->GetContainerSensitiveCenter();

		bool bIsActive = IsInContainerBounds( nodePos );
		if ( !bIsActive )
			continue;

		if ( nodes->HasElement( n ) )
			continue;

		nodes->AddToTail( n );
		n->SetSelected( bSelected );
		if ( n->GetAsContainer() != nullptr )
			n->GetAsContainer()->SelectAllInBounds( bSelected, nodes );
	}
}

Vector2D CBaseContainerNode::GetContainerMins() const
{
	Vector2D center = GetCenter();
	center.x = GetBoundsMaxNodeSpace().x;
	//center.x += m_vecContainerExtents.y;
	center.y += m_vecContainerExtents.z;
	return center;
}

Vector2D CBaseContainerNode::GetContainerMaxs() const
{
	Vector2D center = GetCenter();
	center.x = GetBoundsMaxNodeSpace().x;
	center.x += m_vecContainerExtents.y;
	center.y += m_vecContainerExtents.x;
	return center;
}

bool CBaseContainerNode::IsInContainerBounds( const Vector2D& pos ) const
{
	Vector2D bmin = GetContainerMins();
	Vector2D bmax = GetContainerMaxs();

	if ( pos.x >= bmin.x && pos.x <= bmax.x &&
		 pos.y >= bmin.y && pos.y <= bmax.y )
		return true;
	return false;
}

int CBaseContainerNode::IsInBorderBounds( const Vector2D& pos ) const
{
	for ( int i = CBORDER_FIRST; i <= CBORDER_LAST; i++ )
	{
		Vector2D min = GetContainerBorderMin( i );
		Vector2D max = GetContainerBorderMax( i );
		if ( pos.x >= min.x && pos.x <= max.x &&
			 pos.y >= min.y && pos.y <= max.y )
			return i;
	}
	return CBORDER_INVALID;
}

void CBaseContainerNode::RemoveAllChildren()
{
	for ( int i = 0; i < m_hChildren.Count(); i++ )
	{
		[[maybe_unused]] const bool res = m_hChildren[i]->RemoveFromContainer( this );
		Assert( res );
	}

	m_hChildren.Purge();
}

bool CBaseContainerNode::HasChild( CBaseNode* child ) const
{
	return m_hChildren.IsValidIndex( m_hChildren.Find( child ) );
}

bool CBaseContainerNode::HasAnyChildren() const
{
	return m_hChildren.Count() > 0;
}

void CBaseContainerNode::UpdateOnMove()
{
	int numNodes = pNodeView->GetNumNodes();
	for ( int i = 0; i < numNodes; i++ )
	{
		CBaseNode* n = pNodeView->GetNode( i );
		if ( n == this )
			continue;
		Vector2D nodePos = n->GetContainerSensitiveCenter();

		bool bIsActive = IsInContainerBounds( nodePos );
		if ( !bIsActive )
			RemoveChild( n );
		else
			AddChild( n );
	}
}

void CBaseContainerNode::RemoveChild( CBaseNode* child )
{
	if ( !HasChild( child ) )
		return;

	child->RemoveFromContainer( this );
	m_hChildren.FindAndRemove( child );
}

void CBaseContainerNode::AddChild( CBaseNode* child )
{
	if ( HasChild( child ) )
		return;

	child->InsertToContainer( this );
	m_hChildren.AddToTail( child );
}

bool CBaseContainerNode::MustDragAlone() const
{
	return m_iActiveScalingMode != CBORDER_INVALID;
}

bool CBaseContainerNode::IsWithinBounds_Base( const Vector2D& pos ) const
{
	bool bBaseTest = BaseClass::IsWithinBounds_Base( pos );
	if ( !bBaseTest )
	{
		int BorderTest = IsInBorderBounds( pos );
		bBaseTest = BorderTest != CBORDER_INVALID;
	}
	return bBaseTest;
}

Vector2D CBaseContainerNode::GetContainerBorderMin( int mode ) const
{
	Vector2D out;
	Vector2D center = GetCenter();
	center.x = GetBoundsMaxNodeSpace().x;
	const float bSize = GetBorderSize();

	switch ( mode )
	{
	default:
		Assert( 0 );
	case CBORDER_TOP_LEFT:
		out.x = center.x;
		out.y = center.y + m_vecContainerExtents.x - bSize;
		break;
	case CBORDER_TOP:
		out.x = center.x + bSize;
		out.y = center.y + m_vecContainerExtents.x - bSize;
		break;
	case CBORDER_TOP_RIGHT:
		out.x = center.x + m_vecContainerExtents.y - bSize;
		out.y = center.y + m_vecContainerExtents.x - bSize;
		break;
	case CBORDER_RIGHT:
		out.x = center.x + m_vecContainerExtents.y - bSize;
		out.y = center.y + m_vecContainerExtents.z + bSize;
		break;
	case CBORDER_BOTTOM_RIGHT:
		out.x = center.x + m_vecContainerExtents.y - bSize;
		out.y = center.y + m_vecContainerExtents.z;
		break;
	case CBORDER_BOTTOM:
		out.x = center.x + bSize;
		out.y = center.y + m_vecContainerExtents.z;
		break;
	case CBORDER_BOTTOM_LEFT:
		out.x = center.x;
		out.y = center.y + m_vecContainerExtents.z;
		break;
	case CBORDER_LEFT:
		out.x = center.x;
		out.y = center.y + m_vecContainerExtents.z + bSize;
		break;
	}

	return out;
}

Vector2D CBaseContainerNode::GetContainerBorderMax( int mode ) const
{
	Vector2D out;
	Vector2D center = GetCenter();
	center.x = GetBoundsMaxNodeSpace().x;
	const float bSize = GetBorderSize();

	switch ( mode )
	{
	default:
	case CBORDER_TOP_LEFT:
		out.x = center.x + bSize;
		out.y = center.y + m_vecContainerExtents.x;
		break;
	case CBORDER_TOP:
		out.x = center.x + m_vecContainerExtents.y - bSize;
		out.y = center.y + m_vecContainerExtents.x;
		break;
	case CBORDER_TOP_RIGHT:
		out.x = center.x + m_vecContainerExtents.y;
		out.y = center.y + m_vecContainerExtents.x;
		break;
	case CBORDER_RIGHT:
		out.x = center.x + m_vecContainerExtents.y;
		out.y = center.y + m_vecContainerExtents.x - bSize;
		break;
	case CBORDER_BOTTOM_RIGHT:
		out.x = center.x + m_vecContainerExtents.y;
		out.y = center.y + m_vecContainerExtents.z + bSize;
		break;
	case CBORDER_BOTTOM:
		out.x = center.x + m_vecContainerExtents.y - bSize;
		out.y = center.y + m_vecContainerExtents.z + bSize;
		break;
	case CBORDER_BOTTOM_LEFT:
		out.x = center.x + bSize;
		out.y = center.y + m_vecContainerExtents.z + bSize;
		break;
	case CBORDER_LEFT:
		out.x = center.x + bSize;
		out.y = center.y + m_vecContainerExtents.x - bSize;
		break;
	}

	return out;
}

bool CBaseContainerNode::VguiDraw( bool bShadow ) const
{
	if ( !ShouldSimpleDrawObject( pNodeView, pNodeView, m_vecBounds.AsVector2D(), Vector2D( m_vecBounds.z, m_vecBounds.w ) ) )
		return false;
	if ( !pNodeView )
		return false;

	Vector2D title_min = GetBoundsTitleMin();
	Vector2D title_max = GetBoundsTitleMax();
	title_max.x -= NODE_DRAW_TITLE_SPACE;

	Vector2D box_min = GetBoundsBoxMin();
	Vector2D box_max = GetBoundsBoxMax();

	pNodeView->ToPanelSpace( title_min );
	pNodeView->ToPanelSpace( title_max );
	pNodeView->ToPanelSpace( box_min );
	pNodeView->ToPanelSpace( box_max );

	float flZoom = pNodeView->GetZoomScalar();
	float flO = NODE_DRAW_SHADOW_DELTA * flZoom;
	Vector2D offset( flO, flO );

	if ( bShadow )
	{
		title_min += offset;
		title_max += offset;
		box_min += offset;
		box_max += offset;
	}

	const bool bSelected = IsSelected();

	if ( bShadow )
		vgui::surface()->DrawSetColor( NODE_DRAW_COLOR_SHADOW );
	else
	{
		Color colTitleNoShadow = NODE_DRAW_COLOR_TITLE;
		if ( GetNumContainers() )
			colTitleNoShadow = NODE_DRAW_COLOR_HASCONTAINER_TITLE;
		else if ( bSelected )
			colTitleNoShadow = NODE_DRAW_COLOR_SELECTED_TITLE;
		vgui::surface()->DrawSetColor( colTitleNoShadow );
	}

	vgui::surface()->DrawFilledRect( title_min.x, title_min.y, title_max.x, title_max.y );

	if ( !bShadow )
	{
		Color boxNoShadow = NODE_DRAW_COLOR_BOX;
		if ( bSelected )
			boxNoShadow = NODE_DRAW_COLOR_SELECTED_BOX;

		vgui::surface()->DrawSetColor( boxNoShadow );
	}

	vgui::surface()->DrawFilledRect( box_min.x, box_min.y, box_max.x, box_max.y );

	Vector2D rects_[8] = {
		GetContainerBorderMin( CBORDER_TOP_LEFT ),
		GetContainerBorderMax( CBORDER_TOP_RIGHT ),
		GetContainerBorderMin( CBORDER_RIGHT ),
		GetContainerBorderMax( CBORDER_RIGHT ),
		GetContainerBorderMin( CBORDER_BOTTOM_LEFT ),
		GetContainerBorderMax( CBORDER_BOTTOM_RIGHT ),
		GetContainerBorderMin( CBORDER_LEFT ),
		GetContainerBorderMax( CBORDER_LEFT ),
	};
	for ( int i = 0; i < 8; i++ )
		pNodeView->ToPanelSpace( rects_[i] );

	if ( bShadow )
	{
		for ( int i = 0; i < 8; i++ )
			rects_[i] += offset;
	}

	for ( int i = 0; i < 4; i++ )
		vgui::surface()->DrawFilledRect( rects_[i * 2].x, rects_[i * 2 + 1].y, rects_[i * 2 + 1].x, rects_[i * 2].y );

	if ( bShadow )
	{
		VguiDraw_Jacks( true );
	}
	else
	{
		VguiDraw_Jacks( false );
		Color borderCol = NODE_DRAW_COLOR_BORDER;
		if ( GetErrorLevel() == ERRORLEVEL_MISSING_IO )
			borderCol = DRAWCOLOR_ERRORLEVEL_UNDEFINED;
		else if ( GetErrorLevel() == ERRORLEVEL_WRONG_TYPE )
			borderCol = DRAWCOLOR_ERRORLEVEL_FAIL;
		else if ( IsAllocating() )
			borderCol = NODE_DRAW_COLOR_HASSOLVER_BOX_ALLOC;
		vgui::surface()->DrawSetColor( borderCol );

		int borderSize = 1;
		vgui::surface()->DrawFilledRect( title_min.x, title_min.y, title_max.x, title_min.y + borderSize );
		vgui::surface()->DrawFilledRect( title_min.x, title_max.y - borderSize, title_max.x, title_max.y );
		vgui::surface()->DrawFilledRect( title_min.x, title_min.y, title_min.x + borderSize, title_max.y );
		vgui::surface()->DrawFilledRect( title_max.x - borderSize, title_min.y, title_max.x, title_max.y );

		vgui::surface()->DrawFilledRect( box_min.x, box_min.y, box_max.x, box_min.y + borderSize );
		vgui::surface()->DrawFilledRect( box_min.x, box_max.y - borderSize, box_max.x, box_max.y );
		vgui::surface()->DrawFilledRect( box_min.x, box_min.y, box_min.x + borderSize, box_max.y );
		//surface()->DrawFilledRect( box_max.x - borderSize, box_min.y, box_max.x, box_max.y );

		vgui::surface()->DrawFilledRect( box_max.x, rects_[1].y, box_max.x + borderSize, box_min.y + borderSize );
		vgui::surface()->DrawFilledRect( box_max.x, box_max.y - borderSize, box_max.x + borderSize, rects_[4].y );
		vgui::surface()->DrawFilledRect( rects_[7].x - borderSize, rects_[7].y, rects_[7].x, rects_[6].y );

		vgui::surface()->DrawFilledRect( rects_[2].x, rects_[3].y, rects_[2].x + borderSize, rects_[2].y );
		vgui::surface()->DrawFilledRect( rects_[3].x - borderSize, rects_[1].y, rects_[3].x, rects_[4].y );

		vgui::surface()->DrawFilledRect( rects_[0].x, rects_[1].y, rects_[1].x, rects_[1].y + borderSize );
		vgui::surface()->DrawFilledRect( rects_[4].x, rects_[4].y - borderSize, rects_[5].x, rects_[4].y );

		vgui::surface()->DrawFilledRect( rects_[7].x - borderSize, rects_[0].y - borderSize, rects_[2].x + borderSize, rects_[0].y );
		vgui::surface()->DrawFilledRect( rects_[7].x - borderSize, rects_[5].y, rects_[2].x + borderSize, rects_[5].y + borderSize );
	}

	Vector2D titleMid = ( title_max - title_min ) * 0.5f + title_min;

	wchar_t szconverted[1024];
	int fontWide, fontTall;

	if ( !bShadow )
	{
		bool bDraw;
		vgui::HFont hFont_Small = pNodeView->GetFontScaled( 12, bDraw );
		if ( bDraw )
		{
			ILocalize::ConvertANSIToUnicode( szOpName, szconverted, sizeof( szconverted ) );

			vgui::surface()->DrawSetTextFont( hFont_Small );
			vgui::surface()->GetTextSize( hFont_Small, szconverted, fontWide, fontTall );
			vgui::surface()->DrawSetTextPos( titleMid.x - fontWide * 0.5f, titleMid.y - fontTall * 0.5f );

			vgui::surface()->DrawSetTextColor( bSelected ? NODE_DRAW_COLOR_SELECTED_TEXT_OP : NODE_DRAW_COLOR_TEXT_OP );
			vgui::surface()->DrawPrintText( szconverted, wcslen( szconverted ) );
		}

		DrawGrabIcon();
	}

	if ( *szNodeName )
	{
		vgui::surface()->DrawSetTextColor( bShadow ? NODE_DRAW_COLOR_SHADOW : NODE_DRAW_COLOR_CUSTOMTITLE );

		bool bDraw;
		vgui::HFont hFont_Small = pNodeView->GetFontScaled( 18, bDraw );
		if ( bDraw )
		{
			ILocalize::ConvertANSIToUnicode( szNodeName, szconverted, sizeof( szconverted ) );

			vgui::surface()->DrawSetTextFont( hFont_Small );
			vgui::surface()->GetTextSize( hFont_Small, szconverted, fontWide, fontTall );
			vgui::surface()->DrawSetTextPos( title_min.x, title_min.y - fontTall - 3 * pNodeView->GetZoomScalar() );

			vgui::surface()->DrawPrintText( szconverted, wcslen( szconverted ) );
		}
	}

	return true;
}

void CBaseContainerNode::DrawGrabIcon() const
{
	const float bSize = GetBorderSize();

	vgui::surface()->DrawSetColor( 192, 192, 192, 255 );
	vgui::surface()->DrawSetTexture( m_iGrabIcon );

	Vector2D icooffset( -GRABICO_OFFSET, GRABICO_OFFSET );
	Vector2D gi_max = Vector2D( GetContainerMaxs().x, GetContainerMins().y ) + icooffset;
	Vector2D gi_min = gi_max + Vector2D( -bSize, bSize );

	pNodeView->ToPanelSpace( gi_max );
	pNodeView->ToPanelSpace( gi_min );

	vgui::surface()->DrawTexturedRect( gi_min.x, gi_min.y, gi_max.x, gi_max.y );
}