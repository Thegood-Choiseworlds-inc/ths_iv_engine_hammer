
#ifdef _DEBUG

#include "tier0/dbg.h"

#include "tier1/utlbuffer.h"
#include "filesystem.h"

#include "robin_hood.h"

// C++
#include <sstream>
#include <iostream>
#include <algorithm>
#include <regex>
#include <set>
#include <vector>
#include <iomanip>
#include <string>
#include <string_view>

// C
#include <cstdarg>
#include <cstring>

// local
#include "docgen.h"

#include "tier0/memdbgon.h"

#if AS_GENERATE_DOCUMENTATION

static constexpr const std::string_view base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static FORCEINLINE bool is_base64( const unsigned char c ) { return ( isalnum( c ) || ( c == '+' ) || ( c == '/' ) ); }

size_t base64_encode( const void* pInput, unsigned int in_len, char* pOutput, const size_t nOutSize )
{
	const unsigned char* bytes_to_encode = static_cast<const unsigned char*>( pInput );
	CUtlVector<char> ret;
	ret.EnsureCapacity( 4 * ( ( in_len + 2 ) / 3 ) );
	int i = 0;
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];

	while ( in_len-- )
	{
		char_array_3[i++] = *( bytes_to_encode++ );
		if ( i == 3 )
		{
			char_array_4[0] = ( char_array_3[0] & 0xfc ) >> 2;
			char_array_4[1] = ( ( char_array_3[0] & 0x03 ) << 4 ) + ( ( char_array_3[1] & 0xf0 ) >> 4 );
			char_array_4[2] = ( ( char_array_3[1] & 0x0f ) << 2 ) + ( ( char_array_3[2] & 0xc0 ) >> 6 );
			char_array_4[3] = char_array_3[2] & 0x3f;

			for ( i = 0; ( i < 4 ); i++ )
				ret.AddToTail( base64_chars[char_array_4[i]] );
			i = 0;
		}
	}

	if ( i )
	{
		for ( int j = i; j < 3; j++ )
			char_array_3[j] = '\0';

		char_array_4[0] = ( char_array_3[0] & 0xfc ) >> 2;
		char_array_4[1] = ( ( char_array_3[0] & 0x03 ) << 4 ) + ( ( char_array_3[1] & 0xf0 ) >> 4 );
		char_array_4[2] = ( ( char_array_3[1] & 0x0f ) << 2 ) + ( ( char_array_3[2] & 0xc0 ) >> 6 );

		for ( int j = 0; ( j < i + 1 ); j++ )
			ret.AddToTail( base64_chars[char_array_4[j]] );

		while ( ( i++ < 3 ) )
			ret.AddToTail( '=' );
	}

	if ( nOutSize <= static_cast<size_t>( ret.Count() ) )
	{
		Warning( "Base64 encode: insufficient output buffer size\n" );
		memset( pOutput, 0, nOutSize );
		return 0;
	}

	memcpy( pOutput, ret.Base(), ret.Count() );
	pOutput[ret.Count()] = '\0';

	return ret.Count();
}
namespace
{
	constexpr const char HtmlStart[] = R"^(
<!doctype html>
<html>
<head>
	<title>%%PROJECT_NAME%%</title>
	<script type="text/javascript" src="https://code.jquery.com/jquery-3.5.1.min.js"></script>
	<style>
		* {
			box-sizing: border-box;
		}
		body {
			font-family: %%DEFAULT_FONT%%;
			font-size: %%DEFAULT_FONT_SIZE%%;
			background-color: %%MAIN_BACKGROUND_COLOR%%;
			color: %%MAIN_FOREGROUND_COLOR%%;
			margin: 0;
			padding: 0;
			display: grid;
			grid-template-columns: %%SUMMARY_WIDTH%% 1fr;
			grid-template-rows: auto 1fr;
			height: 100vh;
			overflow: hidden;
		}
		#header {
			font-size: 200%;
			font-weight: bold;
			background-color: %%HEADER_BACKGROUND_COLOR%%;
			color: %%HEADER_FOREGROUND_COLOR%%;
			border-bottom: %%HEADER_BORDER%%;
			padding: 20px;
			display: grid;
			grid-template-columns: auto 1fr;
			grid-area: 1/1/span 1/span 2;
		}
		#header #namecontainer {
			display: grid;
			grid-template-columns: auto 1fr;
			align-items: center;
		}
		#header #namecontainer img {
			padding-right: 10px;
		}
		#header #searchbox {
			align-self: center;
		}
		#header #searchbox > input {
			float: right;
			width: 400px;
		}
		#summary {
			border-right: %%HEADER_BORDER%%;
			padding: 20px;
			overflow: auto;
			grid-area: 2/1/span 1/span 1;
		}
		#content {
			padding: 20px;
			overflow: auto;
			grid-area: 2/2/span 1/span 1;
		}
		h1 {
			font-size: 150%;
			font-weight: bold;
			color: %%SUBHEADER_COLOR%%;
			margin: 0 0 5px 0;
		}
		h2 {
			font-size: 120%;
			text-decoration: underline;
			color: %%SUBHEADER_COLOR%%;
		}
		a, a:hover, a:visited {
			color: %%LINK_COLOR%%;
		}
		.api {
			font-family: %%API_FONT%%;
			font-size: %%API_FONT_SIZE%%;
			white-space: pre;
			tab-size: 20px;
		}
		.AS-keyword {
			font-weight: bold;
			color: %%AS_KEYWORD_COLOR%%;
		}
		.AS-number {
			font-weight: bold;
			color: %%AS_NUMBER_COLOR%%;
		}
		.AS-valuetype {
			font-weight: bold;
			color: %%AS_VALUETYPE_COLOR%%;
		}
		.AS-classtype {
			font-weight: bold;
			color: %%AS_CLASSTYPE_COLOR%%;
		}
		.AS-enumtype {
			font-weight: bold;
			color: %%AS_ENUMTYPE_COLOR%%;
		}
		.AS-enumvalue {
			font-weight: bold;
			color: %%AS_ENUMVALUE_COLOR%%;
		}
		.block {
			margin: 0px 10px 10px 20px;
			padding: 10px;
			border: %%BLOCK_BORDER%%;
			background-color: %%BLOCK_BACKGROUND_COLOR%%;
		}
		.documentation {
			width: 500px;
			padding: 10px 40px;
			font-style: italic;
		}
		.indented {
			padding-left: 20px;
		}
		.hidden {
			display: none;
		}
		.link {
			cursor: pointer;
			color: %%LINK_COLOR%%;
		}
		.link:hover {
			text-decoration: underline;
			color: %%LINK_COLOR%%;
		}
		.generated_on {
			padding-top: 10px;
			font-size: 80%;
			color: %%GENERATED_ON_COLOR%%;
		}

		%%ADDITIONAL_CSS%%
	</style>
	<script type="text/javascript">
		$(document).ready(() => {
			// handle search shortcut
			$(document).keydown((ev) => {
				if (ev.key == '%%SEARCH_HOTKEY%%' || ev.code == '%%SEARCH_HOTKEY%%')
					$('#searchbox input').focus();
			});

			// handle search, need to set same handler to multiple events, so wrap in
			// anonymous function so we don't litter the global namespace
			(() => {

				let handler = (ev) => {
					const
						v = ev.target.value.toLowerCase(),
						m = '[name' + (v.length ? '*=' + v : '') + ']',
						s = '#content '+m,
						h = '#content [name]:not('+m+')';
					$(h).hide();
					$(s).show().parents('[name]').show();
				};
				$('#searchbox input').keyup(handler);
				$('#searchbox input').change(handler);
			})();

			// handle collapsing/expanding the summary
			$('#summary div.expandable').click((ev)=>{
				let s = $(ev.target).parents('div.expandable').first(),
					t = s.siblings('div.indented');
				t.toggleClass('hidden');
				s.find('span').first().html(t.hasClass('hidden') ? '+' : '-');
			});

			// handle anchor jumps
			$('span[targetname]').click((ev)=>{
				let targetName = $(ev.target).attr('targetname'),
					target = $('a[name=' + targetName + ']').first();
				$('#content').animate({
					scrollTop: $('#content').scrollTop() + target.position().top - $('#header').innerHeight() - 5,
				}, 250);
			});

			// any additional javascript
			%%ADDITIONAL_JAVASCRIPT%%
		});
	</script>
</head>
<body>
	<div id="header">
		<div id="namecontainer">
			%%LOGO_IMAGE%%
			<span>%%PROJECT_NAME%% - %%DOCUMENTATION_NAME%% v%%INTERFACE_VERSION%%</span>
		</div>
		<div id="searchbox"><input type="text" class="api" placeholder="Search the documentation, hit %%SEARCH_HOTKEY%% to focus..."></div>
	</div>
)^";

	constexpr const char HtmlEnd[] = R"^(
</body>
</html>
)^";

	class DocumentationOutput
	{
	public:
		DocumentationOutput();
		void setVariable( const char* variable, const char* value );
		void setVariable( std::string variable, std::string value );
		void append( const char* text );
		void appendF( const char* format, ... );
		void appendRaw( const char* text );
		void appendRawF( const char* format, ... );
		int  writeToFile( const char* outputFile );
	private:
		std::string							output;
		robin_hood::unordered_flat_map<std::string, std::string>	m_variables;
		std::string							m_formatBuffer;
	};

	DocumentationOutput::DocumentationOutput()
		: m_formatBuffer( 65536, '\0' )
	{
	}

	void DocumentationOutput::setVariable( const char* variable, const char* value )
	{
		m_variables.emplace( variable, value ? value : std::string{} );
	}

	void DocumentationOutput::setVariable( std::string variable, std::string value )
	{
		m_variables.emplace( std::move( variable ), std::move( value ) );
	}

	void DocumentationOutput::append( const char* text )
	{
		std::string varName;
		while ( const char* param = strstr( text, "%%" ) )
		{
			const char* closeParam = strstr( param + 2, "%%" );
			Assert( closeParam );
			output.append( text, param );
			varName.assign( param + 2, closeParam );
			auto it = m_variables.find( varName );
			Assert( it != m_variables.end() );
			output.append( it->second );
			text = closeParam + 2;
		}
		output.append( text );
	}

	void DocumentationOutput::appendF( const char* format, ... )
	{
		va_list args;
		va_start( args, format );
		vsnprintf( &m_formatBuffer[0], m_formatBuffer.size(), format, args );
		va_end( args );
		append( m_formatBuffer.data() );
	}

	void DocumentationOutput::appendRaw( const char* text )
	{
		output += text;
	}

	void DocumentationOutput::appendRawF( const char* format, ... )
	{
		va_list args;
		va_start( args, format );
		vsnprintf( &m_formatBuffer[0], m_formatBuffer.size(), format, args );
		va_end( args );
		appendRaw( m_formatBuffer.data() );
	}

	int DocumentationOutput::writeToFile( const char* outputFile )
	{
		CUtlBuffer buf( output.c_str(), output.length(), CUtlBuffer::READ_ONLY );
		buf.SeekPut( CUtlBuffer::SEEK_TAIL, 0 );
		char dir[MAX_PATH];
		V_ExtractFilePath( outputFile, dir, MAX_PATH );
		g_pFullFileSystem->CreateDirHierarchy( dir, "hammer" );
		return g_pFullFileSystem->WriteFile( outputFile, "hammer", buf ) ? asDOCGEN_Success : asDOCGEN_FailedToWriteFile;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	class TextDecorator
	{
	public:
		TextDecorator( bool syntaxHighlight, bool nl2br, bool htmlSafe, bool linkifyUrls );
		void appendObjectType( const asITypeInfo* typeInfo );
		void appendEnumType( const asITypeInfo* typeInfo );

		std::string decorateAngelScript( std::string text );
		std::string decorateDocumentation( std::string text );
		std::string htmlSafe( std::string text );
	private:
		std::string nl2br( std::string text );
		template <typename F>
		void	regexReplace( std::string& str, const std::regex& regex, F&& cb );
		size_t	wrapSpan( std::string& str, size_t strIndex, const char* text, size_t length, const char* cssClass );

		using KeywordMap = robin_hood::unordered_flat_map<std::string, std::string>;
		KeywordMap		m_asKeywords;
		std::regex		m_identifierRegex;
		std::regex		m_urlRegex;
		std::regex		m_numberRegex;
		bool			m_syntaxHighlight;
		bool			m_nl2br;
		bool			m_htmlSafe;
		bool			m_linkifyUrls;
	};

	TextDecorator::TextDecorator( bool syntaxHighlight, bool nl2br, bool htmlSafe, bool linkifyUrls )
		: m_syntaxHighlight( syntaxHighlight ), m_nl2br( nl2br ), m_htmlSafe( htmlSafe ), m_linkifyUrls( linkifyUrls )
	{
		if (m_syntaxHighlight) {
			constexpr const char* const keywords[] =
			{
				// official keywords
				"and", "abstract", "auto", "bool", "break", "case", "cast", "catch", "class", "const", "continue", "default", "do", "double", "else", "enum", "explicit", "external ",
				"false", "final", "float", "for", "from", "funcdef", "function", "get", "if", "import", "in", "inout", "int", "interface", "int8", "int16", "int32", "int64", "is",
				"mixin", "namespace", "not", "null", "or", "out", "override ", "private", "property ", "protected", "return", "set ", "shared ", "super ", "switch", "this ", "true",
				"try", "typedef", "uint", "uint8", "uint16", "uint32", "uint64", "void", "while", "xor",

				// not official keywords, but should probably be treated as such
				"string",
				"array",
				"callback"
			};
			for ( const char* keyword : keywords )
				m_asKeywords[keyword] = "AS-keyword";
			m_identifierRegex = "(^|\\b)(\\w+)(\\b|$)";
			m_urlRegex = "([a-z]+\\://(.*?))(\\s|$)";
			m_numberRegex = "(^|\\b)[0-9]x?[0-9a-fA-F]*(\\b|$)";		// this is not very precise, but probably good enough
		}
	}

	void TextDecorator::appendObjectType( const asITypeInfo* typeInfo )
	{
		if ( m_syntaxHighlight )
			m_asKeywords[typeInfo->GetName()] = typeInfo->GetFlags() & asOBJ_VALUE ? "AS-valuetype" : "AS-classtype";
	}

	void TextDecorator::appendEnumType( const asITypeInfo* typeInfo )
	{
		if ( !m_syntaxHighlight )
			return;
		m_asKeywords[typeInfo->GetName()] = "AS-enumtype";
		const int c = typeInfo->GetEnumValueCount();
		for ( int i = 0; i < c; ++i )
			m_asKeywords[typeInfo->GetEnumValueByIndex( i, nullptr )] = "AS-enumvalue";
	}

	std::string TextDecorator::decorateAngelScript( std::string text )
	{
		text = htmlSafe( std::move( text ) );
		if ( m_syntaxHighlight )
		{
			// do identifiers
			std::string match;
			regexReplace( text, m_identifierRegex, [this, &match]( std::string & text, const std::cmatch& matches )
			{
				const size_t offset = matches[0].first - text.c_str();
				match.assign( matches[0].first, matches[0].first + matches[0].length() );
				auto it = m_asKeywords.find( match );
				if ( it != m_asKeywords.end() )
					return wrapSpan( text, offset, match.c_str(), match.size(), it->second.c_str() );
				return offset + matches[0].length();
			} );

			// do numbers
			regexReplace( text, m_numberRegex, [this]( std::string& text, const std::cmatch& matches )
			{
				const size_t offset = matches[0].first - text.c_str();
				return wrapSpan( text, offset, matches[0].first, matches[0].length(), "AS-number" );
			} );
		}
		return text;
	}

	std::string TextDecorator::decorateDocumentation( std::string text )
	{
		text = htmlSafe( std::move( text ) );
		if ( m_linkifyUrls )
		{
			// do urls
			std::string replacement;
			regexReplace( text, m_urlRegex, [&replacement]( std::string & text, const std::cmatch& matches )
			{
				replacement = "<a href=\"";
				replacement.append( matches[0].first, matches[0].first + matches[0].length() );
				replacement += "\">";
				replacement.append( matches[0].first, matches[0].first + matches[0].length() );
				replacement += "</a>";
				const size_t offset = matches[0].first - text.c_str();
				text.replace( offset, (size_t)matches[0].length(), replacement.c_str() );
				return offset + replacement.size();
			} );
		}
		return nl2br( std::move( text ) );
	}

	size_t TextDecorator::wrapSpan( std::string& str, size_t strIndex, const char* text, size_t length, const char* cssClass )
	{
		std::string replacement;
		replacement = "<span class=\"";
		replacement += cssClass;
		replacement += "\">";
		replacement.append( text, length );
		replacement += "</span>";
		str.replace( strIndex, length, replacement );
		return strIndex + replacement.size();
	}

	template <typename F>
	void TextDecorator::regexReplace( std::string& str, const std::regex& regex, F&& cb )
	{
		std::cmatch matches;
		size_t searchOffset = 0;
		while ( std::regex_search( str.c_str() + searchOffset, matches, regex ) )
			searchOffset = cb( str, matches );
	}

	std::string TextDecorator::nl2br( std::string text )
	{
		if ( m_nl2br )
		{
			size_t searchOffset = 0;
			while ( const char* nl = strstr( text.c_str() + searchOffset, "\n" ) )
			{
				const size_t replaceOffset = ( size_t )( nl - text.c_str() );
				text.replace( replaceOffset, 1, "<br/>\n" );
				searchOffset = replaceOffset + 6;
			}
		}
		return text;
	}

	std::string TextDecorator::htmlSafe( std::string text )
	{
		if ( m_htmlSafe )
		{
			size_t searchOffset = 0;
			while (true)
			{
				const size_t index = text.find_first_of( "&<>", searchOffset );
				if ( index == std::string::npos )
					break;
				const char* replacement = nullptr;
				switch ( text[index] )
				{
				case '&':
					replacement = "&amp;";
					break;
				case '<':
					replacement = "&lt;";
					break;
				case '>':
					replacement = "&gt;";
					break;
				default:
					Assert( !"Should not happen" );
				}
				text.replace( index, 1, replacement );
				searchOffset = index + strlen( replacement ) - 1;
			}
		}
		return text;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	class Base64FileTool
	{
	public:
		bool open( const char* filename );
		std::string base64encoded();
	private:
		std::string m_rawData;
	};

	bool Base64FileTool::open( const char* filename )
	{
		FileHandle_t file = g_pFullFileSystem->Open( filename, "rb" );
		if ( !file )
			return false;

		m_rawData.resize( g_pFullFileSystem->Size( file ) );
		g_pFullFileSystem->Read( m_rawData.data(), m_rawData.size(), file );

		g_pFullFileSystem->Close( file );
		return true;
	}

	std::string Base64FileTool::base64encoded()
	{
		const size_t input_length = m_rawData.size();
		const size_t output_length = 4 * ( ( input_length + 2 ) / 3 );
		std::string  output( output_length + 1, '\0' );

		base64_encode( m_rawData.c_str(), m_rawData.size(), output.data(), output.size() );
		output.resize( output_length );

		return output;
	}

}

BEGIN_AS_NAMESPACE

class DocumentationGenerator::Impl
{
public:
	Impl( asIScriptEngine* engine, const ScriptDocumentationOptions& options );
	int DocumentFunction( int funcId, const char* string );
	int DocumentType( int typeId, const char* string );
	int generate();

private:
	template <typename F>
	void GenerateSubHeader( int level, const char* title, const char* name, F&& cb );
	template <typename F>
	void GenerateContentBlock( const char* title, const char* name, F&& cb );
	void GenerateClasses();
	void GenerateGlobalFunctions();
	void GenerateGlobalProperties();
	void GenerateEnums();
private:
	struct SummaryNode;
	using SummaryNodeVector = std::vector<SummaryNode>;
	struct SummaryNode
	{
		std::string text;
		std::string anchor;
		SummaryNodeVector children;
	};

	struct ObjectTypeComparator
	{
		bool operator()( const asITypeInfo* a, const asITypeInfo* b ) const
		{
			int i = strcmp( a->GetName(), b->GetName() );
			if ( i < 0 )
				return true;
			if ( i > 0 )
				return false;
			return a < b;
		}
	};
	using ObjectTypeSet = std::set<const asITypeInfo*, ObjectTypeComparator>;

	SummaryNodeVector	CreateSummary();
	void				OutputSummary( const SummaryNodeVector& nodes );
	const std::string&	LowerCaseTempBuf( const char* text );
	int					Prepare();
	const char*			GetDocumentationForType( const asITypeInfo* type ) const;
	const char*			GetDocumentationForFunction( const asIScriptFunction* function ) const;

	// our input
	asIScriptEngine*						engine;
	ScriptDocumentationOptions				options;

	// documentation strings by pointer
	robin_hood::unordered_flat_map<const asIScriptFunction*, std::string>	functionDocumentation;
	robin_hood::unordered_flat_map<const asITypeInfo*, std::string>			objectTypeDocumentation;

	// our helper objects
	DocumentationOutput						output;
	TextDecorator							decorator;
	std::string								lowercase;

	// our state
	std::vector<const asIScriptFunction*>	globalFunctions;
	struct GlobalProperty
	{
		std::string	name;
		int			typeId;
		bool		cnst;
	};
	std::vector<GlobalProperty>				globalProperties;
	std::vector<asITypeInfo*>				enums;
	ObjectTypeSet							objectTypes;
};

DocumentationGenerator::Impl::Impl( asIScriptEngine* engine, const ScriptDocumentationOptions& options )
	: engine( engine ), options( options ), decorator( options.syntaxHighlight, options.nl2br, options.htmlSafe, options.linkifyUrls )
{
}

int DocumentationGenerator::Impl::Prepare()
{
	// load logo file if required
	std::string logoImageTag;
	if ( options.logoImageFile )
	{
		// this may throw, it uses std file stream
		Base64FileTool filetool;
		if ( !filetool.open( options.logoImageFile ) )
			return asDOCGEN_CouldNotLoadLogoFile;
		std::string base64Img = filetool.base64encoded();

		// now build img tag
		logoImageTag = "<img ";
		constexpr const char* const names[] = { "width", "height" };
		if ( options.logoImageSize[0] && options.logoImageSize[1] )
		{
			for ( int i = 0; i < 2; ++i )
			{
				std::stringstream html;
				html << names[i] << "=\"" << options.logoImageSize[i] << "\"";
				logoImageTag += html.str();
			}
		}
		else
		{
			for ( int i = 0; i < 2; ++i )
			{
				if ( options.logoImageSize[0] )
				{
					std::stringstream html;
					html << names[i] << "=\"" << options.logoImageSize[0] << "\"";
					logoImageTag += html.str();
				}
			}
		}
		logoImageTag += R"^(src="data:image/png;base64,)^";
		logoImageTag += base64Img;
		logoImageTag += R"(" />)";
	}

	// set variables
	output.setVariable( "PROJECT_NAME", options.projectName );
	output.setVariable( "DOCUMENTATION_NAME", options.documentationName );
	output.setVariable( "INTERFACE_VERSION", options.interfaceVersion );
	output.setVariable( "DEFAULT_FONT", options.defaultFont );
	output.setVariable( "DEFAULT_FONT_SIZE", options.defaultFontSize );
	output.setVariable( "API_FONT", options.apiFont );
	output.setVariable( "API_FONT_SIZE", options.apiFontSize );
	output.setVariable( "MAIN_BACKGROUND_COLOR", options.mainBackgroundColor );
	output.setVariable( "MAIN_FOREGROUND_COLOR", options.mainForegroundColor );
	output.setVariable( "HEADER_BACKGROUND_COLOR", options.headerBackgroundColor );
	output.setVariable( "HEADER_FOREGROUND_COLOR", options.headerForegroundColor );
	output.setVariable( "HEADER_BORDER", options.headerBorder );
	output.setVariable( "SUBHEADER_COLOR", options.textHeaderColor );
	output.setVariable( "BLOCK_BORDER", options.blockBorder );
	output.setVariable( "BLOCK_BACKGROUND_COLOR", options.blockBackgroundColor );
	output.setVariable( "LINK_COLOR", options.linkColor );
	output.setVariable( "GENERATED_ON_COLOR", options.generatedDateTimeColor );
	output.setVariable( "AS_KEYWORD_COLOR", options.asKeywordColor );
	output.setVariable( "AS_NUMBER_COLOR", options.asNumberColor );
	output.setVariable( "AS_VALUETYPE_COLOR", options.asValueTypeColor );
	output.setVariable( "AS_CLASSTYPE_COLOR", options.asClassTypeColor );
	output.setVariable( "AS_ENUMTYPE_COLOR", options.asEnumTypeColor );
	output.setVariable( "AS_ENUMVALUE_COLOR", options.asEnumValueColor );
	output.setVariable( "ADDITIONAL_CSS", options.additionalCss );
	output.setVariable( "ADDITIONAL_JAVASCRIPT", options.additionalJavascript );
	output.setVariable( "SEARCH_HOTKEY", options.searchHotKey );
	output.setVariable( "SUMMARY_WIDTH", options.summaryWidth );
	output.setVariable( "LOGO_IMAGE", std::move( logoImageTag ) );

	// get and sort all global functions
	const int funcCount = engine->GetGlobalFunctionCount();
	if ( funcCount )
	{
		globalFunctions.reserve( funcCount );
		for ( int i = 0; i < funcCount; ++i )
			globalFunctions.emplace_back( engine->GetGlobalFunctionByIndex( i ) );
		std::sort( globalFunctions.begin(), globalFunctions.end(), []( const asIScriptFunction* a, const asIScriptFunction* b )
		{
			return strcmp( a->GetName(), b->GetName() ) < 0;
		} );
	}

	const int propCount = engine->GetGlobalPropertyCount();
	if ( propCount )
	{
		globalProperties.reserve( propCount );
		for ( int i = 0; i < propCount; ++i )
		{
			const char* name;
			int typeId;
			bool cnst;
			engine->GetGlobalPropertyByIndex( i, &name, nullptr, &typeId, &cnst );
			globalProperties.emplace_back( GlobalProperty{ name, typeId, cnst } );
		}
		std::sort( globalProperties.begin(), globalProperties.end(), []( const GlobalProperty& a, const GlobalProperty& b )
		{
			return a.name < b.name;
		} );
	}

	const int enumCount = engine->GetEnumCount();
	if ( enumCount )
	{
		enums.reserve( enumCount );
		for ( int i = 0; i < enumCount; ++i )
			enums.emplace_back( engine->GetEnumByIndex( i ) );
		std::sort( enums.begin(), enums.end(), []( const asITypeInfo* a, const asITypeInfo* b )
		{
			return strcmp( a->GetName(), b->GetName() ) < 0;
		} );
	}

	// get and sort all object types
	for ( int t = 0, tcount = engine->GetObjectTypeCount(); t < tcount; ++t )
	{
		const asITypeInfo* const typeInfo = engine->GetObjectTypeByIndex( t );
		if ( !strcmp( typeInfo->GetName(), "array" ) && !options.includeArrayInterface )
			continue;
		if ( !strcmp( typeInfo->GetName(), "string" ) && !options.includeStringInterface )
			continue;
		if ( !strcmp( typeInfo->GetName(), "ref" ) && !options.includeRefInterface )
			continue;
		if ( ( !strcmp( typeInfo->GetName(), "weakref" ) || !strcmp( typeInfo->GetName(), "const_weakref" ) ) && !options.includeWeakRefInterface )
			continue;
		objectTypes.insert( typeInfo );
	}

	// append all types to the decorator
	for ( const auto type : objectTypes )
		decorator.appendObjectType( type );

	for ( const auto type : enums )
		decorator.appendEnumType( type );
	return asDOCGEN_Success;
}

int DocumentationGenerator::Impl::generate()
{
	int r = Prepare();
	if ( r != asDOCGEN_Success )
		return r;

	// start HTML
	output.append( HtmlStart );

	// output summary
	output.appendRaw( R"^(<div id="summary">)^" );
	OutputSummary( CreateSummary() );
	if ( options.addTimestamp )
	{
		auto t  = std::time( nullptr );
		auto tm = *std::localtime( &t );
		std::stringstream dt;
		dt << std::put_time( &tm, "%d-%m-%Y %H:%M:%S" );
		output.appendRawF( R"^(<div class="generated_on">Generated on %s</div>)^", dt.str().c_str() );
	}
	output.appendRaw( R"^(</div>)^" );

	// output content
	output.appendRaw( R"^(<div id="content">)^" );
	GenerateClasses();
	GenerateEnums();
	GenerateGlobalFunctions();
	GenerateGlobalProperties();
	output.appendRaw( R"^(</div>)^" );

	// finish HTML and output
	output.append( HtmlEnd );
	return output.writeToFile( options.outputFile );
}

template <typename F>
void DocumentationGenerator::Impl::GenerateSubHeader( int level, const char* title, const char* name, F&& cb )
{
	if ( name )
	{
		output.appendRawF( R"^(<a name="%s"></a>)^", LowerCaseTempBuf( name ).c_str() );
		output.appendRawF( R"^(<h%d name="%s">%s</h%d>)^" "\n", level, LowerCaseTempBuf( title ).c_str(), decorator.htmlSafe( title ).c_str(), level );
		output.appendRawF( R"^(<div name="%s">)^", LowerCaseTempBuf( name ).c_str() );
	}
	else
	{
		output.appendRawF( R"^(<h%d>%s</h%d>)^" "\n", level, title, level );
		output.appendRaw( R"^(<div>)^" );
	}
	cb();
	output.appendRaw( "</div>" );
}

template <typename F>
void DocumentationGenerator::Impl::GenerateContentBlock( const char* title, const char* name, F&& cb )
{
	output.appendRawF( R"^(<a name="%s"></a>)^", LowerCaseTempBuf( name ).c_str() );
	output.appendRawF( R"^(<div class="block" name="%s">)^" "\n", LowerCaseTempBuf( title ).c_str() );
	cb();
	output.appendRaw( "</div>\n" );
}

void DocumentationGenerator::Impl::GenerateGlobalFunctions()
{
	// bail if nothing to do
	if ( globalFunctions.empty() )
		return;

	// create output
	GenerateSubHeader( 1, "Global Functions", "___globalfunctions", [&]()
	{
		for ( auto func : globalFunctions )
			GenerateContentBlock( func->GetName(), func->GetName(), [&]()
			{
				output.appendRawF( R"^(<div class="api">%s</div>)^", decorator.decorateAngelScript( func->GetDeclaration( true, true, true ) ).c_str() );
				const char* documentation = GetDocumentationForFunction( func );
				if ( documentation && *documentation )
					output.appendRawF( R"^(<div class="documentation">%s</div>)^", decorator.decorateDocumentation( documentation ).c_str() );
			} );
	} );
}

void DocumentationGenerator::Impl::GenerateGlobalProperties()
{
	// bail if nothing to do
	if ( globalProperties.empty() )
		return;

	// create output
	GenerateSubHeader( 1, "Global Properties", "___globalproperties", [&]()
	{
		for ( const auto& prop : globalProperties )
			GenerateContentBlock( prop.name.c_str(), prop.name.c_str(), [&]()
			{
				output.appendRawF( R"^(<div class="api">%s</div>)^", decorator.decorateAngelScript( std::string( prop.cnst ? "const " : "" ) + engine->GetTypeDeclaration( prop.typeId, true ) + " " + prop.name ).c_str() );
				/*const char* documentation = GetDocumentationForFunction( func );
				if ( documentation && *documentation )
					output.appendRawF( R"^(<div class="documentation">%s</div>)^", decorator.decorateDocumentation( documentation ).c_str() );*/
			} );
	} );
}

static FORCEINLINE bool has_popcount_support()
{
	int cpu_infos[4];
	__cpuid(cpu_infos, 1);
	return (cpu_infos[2] & (1 << 23)) != 0;
}
static const bool hasPopCount = has_popcount_support();

static constexpr FORCEINLINE int fallback_popcount(uint32 x)
{
	constexpr const uint32 m1 = 0x55555555;
	constexpr const uint32 m2 = 0x33333333;
	constexpr const uint32 m4 = 0x0f0f0f0f;
	constexpr const uint32 h01 = 0x01010101;

	x -= (x >> 1) & m1;
	x = (x & m2) + ((x >> 2) & m2);
	x = (x + (x >> 4)) & m4;
	return static_cast<int>((x * h01) >> (32 - 8));
}

int UTIL_CountNumBitsSet( uint32 x )
{
#ifdef WIN32_CLANG
	return __builtin_popcount( x );
#else
	return hasPopCount ? _mm_popcnt_u32( x ) : fallback_popcount( x );
#endif
}

void DocumentationGenerator::Impl::GenerateEnums()
{
	// bail if nothing to do
	if ( enums.empty() )
		return;

	// create output
	GenerateSubHeader( 1, "Enums", "___enums", [&]()
	{
		for ( auto en : enums )
			GenerateContentBlock( en->GetName(), LowerCaseTempBuf( en->GetName() ).c_str(), [&]()
			{
				std::string decl = std::string( "enum " ) + en->GetName() + "\n{\n";
				bool writePo2 = true;
				const int enumValueCount = en->GetEnumValueCount();
				for ( int i = 0; i < enumValueCount; i++ )
				{
					int val;
					en->GetEnumValueByIndex( i, &val );
					writePo2 &= IsPowerOfTwo( val );
				}

				for ( int i = 0; i < enumValueCount; i++ )
				{
					int val;
					decl += std::string( "\t" ) + en->GetEnumValueByIndex( i, &val ) + " = ";
					decl += ( writePo2 && val ? "1 << " + std::to_string( UTIL_CountNumBitsSet( val - 1 ) ) : std::to_string( val ) ) + ( i == enumValueCount - 1 ? "" : "," ) + '\n';
				}
				decl += "}";
				output.appendRawF( R"^(<div class="api">%s</div>)^", decorator.decorateAngelScript( decl ).c_str() );
				/*const char* documentation = GetDocumentationForFunction( func );
				if ( documentation && *documentation )
					output.appendRawF( R"^(<div class="documentation">%s</div>)^", decorator.decorateDocumentation( documentation ).c_str() );*/
			} );
	} );
}

DocumentationGenerator::Impl::SummaryNodeVector DocumentationGenerator::Impl::CreateSummary()
{
	SummaryNodeVector functionNodes;
	std::set<std::string> seenFunctions;
	for ( auto func : globalFunctions )
	{
		if ( seenFunctions.insert( func->GetName() ).second )
			functionNodes.emplace_back( SummaryNode{ func->GetName(), LowerCaseTempBuf( func->GetName() ) } );
	}

	SummaryNodeVector propNodes;
	for ( const auto& prop : globalProperties )
	{
		propNodes.emplace_back( SummaryNode{ prop.name, LowerCaseTempBuf( prop.name.c_str() ) } );
	}

	SummaryNodeVector enumNodes;
	for ( const auto& en : enums )
	{
		enumNodes.emplace_back( SummaryNode{ en->GetName(), LowerCaseTempBuf( en->GetName() ) } );
	}

	SummaryNodeVector valueTypes, classes;
	for ( auto typeInfo : objectTypes )
	{
		SummaryNodeVector& v = ( typeInfo->GetFlags() & asOBJ_VALUE ) ? valueTypes : classes;
		v.emplace_back( SummaryNode{ typeInfo->GetName(), LowerCaseTempBuf( typeInfo->GetName() ) } );
	}

	return
		{
			{
				"Value Types",
				"___valuetypes",
				std::move( valueTypes ),
			},
			{
				"Classes",
				"___classes",
				std::move( classes ),
			},
			{
				"Global Functions",
				"___globalfunctions",
				std::move( functionNodes )
			},
			{
				"Enums",
				"___enums",
				std::move( enumNodes )
			},
			{
				"Global Properties",
				"___globalproperties",
				std::move( propNodes )
			}
		};
}

void DocumentationGenerator::Impl::OutputSummary( const SummaryNodeVector& nodes )
{
	for ( const auto& node : nodes )
	{
		output.appendRaw( R"^(<div class="summary">)^" );
		if ( node.children.empty() )
			output.appendRawF( R"^(<span class="api link" targetname="%s">%s</span>)^", node.anchor.c_str(), node.text.c_str() );
		else
		{
			output.appendRawF( R"^(<div class="api expandable link" targetname="%s"><span>+</span> <span>%s</span></div>)^", node.anchor.c_str(), node.text.c_str() );
			output.appendRawF( R"^(<div class="indented hidden">)^" );
			OutputSummary( node.children );
			output.appendRawF( "</div>" );
		}
		output.appendRaw( "</div>" );
	}
}

const std::string& DocumentationGenerator::Impl::LowerCaseTempBuf( const char* text )
{
	lowercase = text;
	for ( auto& c : lowercase )
		c = (char)std::tolower( c );
	return lowercase;
}

static const robin_hood::unordered_flat_map<std::string, std::string> func_replace = {{
	{ "opNeg", "operator-" },
	{ "opCom", "operator~" },
	{ "opPreInc", "operator++" },
	{ "opPreDec", "operator--" },
	{ "opEquals", "operator==" },
	{ "opCmp", "operator<=>" },
	{ "opAssign", "operator=" },
	{ "opHndlAssign", "operator@=" },
	{ "opAddAssign", "operator+=" },
	{ "opSubAssign", "operator-=" },
	{ "opMulAssign", "operator*=" },
	{ "opDivAssign", "operator/=" },
	{ "opModAssign", "operator%=" },
	{ "opPowAssign", "operator*=" },
	{ "opAndAssign", "operator&=" },
	{ "opOrAssign", "operator|=" },
	{ "opXorAssign", "operator^=" },
	{ "opShlAssign", "operator<<=" },
	{ "opShrAssign", "operator>>=" },
	{ "opUShrAssign", "operator>>>=" },
	{ "opAdd", "operator+" },
	{ "opSub", "operator-" },
	{ "opMul", "operator*" },
	{ "opDiv", "operator/" },
	{ "opMod", "operator%" },
	{ "opPow", "operator**" },
	{ "opAnd", "operator&" },
	{ "opOr", "operator|" },
	{ "opXor", "operator^" },
	{ "opShl", "operator<<" },
	{ "opShr", "operator>>" },
	{ "opUShr", "operator>>>" },
	{ "opIndex", "operator[]" },
	{ "opCall", "operator()" }
}};

static constexpr const char* baseTypeNames[] = {
	"void",
	"bool",
	"int8",
	"int16",
	"int",
	"int64",
	"uint8",
	"uint16",
	"uint",
	"uint64",
	"float",
	"double"
};

void DocumentationGenerator::Impl::GenerateClasses()
{
	constexpr const asEBehaviours behaviors[] = { asBEHAVE_CONSTRUCT, asBEHAVE_FACTORY, asBEHAVE_LIST_FACTORY, asBEHAVE_DESTRUCT };
	std::vector<const asIScriptFunction*> funcs;

	auto generate = [&](bool valueTypes)
	{
		GenerateSubHeader( 1, valueTypes ? "Value Types" : "Classes", valueTypes ? "___valuetypes" : "___classes", [&]() {
			for ( const asITypeInfo* typeInfo : objectTypes )
			{
				if ( !!( typeInfo->GetFlags() & asOBJ_VALUE ) != valueTypes )
					continue;

				std::string fullName = typeInfo->GetName();
				if ( typeInfo->GetFlags() & asOBJ_TEMPLATE )
				{
					fullName += '<';
					const int c = typeInfo->GetSubTypeCount();
					for ( int i = 0; i < c; i++ )
					{
						asITypeInfo* subType = typeInfo->GetSubType( i );
						if ( subType->GetFlags() & asTYPEID_MASK_OBJECT )
							fullName += "class";
						else if ( subType->GetTypeId() <= asTYPEID_DOUBLE )
							fullName += baseTypeNames[subType->GetTypeId()];
						else if ( subType->GetFlags() & asOBJ_ENUM )
							fullName += subType->GetBaseType()->GetName();
						fullName += ' ';
						fullName += subType->GetName();
						if ( i != c - 1 )
							fullName += ", ";
					}
					fullName += '>';
				}

				GenerateSubHeader( 2, fullName.c_str(), decorator.htmlSafe( fullName ).c_str(), [&]() {
					// list overview (fake class)
					bool isInterface = false;
					std::string classOverview = typeInfo->GetNamespace() + fullName + "\n{\n";
					funcs.clear();
					for ( auto beh : behaviors )
						for ( int i = 0, c = typeInfo->GetBehaviourCount(); i < c; ++i )
						{
							asEBehaviours b;
							auto behFunc = typeInfo->GetBehaviourByIndex( i, &b );
							if ( beh == b )
							{
								classOverview += std::string( "\t" ) + behFunc->GetDeclaration( false, false, true ) + ";\n";
								funcs.emplace_back( behFunc );
							}
						}

					for ( int i = 0, c = typeInfo->GetMethodCount(); i < c; ++i )
					{
						auto func = typeInfo->GetMethodByIndex( i );
						if ( auto name = func->GetName(); name == V_stristr( name, "get_" ) || name == V_stristr( name, "set_" ) )
							continue;
						funcs.emplace_back( func );
						const std::string funcName = func->GetName();
						const auto f = func_replace.find( funcName );
						if ( f != func_replace.end() )
						{
							std::string name = func->GetDeclaration( false, false, true );
							size_t off = name.find( funcName );

							classOverview += std::string( "\t" ) + name.replace( off, funcName.size(), f->second ) + ";\n";
						}
						else
							classOverview += std::string( "\t" ) + func->GetDeclaration( false, false, true ) + ";\n";
						isInterface |= func->GetFuncType() == asFUNC_INTERFACE;
					}

					robin_hood::unordered_flat_map<std::string, bool> skip;
					for ( int i = 0, c = typeInfo->GetMethodCount(); i < c; ++i )
					{
						auto func = typeInfo->GetMethodByIndex( i );
						if ( auto name = func->GetName(); name != V_stristr( name, "get_" ) && name != V_stristr( name, "set_" ) )
							continue;
						if ( skip.find( func->GetName() + 4 ) != skip.end() )
							continue;
						std::string setterName = "set_";
						setterName += func->GetName() + 4;
						bool isConst = true;
						if ( typeInfo->GetMethodByName( setterName.c_str() ) )
						{
							isConst = false;
							skip[setterName.c_str() + 4] = true;
						}
						funcs.emplace_back( func );
						const char* typeDecl = engine->GetTypeDeclaration( func->GetReturnTypeId() );
						classOverview += std::string( "\t" ) + ( isConst ? "const " : "" ) + typeDecl + " " + ( func->GetName() + 4 ) + ";\n";
						isInterface |= func->GetFuncType() == asFUNC_INTERFACE;
					}
					for ( int i = 0, c = typeInfo->GetPropertyCount(); i < c; ++i )
						classOverview += std::string( "\t" ) + typeInfo->GetPropertyDeclaration( i, true ) + ";\n";
					for ( int i = 0, c = typeInfo->GetChildFuncdefCount(); i < c; ++i )
						classOverview += std::string( "\tcallback " ) + typeInfo->GetChildFuncdef( i )->GetFuncdefSignature()->GetDeclaration( false, false, true ) + ";\n";
					classOverview += "}";
					GenerateContentBlock( typeInfo->GetName(), typeInfo->GetName(), [&]()
					{
						output.appendRawF( R"^(<div class="api">%s</div>)^", decorator.decorateAngelScript( ( isInterface ? "interface " : "class " ) + classOverview ).c_str() );
						const char* documentation = GetDocumentationForType( typeInfo );
						if ( documentation && *documentation )
							output.appendRawF( R"^(<div class="documentation">%s</div>)^", decorator.decorateDocumentation( documentation ).c_str() );
					} );

					// list behaviors + methods
					for ( auto func : funcs )
					{
						const char* documentation = GetDocumentationForFunction( func );
						if ( !documentation || !*documentation )
							continue;

						std::string fullName_ = fullName + "::" + func->GetName();
						GenerateContentBlock( fullName_.c_str(), fullName_.c_str(), [&]()
						{
							output.appendRawF( R"^(<div class="api">%s</div>)^", decorator.decorateAngelScript( func->GetDeclaration( true, false, true ) ).c_str() );
							output.appendRawF( R"^(<div class="documentation">%s</div>)^", decorator.decorateDocumentation( documentation ).c_str() );
						} );
					}
				} );
			}
		} );
	};
	generate( true );
	generate( false );
}

int DocumentationGenerator::Impl::DocumentFunction( int funcId, const char* string )
{
	if ( funcId >= 0 )
	{
		const asIScriptFunction* function = engine->GetFunctionById( funcId );
		if ( !function )
			return asDOCGEN_CouldNotFindFunctionById;
		if ( !functionDocumentation.insert( { function, string } ).second )
			return asDOCGEN_AlreadyDocumented;
	}
	return funcId;
}

int DocumentationGenerator::Impl::DocumentType( int typeId, const char* string )
{
	if (typeId >= 0)
	{
		const asITypeInfo* type = engine->GetTypeInfoById( typeId );
		if ( !type )
			return asDOCGEN_CouldNotFindTypeById;
		if ( !objectTypeDocumentation.insert( { type, string } ).second )
			return asDOCGEN_AlreadyDocumented;
	}
	return typeId;
}

const char* DocumentationGenerator::Impl::GetDocumentationForType( const asITypeInfo* type ) const
{
	auto it = objectTypeDocumentation.find( type );
	if ( it != objectTypeDocumentation.end() )
		return it->second.c_str();
	return "";
}

const char* DocumentationGenerator::Impl::GetDocumentationForFunction( const asIScriptFunction* function ) const
{
	auto it = functionDocumentation.find( function );
	if ( it != functionDocumentation.end() )
		return it->second.c_str();
	return "";
}

DocumentationGenerator::DocumentationGenerator( asIScriptEngine* engine, const ScriptDocumentationOptions& options )
	: impl( new Impl( engine, options ) )
{
}

DocumentationGenerator::~DocumentationGenerator()
{
	delete impl;
}

int DocumentationGenerator::DocumentGlobalFunction( int r, const char* string )
{
	return impl->DocumentFunction( r, string );
}

int DocumentationGenerator::DocumentObjectType( int r, const char* string )
{
	return impl->DocumentType( r, string );
}

int DocumentationGenerator::DocumentObjectMethod( int r, const char* string )
{
	return impl->DocumentFunction( r, string );
}

int DocumentationGenerator::DocumentInterface( int r, const char* string )
{
	return impl->DocumentFunction( r, string );
}

int DocumentationGenerator::DocumentInterfaceMethod( int r, const char* string )
{
	return impl->DocumentFunction( r, string );
}

int DocumentationGenerator::Generate()
{
	return impl->generate();
}

END_AS_NAMESPACE

#endif

#endif // _DEBUG