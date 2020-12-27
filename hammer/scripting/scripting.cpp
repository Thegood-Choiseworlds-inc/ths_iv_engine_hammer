
#include "stdafx.h"

#include "stocksolids.h"
#include "mapsolid.h"
#include "mapgroup.h"
#include "globalfunctions.h"
#include "mainfrm.h"
#include "options.h"
#include "tier0/icommandline.h"
#include "tier1/fmtstr.h"
#include "tier1/utlhashdict.h"
#include "filesystem.h"

#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/TextEntry.h"
#include "HammerVGui.h"
#include "VGuiWnd.h"
#include "resource.h"

#include "scriptarray.h"
#include "scriptbuilder.h"
#include "scriptdictionary.h"
#include "scriptgrid.h"
#include "scripthelper.h"
#include "scriptmath.h"
#include "scriptstring.h"
#include "asbind.h"
#ifdef DEBUG
#include "docgen.h"
#endif

#include <charconv>

#include "tier0/memdbgon.h"

#pragma comment( lib, "angelscript.lib" )

ASBIND_TYPE( CScriptDictionary, dictionary );
ASBIND_TYPE( ScriptString, string );
ASBIND_TYPE( Vector, Vector );
ASBIND_TYPE( QAngle, QAngle );
ASBIND_TYPE( Vector2D, Vector2D );
ASBIND_TYPE( Vector4D, Vector4D );
ASBIND_TYPE( VMatrix, VMatrix );
ASBIND_TYPE( BoundBox, BoundBox );
ASBIND_TYPE( CMapClass, CMapClass );
ASBIND_TYPE( CMapGroup, CMapGroup );
ASBIND_TYPE( CMapSolid, CMapSolid );
ASBIND_TYPE( CMapFace, CMapFace );
ASBIND_TYPE( TextureAlignment_t, TextureAlignment_t );
ASBIND_ARRAY_TYPE( CScriptArrayT<Vector>, Vector );

static asIScriptEngine* engine = nullptr;
static CUtlVector<asIScriptModule*> modules;

static void msgCallback( const asSMessageInfo* msg, void* )
{
	Color clr;
	switch ( msg->type )
	{
	case asMSGTYPE_ERROR:
		clr.SetColor( 255, 64, 64, 255 );
		break;
	case asMSGTYPE_WARNING:
		clr.SetColor( 255, 178, 0, 255 );
		break;
	case asMSGTYPE_INFORMATION:
	default:
		clr.SetColor( 153, 204, 255, 255 );
		break;
	}

	ConColorMsg( clr, "%s(%d:%d): %s\n", msg->section, msg->row, msg->col, msg->message );
}

static void Print( const ScriptString& str )
{
	Msg( "%s", str.Get() );
}

float& WrapAccessVMatrix( VMatrix* pThis, uint x, uint y )
{
	if ( x > 3 || y > 3 )
	{
		asGetActiveContext()->SetException( "Out of bounds" );
		static float f = 0;
		return f;
	}
	return pThis->m[x][y];
}

const float& WrapAccessVMatrixConst( const VMatrix* pThis, uint x, uint y )
{
	if ( x > 3 || y > 3 )
	{
		asGetActiveContext()->SetException( "Out of bounds" );
		static const float f = 0;
		return f;
	}
	return pThis->m[x][y];
}

static void WrapCreateFace( CMapFace* pThis, CScriptArrayT<Vector>* points, int nPoints )
{
	Assert( (unsigned)abs( nPoints ) <= points->GetSize() );
	int size = Min<unsigned>( nPoints, points->GetSize() );
	CUtlVector<Vector> copy;
	copy.SetCount( size );
	for ( int i = 0; i < size; i++ )
		copy[i] = *points->At( i );
	pThis->CreateFace( copy.Base(), nPoints );
}

static void WrapAddFace( CMapSolid* pThis, const CMapFace& face )
{
	pThis->AddFace( const_cast<CMapFace*>( &face ) );
}

template <typename T>
static void WrapCalcBounds( T* pThis, bool full )
{
	pThis->CalcBounds( full );
}

template <typename T>
static CMapClass* WrapCastMapClass( T* pThis )
{
	return pThis;
}

template <typename T>
static T* WrapCreateMapClass()
{
	return new T();
}

class ScriptSolid
{
public:
	ScriptSolid( const ScriptString& name, asIScriptObject* instance )
		: m_name( name ), m_pScriptInstance( instance )
	{
		m_pEngineCtx = instance->GetEngine()->RequestContext();
		asITypeInfo* type = instance->GetObjectType();
		m_getGuiData = type->GetMethodByName( "GetGuiData" );
		m_guiClosed = type->GetMethodByName( "GuiClosed" );
		m_createMapSolid = type->GetMethodByName( "CreateMapSolid" );
		Assert( m_getGuiData.isValid() && m_guiClosed.isValid() && m_createMapSolid.isValid() );

		static const bool debug = CommandLine_Tier0()->FindParm( "-script_debug" ) != 0;
		if ( debug )
			m_pEngineCtx->SetExceptionCallback( asMETHOD( ScriptSolid, ExceptionCallback ), this, asCALL_THISCALL );

		m_getGuiData.setContext( m_pEngineCtx );
		m_guiClosed.setContext( m_pEngineCtx );
		m_createMapSolid.setContext( m_pEngineCtx );

		m_getGuiData.setObject( instance );
		m_guiClosed.setObject( instance );
		m_createMapSolid.setObject( instance );

		m_getGuiData.addref();
		m_guiClosed.addref();
		m_createMapSolid.addref();
	}

	~ScriptSolid()
	{
		m_pEngineCtx->Unprepare();

		m_getGuiData.release();
		m_guiClosed.release();
		m_createMapSolid.release();

		if ( auto eng = m_pScriptInstance->GetEngine() )
			eng->ReturnContext( m_pEngineCtx );
		m_pScriptInstance->Release();
	}

	enum GuiElement_t : uint32 // needs to be 32-bit for AS
	{
		Label,
		TextBox,
		Divider
	};

	struct GUIData
	{
		GuiElement_t element;
		ScriptString text;
		union
		{
			int64 defaultValI;
			double defaultValD;
		};
		bool dbl;

		GUIData( GuiElement_t el, const ScriptString& text, int64 def )
			: element( el ), text( text ), defaultValI( def ), dbl( false ) { Assert( el == TextBox ); }

		GUIData( GuiElement_t el, const ScriptString& text, double def )
			: element( el ), text( text ), defaultValD( def ), dbl( true ) { Assert( el == TextBox ); }

		GUIData( GuiElement_t el, const ScriptString& text )
			: element( el ), text( text ), defaultValI( 0 ), dbl( false ) { Assert( el != Divider ); }

		GUIData( GuiElement_t el )
			: element( el ), defaultValI( 0 ), dbl( false ) { Assert( el == Divider ); }
	};

	bool ShowGui();

	CMapClass* CreateMapSolid( const BoundBox* box, TextureAlignment_t align )
	{
		if ( auto solid = m_createMapSolid( box, align ); !m_createMapSolid.failed() )
			return solid;
		return nullptr;
	}

	const ScriptString& Name() const { return m_name; }

private:
	ScriptString		m_name;
	asIScriptContext*	m_pEngineCtx;
	asIScriptObject*	m_pScriptInstance;

	void ExceptionCallback( asIScriptContext* ctx )
	{
		const Color c{ 32, 32, 255, 0 };
		ConColorMsg( c, "%s", GetExceptionInfo( ctx, true ).Get() );

		auto eng = ctx->GetEngine();

		const auto& printType = [c, eng, strType = eng->GetTypeIdByDecl( "string" ),
									vecType  = eng->GetTypeIdByDecl( "Vector" ),
									vec2Type = eng->GetTypeIdByDecl( "Vector2D" ),
									vec4Type = eng->GetTypeIdByDecl( "Vector4D" ),
									angType = eng->GetTypeIdByDecl( "QAngle" )]( const char* decl, void* addr, int id, const auto& printT ) -> void
		{
			switch ( id )
			{
			case asTYPEID_BOOL:
				ConColorMsg( c, "%s = %d\n", decl, *static_cast<bool*>( addr ) );
				break;
			case asTYPEID_INT8:
				ConColorMsg( c, "%s = %d\n", decl, *static_cast<int8*>( addr ) );
				break;
			case asTYPEID_INT16:
				ConColorMsg( c, "%s = %d\n", decl, *static_cast<int16*>( addr ) );
				break;
			case asTYPEID_INT32:
				ConColorMsg( c, "%s = %d\n", decl, *static_cast<int32*>( addr ) );
				break;
			case asTYPEID_INT64:
				ConColorMsg( c, "%s = %lld\n", decl, *static_cast<int64*>( addr ) );
				break;
			case asTYPEID_UINT8:
				ConColorMsg( c, "%s = %u\n", decl, *static_cast<uint8*>( addr ) );
				break;
			case asTYPEID_UINT16:
				ConColorMsg( c, "%s = %u\n", decl, *static_cast<uint16*>( addr ) );
				break;
			case asTYPEID_UINT32:
				ConColorMsg( c, "%s = %u\n", decl, *static_cast<uint32*>( addr ) );
				break;
			case asTYPEID_UINT64:
				ConColorMsg( c, "%s = %llu\n", decl, *static_cast<uint64*>( addr ) );
				break;
			case asTYPEID_FLOAT:
				ConColorMsg( c, "%s = %g\n", decl, *static_cast<float*>( addr ) );
				break;
			case asTYPEID_DOUBLE:
				ConColorMsg( c, "%s = %g\n", decl, *static_cast<double*>( addr ) );
				break;
			default:
				if ( id == strType )
					ConColorMsg( c, "%s = %s\n", decl, static_cast<ScriptString*>( addr )->Get() );
				else if ( id == vecType || id == angType )
					ConColorMsg( c, "%s = [%g %g %g]\n", decl, XYZ( *static_cast<Vector*>( addr ) ) );
				else if ( id == vec4Type )
					ConColorMsg( c, "%s = [%g %g %g %g]\n", decl, XYZ( *static_cast<Vector4D*>( addr ) ), static_cast<Vector4D*>( addr )->w );
				else if ( id == vec2Type )
					ConColorMsg( c, "%s = [%g %g]\n", decl, static_cast<Vector2D*>( addr )->x, static_cast<Vector2D*>( addr )->y );
				else
				{
					auto type = eng->GetTypeInfoById( id );

					ConColorMsg( c, "%s = 0x%x\n", decl, addr );
					if ( addr == nullptr )
						break;
					if ( V_stristr( type->GetName(), "array" ) == type->GetName() )
					{
						auto arrDecl = decl + V_strlen( decl );
						while ( *( --arrDecl - 1 ) != ' ' );
						auto arr = strchr( decl, '@' ) + 2 == arrDecl ? *static_cast<CScriptArray**>( addr ) : static_cast<CScriptArray*>( addr );
						auto elId = arr->GetElementTypeId();
						for ( size_t i = 0; i < arr->GetSize(); i++ )
							printT( CFmtStr( "%s[%u]", arrDecl, i ), arr->At( i ), elId, printT );
					}
					else if ( !V_stricmp( type->GetName(), "dictionary" ) )
					{
						auto dictDecl = decl + V_strlen( decl );
						while ( *( --dictDecl - 1 ) != ' ' );
						auto dict = strchr( decl, '@' ) + 2 == dictDecl ? *static_cast<CScriptDictionary**>( addr ) : static_cast<CScriptDictionary*>( addr );

						for ( const CScriptDictionary::CIterator& iter : *dict )
							printT( CFmtStr( "%s[\"%s\"]", dictDecl, iter.GetKey().Get() ), const_cast<void*>( iter.GetAddressOfValue() ), iter.GetTypeId(), printT );
					}
				}
				break;
			}
		};

		if ( auto thisId = ctx->GetThisTypeId(); thisId > 0 )
		{
			auto _this = ctx->GetThisPointer();
			auto thisType = eng->GetTypeInfoById( thisId );

			ConColorMsg( c, "\n-- this --\n" );
			for ( size_t i = 0; i < thisType->GetPropertyCount(); i++ )
			{
				auto decl = thisType->GetPropertyDeclaration( i );
				int  id, offset;
				thisType->GetProperty( i, nullptr, &id, nullptr, nullptr, &offset );

				printType( decl, static_cast<byte*>( _this ) + offset, id, printType );
			}
		}

		ConColorMsg( c, "-- locals --\n" );
		for ( int i = 0; i < ctx->GetVarCount(); i++ )
		{
			auto decl = ctx->GetVarDeclaration( i );
			auto addr = ctx->GetAddressOfVar( i );
			auto id = ctx->GetVarTypeId( i );

			if ( !ctx->IsVarInScope( i ) )
				continue;

			printType( decl, addr, id, printType );
		}
	}

	ASBind::FunctionPtr<CScriptArrayT<ScriptSolid::GUIData>*()> m_getGuiData;
	ASBind::FunctionPtr<bool( const CScriptDictionary* )> m_guiClosed;
	ASBind::FunctionPtr<CMapClass*( const BoundBox*, TextureAlignment_t )> m_createMapSolid;
};

class ScriptSolidGui : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( ScriptSolidGui, vgui::EditablePanel );

public:
	ScriptSolidGui( CScriptArrayT<ScriptSolid::GUIData>* pData ) : vgui::EditablePanel( nullptr, "ScriptGui" )
	{
		auto sizer = new vgui::CBoxSizer( vgui::ESLD_VERTICAL );
		for ( size_t i = 0; i < pData->GetSize(); i++ )
		{
			auto& data = *pData->At( i );

			switch ( data.element )
			{
			case ScriptSolid::Label:
				{
					auto label = new vgui::Label( this, "", data.text );
					label->SetContentAlignment( vgui::Label::a_center );
					label->SetCenterWrap( true );
					sizer->AddPanel( label, vgui::SizerAddArgs_t() );
					break;
				}
			case ScriptSolid::TextBox:
				{
					auto row = new vgui::CBoxSizer( vgui::ESLD_HORIZONTAL );

					auto label = new vgui::Label( this, "", data.text );
					row->AddPanel( label, vgui::SizerAddArgs_t().Padding( 0 ) );

					row->AddSpacer( vgui::SizerAddArgs_t().Expand( 1.0f ).Padding( 0 ) );

					auto entry = new vgui::TextEntry( this, "" );
					entry->SetAllowNumericInputOnly( true );
					if ( data.dbl )
					{
						char value[24] = { 0 };
						std::to_chars( value, value + ARRAYSIZE( value ), data.defaultValD );
						entry->SetText( value );
					}
					else if ( data.defaultValI )
						entry->SetText( CNumStr( data.defaultValI ) );

					row->AddPanel( entry, vgui::SizerAddArgs_t().Padding( 0 ).MinX( 48 ) );

					m_entries.Insert( data.text, entry );

					sizer->AddSizer( row, vgui::SizerAddArgs_t().Padding( 3 ) );
				}
				break;
			case ScriptSolid::Divider:
				sizer->AddSpacer( vgui::SizerAddArgs_t().Padding( 5 ) );
				break;
			default:
				Assert( 0 );
				break;
			};
		}

		auto row = new vgui::CBoxSizer( vgui::ESLD_HORIZONTAL );

		row->AddSpacer( vgui::SizerAddArgs_t().Expand( 0.5f ).Padding( 4 ) );
		row->AddPanel( new vgui::Button( this, "cancel", "Cancel", this, "Cancel" ), vgui::SizerAddArgs_t().Padding( 0 ) );
		row->AddSpacer( vgui::SizerAddArgs_t().Padding( 4 ) );
		row->AddPanel( new vgui::Button( this, "ok", "Create", this, "OK" ), vgui::SizerAddArgs_t().Padding( 0 ) );

		sizer->AddSpacer( vgui::SizerAddArgs_t().Expand( 1.0f ) );
		sizer->AddSizer( row, vgui::SizerAddArgs_t().Padding( 5 ) );
		sizer->AddSpacer( vgui::SizerAddArgs_t().Padding( 3 ) );

		SetSizer( sizer );
		InvalidateLayout( true );
	}

	~ScriptSolidGui() = default;

	const CUtlHashDict<vgui::TextEntry*>& Entries() const { return m_entries; }

	void OnCommand( const char* cmd ) override
	{
		GetParent()->OnCommand( cmd );
	}

private:
	CUtlHashDict<vgui::TextEntry*> m_entries;
};

class CScriptDialog : public CVguiDialog
{
	DECLARE_DYNAMIC( CScriptDialog )

private:
	class CScriptDialogPanel : public vgui::EditablePanel
	{
		DECLARE_CLASS_SIMPLE( CScriptDialogPanel, vgui::EditablePanel );
	public:
		CScriptDialogPanel( CScriptDialog* pBrowser, const char* panelName, vgui::HScheme hScheme ) :
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
		CScriptDialog* m_pBrowser;
	};

public:
	CScriptDialog( CWnd* pParent, CScriptArrayT<ScriptSolid::GUIData>* pData ) : CVguiDialog( CScriptDialog::IDD, pParent )
	{
		m_pDialog = new ScriptSolidGui( pData );
	}
	~CScriptDialog() override
	{
		// CDialog isn't going to clean up its vgui children
		delete m_pDialog;
	}

	// Dialog Data
	enum { IDD = IDD_SCRIPT_GUI };

protected:
	void DoDataExchange( CDataExchange* pDX ) override
	{
		CDialog::DoDataExchange( pDX );
	}
	BOOL PreTranslateMessage( MSG* pMsg ) override
	{
		// don't filter dialog message
		return CWnd::PreTranslateMessage( pMsg );
	}

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnSize( UINT nType, int cx, int cy )
	{
		if ( nType == SIZE_MINIMIZED || !IsWindow( m_VGuiWindow.m_hWnd ) )
			return CDialog::OnSize( nType, cx, cy );

		Resize();

		CDialog::OnSize(nType, cx, cy);
	}

	afx_msg BOOL OnEraseBkgnd( CDC* )
	{
		return TRUE;
	}

	BOOL OnInitDialog() override
	{
		CDialog::OnInitDialog();

		m_VGuiWindow.Create( NULL, _T("ScriptGui"), WS_VISIBLE|WS_CHILD, CRect(0,0,100,100), this, IDD_SCRIPT_GUI );

		vgui::EditablePanel* pMainPanel = new CScriptDialogPanel( this, "ScriptGui", HammerVGui()->GetHammerScheme() );

		m_VGuiWindow.SetParentWindow( &m_VGuiWindow );
		m_VGuiWindow.SetMainPanel( pMainPanel );
		pMainPanel->MakePopup( false, false );
		m_VGuiWindow.SetRepaintInterval( 30 );

		m_pDialog->SetParent( pMainPanel );
		m_pDialog->AddActionSignalTarget( pMainPanel );
		pMainPanel->InvalidateLayout( true );

		Resize();

		return TRUE;
	}

	void Resize()
	{
		// reposition controls
		CRect rect;
		GetClientRect( &rect );

		m_VGuiWindow.MoveWindow( rect );

		m_pDialog->SetBounds( 0, 0, rect.Width(), rect.Height() );
	}

	CVGuiPanelWnd	m_VGuiWindow;

	ScriptSolidGui*	m_pDialog;

	void Show()
	{
		if ( m_pDialog )
			m_pDialog->SetVisible( true );
	}
};
IMPLEMENT_DYNAMIC( CScriptDialog, CVguiDialog )

BEGIN_MESSAGE_MAP(CScriptDialog, CDialog)
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

bool ScriptSolid::ShowGui()
{
	auto data = m_getGuiData();
	if ( m_getGuiData.failed() )
		return false;
	if ( !data || data->IsEmpty() )
		return true;

	CScriptDialog dialog( GetMainWnd(), data );
	dialog.Show();
	if ( dialog.DoModal() != IDOK )
		return false;

	auto& entries = dialog.m_pDialog->Entries();

	auto dict = CScriptDictionary::Create( m_pEngineCtx->GetEngine() );

	for ( auto i = entries.First(); entries.IsValidIndex( i ); i = entries.Next( i ) )
	{
		char _value[64];
		entries.Element( i )->GetText( _value, 64 );

		if ( V_strrchr( _value, '.' ) )
			dict->Set( entries.GetElementName( i ), V_atod( _value ) );
		else
			dict->Set( entries.GetElementName( i ), V_atoi64( _value ) );
	}

	return m_guiClosed( dict ) && !m_guiClosed.failed();
}

ASBIND_TYPE( ScriptSolid::GuiElement_t, GuiElement_t );
ASBIND_TYPE( ScriptSolid::GUIData, GUIData );
ASBIND_ARRAY_TYPE( CScriptArrayT<ScriptSolid::GUIData>, GUIData );

static CUtlVector<ScriptSolid*> scriptSolids;
static void RegisterScriptSolid( const ScriptString& name, asIScriptObject* solidClass )
{
	scriptSolids.AddToTail( new ScriptSolid( name, solidClass ) );
}

void ScriptInit()
{
	asPrepareMultithread();
	engine = asCreateScriptEngine();
	engine->SetMessageCallback( asFUNCTION( msgCallback ), nullptr, asCALL_CDECL );
	engine->SetEngineProperty( asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, 1 );
	engine->SetEngineProperty( asEP_BUILD_WITHOUT_LINE_CUES, 1 );
	engine->SetEngineProperty( asEP_COMPILER_WARNINGS, 1 );
	engine->SetDefaultAccessMask( 0xFFFFFFFF );

	RegisterScriptMath( engine );
	RegisterScriptArray( engine, true );
	RegisterString( engine );
	RegisterScriptDictionary( engine );
	RegisterScriptGrid( engine );
	RegisterExceptionRoutines( engine );

	ASBind::Class<Vector2D, ASBind::class_pod_allfloats>{ engine, asOBJ_APP_CLASS_MORE_CONSTRUCTORS }
		.member( &Vector2D::x, "x" )
		.member( &Vector2D::y, "y" )
		.constructor<void()>()
		.constructor<void( float, float )>()
		.constructor<void( const Vector2D& )>()
		.constmethod( &Vector2D::operator[], "opIndex" )
		.method<vec_t&( Vector2D::* )( int )>( &Vector2D::operator[], "opIndex" )
		.method( &Vector2D::Init, "Init" )
		.method( &Vector2D::IsValid, "IsValid" )
		.method( &Vector2D::Random, "Random" )
		.method( &Vector2D::Negate, "Negate" )
		.method( &Vector2D::Length, "Length" )
		.method( &Vector2D::LengthSqr, "LengthSqr" )
		.method( &Vector2D::IsZero, "IsZero" )
		.method( &Vector2D::NormalizeInPlace, "NormalizeInPlace" )
		.method( &Vector2D::IsLengthGreaterThan, "IsLengthGreaterThan" )
		.method( &Vector2D::IsLengthLessThan, "IsLengthLessThan" )
		.method( &Vector2D::DistTo, "DistTo" )
		.method( &Vector2D::DistToSqr, "DistToSqr" )
		.method( &Vector2D::MulAdd, "MulAdd" )
		.method( &Vector2D::Dot, "Dot" )
		.method( &Vector2D::Min, "Min" )
		.method( &Vector2D::Max, "Max" )
		.method( &Vector2D::operator==, "opEquals" )
		.method<Vector2D&( Vector2D::* )( const Vector2D& )>( &Vector2D::operator+=, "opAddAssign" )
		.method<Vector2D&( Vector2D::* )( const Vector2D& )>( &Vector2D::operator-=, "opSubAssign" )
		.method<Vector2D&( Vector2D::* )( const Vector2D& )>( &Vector2D::operator*=, "opMulAssign" )
		.method<Vector2D&( Vector2D::* )( float )>( &Vector2D::operator*=, "opMulAssign" )
		.method<Vector2D&( Vector2D::* )( const Vector2D& )>( &Vector2D::operator/=, "opDivAssign" )
		.method<Vector2D&( Vector2D::* )( float )>( &Vector2D::operator/=, "opDivAssign" )
		.method( &Vector2D::operator=, "opAssign" )
		.constmethod<Vector2D, Vector2D>( &Vector2D::operator-, "opNeg" )
		.constmethod<Vector2D, Vector2D, const Vector2D&>( &Vector2D::operator+, "opAdd" )
		.constmethod<Vector2D, Vector2D, const Vector2D&>( &Vector2D::operator-, "opSub" )
		.constmethod<Vector2D, Vector2D, const Vector2D&>( &Vector2D::operator*, "opMul" )
		.constmethod<Vector2D, Vector2D, const Vector2D&>( &Vector2D::operator/, "opDiv" )
		.constmethod<Vector2D, Vector2D, float>( &Vector2D::operator*, "opMul" )
		.constmethod<Vector2D, Vector2D, float>( &Vector2D::operator/, "opDiv" )
		;

	ASBind::Class<Vector, ASBind::class_pod_allfloats>{ engine, asOBJ_APP_CLASS_MORE_CONSTRUCTORS }
		.member( &Vector::x, "x" )
		.member( &Vector::y, "y" )
		.member( &Vector::z, "z" )
		.constructor<void()>()
		.constructor<void( float, float, float )>()
		.constructor<void( float )>()
		.constmethod( &Vector::operator[], "opIndex" )
		.method<vec_t&( Vector::* )( int )>( &Vector::operator[], "opIndex" )
		.method( &Vector::Init, "Init" )
		.method( &Vector::IsValid, "IsValid" )
		.method( &Vector::Invalidate, "Invalidate" )
		.method( &Vector::Random, "Random" )
		.method( &Vector::Zero, "Zero" )
		.method( &Vector::NormalizeInPlace, "NormalizeInPlace" )
		.method( &Vector::Normalized, "Normalized" )
		.method( &Vector::Negate, "Negate" )
		.method( &Vector::Length, "Length" )
		.method( &Vector::Length2D, "Length2D" )
		.method( &Vector::Length2DSqr, "Length2DSqr" )
		.method( &Vector::LengthSqr, "LengthSqr" )
		.method( &Vector::IsZero, "IsZero" )
		.method( &Vector::IsLengthGreaterThan, "IsLengthGreaterThan" )
		.method( &Vector::IsLengthLessThan, "IsLengthLessThan" )
		.method( &Vector::WithinAABox, "WithinAABox" )
		.method( &Vector::DistTo, "DistTo" )
		.method( &Vector::DistToSqr, "DistToSqr" )
		.method( &Vector::MulAdd, "MulAdd" )
		.method( &Vector::Dot, "Dot" )
		.method( &Vector::Cross, "Cross" )
		.method( &Vector::Min, "Min" )
		.method( &Vector::Max, "Max" )
		.constmethod( &Vector::AsVector2D, "AsVector2D" )
		.method<Vector2D&( Vector::* )()>( &Vector::AsVector2D, "AsVector2D" )
		.method( &Vector::operator==, "opEquals" )
		.method<Vector&( Vector::* )( const Vector& )>( &Vector::operator+=, "opAddAssign" )
		.method<Vector&( Vector::* )( float )>( &Vector::operator+=, "opAddAssign" )
		.method<Vector&( Vector::* )( const Vector& )>( &Vector::operator-=, "opSubAssign" )
		.method<Vector&( Vector::* )( float )>( &Vector::operator-=, "opSubAssign" )
		.method<Vector&( Vector::* )( const Vector& )>( &Vector::operator*=, "opMulAssign" )
		.method<Vector&( Vector::* )( float )>( &Vector::operator*=, "opMulAssign" )
		.method<Vector&( Vector::* )( const Vector& )>( &Vector::operator/=, "opDivAssign" )
		.method<Vector&( Vector::* )( float )>( &Vector::operator/=, "opDivAssign" )
		.method( &Vector::operator=, "opAssign" )
		.constmethod<Vector, Vector>( &Vector::operator-, "opNeg" )
		.constmethod<Vector, Vector, const Vector&>( &Vector::operator+, "opAdd" )
		.constmethod<Vector, Vector, const Vector&>( &Vector::operator-, "opSub" )
		.constmethod<Vector, Vector, const Vector&>( &Vector::operator*, "opMul" )
		.constmethod<Vector, Vector, const Vector&>( &Vector::operator/, "opDiv" )
		.constmethod<Vector, Vector, float>( &Vector::operator*, "opMul" )
		.constmethod<Vector, Vector, float>( &Vector::operator/, "opDiv" )
		;

	ASBind::Class<QAngle, ASBind::class_pod_allfloats>{ engine, asOBJ_APP_CLASS_MORE_CONSTRUCTORS }
		.member( &QAngle::x, "x" )
		.member( &QAngle::y, "y" )
		.member( &QAngle::z, "z" )
		.constructor<void()>()
		.constructor<void( float, float, float )>()
		.method( &QAngle::Init, "Init" )
		.method( &QAngle::Random, "Random" )
		.method( &QAngle::IsValid, "IsValid" )
		.method( &QAngle::Invalidate, "Invalidate" )
		.method( &QAngle::Length, "Length" )
		.method( &QAngle::LengthSqr, "LengthSqr" )
		.constmethod( &QAngle::operator[], "opIndex" )
		.method<vec_t&( QAngle::* )( int )>( &QAngle::operator[], "opIndex" )
		.method( &QAngle::operator==, "opEquals" )
		.method( &QAngle::operator=, "opAssign" )
		.method( &QAngle::operator+=, "opAddAssign" )
		.method( &QAngle::operator-=, "opSubAssign" )
		.method( &QAngle::operator*=, "opMulAssign" )
		.method( &QAngle::operator/=, "opDivAssign" )
		.constmethod<QAngle, QAngle>( &QAngle::operator-, "opNeg" )
		.constmethod<QAngle, QAngle, const QAngle&>( &QAngle::operator+, "opAdd" )
		.constmethod<QAngle, QAngle, const QAngle&>( &QAngle::operator-, "opSub" )
		.constmethod<QAngle, QAngle, float>( &QAngle::operator*, "opMul" )
		.constmethod<QAngle, QAngle, float>( &QAngle::operator/, "opDiv" )
		;

	ASBind::Class<Vector4D, ASBind::class_pod_allfloats>{ engine, asOBJ_APP_CLASS_MORE_CONSTRUCTORS }
		.member( &Vector4D::x, "x" )
		.member( &Vector4D::y, "y" )
		.member( &Vector4D::z, "z" )
		.member( &Vector4D::w, "w" )
		.constructor<void()>()
		.constructor<void( float, float, float, float )>()
		.constructor<void( const Vector&, float )>()
		.constructor<void( const Vector4D& )>()
		.method<void( Vector4D::* )( float, float, float, float )>( &Vector4D::Init, "Init" )
		.method<void( Vector4D::* )( const Vector&, float )>( &Vector4D::Init, "Init" )
		.method( &Vector4D::IsValid, "IsValid" )
		.method( &Vector4D::Negate, "Negate" )
		.method( &Vector4D::Length, "Length" )
		.method( &Vector4D::LengthSqr, "LengthSqr" )
		.method( &Vector4D::IsZero, "IsZero" )
		.method( &Vector4D::DistTo, "DistTo" )
		.method( &Vector4D::DistToSqr, "DistToSqr" )
		.method( &Vector4D::MulAdd, "MulAdd" )
		.method( &Vector4D::Dot, "Dot" )
		.constmethod( &Vector4D::operator[], "opIndex" )
		.method<vec_t&( Vector4D::* )( int )>( &Vector4D::operator[], "opIndex" )
		.constmethod( &Vector4D::AsVector3D, "AsVector3D" )
		.method<Vector&( Vector4D::* )()>( &Vector4D::AsVector3D, "AsVector3D" )
		.constmethod( &Vector4D::AsVector2D, "AsVector2D" )
		.method<Vector2D&( Vector4D::* )()>( &Vector4D::AsVector2D, "AsVector2D" )
		.method( &Vector4D::Random, "Random" )
		.method( &Vector4D::operator==, "opEquals" )
		.method<Vector4D&( Vector4D::* )( const Vector4D& )>( &Vector4D::operator+=, "opAddAssign" )
		.method<Vector4D&( Vector4D::* )( const Vector4D& )>( &Vector4D::operator-=, "opSubAssign" )
		.method<Vector4D&( Vector4D::* )( const Vector4D& )>( &Vector4D::operator*=, "opMulAssign" )
		.method<Vector4D&( Vector4D::* )( float )>( &Vector4D::operator*=, "opMulAssign" )
		.method<Vector4D&( Vector4D::* )( const Vector4D& )>( &Vector4D::operator/=, "opDivAssign" )
		.method<Vector4D&( Vector4D::* )( float )>( &Vector4D::operator/=, "opDivAssign" )
		;

	ASBind::Class<VMatrix, ASBind::class_pod_allfloats>{ engine, asOBJ_APP_CLASS_MORE_CONSTRUCTORS }
		.constructor<void()>()
		.constructor<void( float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float )>()
		.constructor<void( const Vector&, const Vector&, const Vector& )>()
		.constructor<void( const Vector&, const Vector&, const Vector&, const Vector& )>()
		.method<void ( VMatrix::* )( float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float )>( &VMatrix::Init, "Init" )
		.method( &WrapAccessVMatrix, "opIndex" )
		.constmethod( &WrapAccessVMatrixConst, "opIndex" )
		.method( &VMatrix::SetLeft, "SetLeft" )
		.method( &VMatrix::SetUp, "SetUp" )
		.method( &VMatrix::SetForward, "SetForward" )
		.method( &VMatrix::GetBasisVectors, "GetBasisVectors" )
		.method( &VMatrix::SetBasisVectors, "SetBasisVectors" )
		.method( &VMatrix::SetTranslation, "SetTranslation" )
		.method( &VMatrix::PreTranslate, "PreTranslate" )
		.method( &VMatrix::PostTranslate, "PostTranslate" )
		.method( &VMatrix::GetLeft, "GetLeft" )
		.method( &VMatrix::GetUp, "GetUp" )
		.method( &VMatrix::GetForward, "GetForward" )
		.method<Vector ( VMatrix::* )() const>( &VMatrix::GetTranslation, "GetTranslation" )
		.method( &VMatrix::ApplyRotation, "ApplyRotation" )
		.method( &VMatrix::VMul3x3, "VMul3x3" )
		.method( &VMatrix::VMul3x3Transpose, "VMul3x3Transpose" )
		.method( &VMatrix::VMul4x3, "VMul4x3" )
		.method( &VMatrix::VMul4x3Transpose, "VMul4x3Transpose" )
		.method( &VMatrix::MatrixMul, "MatrixMul" )
		.method( &VMatrix::Identity, "Identity" )
		.method( &VMatrix::IsIdentity, "IsIdentity" )
		.method( &VMatrix::SetupMatrixOrgAngles, "SetupMatrixOrgAngles" )
		.method( &VMatrix::SetupMatrixAngles, "SetupMatrixAngles" )
		.method( &VMatrix::InverseGeneral, "InverseGeneral" )
		.method<VMatrix ( VMatrix::* )() const>( &VMatrix::InverseTR, "InverseTR" )
		.method( &VMatrix::IsRotationMatrix, "IsRotationMatrix" )
		.method( &VMatrix::GetScale, "GetScale" )
		.method( &VMatrix::Scale, "Scale" )
		.method( &VMatrix::NormalizeBasisVectors, "NormalizeBasisVectors" )
		.method( &VMatrix::Transpose, "Transpose" )
		.method( &VMatrix::Transpose3x3, "Transpose3x3" )
		;

	ASBind::Class<BoundBox, ASBind::class_nocount>{ engine }
		.constmember( &BoundBox::bmaxs, "maxs" )
		.constmember( &BoundBox::bmins, "mins" )
		.constmethod( &BoundBox::GetBoundsCenter, "GetBoundsCenter" )
		.constmethod( &BoundBox::ContainsPoint, "ContainsPoint" )
		.constmethod( &BoundBox::GetBoundsSize, "GetBoundsSize" )
		;

#define __ENUM( n ) .add( #n, n )

	ASBind::Enum{ engine, "TextureAlignment_t" }
		__ENUM( TEXTURE_ALIGN_NONE )
		.all_flags<TextureAlignment_t>()
		;

	ASBind::Class<CMapFace, ASBind::class_class>{ engine }
		.constructor<void()>()
		.destructor()
		.method( &WrapCreateFace, "CreateFace" )
		;

	ASBind::Enum{ engine, "InitTexFlags_t" }
		__ENUM( INIT_TEXTURE_FORCE )
		__ENUM( INIT_TEXTURE_AXES )
		__ENUM( INIT_TEXTURE_ROTATION )
		__ENUM( INIT_TEXTURE_SHIFT )
		__ENUM( INIT_TEXTURE_SCALE )
		__ENUM( INIT_TEXTURE_ALL )
		;

#undef __ENUM

	ASBind::Class<CMapClass, ASBind::class_nocount>{ engine }
		;

	ASBind::Class<CMapGroup, ASBind::class_nocount>{ engine }
		.factory( &WrapCreateMapClass<CMapGroup> )
		.method( &CMapGroup::AddChild, "AddChild" )
		.method( &WrapCalcBounds<CMapGroup>, "CalcBounds" )
		.method( &CMapClass::GetBoundsSize, "GetBoundsSize" )
		.method( &CMapClass::TransMove, "TransMove" )
		.method( &CMapClass::TransRotate, "TransRotate" )
		.method( &CMapClass::TransScale, "TransScale" )
		.refcast( &WrapCastMapClass<CMapGroup>, true )
		;

	ASBind::Class<CMapSolid, ASBind::class_nocount>{ engine }
		.factory( &WrapCreateMapClass<CMapSolid> )
		.method( &WrapAddFace, "AddFace" )
		.method( &CMapSolid::InitializeTextureAxes, "InitializeTextureAxes" )
		.method( &WrapCalcBounds<CMapSolid>, "CalcBounds" )
		.method( &CMapClass::GetBoundsSize, "GetBoundsSize" )
		.method( &CMapClass::TransMove, "TransMove" )
		.method( &CMapClass::TransRotate, "TransRotate" )
		.method( &CMapClass::TransScale, "TransScale" )
		.refcast( &WrapCastMapClass<CMapSolid>, true )
		;

	ASBind::Enum{ engine, "GuiElement_t" }.all<ScriptSolid::GuiElement_t>();

	ASBind::Class<ScriptSolid::GUIData, ASBind::class_pod>{ engine }
		.constructor<void( ScriptSolid::GuiElement_t, const ScriptString&, int64 )>()
		.constructor<void( ScriptSolid::GuiElement_t, const ScriptString&, double )>()
		.constructor<void( ScriptSolid::GuiElement_t, const ScriptString& )>()
		.constructor<void( ScriptSolid::GuiElement_t )>()
		;

	ASBind::Interface{ engine, "ScriptSolid" }
		.constmethod<CScriptArrayT<ScriptSolid::GUIData>*()>( "GetGuiData" )
		.method<bool( const CScriptDictionary* )>( "GuiClosed" )
		.method<CMapClass*( const BoundBox*, TextureAlignment_t )>( "CreateMapSolid" )
		;

	ASBind::Global{ engine }
		.constvar( vec3_origin, "vec3_origin" )
		.constvar( vec3_invalid, "vec3_invalid" )
		.constvar( vec3_angle, "vec3_angle" )
		.constvar( vec4_origin, "vec4_origin" )
		.function( &Print, "print" )
		.function2( &RegisterScriptSolid, "void RegisterScriptSolid( const string &in name, ScriptSolid@ instance )" )
		;

	CScriptBuilder builder;
	asIScriptContext* ctx = nullptr;
	FileFindHandle_t h = 0;
	const char* find = g_pFullFileSystem->FindFirstEx( "scripts/*.as", "hammer", &h );

	while ( find )
	{
		const CFmtStr fullScript( "scripts/%s", find );

		if ( !ctx )
			ctx = engine->RequestContext();
		builder.StartNewModule( engine, find );
		builder.AddSectionFromFile( fullScript );
		builder.BuildModule();
		asIScriptModule* module = builder.GetModule();

		auto reg = ASBind::CreateFunctionPtr<void()>( "RegisterCallback", module );
		if ( reg.isValid() )
		{
			reg.setContext( ctx );
			reg();
			modules.AddToTail( module );
		}
		else
		{
			Warning( "Script '%s' doesn't have RegisterCallback function. Destroying!\n", find );
			module->Discard();
		}

		find = g_pFullFileSystem->FindNext( h );
	}

	g_pFullFileSystem->FindClose( h );

	if ( ctx )
		engine->ReturnContext( ctx );

#ifdef DEBUG
	if ( CommandLine_Tier0()->FindParm( "-doc" ) )
	{
		DocumentationGenerator doc{ engine, { "scripts/api_reference.html", true, true, false, false, false, true, true, true, true, "0.1", "Hammer", "AS Interface" } };
		doc.Generate();
	}
#endif
}

void ScriptShutdown()
{
	for ( auto& solid : scriptSolids )
		delete solid;

	for ( auto module : modules )
		module->Discard();
	modules.Purge();

	engine->ShutDownAndRelease();
	asUnprepareMultithread();
}

static void SetDefTexture( CMapClass* pThis )
{
	if ( pThis->IsMapClass( MAPCLASS_TYPE( CMapGroup ) ) )
	{
		auto& children = *pThis->GetChildren();
		for ( CMapClass* c : children )
			SetDefTexture( c );
		return;
	}
	Assert( pThis->IsMapClass( MAPCLASS_TYPE( CMapSolid ) ) );
	static_cast<CMapSolid*>( pThis )->SetTexture( GetDefaultTextureName() );
}

CMapClass* ScriptableSolid_Create( int index, const BoundBox* box )
{
	if ( !scriptSolids.IsValidIndex( index ) )
		return nullptr;
	auto solid = scriptSolids[index];
	if ( !solid->ShowGui() )
		return nullptr;
	CMapClass* ret = solid->CreateMapSolid( box, Options.GetTextureAlignment() );
	if ( ret )
		SetDefTexture( ret );

	// return new solid
	return ret;
}

int ScriptableSolid_Count()
{
	return scriptSolids.Count();
}

void ScriptableSolid_GetNames( CUtlVector<CString>& names )
{
	for ( auto solid : scriptSolids )
	{
		auto& name = solid->Name();
		names.AddToTail( name.String(), name.Length() );
	}
}

int ScriptableSolid_Find( const char* text )
{
	return scriptSolids.FindMatch( [text]( const ScriptSolid* pSolid ) { return pSolid->Name().IsEqual_CaseInsensitive( text ); } );
}