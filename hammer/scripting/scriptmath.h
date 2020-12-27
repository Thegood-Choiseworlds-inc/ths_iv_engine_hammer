#pragma once

#include "angelscript.h"

BEGIN_AS_NAMESPACE

// This function will determine the configuration of the engine
// and use one of the two functions below to register the math functions
void RegisterScriptMath( asIScriptEngine* engine );

END_AS_NAMESPACE