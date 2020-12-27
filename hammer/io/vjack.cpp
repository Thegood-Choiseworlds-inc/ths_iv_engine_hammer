
#include "vjack.h"

#include "editorcommon.h"
#include "vbasenode.h"
#include "vbasecontainer.h"
#include "vbridge.h"
#include "vnodeview.h"

#include "vgui/ISurface.h"

#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterialsystem.h"

#include "tier1/ilocalize.h"

#include "tier0/memdbgon.h"

static constexpr const wchar_t* TypeMap[] =
{
	L"void",
	L"integer",
	L"bool",
	L"string",
	L"float",
	L"vector",
	L"entity",
	L"color255",

	L"target"
};
static_assert( ARRAYSIZE( TypeMap ) == TYPE_COUNT );

CJack::CJack( CBaseNode* p, int slot, bool input )
{
	pNode = p;
	pNodeView = p->GetParent();

	m_vecPosition.Init();
	m_vecSize.Init( JACK_SIZE_X, JACK_SIZE_Y );

	bHasFocus = false;
	m_bInput = input;
	m_iSlot = slot;

	m_iSmartType = -1;
	bLockSmartType = false;

	szName[0] = '\0';
}

CJack::~CJack()
{
	pNodeView->OnJackRemoved( this );
	PurgeBridges();
}

void CJack::ConnectBridge( CBridge* b )
{
	m_hBridgeList.AddToTail( b );

	const bool bConnected = b->GetEndJack( this ) && b->GetEndNode( this );
	if ( !bConnected )
		return;

	UpdateSmartType( b );
}

void CJack::OnBridgeRemoved( CBridge* b )
{
	/*CBaseNode* pOtherNode = b->GetEndNode( this );
	CJack* pOtherJack = b->GetEndJack( this );*/

	m_hBridgeList.FindAndRemove( b );

	if ( IsInput() )
	{
		UpdateSmartType( nullptr );
	}
}

void CJack::PurgeBridges()
{
	SafeDeleteVector( m_hBridgeList );
}

void CJack::DisconnectBridges()
{
	PurgeBridges();
	//for ( int b = 0; b < m_hBridgeList.Count(); b++ )
	//{
	//	CBridge *pB = m_hBridgeList[ b ];
	//	if ( this == pB->GetDestinationJack() )
	//		pB->DisconnectDestination();
	//	else
	//		pB->DisconnectSource();
	//}
}

int CJack::GetNumBridges() const
{
	return m_hBridgeList.Count();
}

int CJack::GetNumBridgesConnected() const
{
	int b = 0;
	for ( int i = 0; i < m_hBridgeList.Count(); i++ )
	{
		if ( m_hBridgeList[i]->GetEndJack( this ) && m_hBridgeList[i]->GetEndNode( this ) )
			b++;
	}
	return b;
}

CBridge* CJack::BridgeBeginBuild()
{
	CBridge* b = new CBridge( pNodeView );
	if ( !IsInput() )
		b->ConnectSource( this, GetParentNode() );
	else
		b->ConnectDestination( this, GetParentNode() );
	return b;
}

void CJack::BridgeEndBuild( CBridge* pBridge )
{
	if ( !IsInput() )
		pBridge->ConnectSource( this, GetParentNode() );
	else
		pBridge->ConnectDestination( this, GetParentNode() );
	pNodeView->AddBridgeToList( pBridge );
}

void CJack::UpdateSmartType( CBridge* b )
{
	if ( IsOutput() )
		return;

	if ( !b || !b->GetEndJack( this ) )
		return SetSmartType( -1 );

	//SetSmartType( b->GetEndJack( this )->GetSmartType() );
}

void CJack::SetSmartTypeLocked( bool b )
{
	bLockSmartType = b;
}

void CJack::SetSmartType( const int t )
{
	if ( IsSmartTypeLocked() )
		return;

	m_iSmartType = t;
}

void CJack::SetName( const char* name )
{
	if ( !name || !*name )
		return;
	V_strcpy_safe( szName, name );
	pNode->UpdateSize();
}

Vector2D CJack::GetBoundsMin() const
{
	return m_vecPosition;
}

Vector2D CJack::GetBoundsMax() const
{
	return m_vecPosition + m_vecSize;
}

Vector2D CJack::GetCenter() const
{
	return GetBoundsMin() + ( GetBoundsMax() - GetBoundsMin() ) * 0.5f;
}

Vector2D CJack::GetBoundsMinPanelSpace() const
{
	Vector2D _min( GetBoundsMin().x, GetBoundsMax().y );
	pNodeView->ToPanelSpace( _min );
	return _min;
}

Vector2D CJack::GetBoundsMaxPanelSpace() const
{
	Vector2D _max( GetBoundsMax().x, GetBoundsMin().y );
	pNodeView->ToPanelSpace( _max );
	return _max;
}

Vector2D CJack::GetCenterPanelSpace() const
{
	Vector2D pos = GetCenter();
	pNodeView->ToPanelSpace( pos );
	return pos;
}

bool CJack::IsWithinBounds_Base( const Vector2D& pos ) const
{
	Vector2D _min, _max;
	_min = GetBoundsMin();
	_max = GetBoundsMax();

	if ( pos.x >= _min.x && pos.y >= _min.y &&
		 pos.x <= _max.x && pos.y <= _max.y )
		return true;
	return false;
}

void CJack::SetPosition( Vector2D vec, bool bCenter )
{
	if ( bCenter )
	{
		Vector2D delta = GetBoundsMin() + ( GetBoundsMax() - GetBoundsMin() ) * 0.5f;
		vec -= delta;
	}
	m_vecPosition = vec;
}

void CJack::UpdatePosition()
{
	Vector2D parentBoxStart = pNode->GetBoundsBoxMin();
	Vector2D parentBoxEnd = pNode->GetBoundsBoxMax();
	const bool bContainer = !!pNode->GetAsContainer();

	float x = m_bInput ? parentBoxStart.x : ( parentBoxEnd.x + ( bContainer ? CBORDER_SIZE : 0 ) );
	x += m_bInput ? -JACK_SIZE_X : 0;
	float y = parentBoxStart.y - ( JACK_SIZE_Y + JACK_DELTA_Y ) * m_iSlot - JACK_SIZE_Y * 2;

	SetPosition( Vector2D( x, y ) );
}

void CJack::UpdatePositionData()
{
	UpdatePosition();
}

Vector2D CJack::GetPosition() const
{
	return m_vecPosition;
}

void CJack::OnParentMoved()
{
	UpdatePosition();
}

void CJack::VguiDraw( bool bShadow ) const
{
	if ( !pNodeView->ShouldDraw_Jacks() )
		return;

	Vector2D panelMin = GetBoundsMinPanelSpace();
	Vector2D panelMax = GetBoundsMaxPanelSpace();

	float flZoom = pNodeView->GetZoomScalar();
	if ( bShadow )
	{
		float flO = NODE_DRAW_SHADOW_DELTA * flZoom;
		Vector2D offset( flO, flO );
		panelMin += offset;
		panelMax += offset;
	}

	Color colText = JACK_COLOR_DATATYPE;

	switch ( m_iSmartType )
	{
	case TYPE_VOID:
		vgui::surface()->DrawSetColor( JACK_COLOR_NORM );
		break;
	case TYPE_COLOR:
		vgui::surface()->DrawSetColor( JACK_COLOR_R );
		break;
	case TYPE_VECTOR:
		vgui::surface()->DrawSetColor( JACK_COLOR_G );
		break;
	case TYPE_STRING:
		vgui::surface()->DrawSetColor( JACK_COLOR_B );
		break;
	case TYPE_INT:
	case TYPE_BOOL:
	case TYPE_FLOAT:
		vgui::surface()->DrawSetColor( JACK_COLOR_A );
		break;
	case TYPE_EHANDLE:
		vgui::surface()->DrawSetColor( JACK_COLOR_PPMASTER );
		colText = JACK_COLOR_DATATYPE_DARK;
		break;
	}

	if ( HasFocus() )
		vgui::surface()->DrawSetColor( JACK_COLOR_FOCUS );

	if ( bShadow )
		vgui::surface()->DrawSetColor( NODE_DRAW_COLOR_SHADOW );
	else
		VguiDrawName();

	vgui::surface()->DrawFilledRect( panelMin.x, panelMin.y, panelMax.x, panelMax.y );

	if ( pNodeView->ShouldDraw_Datatypes() )
	{
		wchar_t szconverted[1024];
		int fontWide, fontTall;

		if ( !bShadow )
		{
			bool bDraw;
			vgui::HFont hFont_Small = pNodeView->GetFontScaled( 3, bDraw );
			if ( bDraw )
			{
				int smartType = GetSmartType();
				if ( smartType >= 0 && smartType < TYPE_COUNT )
				{
					wcscpy( szconverted, TypeMap[smartType] );
					vgui::surface()->DrawSetTextColor( colText );
				}
				else
				{
					ILocalize::ConvertANSIToUnicode( "NONE", szconverted, sizeof( szconverted ) );
					vgui::surface()->DrawSetTextColor( JACK_COLOR_DATATYPE_UNDEFINED );
				}

				vgui::surface()->DrawSetTextFont( hFont_Small );
				vgui::surface()->GetTextSize( hFont_Small, szconverted, fontWide, fontTall );

				Vector2D mid = panelMin + ( panelMax - panelMin ) * 0.5f;
				vgui::surface()->DrawSetTextPos( mid.x - fontWide * 0.5f, mid.y - fontTall * 0.5f );

				vgui::surface()->DrawPrintText( szconverted, wcslen( szconverted ) );
			}
		}
	}
}

void CJack::VguiDrawName() const
{
	if ( !*szName )
		return;

	Vector2D _min = GetBoundsMinPanelSpace();
	Vector2D _max = GetBoundsMaxPanelSpace();

	Vector2D drawpos( _min + ( _max - _min ) * 0.5f );

	bool bDraw;
	vgui::HFont font = pNodeView->GetFontScaled( 9, bDraw );
	if ( !bDraw )
		return;

	wchar_t szconverted[1024];
	ILocalize::ConvertANSIToUnicode( szName, szconverted, sizeof( szconverted ) );

	int fontWide, fontTall;
	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->GetTextSize( font, szconverted, fontWide, fontTall );

	Vector2D edge = m_bInput ? GetBoundsMax() : GetBoundsMin();
	edge.x = edge.x + ( m_bInput ? JACK_TEXT_INSET : -JACK_TEXT_INSET );
	pNodeView->ToPanelSpace( edge );
	if ( !m_bInput )
		edge.x -= fontWide;

	vgui::surface()->DrawSetTextPos( edge.x, drawpos.y - fontTall * 0.5f );

	vgui::surface()->DrawSetTextColor( HasFocus() ? JACK_COLOR_NAME_FOCUS : JACK_COLOR_NAME );
	vgui::surface()->DrawPrintText( szconverted, wcslen( szconverted ) );
}

int CJack::GetFinalTextInset() const
{
	int i = 0;
	if ( *szName )
	{
		float z = 1.15f;
		bool bDraw;
		vgui::HFont font = pNodeView->GetFontScaled( 9, bDraw, &z );
		Assert( bDraw );

		wchar_t szconverted[1024];
		V_memset( szconverted, 0, sizeof( szconverted ) );
		ILocalize::ConvertANSIToUnicode( szName, szconverted, sizeof( szconverted ) );

		int fontWide, fontTall;
		vgui::surface()->DrawSetTextFont( font );
		vgui::surface()->GetTextSize( font, szconverted, fontWide, fontTall );

		i = fontWide + JACK_TEXT_INSET + 3;
	}
	return i;
}

bool CJack::IsTypeCompatibleWith( const CJack* other ) const
{
	return other->GetSmartType() == GetSmartType();
}