#pragma once

#include "tier1/utlbuffer.h"
#include "tier1/utlstring.h"

#include "angelscript.h"

BEGIN_AS_NAMESPACE

// Compare relation between two objects of the same type
int CompareRelation( asIScriptEngine* engine, void* lobj, void* robj, int typeId, int& result );

// Compare equality between two objects of the same type
int CompareEquality( asIScriptEngine* engine, void* lobj, void* robj, int typeId, bool& result );

// Compile and execute simple statements
// The module is optional. If given the statements can access the entities compiled in the module.
// The caller can optionally provide its own context, for example if a context should be reused.
int ExecuteString( asIScriptEngine* engine, const char* code, asIScriptModule* mod = nullptr, asIScriptContext* ctx = nullptr );

// Compile and execute simple statements with option of return value
// The module is optional. If given the statements can access the entitites compiled in the module.
// The caller can optionally provide its own context, for example if a context should be reused.
int ExecuteString( asIScriptEngine* engine, const char* code, void* ret, int retTypeId, asIScriptModule* mod = nullptr, asIScriptContext* ctx = nullptr );

// Format the details of the script exception into a human readable text
CUtlString GetExceptionInfo( asIScriptContext* ctx, bool showStack = false );

// Register the exception routines
//  'void throw(const string &msg)'
//  'string getExceptionInfo()'
void RegisterExceptionRoutines( asIScriptEngine* engine );

END_AS_NAMESPACE