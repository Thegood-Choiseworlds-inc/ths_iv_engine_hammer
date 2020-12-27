
#include "scripthelper.h"
#include "scriptstring.h"

#include "tier0/dbg.h"
#include "tier1/strtools.h"

#include <cstdlib>
#include <set>
#include <string>

#include "tier0/memdbgon.h"

using namespace std;

BEGIN_AS_NAMESPACE

int CompareRelation( asIScriptEngine* engine, void* lobj, void* robj, int typeId, int& result )
{
	// TODO: If a lot of script objects are going to be compared, e.g. when sorting an array,
	//       then the method id and context should be cached between calls.

	int retval = -1;
	asIScriptFunction *func = 0;

	asITypeInfo* ti = engine->GetTypeInfoById( typeId );
	if ( ti )
	{
		// Check if the object type has a compatible opCmp method
		for ( asUINT n = 0; n < ti->GetMethodCount(); n++ )
		{
			asIScriptFunction* f = ti->GetMethodByIndex( n );
			asDWORD flags;
			if ( V_strcmp( f->GetName(), "opCmp" ) == 0 && f->GetReturnTypeId( &flags ) == asTYPEID_INT32 &&
				flags == asTM_NONE && f->GetParamCount() == 1 )
			{
				int paramTypeId;
				f->GetParam( 0, &paramTypeId, &flags );

				// The parameter must be an input reference of the same type
				// If the reference is a inout reference, then it must also be read-only
				if ( !( flags & asTM_INREF ) || typeId != paramTypeId || ( ( flags & asTM_OUTREF ) && !( flags & asTM_CONST ) ) )
					break;

				// Found the method
				func = f;
				break;
			}
		}
	}

	if ( func )
	{
		// Call the method
		asIScriptContext* ctx = engine->CreateContext();
		ctx->Prepare( func );
		ctx->SetObject( lobj );
		ctx->SetArgAddress( 0, robj );
		int r = ctx->Execute();
		if ( r == asEXECUTION_FINISHED )
		{
			result = (int)ctx->GetReturnDWord();

			// The comparison was successful
			retval = 0;
		}
		ctx->Release();
	}

	return retval;
}

int CompareEquality( asIScriptEngine* engine, void* lobj, void* robj, int typeId, bool& result )
{
	// TODO: If a lot of script objects are going to be compared, e.g. when searching for an
	//       entry in a set, then the method and context should be cached between calls.

	int retval = -1;
	asIScriptFunction* func = 0;

	asITypeInfo* ti = engine->GetTypeInfoById( typeId );
	if ( ti )
	{
		// Check if the object type has a compatible opEquals method
		for ( asUINT n = 0; n < ti->GetMethodCount(); n++ )
		{
			asIScriptFunction* f = ti->GetMethodByIndex( n );
			asDWORD flags;
			if ( V_strcmp( f->GetName(), "opEquals" ) == 0 && f->GetReturnTypeId( &flags ) == asTYPEID_BOOL &&
				flags == asTM_NONE && f->GetParamCount() == 1 )
			{
				int paramTypeId;
				f->GetParam( 0, &paramTypeId, &flags );

				// The parameter must be an input reference of the same type
				// If the reference is a inout reference, then it must also be read-only
				if ( !( flags & asTM_INREF ) || typeId != paramTypeId || ( ( flags & asTM_OUTREF ) && !( flags & asTM_CONST ) ) )
					break;

				// Found the method
				func = f;
				break;
			}
		}
	}

	if ( func )
	{
		// Call the method
		asIScriptContext* ctx = engine->CreateContext();
		ctx->Prepare( func );
		ctx->SetObject( lobj );
		ctx->SetArgAddress( 0, robj );
		int r = ctx->Execute();
		if ( r == asEXECUTION_FINISHED )
		{
			result = ctx->GetReturnByte() != 0;

			// The comparison was successful
			retval = 0;
		}
		ctx->Release();
	}
	else
	{
		// If the opEquals method doesn't exist, then we try with opCmp instead
		int relation;
		retval = CompareRelation( engine, lobj, robj, typeId, relation );
		if ( retval >= 0 )
			result = relation == 0;
	}

	return retval;
}

int ExecuteString( asIScriptEngine* engine, const char* code, asIScriptModule* mod, asIScriptContext* ctx )
{
	return ExecuteString( engine, code, 0, asTYPEID_VOID, mod, ctx );
}

int ExecuteString( asIScriptEngine* engine, const char* code, void* ref, int refTypeId, asIScriptModule* mod, asIScriptContext* ctx )
{
	// Wrap the code in a function so that it can be compiled and executed
	string funcCode = " ExecuteString() {\n";
	funcCode += code;
	funcCode += "\n;}";

	// Determine the return type based on the type of the ref arg
	funcCode = engine->GetTypeDeclaration( refTypeId, true ) + funcCode;

	// GetModule will free unused types, so to be on the safe side we'll hold on to a reference to the type
	asITypeInfo* type = nullptr;
	if ( refTypeId & asTYPEID_MASK_OBJECT )
	{
		type = engine->GetTypeInfoById( refTypeId );
		if ( type )
			type->AddRef();
	}

	// If no module was provided, get a dummy from the engine
	asIScriptModule* execMod = mod ? mod : engine->GetModule( "ExecuteString", asGM_ALWAYS_CREATE );

	// Now it's ok to release the type
	if ( type )
		type->Release();

	// Compile the function that can be executed
	asIScriptFunction* func = nullptr;
	int r = execMod->CompileFunction( "ExecuteString", funcCode.c_str(), -1, 0, &func );
	if ( r < 0 )
		return r;

	// If no context was provided, request a new one from the engine
	asIScriptContext* execCtx = ctx ? ctx : engine->RequestContext();
	r = execCtx->Prepare( func );
	if ( r >= 0 )
	{
		// Execute the function
		r = execCtx->Execute();

		// Unless the provided type was void retrieve it's value
		if ( ref != 0 && refTypeId != asTYPEID_VOID )
		{
			if ( refTypeId & asTYPEID_OBJHANDLE )
			{
				// Expect the pointer to be null to start with
				Assert( *reinterpret_cast<void**>( ref ) == 0 );
				*reinterpret_cast<void**>( ref ) = *reinterpret_cast<void**>( execCtx->GetAddressOfReturnValue() );
				engine->AddRefScriptObject( *reinterpret_cast<void**>( ref ), engine->GetTypeInfoById( refTypeId ) );
			}
			else if ( refTypeId & asTYPEID_MASK_OBJECT )
			{
				// Use the registered assignment operator to do a value assign.
				// This assumes that the ref is pointing to a valid object instance.
				engine->AssignScriptObject( ref, execCtx->GetAddressOfReturnValue(), engine->GetTypeInfoById( refTypeId ) );
			}
			else
			{
				// Copy the primitive value
				memcpy( ref, execCtx->GetAddressOfReturnValue(), engine->GetSizeOfPrimitiveType( refTypeId ) );
			}
		}
	}

	// Clean up
	func->Release();
	if ( !ctx )
		engine->ReturnContext( execCtx );

	return r;
}

CUtlString GetExceptionInfo( asIScriptContext* ctx, bool showStack )
{
	if ( ctx->GetState() != asEXECUTION_EXCEPTION )
		return {};

	CUtlBuffer text( 0, 0, CUtlBuffer::TEXT_BUFFER );

	const asIScriptFunction* function = ctx->GetExceptionFunction();
	text << "function:  " << function->GetDeclaration() << "\n";
	text << "module:    " << ( function->GetModuleName() ? function->GetModuleName() : "" ) << "\n";
	text << "section:   " << ( function->GetScriptSectionName() ? function->GetScriptSectionName() : "" ) << "\n";
	text << "line:      " << ctx->GetExceptionLineNumber() << "\n";
	text << "exception: " << ctx->GetExceptionString() << "\n";

	if ( showStack && ctx->GetCallstackSize() > 1 )
	{
		text << "--- call stack ---\n";
		for ( asUINT n = 0; n < ctx->GetCallstackSize(); n++ )
		{
			function = ctx->GetFunction( n );
			if ( function )
			{
				if ( function->GetFuncType() == asFUNC_SCRIPT )
				{
					text << ( function->GetScriptSectionName() ? function->GetScriptSectionName() : "" ) << " (" << ctx->GetLineNumber( n ) << "): " << function->GetDeclaration() << "\n";
				}
				else
				{
					// The context is being reused by the application for a nested call
					text << "{...application...}: " << function->GetDeclaration() << "\n";
				}
			}
			else
			{
				// The context is being reused by the script engine for a nested call
				text << "{...script engine...}\n";
			}
		}
	}

	return text.String();
}

void ScriptThrow( const ScriptString& msg )
{
	asIScriptContext* ctx = asGetActiveContext();
	if ( ctx )
		ctx->SetException( msg );
}

ScriptString ScriptGetExceptionInfo()
{
	asIScriptContext* ctx = asGetActiveContext();
	if ( !ctx )
		return {};

	const char* msg = ctx->GetExceptionString();
	if ( !msg )
		return {};

	return msg;
}

void RegisterExceptionRoutines( asIScriptEngine* engine )
{
	int r;

	// The string type must be available
	Assert( engine->GetTypeInfoByDecl( "string" ) );

	r = engine->RegisterGlobalFunction( "void throw(const string &in)", asFUNCTION( ScriptThrow ), asCALL_CDECL ); Assert( r >= 0 );
	r = engine->RegisterGlobalFunction( "string getExceptionInfo()", asFUNCTION( ScriptGetExceptionInfo ), asCALL_CDECL ); Assert( r >= 0 );
}

END_AS_NAMESPACE