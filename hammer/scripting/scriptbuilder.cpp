#include "scriptbuilder.h"

#include "tier1/utlbuffer.h"
#include "filesystem.h"

#include "tier0/memdbgon.h"

using namespace std;

BEGIN_AS_NAMESPACE

CScriptBuilder::CScriptBuilder()
{
	engine = nullptr;
	module = nullptr;

	includeCallback = nullptr;
	includeParam = nullptr;

	pragmaCallback = nullptr;
	pragmaParam = nullptr;
}

void CScriptBuilder::SetIncludeCallback( INCLUDECALLBACK_t callback, void* userParam )
{
	includeCallback = callback;
	includeParam = userParam;
}

void CScriptBuilder::SetPragmaCallback( PRAGMACALLBACK_t callback, void* userParam )
{
	pragmaCallback = callback;
	pragmaParam = userParam;
}

bool CScriptBuilder::StartNewModule( asIScriptEngine* inEngine, const char* moduleName )
{
	if ( !inEngine )
		return false;

	engine = inEngine;
	module = inEngine->GetModule( moduleName, asGM_ALWAYS_CREATE );
	if ( !module )
		return false;

	ClearAll();

	return true;
}

asIScriptEngine* CScriptBuilder::GetEngine()
{
	return engine;
}

asIScriptModule* CScriptBuilder::GetModule()
{
	return module;
}

unsigned int CScriptBuilder::GetSectionCount() const
{
	return (unsigned int)includedScripts.size();
}

string CScriptBuilder::GetSectionName( unsigned int idx ) const
{
	if ( idx >= includedScripts.size() )
		return {};

	auto it = includedScripts.begin();
	while ( idx-- > 0 )
		++it;
	return *it;
}

// Returns 1 if the section was included
// Returns 0 if the section was not included because it had already been included before
// Returns <0 if there was an error
int CScriptBuilder::AddSectionFromFile( const char* filename )
{
	// The file name stored in the set should be the fully resolved name because
	// it is possible to name the same file in multiple ways using relative paths.
	char fullpath[MAX_PATH];
	g_pFullFileSystem->RelativePathToFullPath_safe( filename, "hammer", fullpath );

	if ( IncludeIfNotAlreadyIncluded( fullpath ) )
	{
		int r = LoadScriptSection( filename );
		if ( r < 0 )
			return r;
		else
			return 1;
	}

	return 0;
}

// Returns 1 if the section was included
// Returns 0 if the section was not included because it had already been included before
// Returns <0 if there was an error
int CScriptBuilder::AddSectionFromMemory( const char* sectionName, const char* scriptCode, unsigned int scriptLength, int lineOffset )
{
	if ( IncludeIfNotAlreadyIncluded( sectionName ) )
	{
		int r = ProcessScriptSection( scriptCode, scriptLength, sectionName, lineOffset );
		if ( r < 0 )
			return r;
		else
			return 1;
	}

	return 0;
}

int CScriptBuilder::BuildModule()
{
	return Build();
}

void CScriptBuilder::DefineWord( const char* word )
{
	string sword = word;
	if ( definedWords.find( sword ) == definedWords.end() )
		definedWords.insert( sword );
}

void CScriptBuilder::ClearAll()
{
	includedScripts.clear();

#if AS_PROCESS_METADATA
	currentClass.clear();
	currentNamespace.clear();

	foundDeclarations.clear();
	typeMetadataMap.clear();
	funcMetadataMap.clear();
	varMetadataMap.clear();
#endif
}

bool CScriptBuilder::IncludeIfNotAlreadyIncluded( const char* filename )
{
	string scriptFile = filename;
	if ( includedScripts.find( scriptFile ) != includedScripts.end() )
	{
		// Already included
		return false;
	}

	// Add the file to the set of included sections
	includedScripts.insert( scriptFile );

	return true;
}

int CScriptBuilder::LoadScriptSection( const char* filename )
{
	// Read the script file
	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( filename, "hammer", buf ) )
	{
		using namespace std::string_literals;
		// Write a message to the engine's message callback
		char fullpath[MAX_PATH];
		g_pFullFileSystem->RelativePathToFullPath_safe( filename, "hammer", fullpath );
		string msg = "Failed to open script file '"s + fullpath + '\'';
		engine->WriteMessage( filename, 0, 0, asMSGTYPE_ERROR, msg.c_str() );

		// TODO: Write the file where this one was included from

		return -1;
	}

	// Process the script section even if it is zero length so that the name is registered
	return ProcessScriptSection( (const char*)buf.Base(), (unsigned int)buf.TellPut(), filename, 0 );
}

int CScriptBuilder::ProcessScriptSection( const char* script, unsigned int length, const char* sectionname, int lineOffset )
{
	vector<string> includes;

	// Perform a superficial parsing of the script first to store the metadata
	if ( length )
		modifiedScript.assign( script, length );
	else
		modifiedScript = script;

	// First perform the checks for #if directives to exclude code that shouldn't be compiled
	unsigned int pos = 0;
	int nested = 0;
	while ( pos < modifiedScript.size() )
	{
		asUINT len = 0;
		asETokenClass t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
		if ( t == asTC_UNKNOWN && modifiedScript[pos] == '#' && ( pos + 1 < modifiedScript.size() ) )
		{
			int start = pos++;

			// Is this an #if directive?
			t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );

			string token;
			token.assign( &modifiedScript[pos], len );

			pos += len;

			if ( token == "if" )
			{
				t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
				if ( t == asTC_WHITESPACE )
				{
					pos += len;
					t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
				}

				if ( t == asTC_IDENTIFIER )
				{
					const string word( &modifiedScript[pos], len );

					// Overwrite the #if directive with space characters to avoid compiler error
					pos += len;
					OverwriteCode( start, pos - start );

					// Has this identifier been defined by the application or not?
					if ( definedWords.find( word ) == definedWords.end() )
						// Exclude all the code until and including the #endif
						pos = ExcludeCode( pos );
					else
						nested++;
				}
			}
			else if ( token == "endif" )
			{
				// Only remove the #endif if there was a matching #if
				if ( nested > 0 )
				{
					OverwriteCode( start, pos - start );
					nested--;
				}
			}
		}
		else
			pos += len;
	}

#if AS_PROCESS_METADATA
	// Preallocate memory
	string name, declaration;
	vector<string> metadata;
	declaration.reserve( 100 );
#endif

	// Then check for meta data and #include directives
	pos = 0;
	while ( pos < modifiedScript.size() )
	{
		asUINT len = 0;
		asETokenClass t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
		if ( t == asTC_COMMENT || t == asTC_WHITESPACE )
		{
			pos += len;
			continue;
		}

#if AS_PROCESS_METADATA
		// Check if class
		if ( currentClass.empty() && modifiedScript.substr( pos, len ) == "class" )
		{
			// Get the identifier after "class"
			do
			{
				pos += len;
				if ( pos >= modifiedScript.size() )
				{
					t = asTC_UNKNOWN;
					break;
				}
				t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
			} while ( t == asTC_COMMENT || t == asTC_WHITESPACE );

			if ( t == asTC_IDENTIFIER )
			{
				currentClass = modifiedScript.substr( pos, len );

				// Search until first { or ; is encountered
				while ( pos < modifiedScript.length() )
				{
					engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );

					// If start of class section encountered stop
					if ( modifiedScript[pos] == '{' )
					{
						pos += len;
						break;
					}
					else if ( modifiedScript[pos] == ';' )
					{
						// The class declaration has ended and there are no children
						currentClass.clear();
						pos += len;
						break;
					}

					// Check next symbol
					pos += len;
				}
			}

			continue;
		}

		// Check if end of class
		if ( !currentClass.empty() && modifiedScript[pos] == '}' )
		{
			currentClass.clear();
			pos += len;
			continue;
		}

		// Check if namespace
		if ( modifiedScript.substr( pos, len ) == "namespace" )
		{
			// Get the identifier after "namespace"
			do
			{
				pos += len;
				t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
			} while ( t == asTC_COMMENT || t == asTC_WHITESPACE );

			if ( !currentNamespace.empty() )
				currentNamespace += "::";
			currentNamespace += modifiedScript.substr( pos, len );

			// Search until first { is encountered
			while ( pos < modifiedScript.length() )
			{
				engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );

				// If start of namespace section encountered stop
				if ( modifiedScript[pos] == '{' )
				{
					pos += len;
					break;
				}

				// Check next symbol
				pos += len;
			}

			continue;
		}

		// Check if end of namespace
		if ( !currentNamespace.empty() && modifiedScript[pos] == '}' )
		{
			size_t found = currentNamespace.rfind( "::" );
			if ( found != string::npos )
				currentNamespace.erase( found );
			else
				currentNamespace.clear();
			pos += len;
			continue;
		}

		// Is this the start of metadata?
		if ( modifiedScript[pos] == '[' )
		{
			// Get the metadata string
			pos = ExtractMetadata( pos, metadata );

			// Determine what this metadata is for
			int type;
			ExtractDeclaration( pos, name, declaration, type );

			// Store away the declaration in a map for lookup after the build has completed
			if ( type > 0 )
			{
				foundDeclarations.emplace_back( metadata, name, declaration, type, currentClass, currentNamespace );
			}
		}
		else
#endif
			// Is this a preprocessor directive?
			if ( modifiedScript[pos] == '#' && ( pos + 1 < modifiedScript.size() ) )
			{
				int start = pos++;

				t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
				if ( t == asTC_IDENTIFIER )
				{
					const string token( &modifiedScript[pos], len );
					if ( token == "include" )
					{
						pos += len;
						t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
						if ( t == asTC_WHITESPACE )
						{
							pos += len;
							t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
						}

						if ( t == asTC_VALUE && len > 2 && ( modifiedScript[pos] == '"' || modifiedScript[pos] == '\'' ) )
						{
							// Get the include file

							// Store it for later processing
							includes.emplace_back( &modifiedScript[pos + 1], len - 2 );

							pos += len;

							// Overwrite the include directive with space characters to avoid compiler error
							OverwriteCode( start, pos - start );
						}
					}
					else if ( token == "pragma" )
					{
						// Read until the end of the line
						pos += len;
						for ( ; pos < modifiedScript.size() && modifiedScript[pos] != '\n'; pos++ );

						// Call the pragma callback
						string pragmaText( &modifiedScript[start + 7], pos - start - 7 );
						int r = pragmaCallback ? pragmaCallback( pragmaText, *this, pragmaParam ) : -1;
						if ( r < 0 )
						{
							// TODO: Report the correct line number
							engine->WriteMessage( sectionname, 0, 0, asMSGTYPE_ERROR, "Invalid #pragma directive" );
							return r;
						}

						// Overwrite the pragma directive with space characters to avoid compiler error
						OverwriteCode( start, pos - start );
					}
				}
			}
		// Don't search for metadata/includes within statement blocks or between tokens in statements
			else
			{
				pos = SkipStatement( pos );
			}
	}

	// Build the actual script
	engine->SetEngineProperty( asEP_COPY_SCRIPT_SECTIONS, true );
	module->AddScriptSection( sectionname, modifiedScript.c_str(), modifiedScript.size(), lineOffset );

	if ( !includes.empty() )
	{
		// If the callback has been set, then call it for each included file
		if ( includeCallback )
		{
			for ( size_t n = 0; n < includes.size(); n++ )
			{
				int r = includeCallback( includes[n].c_str(), sectionname, this, includeParam );
				if ( r < 0 )
					return r;
			}
		}
		else
		{
			// By default we try to load the included file from the relative directory of the current file

			// Determine the path of the current script so that we can resolve relative paths for includes
			string path = sectionname;
			size_t posOfSlash = path.find_last_of( "/\\" );
			if ( posOfSlash != string::npos )
				path.resize( posOfSlash + 1 );
			else
				path.clear();

			// Load the included scripts
			for ( size_t n = 0; n < includes.size(); n++ )
			{
				// If the include is a relative path, then prepend the path of the originating script
				if ( includes[n].find_first_of( "/\\" ) != 0 && includes[n].find_first_of( ":" ) == string::npos )
				{
					includes[n] = path + includes[n];
				}

				// Include the script section
				int r = AddSectionFromFile( includes[n].c_str() );
				if ( r < 0 )
					return r;
			}
		}
	}

	return 0;
}

int CScriptBuilder::Build()
{
	int r = module->Build();
	if ( r < 0 )
		return r;

#if AS_PROCESS_METADATA
	// After the script has been built, the metadata strings should be
	// stored for later lookup by function id, type id, and variable index
	for ( int n = 0; n < (int)foundDeclarations.size(); n++ )
	{
		SMetadataDecl* decl = &foundDeclarations[n];
		module->SetDefaultNamespace( decl->nameSpace.c_str() );
		if ( decl->type == MDT_TYPE )
		{
			// Find the type id
			int typeId = module->GetTypeIdByDecl( decl->declaration.c_str() );
			Assert( typeId >= 0 );
			if ( typeId >= 0 )
				typeMetadataMap.insert( { typeId, decl->metadata } );
		}
		else if ( decl->type == MDT_FUNC )
		{
			if ( decl->parentClass.empty() )
			{
				// Find the function id
				asIScriptFunction* func = module->GetFunctionByDecl( decl->declaration.c_str() );
				Assert( func );
				if ( func )
					funcMetadataMap.insert( { func->GetId(), decl->metadata } );
			}
			else
			{
				// Find the method id
				int typeId = module->GetTypeIdByDecl( decl->parentClass.c_str() );
				Assert( typeId > 0 );
				auto it = classMetadataMap.find( typeId );
				if ( it == classMetadataMap.end() )
				{
					classMetadataMap.insert( { typeId, SClassMetadata( decl->parentClass ) } );
					it = classMetadataMap.find( typeId );
				}

				asITypeInfo* type = engine->GetTypeInfoById( typeId );
				asIScriptFunction* func = type->GetMethodByDecl( decl->declaration.c_str() );
				Assert( func );
				if ( func )
					it->second.funcMetadataMap.insert( { func->GetId(), decl->metadata } );
			}
		}
		else if ( decl->type == MDT_VIRTPROP )
		{
			if ( decl->parentClass.empty() )
			{
				// Find the global virtual property accessors
				asIScriptFunction* func = module->GetFunctionByName( ( "get_" + decl->declaration ).c_str() );
				if ( func )
					funcMetadataMap.insert( { func->GetId(), decl->metadata } );
				func = module->GetFunctionByName( ( "set_" + decl->declaration ).c_str() );
				if ( func )
					funcMetadataMap.insert( { func->GetId(), decl->metadata } );
			}
			else
			{
				// Find the method virtual property accessors
				int typeId = module->GetTypeIdByDecl( decl->parentClass.c_str() );
				Assert( typeId > 0 );
				auto it = classMetadataMap.find( typeId );
				if ( it == classMetadataMap.end() )
				{
					classMetadataMap.insert( { typeId, SClassMetadata( decl->parentClass ) } );
					it = classMetadataMap.find( typeId );
				}

				asITypeInfo* type = engine->GetTypeInfoById( typeId );
				asIScriptFunction* func = type->GetMethodByName( ( "get_" + decl->declaration ).c_str() );
				if ( func )
					it->second.funcMetadataMap.insert( { func->GetId(), decl->metadata } );
				func = type->GetMethodByName( ( "set_" + decl->declaration ).c_str() );
				if ( func )
					it->second.funcMetadataMap.insert( { func->GetId(), decl->metadata } );
			}
		}
		else if ( decl->type == MDT_VAR )
		{
			if ( decl->parentClass.empty() )
			{
				// Find the global variable index
				int varIdx = module->GetGlobalVarIndexByName( decl->declaration.c_str() );
				Assert( varIdx >= 0 );
				if ( varIdx >= 0 )
					varMetadataMap.insert( { varIdx, decl->metadata } );
			}
			else
			{
				int typeId = module->GetTypeIdByDecl( decl->parentClass.c_str() );
				Assert( typeId > 0 );

				// Add the classes if needed
				auto it = classMetadataMap.find( typeId );
				if ( it == classMetadataMap.end() )
				{
					classMetadataMap.insert( { typeId, SClassMetadata( decl->parentClass ) } );
					it = classMetadataMap.find( typeId );
				}

				// Add the variable to class
				asITypeInfo* objectType = engine->GetTypeInfoById( typeId );
				int idx = -1;

				// Search through all properties to get proper declaration
				for ( asUINT i = 0; i < objectType->GetPropertyCount(); ++i )
				{
					const char* name;
					objectType->GetProperty( i, &name );
					if ( decl->declaration == name )
					{
						idx = i;
						break;
					}
				}

				// If found, add it
				Assert( idx >= 0 );
				if ( idx >= 0 )
					it->second.varMetadataMap.insert( { idx, decl->metadata } );
			}
		}
		else if ( decl->type == MDT_FUNC_OR_VAR )
		{
			if ( decl->parentClass.empty() )
			{
				// Find the global variable index
				int varIdx = module->GetGlobalVarIndexByName( decl->name.c_str() );
				if ( varIdx >= 0 )
					varMetadataMap.insert( { varIdx, decl->metadata } );
				else
				{
					asIScriptFunction* func = module->GetFunctionByDecl( decl->declaration.c_str() );
					Assert( func );
					if ( func )
						funcMetadataMap.insert( { func->GetId(), decl->metadata } );
				}
			}
			else
			{
				int typeId = module->GetTypeIdByDecl( decl->parentClass.c_str() );
				Assert( typeId > 0 );

				// Add the classes if needed
				auto it = classMetadataMap.find( typeId );
				if ( it == classMetadataMap.end() )
				{
					classMetadataMap.insert( { typeId, SClassMetadata( decl->parentClass ) } );
					it = classMetadataMap.find( typeId );
				}

				// Add the variable to class
				asITypeInfo* objectType = engine->GetTypeInfoById( typeId );
				int idx = -1;

				// Search through all properties to get proper declaration
				for ( asUINT i = 0; i < objectType->GetPropertyCount(); ++i )
				{
					const char* name;
					objectType->GetProperty( i, &name );
					if ( decl->name == name )
					{
						idx = i;
						break;
					}
				}

				// If found, add it
				if ( idx >= 0 )
					it->second.varMetadataMap.insert( { idx, decl->metadata } );
				else
				{
					// Look for the matching method instead
					asITypeInfo* type = engine->GetTypeInfoById( typeId );
					asIScriptFunction* func = type->GetMethodByDecl( decl->declaration.c_str() );
					Assert( func );
					if ( func )
						it->second.funcMetadataMap.insert( { func->GetId(), decl->metadata } );
				}
			}
		}
	}
	module->SetDefaultNamespace( "" );
#endif

	return 0;
}

int CScriptBuilder::SkipStatement( int pos )
{
	asUINT len = 0;

	// Skip until ; or { whichever comes first
	while ( pos < (int)modifiedScript.length() && modifiedScript[pos] != ';' && modifiedScript[pos] != '{' )
	{
		engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
		pos += len;
	}

	// Skip entire statement block
	if ( pos < (int)modifiedScript.length() && modifiedScript[pos] == '{' )
	{
		pos += 1;

		// Find the end of the statement block
		int level = 1;
		while ( level > 0 && pos < (int)modifiedScript.size() )
		{
			asETokenClass t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
			if ( t == asTC_KEYWORD )
			{
				if ( modifiedScript[pos] == '{' )
					level++;
				else if ( modifiedScript[pos] == '}' )
					level--;
			}

			pos += len;
		}
	}
	else
		pos += 1;

	return pos;
}

// Overwrite all code with blanks until the matching #endif
int CScriptBuilder::ExcludeCode( int pos )
{
	asUINT len = 0;
	int nested = 0;
	while ( pos < (int)modifiedScript.size() )
	{
		engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
		if ( modifiedScript[pos] == '#' )
		{
			modifiedScript[pos] = ' ';
			pos++;

			// Is it an #if or #endif directive?
			engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
			const string token( &modifiedScript[pos], len );
			OverwriteCode( pos, len );

			if ( token == "if" )
			{
				nested++;
			}
			else if ( token == "endif" )
			{
				if ( nested-- == 0 )
				{
					pos += len;
					break;
				}
			}
		}
		else if ( modifiedScript[pos] != '\n' )
		{
			OverwriteCode( pos, len );
		}
		pos += len;
	}

	return pos;
}

// Overwrite all characters except line breaks with blanks
void CScriptBuilder::OverwriteCode( int start, int len )
{
	char* code = &modifiedScript[start];
	for ( int n = 0; n < len; n++ )
	{
		if ( *code != '\n' )
			*code = ' ';
		code++;
	}
}

#if AS_PROCESS_METADATA
int CScriptBuilder::ExtractMetadata( int pos, vector<string>& metadata )
{
	metadata.clear();

	// Extract all metadata. They can be separated by whitespace and comments
	for ( ;;)
	{
		string metadataString = {};

		// Overwrite the metadata with space characters to allow compilation
		modifiedScript[pos] = ' ';

		// Skip opening brackets
		pos += 1;

		int level = 1;
		asUINT len = 0;
		while ( level > 0 && pos < (int)modifiedScript.size() )
		{
			asETokenClass t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
			if ( t == asTC_KEYWORD )
			{
				if ( modifiedScript[pos] == '[' )
					level++;
				else if ( modifiedScript[pos] == ']' )
					level--;
			}

			// Copy the metadata to our buffer
			if ( level > 0 )
				metadataString.append( &modifiedScript[pos], len );

			// Overwrite the metadata with space characters to allow compilation
			if ( t != asTC_WHITESPACE )
				OverwriteCode( pos, len );

			pos += len;
		}

		metadata.emplace_back( std::move( metadataString ) );

		// Check for more metadata. Possibly separated by comments
		asETokenClass t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
		while ( t == asTC_COMMENT || t == asTC_WHITESPACE )
		{
			pos += len;
			t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
		}

		if ( modifiedScript[pos] != '[' )
			break;
	}

	return pos;
}

int CScriptBuilder::ExtractDeclaration( int pos, string& name, string& declaration, int& type )
{
	declaration.clear();
	type = 0;

	int start = pos;

	std::string token;
	asUINT len = 0;
	asETokenClass t = asTC_WHITESPACE;

	// Skip white spaces, comments, and leading decorators
	do
	{
		pos += len;
		t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
		token.assign( &modifiedScript[pos], len );
	} while ( t == asTC_WHITESPACE || t == asTC_COMMENT ||
		token == "private" || token == "protected" ||
		token == "shared" || token == "external" ||
		token == "final" || token == "abstract" );

	// We're expecting, either a class, interface, function, or variable declaration
	if ( t == asTC_KEYWORD || t == asTC_IDENTIFIER )
	{
		token.assign( &modifiedScript[pos], len );
		if ( token == "interface" || token == "class" || token == "enum" )
		{
			// Skip white spaces and comments
			do
			{
				pos += len;
				t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
			} while ( t == asTC_WHITESPACE || t == asTC_COMMENT );

			if ( t == asTC_IDENTIFIER )
			{
				type = MDT_TYPE;
				declaration.assign( &modifiedScript[pos], len );
				pos += len;
				return pos;
			}
		}
		else
		{
			// For function declarations, store everything up to the start of the
			// statement block, except for succeeding decorators (final, override, etc)

			// For variable declaration store just the name as there can only be one

			// We'll only know if the declaration is a variable or function declaration
			// when we see the statement block, or absense of a statement block.
			bool hasParenthesis = false;
			int nestedParenthesis = 0;
			declaration.append( &modifiedScript[pos], len );
			pos += len;
			for ( ; pos < (int)modifiedScript.size();)
			{
				t = engine->ParseToken( &modifiedScript[pos], modifiedScript.size() - pos, &len );
				token.assign( &modifiedScript[pos], len );
				if ( t == asTC_KEYWORD )
				{
					if ( token == "{" && nestedParenthesis == 0 )
					{
						if ( hasParenthesis )
						{
							// We've found the end of a function signature
							type = MDT_FUNC;
						}
						else
						{
							// We've found a virtual property. Just keep the name
							declaration = name;
							type = MDT_VIRTPROP;
						}
						return pos;
					}
					if ( ( token == "=" && !hasParenthesis ) || token == ";" )
					{
						if ( hasParenthesis )
						{
							// The declaration is ambigous. It can be a variable with initialization, or a function prototype
							type = MDT_FUNC_OR_VAR;
						}
						else
						{
							// Substitute the declaration with just the name
							declaration = name;
							type = MDT_VAR;
						}
						return pos;
					}
					else if ( token == "(" )
					{
						nestedParenthesis++;

						// This is the first parenthesis we encounter. If the parenthesis isn't followed
						// by a statement block, then this is a variable declaration, in which case we
						// should only store the type and name of the variable, not the initialization parameters.
						hasParenthesis = true;
					}
					else if ( token == ")" )
					{
						nestedParenthesis--;
					}
				}
				else if ( t == asTC_IDENTIFIER )
				{
					name = token;
				}

				// Skip trailing decorators
				if ( !hasParenthesis || nestedParenthesis > 0 || t != asTC_IDENTIFIER || ( token != "final" && token != "override" ) )
					declaration += token;

				pos += len;
			}
		}
	}

	return start;
}

vector<string> CScriptBuilder::GetMetadataForType( int typeId )
{
	auto it = typeMetadataMap.find( typeId );
	if ( it != typeMetadataMap.end() )
		return it->second;

	return {};
}

vector<string> CScriptBuilder::GetMetadataForFunc( asIScriptFunction* func )
{
	if ( func )
	{
		auto it = funcMetadataMap.find( func->GetId() );
		if ( it != funcMetadataMap.end() )
			return it->second;
	}

	return {};
}

vector<string> CScriptBuilder::GetMetadataForVar( int varIdx )
{
	auto it = varMetadataMap.find( varIdx );
	if ( it != varMetadataMap.end() )
		return it->second;

	return {};
}

vector<string> CScriptBuilder::GetMetadataForTypeProperty( int typeId, int varIdx )
{
	auto typeIt = classMetadataMap.find( typeId );
	if ( typeIt == classMetadataMap.end() )
		return {};

	auto propIt = typeIt->second.varMetadataMap.find( varIdx );
	if ( propIt == typeIt->second.varMetadataMap.end() )
		return {};

	return propIt->second;
}

vector<string> CScriptBuilder::GetMetadataForTypeMethod( int typeId, asIScriptFunction* method )
{
	if ( method )
	{
		auto typeIt = classMetadataMap.find( typeId );
		if ( typeIt == classMetadataMap.end() )
			return {};

		auto methodIt = typeIt->second.funcMetadataMap.find( method->GetId() );
		if ( methodIt == typeIt->second.funcMetadataMap.end() )
			return {};

		return methodIt->second;
	}

	return {};
}
#endif

END_AS_NAMESPACE