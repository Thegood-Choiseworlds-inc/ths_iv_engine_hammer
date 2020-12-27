
#include "vbasenode.h"
#include "editorcommon.h"
#include "vbasecontainer.h"
#include "vnodeview.h"
#include "vjack.h"
#include "vbridge.h"

#include "vgui/ISurface.h"

#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterialsystem.h"

#include "tier1/ilocalize.h"

#include "tier0/memdbgon.h"

HNODE CBaseNode::m_iUniqueIndexCount = 0;

CBaseNode::CBaseNode( const char* opName, CNodeView* p )
{
	szOpName[0] = '\0';
	szNodeName[0] = '\0';

	m_vecPosition.Init();
	m_flMinSizeX = 80;
	m_flMinSizeY = 80;

	m_bIsAllocating = false;
	m_bMarkedForDeletion = false;
	bSelected = false;
	m_bAllInputsRequired = true;

	m_iUniqueIndex = m_iUniqueIndexCount;
	m_vecBorderInfo.Init();

	SetErrorLevel( ERRORLEVEL_NONE );

	pNodeView = p;

	m_iUniqueIndexCount++;

	SetName( opName );
}

CBaseNode::~CBaseNode()
{
	//for ( int i = 0; i < GetNumContainers(); i++ )
	while ( GetNumContainers() )
		GetContainer( 0 )->RemoveChild( this );
	Assert( !m_hParentContainers.Count() );

	CUtlVector<CJack*> m_hTmpJacks;
	m_hTmpJacks.AddVectorToTail( m_hInputs );
	m_hTmpJacks.AddVectorToTail( m_hOutputs );

	m_hInputs.Purge();
	m_hOutputs.Purge();
	SafeDeleteVector( m_hTmpJacks );
}

void CBaseNode::SetName( const char* opName )
{
	if ( !opName || !*opName )
		return;
	V_strcpy_safe( szOpName, opName );
	UpdateSize();
}

const char* CBaseNode::GetName() const
{
	return szOpName;
}

const char* CBaseNode::GetUserName() const
{
	return szNodeName;
}

int CBaseNode::GetFinalTextSize() const
{
	int i = 0;
	if ( *szOpName )
	{
		float z = 1.5f;
		bool bDraw;
		vgui::HFont font = pNodeView->GetFontScaled( 9, bDraw, &z );
		Assert( bDraw );

		wchar_t szconverted[1024];
		Q_memset( szconverted, 0, sizeof( szconverted ) );
		ILocalize::ConvertANSIToUnicode( szOpName, szconverted, sizeof( szconverted ) );

		int fontWide, fontTall;
		vgui::surface()->DrawSetTextFont( font );
		vgui::surface()->GetTextSize( font, szconverted, fontWide, fontTall );

		i = fontWide + 3;
	}
	return i;
}

void CBaseNode::Spawn()
{
	UpdateSize();
}

bool CBaseNode::InsertToContainer( CBaseContainerNode* container )
{
	if ( HasContainerParent( container ) )
		return false;
	m_hParentContainers.AddToTail( container );
	return true;
}

bool CBaseNode::HasContainerParent( CBaseContainerNode* container ) const
{
	if ( !container )
		return false;
	return m_hParentContainers.HasElement( container );
}

bool CBaseNode::RemoveFromContainer( CBaseContainerNode* container )
{
	int idx = m_hParentContainers.Find( container );
	if ( !m_hParentContainers.IsValidIndex( idx ) )
		return false;
	m_hParentContainers.Remove( idx );
	Assert( !HasContainerParent( container ) );
	return true;
}

int CBaseNode::GetNumContainers() const
{
	return m_hParentContainers.Count();
}

CBaseContainerNode* CBaseNode::GetContainer( int idx ) const
{
	Assert( m_hParentContainers.IsValidIndex( idx ) );
	return m_hParentContainers[idx];
}

Vector2D CBaseNode::GetContainerSensitiveCenter() const
{
	return GetPosition() + Vector2D( 45, -30 );
}
void CBaseNode::ListContainersChronologically_Internal( CUtlVector<CBaseNode*>& m_hNodesProcessed, CUtlVector<CBaseContainerNode*>& hList )
{
	CBaseContainerNode* Self = GetAsContainer();
	int numContainerParents = GetNumContainers();

	//if ( !numContainerParents )
	{
		if ( Self && !hList.IsValidIndex( hList.Find( Self ) ) )
			hList.AddToHead( Self );
	}
	//else
	{
		for ( int i = 0; i < numContainerParents; i++ )
		{
			CBaseContainerNode* c = m_hParentContainers[i];
			if ( m_hNodesProcessed.IsValidIndex( m_hNodesProcessed.Find( c ) ) )
				continue;
			m_hNodesProcessed.AddToTail( c );
			c->ListContainersChronologically_Internal( m_hNodesProcessed, hList );
		}
	}
}

static int ContainerSort( CBaseContainerNode* const* c1, CBaseContainerNode* const* c2 )
{
	if ( ( *c1 )->HasContainerParent( *c2 ) )
		return 1;
	else if ( ( *c2 )->HasContainerParent( *c1 ) )
		return -1;
	return 0;
}

void CBaseNode::ListContainersChronologically( CUtlVector<CBaseContainerNode*>& hList )
{
	CUtlVector<CBaseNode*> hNodes;
	ListContainersChronologically_Internal( hNodes, hList );
	hNodes.Purge();

	hList.Sort( ContainerSort );
}

bool CBaseNode::RecursiveTestContainerError_Internal( CUtlVector<CBaseNode*>& m_hNodesProcessed, bool& bLeftContainerOnce, const bool& bHierachyUp, CBaseContainerNode* container )
{
	bool bError = false;
	const bool bIsInContainer = HasContainerParent( container );
	if ( bLeftContainerOnce && bIsInContainer )
		bError = true;
	//else if ( !bIsInContainer )
	//	bLeftContainerOnce = true;

	const int numJacks = bHierachyUp ? GetNumJacks_In() : GetNumJacks_Out();
	for ( int i = 0; i < numJacks; i++ )
	{
		CJack* j = bHierachyUp ? GetJack_In( i ) : GetJack_Out( i );
		for ( int b = 0; b < j->GetNumBridges(); b++ )
		{
			CBridge* pBridge = j->GetBridge( b );
			CBaseNode* pNodeEnd = pBridge->GetEndNode( this );
			if ( !pNodeEnd )
				continue;

			if ( !bIsInContainer )
				bLeftContainerOnce = true;
			bool bOtherInContainer = pNodeEnd->HasContainerParent( container );
			if ( bLeftContainerOnce && bOtherInContainer )
				bError = true;

			if ( m_hNodesProcessed.IsValidIndex( m_hNodesProcessed.Find( pNodeEnd ) ) )
				continue;

			m_hNodesProcessed.AddToTail( pNodeEnd );
			bError = bError || pNodeEnd->RecursiveTestContainerError_Internal( m_hNodesProcessed,
																			   bLeftContainerOnce, bHierachyUp, container );
		}
	}
	bLeftContainerOnce = false;
	return bError;
}

bool CBaseNode::RecursiveTestContainerError( const bool& bHierachyUp, CBaseContainerNode* container )
{
	CUtlVector<CBaseNode*> m_hProcessed;
	bool bLeftContainer = false;
	return RecursiveTestContainerError_Internal( m_hProcessed, bLeftContainer, bHierachyUp, container );
}

void CBaseNode::Recursive_AddTailNodes_Internal( CUtlVector<CBaseNode*>& m_hProcessedNodes,
												 CUtlVector<CBaseNode*>& m_hList,
												 bool bHierachyUp, CBaseContainerNode* pContainer, bool bAddContainers )
{
	m_hProcessedNodes.AddToTail( this );
	Assert( !pContainer || HasContainerParent( pContainer ) );
	const int numJacks = bHierachyUp ? GetNumJacks_In() : GetNumJacks_Out();
	bool bEndOfContainer = pContainer != nullptr;
	const bool bIsContainer = GetAsContainer() != nullptr;

	if ( bEndOfContainer )
		for ( int i = 0; i < numJacks; i++ )
		{
			CJack* j = bHierachyUp ? GetJack_In( i ) : GetJack_Out( i );
			for ( int b = 0; b < j->GetNumBridges(); b++ )
			{
				CBridge* pBridge = j->GetBridge( b );
				CBaseNode* pNodeEnd = pBridge->GetEndNode( this );
				if ( pNodeEnd && pNodeEnd->HasContainerParent( pContainer ) )
					bEndOfContainer = false;
			}
		}

	const bool bAnyConnected = bHierachyUp ? GetNumJacks_In_Connected() != 0 : GetNumJacks_Out_Connected() != 0;
	if ( !numJacks || !bAnyConnected || bEndOfContainer )
	{
		if ( m_hList.IsValidIndex( m_hList.Find( this ) ) )
			return;
		if ( !bIsContainer || bAddContainers )
			m_hList.AddToTail( this );
		return;
	}

	for ( int i = 0; i < numJacks; i++ )
	{
		CJack* j = bHierachyUp ? GetJack_In( i ) : GetJack_Out( i );
		if ( !bAnyConnected && !j->GetNumBridgesConnected() )
		{
			if ( m_hList.IsValidIndex( m_hList.Find( this ) ) )
				continue;
			if ( !bIsContainer || bAddContainers )
				m_hList.AddToTail( this );
			continue;
		}

		for ( int x = 0; x < j->GetNumBridges(); x++ )
		{
			CBridge* b = j->GetBridge( x );
			CBaseNode* next = b->GetEndNode( j );
			if ( next && !m_hProcessedNodes.IsValidIndex( m_hProcessedNodes.Find( next ) ) &&
				 ( !pContainer || next->HasContainerParent( pContainer ) ) &&
				 ( !next->GetAsContainer() || bAddContainers ) )
				next->Recursive_AddTailNodes_Internal( m_hProcessedNodes, m_hList, bHierachyUp, pContainer, bAddContainers );
		}
	}
}

void CBaseNode::Recursive_AddTailNodes( CUtlVector<CBaseNode*>& m_hList, bool bHierachyUp, CBaseContainerNode* pContainer, bool bAddContainers )
{
	//if ( GetAsContainer() && !bAddContainers )
	//	return;
	CUtlVector<CBaseNode*> m_hProcessed;
	Recursive_AddTailNodes_Internal( m_hProcessed, m_hList, bHierachyUp, pContainer, bAddContainers );
}

void CBaseNode::MarkForDeletion()
{
	m_bMarkedForDeletion = true;
}

bool CBaseNode::IsMarkedForDeletion() const
{
	return m_bMarkedForDeletion;
}

int CBaseNode::UpdateInputsValid()
{
	for ( int i = 0; i < GetNumJacks_In(); i++ )
	{
		CJack* pJIn = GetJack_In( i );
		for ( int a = 0; a < pJIn->GetNumBridges(); a++ )
		{
			CBridge* b = pJIn->GetBridge( a );
			CJack* pEnd = b ? b->GetEndJack( this ) : nullptr;
			CBaseNode* pOtherNode = b->GetEndNode( this );
			if ( !pEnd || !pOtherNode )
				continue;

			int parentError = pOtherNode->GetErrorLevel();

			bool bDefinedSmarttype_Local = pJIn->GetSmartType() >= 0;
			bool bDefinedSmarttype_Remote = pEnd->GetSmartType() >= 0;
			if ( bDefinedSmarttype_Local && bDefinedSmarttype_Remote &&
				 !pJIn->IsTypeCompatibleWith( pEnd ) )
			{
				return ERRORLEVEL_WRONG_TYPE;
			}
			else if ( parentError == ERRORLEVEL_WRONG_TYPE )
				return ERRORLEVEL_MISSING_IO;
			else if ( !bDefinedSmarttype_Local || !bDefinedSmarttype_Remote ||
					  parentError == ERRORLEVEL_MISSING_IO )
				return ERRORLEVEL_MISSING_IO;
		}
	}

	if ( m_bAllInputsRequired && !JacksAllConnected_In() )
		return ERRORLEVEL_MISSING_IO;
	return ERRORLEVEL_NONE;
}

void CBaseNode::UpdateBridgeValidity( CBridge* pBridge, CJack* pCaller, int inputErrorLevel )
{
	CBaseNode* pNodeViewNode = pBridge->GetInputNode();
	CJack* pOther = pBridge->GetInputJack();

	bool bDefined_Other = pOther && pOther->GetSmartType() > -1;
	bool bDefined_Local = pCaller->GetSmartType() > -1;
	//bool bLocked_Other = pOther->IsSmartTypeLocked();
	bool bLocked_Local = pCaller->IsSmartTypeLocked();

	int iParentErrorLevel = pNodeViewNode ? pNodeViewNode->GetErrorLevel() : ERRORLEVEL_WRONG_TYPE;
	int iGoalErrorLevel = inputErrorLevel;

	bool bCorrectType = pOther && pOther->IsTypeCompatibleWith( pCaller );

	// something isn't defined at all
	if ( !bDefined_Local || !bDefined_Other || iParentErrorLevel != ERRORLEVEL_NONE )
		iGoalErrorLevel = ERRORLEVEL_MISSING_IO;
	// wrong type, fail for sure
	else if ( !bCorrectType )
		iGoalErrorLevel = ERRORLEVEL_WRONG_TYPE;
	else if ( pNodeViewNode && pNodeViewNode->GetAsContainer() && !HasContainerParent( pNodeViewNode->GetAsContainer() ) )
		iGoalErrorLevel = ERRORLEVEL_WRONG_TYPE;
	else if ( inputErrorLevel == ERRORLEVEL_MISSING_IO )
	{
		// Our parent is ready but we failed locally, nothing wrong with the bridge after all
		if ( iParentErrorLevel == ERRORLEVEL_NONE )
			iGoalErrorLevel = ERRORLEVEL_NONE;
	}
	else if ( inputErrorLevel == ERRORLEVEL_WRONG_TYPE )
	{
		// we failed although our parent is ready and the target input is static and correct
		if ( iParentErrorLevel == ERRORLEVEL_NONE && bLocked_Local )
			iGoalErrorLevel = ERRORLEVEL_NONE;
	}

	pBridge->SetErrorLevel( iGoalErrorLevel );
}

void CBaseNode::UpdateOutputs()
{
}

void CBaseNode::SetOutputsUndefined()
{
	for ( int i = 0; i < GetNumJacks_Out(); i++ )
	{
		CJack* pJack = GetJack_Out( i );
		pJack->SetSmartType( -1 );
	}
}

int CBaseNode::GetErrorLevel() const
{
	if ( ShouldErrorOnUndefined() && iErrorLevel == ERRORLEVEL_MISSING_IO )
		return ERRORLEVEL_WRONG_TYPE;
	return iErrorLevel;
}

void CBaseNode::SetErrorLevel( int e )
{
	iErrorLevel = e;
}

void CBaseNode::UpdateSize()
{
	int maxJacks = Max( m_hInputs.Count(), m_hOutputs.Count() );
	float sizeMin = ( JACK_SIZE_Y + JACK_DELTA_Y ) * maxJacks + JACK_SIZE_Y;
	sizeMin = Max( sizeMin, m_flMinSizeY );

	m_vecBorderInfo.Init();

	for ( int i = 0; i < m_hInputs.Count(); i++ )
		m_vecBorderInfo.x = Max( m_vecBorderInfo.x, float( GetJack_In( i )->GetFinalTextInset() ) );
	for ( int i = 0; i < m_hOutputs.Count(); i++ )
		m_vecBorderInfo.y = Max( m_vecBorderInfo.y, float( GetJack_Out( i )->GetFinalTextInset() ) );

	float localminX = Max( float( GetFinalTextSize() ), m_flMinSizeX );
	m_vecSize.Init( Max( localminX, m_vecBorderInfo.x + m_vecBorderInfo.y ), -sizeMin );

	TouchJacks();

	UpdateSimpleObjectBounds( m_vecPosition, m_vecSize, m_vecBounds );
}

void CBaseNode::GenerateJacks_Input( int num )
{
	if ( num )
		SetErrorLevel( ERRORLEVEL_MISSING_IO );
	else
		SetErrorLevel( ERRORLEVEL_NONE );

	SafeDeleteVector( m_hInputs );

	for ( int i = 0; i < num; i++ )
		m_hInputs.AddToTail( new CJack( this, i, true ) );

	UpdateSize();
}

void CBaseNode::GenerateJacks_Output( int num )
{
	SafeDeleteVector( m_hOutputs );

	for ( int i = 0; i < num; i++ )
		m_hOutputs.AddToTail( new CJack( this, i, false ) );

	UpdateSize();
}

void CBaseNode::TouchJacks()
{
	for ( int i = 0; i < m_hInputs.Count(); i++ )
		m_hInputs[i]->UpdatePosition();
	for ( int i = 0; i < m_hOutputs.Count(); i++ )
		m_hOutputs[i]->UpdatePosition();
}

int CBaseNode::GetNumJacks_Out() const
{
	return m_hOutputs.Count();
}

int CBaseNode::GetNumJacks_In() const
{
	return m_hInputs.Count();
}

int CBaseNode::GetNumJacks_Out_Connected() const
{
	int o = 0;
	for ( int i = 0; i < GetNumJacks_Out(); i++ )
		if ( GetJack_Out( i )->GetNumBridgesConnected() )
			o++;
	return o;
}

int CBaseNode::GetNumJacks_In_Connected() const
{
	int o = 0;
	for ( int i = 0; i < GetNumJacks_In(); i++ )
	{
		if ( GetJack_In( i )->GetNumBridgesConnected() )
			o++;
	}
	return o;
}

CJack* CBaseNode::GetJack_Out( int i ) const
{
	return m_hOutputs[i];
}

CJack* CBaseNode::GetJack_In( int i ) const
{
	return m_hInputs[i];
}

CJack* CBaseNode::GetJackByName_Out( const char* name ) const
{
	for ( int i = 0; i < GetNumJacks_Out(); i++ )
		if ( !V_stricmp( GetJack_Out( i )->GetName(), name ) )
			return GetJack_Out( i );
	return nullptr;
}

CJack* CBaseNode::GetJackByName_In( const char* name ) const
{
	for ( int i = 0; i < GetNumJacks_In(); i++ )
		if ( !V_stricmp( GetJack_In( i )->GetName(), name ) )
			return GetJack_In( i );
	return nullptr;
}

bool CBaseNode::JacksAllConnected_Out() const
{
	return GetNumJacks_Out_Connected() == GetNumJacks_Out();
}

bool CBaseNode::JacksAllConnected_In() const
{
	return GetNumJacks_In_Connected() == GetNumJacks_In();
}

void CBaseNode::LockJackOutput_Flags( int idx, int Flag, const char* name )
{
	CJack* j = GetJack_Out( idx );
	if ( name )
		j->SetName( name );

	j->SetSmartTypeLocked( false );
	j->SetSmartType( Flag );
	j->SetSmartTypeLocked( true );
}

void CBaseNode::LockJackInput_Flags( int idx, int Flag, const char* name )
{
	CJack* j = GetJack_In( idx );
	if ( name )
		j->SetName( name );

	j->SetSmartTypeLocked( false );
	j->SetSmartType( Flag );
	j->SetSmartTypeLocked( true );
}

int CBaseNode::TestJackFlags_In()
{
	/*bool bError = false;
	bool bUndefined = false;
	for ( int i = 0; i < GetNumJacks_In(); i++ )
	{
		CJack* j = GetJack_In( i );
		int type = j->GetSmartType();
		if ( type < 0 )
			bUndefined = true;
		else if ( !j->HasVarFlag( type ) )
			bError = true;
	}
	if ( bError )
		return ERRORLEVEL_FAIL;
	if ( bUndefined )
		return ERRORLEVEL_UNDEFINED;*/
	return ERRORLEVEL_NONE;
}

void CBaseNode::PurgeBridges( bool bInputs, bool bOutputs )
{
	if ( bInputs )
		for ( int i = 0; i < GetNumJacks_In(); i++ )
			GetJack_In( i )->PurgeBridges();
	if ( bOutputs )
		for ( int i = 0; i < GetNumJacks_Out(); i++ )
			GetJack_Out( i )->PurgeBridges();
}

bool CBaseNode::RecursiveFindNode_Internal( CUtlVector<const CBaseNode*>& m_hList, CBaseNode* n, bool bHierachyUp ) const
{
	m_hList.AddToTail( this );
	bool bFound = false;
	const int numJacks = bHierachyUp ? GetNumJacks_In() : GetNumJacks_Out();
	for ( int i = 0; i < numJacks && !bFound; i++ )
	{
		CJack* j = bHierachyUp ? GetJack_In( i ) : GetJack_Out( i );
		for ( int x = 0; x < j->GetNumBridges() && !bFound; x++ )
		{
			CBridge* b = j->GetBridge( x );
			CBaseNode* next = bHierachyUp ? b->GetInputNode() : b->GetDestinationNode();
			if ( next == nullptr )
				continue;
			if ( next == n )
				return true;
			else if ( !m_hList.IsValidIndex( m_hList.Find( next ) ) )
				bFound = bFound || next->RecursiveFindNode_Internal( m_hList, n, bHierachyUp );
		}
	}
	return bFound;
}
bool CBaseNode::RecursiveFindNode( CBaseNode* n, bool bHierachyUp ) const
{
	CUtlVector<const CBaseNode*> m_hList;
	return RecursiveFindNode_Internal( m_hList, n, bHierachyUp );
}

Vector2D CBaseNode::GetBoundsMin() const
{
	return GetBoundsTitleMin();
}

Vector2D CBaseNode::GetBoundsMax() const
{
	return GetBoundsBoxMax();
}

Vector2D CBaseNode::GetBoundsMinNodeSpace() const
{
	const Vector2D& _min = GetBoundsMin();
	const Vector2D& _max = GetBoundsMax();
	return _min.Min( _max );
}

Vector2D CBaseNode::GetBoundsMaxNodeSpace() const
{
	const Vector2D& _min = GetBoundsMin();
	const Vector2D& _max = GetBoundsMax();
	return _min.Max( _max );
}

Vector2D CBaseNode::GetSelectionBoundsMinNodeSpace() const
{
	return GetBoundsMinNodeSpace();
}

Vector2D CBaseNode::GetSelectionBoundsMaxNodeSpace() const
{
	return GetBoundsMaxNodeSpace();
}

Vector2D CBaseNode::GetCenter() const
{
	Vector2D _min = GetBoundsMinNodeSpace();
	Vector2D _max = GetBoundsMaxNodeSpace();
	return _min + ( _max - _min ) * 0.5f;
}

Vector2D CBaseNode::GetBoundsTitleMin() const
{
	return m_vecPosition;
}

Vector2D CBaseNode::GetBoundsTitleMax() const
{
	return m_vecPosition + Vector2D( m_vecSize.x, -NODE_DRAW_TITLE_Y );
}

Vector2D CBaseNode::GetBoundsBoxMin() const
{
	return m_vecPosition - Vector2D( 0, NODE_DRAW_TITLE_Y + NODE_DRAW_TITLE_SPACE );
}

Vector2D CBaseNode::GetBoundsBoxMax() const
{
	return GetBoundsBoxMin() + m_vecSize;
}

bool CBaseNode::IsWithinBounds_Base( const Vector2D& pos ) const
{
	Vector2D _min, _max;
	_min = GetBoundsMin();
	_max = GetBoundsMax();

	if ( pos.x >= _min.x && pos.y <= _min.y &&
		 pos.x <= _max.x && pos.y >= _max.y )
		return true;
	return false;
}

void CBaseNode::OnLeftClick( const Vector2D& pos )
{
}

void CBaseNode::OnDragStart()
{
}

void CBaseNode::OnDrag( const Vector2D& delta )
{
	SetPosition( GetPosition() + delta );
}

void CBaseNode::OnDragEnd()
{
}

bool CBaseNode::MustDragAlone() const
{
	return false;
}

void CBaseNode::UpdateParentContainers()
{
	Vector2D center = GetContainerSensitiveCenter();
	CUtlVector<CBaseContainerNode*> hContainers_Add;
	CUtlVector<CBaseContainerNode*> hContainers_Remove;
	pNodeView->ListContainerAtPos( center, hContainers_Add );

	for ( int i = 0; i < GetNumContainers(); i++ )
	{
		CBaseContainerNode* c = GetContainer( i );
		if ( hContainers_Add.IsValidIndex( hContainers_Add.Find( c ) ) )
			hContainers_Add.FindAndRemove( c );
		else
			hContainers_Remove.AddToTail( c );
	}

	for ( int i = 0; i < hContainers_Remove.Count(); i++ )
		hContainers_Remove[i]->RemoveChild( this );
	for ( int i = 0; i < hContainers_Add.Count(); i++ )
		hContainers_Add[i]->AddChild( this );

	hContainers_Add.Purge();
	hContainers_Remove.Purge();
}

void CBaseNode::SetPosition( Vector2D vec, bool bCenter )
{
	if ( bCenter )
		vec -= GetBoundsMin() + ( GetBoundsMax() - GetBoundsMin() ) * 0.5f;
	m_vecPosition = vec;

	TouchJacks();

	UpdateSimpleObjectBounds( m_vecPosition, m_vecSize, m_vecBounds );

	UpdateParentContainers();
}

Vector2D CBaseNode::GetPosition() const
{
	return m_vecPosition;
}

Vector2D CBaseNode::GetSize() const
{
	return m_vecSize;
}

const Vector4D& CBaseNode::GetBoundsFast() const
{
	return m_vecBounds;
}

void CBaseNode::SetAllocating( bool a )
{
	m_bIsAllocating = a;
}

bool CBaseNode::IsAllocating() const
{
	return m_bIsAllocating;
}

bool CBaseNode::VguiDraw( bool bShadow ) const
{
	if ( !ShouldSimpleDrawObject( pNodeView, pNodeView, m_vecBounds.AsVector2D(), Vector2D( m_vecBounds.z, m_vecBounds.w ) ) )
		return false;
	if ( !pNodeView )
		return false;

	Vector2D title_min = GetBoundsTitleMin();
	Vector2D title_max = GetBoundsTitleMax();
	Vector2D box_min = GetBoundsBoxMin();
	Vector2D box_max = GetBoundsBoxMax();

	pNodeView->ToPanelSpace( title_min );
	pNodeView->ToPanelSpace( title_max );
	pNodeView->ToPanelSpace( box_min );
	pNodeView->ToPanelSpace( box_max );

	float flZoom = pNodeView->GetZoomScalar();
	if ( bShadow )
	{
		float flO = NODE_DRAW_SHADOW_DELTA * flZoom;
		Vector2D offset( flO, flO );
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
		else if ( GetNumContainers() )
			boxNoShadow = NODE_DRAW_COLOR_HASCONTAINER_BOX;

		vgui::surface()->DrawSetColor( boxNoShadow );
	}

	vgui::surface()->DrawFilledRect( box_min.x, box_min.y, box_max.x, box_max.y );

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
		vgui::surface()->DrawFilledRect( box_max.x - borderSize, box_min.y, box_max.x, box_max.y );

		//float offsetTextBorder = m_vecSize.x - m_flMinSizeX;

		if ( m_vecBorderInfo.y > 1 && m_vecBorderInfo.x > 1 )
		{
			Vector2D DivideStart( m_vecPosition.x + m_vecSize.x - m_vecBorderInfo.y, m_vecPosition.y );
			pNodeView->ToPanelSpace( DivideStart );
			vgui::surface()->DrawFilledRect( DivideStart.x, box_min.y, DivideStart.x + borderSize, box_max.y );
		}
	}

	Vector2D titleMid = ( title_max - title_min ) * 0.5f + title_min;

	wchar_t szconverted[256];
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

void CBaseNode::VguiDraw_Jacks( bool bShadow ) const
{
	for ( int i = 0; i < m_hInputs.Count(); i++ )
		m_hInputs[i]->VguiDraw( bShadow );
	for ( int i = 0; i < m_hOutputs.Count(); i++ )
		m_hOutputs[i]->VguiDraw( bShadow );
}