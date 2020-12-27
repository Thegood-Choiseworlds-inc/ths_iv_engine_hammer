#pragma once

//---------------------------
// Compilation settings
//

// Set this flag to turn on/off metadata processing
//  0 = off
//  1 = on
#ifndef AS_PROCESS_METADATA
#define AS_PROCESS_METADATA 0
#endif

// TODO: Implement flags for turning on/off include directives and conditional programming



//---------------------------
// Declaration
//

#include "angelscript.h"

#include "robin_hood.h"
#undef _tzcnt_u32
#include <string>
#include <vector>

BEGIN_AS_NAMESPACE

class CScriptBuilder;

// This callback will be called for each #include directive encountered by the
// builder. The callback should call the AddSectionFromFile or AddSectionFromMemory
// to add the included section to the script. If the include cannot be resolved
// then the function should return a negative value to abort the compilation.
using INCLUDECALLBACK_t = int ( * )( const char* include, const char* from, CScriptBuilder* builder, void* userParam );

// This callback will be called for each #pragma directive encountered by the builder.
// The application can interpret the pragmaText and decide what do to based on that.
// If the callback returns a negative value the builder will report an error and abort the compilation.
using PRAGMACALLBACK_t = int( * )( const std::string& pragmaText, CScriptBuilder& builder, void* userParam );

// Helper class for loading and pre-processing script files to
// support include directives and metadata declarations
class CScriptBuilder
{
public:
	CScriptBuilder();

	// Start a new module
	bool StartNewModule( asIScriptEngine* engine, const char* moduleName );

	// Load a script section from a file on disk
	// Returns  1 if the file was included
	//          0 if the file had already been included before
	//         <0 on error
	int AddSectionFromFile( const char* filename );

	// Load a script section from memory
	// Returns  1 if the section was included
	//          0 if a section with the same name had already been included before
	//         <0 on error
	int AddSectionFromMemory( const char* sectionName, const char* scriptCode, unsigned int scriptLength = 0, int lineOffset = 0 );

	// Build the added script sections
	int BuildModule();

	// Returns the engine
	asIScriptEngine* GetEngine();

	// Returns the current module
	asIScriptModule* GetModule();

	// Register the callback for resolving include directive
	void SetIncludeCallback( INCLUDECALLBACK_t callback, void* userParam );

	// Register the callback for resolving pragma directive
	void SetPragmaCallback( PRAGMACALLBACK_t callback, void* userParam );

	// Add a pre-processor define for conditional compilation
	void DefineWord( const char* word );

	// Enumerate included script sections
	unsigned int GetSectionCount() const;
	std::string  GetSectionName( unsigned int idx ) const;

#if AS_PROCESS_METADATA
	// Get metadata declared for classes, interfaces, and enums
	std::vector<std::string> GetMetadataForType( int typeId );

	// Get metadata declared for functions
	std::vector<std::string> GetMetadataForFunc( asIScriptFunction* func );

	// Get metadata declared for global variables
	std::vector<std::string> GetMetadataForVar( int varIdx );

	// Get metadata declared for class variables
	std::vector<std::string> GetMetadataForTypeProperty( int typeId, int varIdx );

	// Get metadata declared for class methods
	std::vector<std::string> GetMetadataForTypeMethod( int typeId, asIScriptFunction* method );
#endif

protected:
	void ClearAll();
	int  Build();
	int  ProcessScriptSection( const char* script, unsigned int length, const char* sectionname, int lineOffset );
	int  LoadScriptSection( const char* filename );
	bool IncludeIfNotAlreadyIncluded( const char* filename );

	int  SkipStatement( int pos );

	int  ExcludeCode( int start );
	void OverwriteCode( int start, int len );

	asIScriptEngine*	engine;
	asIScriptModule*	module;
	std::string			modifiedScript;

	INCLUDECALLBACK_t	includeCallback;
	void*				includeParam;

	PRAGMACALLBACK_t	pragmaCallback;
	void*				pragmaParam;

#if AS_PROCESS_METADATA
	int  ExtractMetadata( int pos, std::vector<std::string>& outMetadata );
	int  ExtractDeclaration( int pos, std::string& outName, std::string& outDeclaration, int& outType );

	enum METADATATYPE
	{
		MDT_TYPE = 1,
		MDT_FUNC = 2,
		MDT_VAR = 3,
		MDT_VIRTPROP = 4,
		MDT_FUNC_OR_VAR = 5
	};

	// Temporary structure for storing metadata and declaration
	struct SMetadataDecl
	{
		SMetadataDecl( std::vector<std::string> m, std::string n, std::string d, int t, std::string c, std::string ns ) : metadata( m ), name( n ), declaration( d ), type( t ), parentClass( c ), nameSpace( ns ) {}
		std::vector<std::string>	metadata;
		std::string					name;
		std::string					declaration;
		int							type;
		std::string					parentClass;
		std::string					nameSpace;
	};
	std::vector<SMetadataDecl>	foundDeclarations;
	std::string					currentClass;
	std::string					currentNamespace;

	// Storage of metadata for global declarations
	robin_hood::unordered_map<int, std::vector<std::string>> typeMetadataMap;
	robin_hood::unordered_map<int, std::vector<std::string>> funcMetadataMap;
	robin_hood::unordered_map<int, std::vector<std::string>> varMetadataMap;

	// Storage of metadata for class member declarations
	struct SClassMetadata
	{
		SClassMetadata( const std::string& aName ) : className( aName ) {}
		std::string className;
		robin_hood::unordered_map<int, std::vector<std::string>> funcMetadataMap;
		robin_hood::unordered_map<int, std::vector<std::string>> varMetadataMap;
	};
	robin_hood::unordered_map<int, SClassMetadata> classMetadataMap;
#endif

	robin_hood::unordered_flat_set<std::string>		includedScripts;
	robin_hood::unordered_flat_set<std::string>		definedWords;
};

END_AS_NAMESPACE