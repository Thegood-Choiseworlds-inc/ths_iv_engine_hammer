
#include "scriptstring.h"
#include "scriptarray.h"

#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "tier1/fmtstr.h"
#include "tier1/generichash.h"
#include "tier1/strtools.h"
#include "tier1/stringpool.h"

#include "robin_hood.h"

#include "tier0/memdbgon.h"

static class StringFactory final : public AS_NAMESPACE_QUALIFIER asIStringFactory
{
public:
	~StringFactory() override = default;
	const void* GetStringConstant( const char* data, asUINT length ) override
	{
		AUTO_LOCK( m_mutex );

		ScriptString str( data, length );
		map_t::iterator it = m_dict.find( str );
		if ( it != m_dict.end() )
			++it->second;
		else
			it = m_dict.insert( map_t::value_type( std::move( str ), 1 ) ).first;

		return &it->first;
	}

	int ReleaseStringConstant( const void* str ) override
	{
		if ( !str )
			return asERROR;
		AUTO_LOCK( m_mutex );

		map_t::iterator it = m_dict.find( *reinterpret_cast<const ScriptString*>( str ) );
		if ( it == m_dict.end() )
			return asERROR;
		else
		{
			--it->second;
			if ( it->second == 0 )
				m_dict.erase( it );
		}

		return asSUCCESS;
	}

	int GetRawStringData( const void* str, char* data, asUINT* length ) const override
	{
		if ( !str )
			return asERROR;

		auto* string = static_cast<const ScriptString*>( str );
		if ( length )
			*length = string->Length();
		if ( data )
			V_memcpy( data, string->Get(), *length );
		return asSUCCESS;
	}

private:
	using map_t = robin_hood::unordered_node_map<ScriptString, int>;
	map_t m_dict;
	CThreadFastMutex m_mutex;
} stringFactory;

void ScriptString::Storage::Swap( Storage& other )
{
	V_swap( m_pMem, other.m_pMem );
	V_swap( m_nAllocCount, other.m_nAllocCount );
}

void ScriptString::Storage::Grow()
{
	ResizeTo( m_nAllocCount + 1 );
}

void ScriptString::Storage::Purge()
{
	if ( m_pMem )
		free( m_pMem );
	m_pMem = nullptr;
	m_nAllocCount = 0;
}

void ScriptString::Storage::ResizeTo( size_t size )
{
	const size_t oldSize = m_nAllocCount;
	m_nAllocCount = size;
	m_pMem = static_cast<char*>( realloc( m_pMem, m_nAllocCount ) );
	if ( m_nAllocCount > oldSize )
		V_memset( m_pMem + oldSize, 0, m_nAllocCount - oldSize );
	else if ( oldSize > m_nAllocCount )
		V_memset( m_pMem + m_nAllocCount, 0, oldSize - m_nAllocCount );
}


ScriptString::ScriptString( const char* pString )
{
	Set( pString );
}

ScriptString::ScriptString( const char* pString, int length )
{
	SetDirect( pString, length );
}

ScriptString::ScriptString( const ScriptString& string )
{
	SetDirect( string.Get(), string.Length() );
}

int ScriptString::Length() const
{
	return Max( m_pString.Count() - 1, 0 );
}

bool ScriptString::IsEmpty() const
{
	return !m_pString.Count() || m_pString[0] == 0;
}

int __cdecl ScriptString::SortCaseInsensitive( const ScriptString* pString1, const ScriptString* pString2 )
{
	return V_stricmp( pString1->String(), pString2->String() );
}

int __cdecl ScriptString::SortCaseSensitive( const ScriptString* pString1, const ScriptString* pString2 )
{
	return V_strcmp( pString1->String(), pString2->String() );
}

// Converts to c-strings
ScriptString::operator const char*() const
{
	return Get();
}

//-----------------------------------------------------------------------------
void ScriptString::SetDirect( const char* pValue, int nChars )
{
	if ( pValue && nChars > 0 )
	{
		if ( pValue == m_pString.Base() )
		{
			AssertMsg( nChars == m_pString.Count() - 1, "ScriptString::SetDirect does not support resizing strings in place." );
			return; // Do nothing. Realloc in AllocMemory might move pValue's location resulting in a bad memcpy.
		}

		Assert( nChars <= Min<int>( strnlen( pValue, nChars ) + 1, nChars ) );
		m_pString.ResizeTo( nChars + 1 );
		Q_memcpy( m_pString.Base(), pValue, nChars );
	}
	else
	{
		Clear();
	}
}

void ScriptString::Set( const char* pValue )
{
	int length = pValue ? V_strlen( pValue ) : 0;
	SetDirect( pValue, length );
}

void ScriptString::SetLength( int nLen )
{
	if ( nLen > 0 )
	{
		int prevLen = Max( m_pString.Count() - 1, 0 );
		m_pString.ResizeTo( nLen + 1 );
		if ( nLen > prevLen )
			V_memset( m_pString.Base() + prevLen, 0, nLen - prevLen );
	}
	else
	{
		Clear();
	}
}

const char *ScriptString::Get() const
{
	if ( !m_pString.Count() )
		return "";
	return m_pString.Base();
}

char *ScriptString::GetForModify()
{
	if ( !m_pString.Count() )
		m_pString.Grow();

	return m_pString.Base();
}

char ScriptString::operator[]( int i ) const
{
	if ( !m_pString.Count() )
		return '\0';

	if ( i >= Length() )
		return '\0';

	return m_pString[i];
}

void ScriptString::Clear()
{
	m_pString.Purge();
}

bool ScriptString::IsEqual_CaseSensitive( const char* src ) const
{
	if ( !src )
		return Length() == 0;
	return V_strcmp( Get(), src ) == 0;
}

bool ScriptString::IsEqual_CaseInsensitive( const char* src ) const
{
	if ( !src )
		return Length() == 0;
	return V_stricmp( Get(), src ) == 0;
}


void ScriptString::ToLowerSelf()
{
	if ( !m_pString.Count() )
		return;

	V_strlower( m_pString.Base() );
}

void ScriptString::ToUpperSelf()
{
	if ( !m_pString.Count() )
		return;

	V_strupr( m_pString.Base() );
}

ScriptString& ScriptString::operator=( const ScriptString& src )
{
	SetDirect( src.Get(), src.Length() );
	return *this;
}

ScriptString& ScriptString::operator=( const char* src )
{
	Set( src );
	return *this;
}

bool ScriptString::operator==( const ScriptString& src ) const
{
	if ( IsEmpty() )
		return src.IsEmpty();
	else
	{
		if ( src.IsEmpty() )
			return false;

		return Q_strcmp( m_pString.Base(), src.m_pString.Base() ) == 0;
	}
}

ScriptString& ScriptString::operator+=( const ScriptString& rhs )
{
	const int lhsLength( Length() );
	const int rhsLength( rhs.Length() );

	if ( !rhsLength )
		return *this;

	const int requestedLength( lhsLength + rhsLength );

	m_pString.ResizeTo( requestedLength + 1 );
	Q_memcpy( m_pString.Base() + lhsLength, rhs.m_pString.Base(), rhsLength );

	return *this;
}

ScriptString& ScriptString::operator+=( const char* rhs )
{
	const int lhsLength( Length() );
	const int rhsLength( V_strlen( rhs ) );
	const int requestedLength( lhsLength + rhsLength );

	if ( !requestedLength )
		return *this;

	m_pString.ResizeTo( requestedLength + 1 );
	Q_memcpy( m_pString.Base() + lhsLength, rhs, rhsLength );

	return *this;
}

ScriptString &ScriptString::operator+=( char c )
{
	const int lhsLength( Length() );

	m_pString.Grow();
	m_pString[lhsLength] = c;

	return *this;
}

bool ScriptString::MatchesPattern( const ScriptString& Pattern, int nFlags ) const
{
	const char* pszSource  = String();
	const char* pszPattern = Pattern.String();
	bool        bExact     = true;

	while ( true )
	{
		if ( *pszPattern == 0 )
			return *pszSource == 0;

		if ( *pszPattern == '*' )
		{
			pszPattern++;

			if ( *pszPattern == 0 )
				return true;

			bExact = false;
			continue;
		}

		int nLength = 0;

		while ( *pszPattern != '*' && *pszPattern != 0 )
		{
			nLength++;
			pszPattern++;
		}

		while ( true )
		{
			const char* pszStartPattern = pszPattern - nLength;
			const char* pszSearch       = pszSource;

			for ( int i = 0; i < nLength; i++, pszSearch++, pszStartPattern++ )
			{
				if ( *pszSearch == 0 )
					return false;

				if ( *pszSearch != *pszStartPattern )
					break;
			}

			if ( pszSearch - pszSource == nLength )
				break;

			if ( bExact )
				return false;

			if ( ( nFlags & PATTERN_DIRECTORY ) != 0 )
			{
				if ( *pszPattern != '/' && *pszSource == '/' )
					return false;
			}

			pszSource++;
		}

		pszSource += nLength;
	}
}


int ScriptString::Format( const char* pFormat, ... )
{
	va_list marker;

	va_start( marker, pFormat );
	int len = FormatV( pFormat, marker );
	va_end( marker );

	return len;
}

//--------------------------------------------------------------------------------------------------
// This can be called from functions that take varargs.
//--------------------------------------------------------------------------------------------------

int ScriptString::FormatV( const char* pFormat, va_list marker )
{
	char tmpBuf[4096]; //< Nice big 4k buffer, as much memory as my first computer had, a Radio Shack Color Computer

	//va_start( marker, pFormat );
	int len = V_vsprintf_safe( tmpBuf, pFormat, marker );
	//va_end( marker );
	Set( tmpBuf );
	return len;
}

//-----------------------------------------------------------------------------
// Strips the trailing slash
//-----------------------------------------------------------------------------
void ScriptString::StripTrailingSlash()
{
	if ( IsEmpty() )
		return;

	int  nLastChar = Length() - 1;
	char c         = m_pString[nLastChar];
	if ( c == '\\' || c == '/' )
		SetLength( nLastChar );
}

void ScriptString::FixSlashes( char cSeparator/*=CORRECT_PATH_SEPARATOR*/ )
{
	if ( m_pString.Count() )
		V_FixSlashes( m_pString.Base(), cSeparator );
}

//-----------------------------------------------------------------------------
// Trim functions
//-----------------------------------------------------------------------------
void ScriptString::TrimLeft( char cTarget )
{
	int nIndex = 0;

	if ( IsEmpty() )
		return;

	while ( m_pString[nIndex] == cTarget )
		++nIndex;

	// We have some whitespace to remove
	if ( nIndex > 0 )
	{
		V_memcpy( m_pString.Base(), &m_pString[nIndex], Length() - nIndex );
		SetLength( Length() - nIndex );
	}
}


void ScriptString::TrimLeft( const char* szTargets )
{
	if ( IsEmpty() )
		return;

	int i;
	for ( i = 0; m_pString[i] != 0; i++ )
	{
		bool bWhitespace = false;

		for ( int j = 0; szTargets[j] != 0; j++ )
		{
			if ( m_pString[i] == szTargets[j] )
			{
				bWhitespace = true;
				break;
			}
		}

		if ( !bWhitespace )
			break;
	}

	// We have some whitespace to remove
	if ( i > 0 )
	{
		V_memcpy( m_pString.Base(), &m_pString[i], Length() - i );
		SetLength( Length() - i );
	}
}


void ScriptString::TrimRight( char cTarget )
{
	const int nLastCharIndex = Length() - 1;
	int nIndex = nLastCharIndex;

	while ( nIndex >= 0 && m_pString[nIndex] == cTarget )
		--nIndex;

	// We have some whitespace to remove
	if ( nIndex < nLastCharIndex )
	{
		m_pString[nIndex + 1] = 0;
		SetLength( nIndex + 1 );
	}
}


void ScriptString::TrimRight( const char* szTargets )
{
	const int nLastCharIndex = Length() - 1;

	int i;
	for ( i = nLastCharIndex; i > 0; i-- )
	{
		bool bWhitespace = false;

		for ( int j = 0; szTargets[j] != 0; j++ )
		{
			if ( m_pString[i] == szTargets[j] )
			{
				bWhitespace = true;
				break;
			}
		}

		if ( !bWhitespace )
			break;
	}

	// We have some whitespace to remove
	if ( i < nLastCharIndex )
	{
		m_pString[i + 1] = 0;
		SetLength( i + 1 );
	}
}

void ScriptString::TrimSelf( char cTarget )
{
	TrimLeft( cTarget );
	TrimRight( cTarget );
}

void ScriptString::TrimSelf( const char* szTargets )
{
	TrimLeft( szTargets );
	TrimRight( szTargets );
}

ScriptString ScriptString::Slice( int32 nStart, int32 nEnd ) const
{
	int length = Length();
	if ( length == 0 )
		return ScriptString();

	if ( nStart < 0 )
		nStart = length - ( -nStart % length );
	else if ( nStart >= length )
		nStart = length;

	if ( nEnd == INT32_MAX )
		nEnd = length;
	else if ( nEnd < 0 )
		nEnd = length - ( -nEnd % length );
	else if ( nEnd >= length )
		nEnd = length;

	if ( nStart >= nEnd )
		return {};

	const char* pIn = String();

	ScriptString ret;
	ret.SetDirect( pIn + nStart, nEnd - nStart );
	return ret;
}

// Grab a substring starting from the left or the right side.
ScriptString ScriptString::Left( int32 nChars ) const
{
	return Slice( 0, nChars );
}

ScriptString ScriptString::Right( int32 nChars ) const
{
	return Slice( -nChars );
}

ScriptString ScriptString::Replace( char cFrom, char cTo ) const
{
	if ( !m_pString.Count() )
		return {};

	ScriptString ret = *this;
	int len = ret.Length();
	for ( int i=0; i < len; i++ )
	{
		if ( ret.m_pString[i] == cFrom )
			ret.m_pString[i] = cTo;
	}

	return ret;
}

ScriptString ScriptString::Replace( const char* pszFrom, const char* pszTo ) const
{
	Assert( pszTo ); // Can be 0 length, but not null
	Assert( pszFrom && *pszFrom ); // Must be valid and have one character.

	const char* pos = V_strstr( String(), pszFrom );
	if ( !pos )
		return *this;

	const char* pFirstFound = pos;

	// count number of search string
	int nSearchCount = 0;
	int nSearchLength = V_strlen( pszFrom );
	while ( pos )
	{
		nSearchCount++;
		int nSrcOffset = ( pos - String() ) + nSearchLength;
		pos = V_strstr( String() + nSrcOffset, pszFrom );
	}

	// allocate the new string
	int nReplaceLength = V_strlen( pszTo );
	int nAllocOffset = nSearchCount * ( nReplaceLength - nSearchLength );
	size_t srcLength = Length();
	ScriptString strDest;
	size_t destLength = srcLength + nAllocOffset;
	strDest.SetLength( destLength );

	// find and replace the search string
	pos = pFirstFound;
	int nDestOffset = 0;
	int nSrcOffset = 0;
	while ( pos )
	{
		// Found an instance
		int nCurrentSearchOffset = pos - String();
		int nCopyLength = nCurrentSearchOffset - nSrcOffset;
		V_strncpy( strDest.GetForModify() + nDestOffset, String() + nSrcOffset, nCopyLength + 1 );
		nDestOffset += nCopyLength;
		V_strncpy( strDest.GetForModify() + nDestOffset, pszTo, nReplaceLength + 1 );
		nDestOffset += nReplaceLength;

		nSrcOffset = nCurrentSearchOffset + nSearchLength;
		pos = V_strstr( String() + nSrcOffset, pszFrom );
	}

	// making sure that the left over string from the source is the same size as the left over dest buffer
	Assert( destLength - nDestOffset == srcLength - nSrcOffset );
	if ( destLength - nDestOffset > 0 )
		V_strncpy( strDest.GetForModify() + nDestOffset, String() + nSrcOffset, destLength - nDestOffset + 1 );

	return strDest;
}

ScriptString ScriptString::AbsPath( const char* pStartingDir ) const
{
	char szNew[MAX_PATH];
	V_MakeAbsolutePath( szNew, sizeof( szNew ), String(), pStartingDir );
	return ScriptString( szNew );
}

ScriptString ScriptString::UnqualifiedFilename() const
{
	const char* pFilename = V_UnqualifiedFileName( String() );
	return ScriptString( pFilename );
}

ScriptString ScriptString::DirName() const
{
	ScriptString ret = *this;
	V_StripLastDir( ret.GetForModify(), ret.Length() + 1 );
	V_StripTrailingSlash( ret.GetForModify() );
	return ret;
}

ScriptString ScriptString::StripExtension() const
{
	char szTemp[MAX_PATH];
	V_StripExtension( String(), szTemp, sizeof( szTemp ) );
	return szTemp;
}

ScriptString ScriptString::StripFilename() const
{
	const char*  pFilename    = V_UnqualifiedFileName( Get() ); // NOTE: returns 'Get()' on failure, never nullptr
	int          nCharsToCopy = pFilename - Get();
	ScriptString result;
	result.SetDirect( Get(), nCharsToCopy );
	result.StripTrailingSlash();
	return result;
}

ScriptString ScriptString::GetBaseFilename() const
{
	char szTemp[MAX_PATH];
	V_FileBase( String(), szTemp, sizeof( szTemp ) );
	return szTemp;
}

ScriptString ScriptString::GetExtension() const
{
	char szTemp[MAX_PATH];
	V_ExtractFileExtension( String(), szTemp, sizeof( szTemp ) );
	return szTemp;
}


ScriptString ScriptString::PathJoin( const char* pStr1, const char* pStr2 )
{
	char szPath[MAX_PATH];
	V_ComposeFileName( pStr1, pStr2, szPath, sizeof( szPath ) );
	return szPath;
}

ScriptString ScriptString::operator+( const char* pOther ) const
{
	ScriptString s = *this;
	s += pOther;
	return s;
}

ScriptString ScriptString::operator+( const ScriptString& other ) const
{
	ScriptString s = *this;
	s += other;
	return s;
}

ScriptString ScriptString::operator+( char rhs ) const
{
	ScriptString ret = *this;
	ret += rhs;
	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: concatenate the provided string to our current content
//-----------------------------------------------------------------------------
void ScriptString::Append( const char* pchAddition )
{
	*this += pchAddition;
}

void ScriptString::Append( const char* pchAddition, int nChars )
{
	nChars = Min<int>( nChars, V_strlen( pchAddition ) );

	const int lhsLength( Length() );
	const int rhsLength( nChars );
	const int requestedLength( lhsLength + rhsLength );

	m_pString.ResizeTo( requestedLength + 1 );
	const int allocatedLength( requestedLength );
	const int copyLength( allocatedLength - lhsLength < rhsLength ? allocatedLength - lhsLength : rhsLength );
	V_memcpy( GetForModify() + lhsLength, pchAddition, copyLength );
	m_pString[allocatedLength] = '\0';
}

// Shared static empty string.
const ScriptString& ScriptString::GetEmptyString()
{
	static const ScriptString s_emptyString;
	return s_emptyString;
}

ScriptString& ScriptString::AssignString( const ScriptString& other )
{
	return *this = other;
}

template <typename T>
ScriptString& ScriptString::AssignNumber( T other )
{
	Set( CNumStr( other ) );
	return *this;
}

ScriptString& ScriptString::AppendString( const ScriptString& other )
{
	*this += other;
	return *this;
}

template <typename T>
ScriptString& ScriptString::AppendNumber( T other )
{
	*this += CNumStr( other );
	return *this;
}

int64 ScriptString::ToInt64() const
{
	return V_atoi64( Get() );
}

uint64 ScriptString::ToUInt64() const
{
	return V_atoui64( Get() );
}

float ScriptString::ToFloat() const
{
	return V_atof( Get() );
}

double ScriptString::ToDouble() const
{
	return V_atod( Get() );
}

char* ScriptString::CharAt( uint32 n )
{
	if ( n >= static_cast<uint32>( Length() ) )
	{
		asGetActiveContext()->SetException( "Out of range" );
		return nullptr;
	}
	return &GetForModify()[n];
}

const char* ScriptString::CharAt( uint32 n ) const
{
	if ( n >= static_cast<uint32>( Length() ) )
	{
		asGetActiveContext()->SetException( "Out of range" );
		return nullptr;
	}
	return &Get()[n];
}

ScriptString ScriptString::ToLower() const
{
	ScriptString str = *this;
	str.ToLowerSelf();
	return str;
}

ScriptString ScriptString::ToUpper() const
{
	ScriptString str = *this;
	str.ToUpperSelf();
	return str;
}

ScriptString ScriptString::Trim() const
{
	ScriptString str = *this;
	str.TrimSelf();
	return str;
}

ScriptString ScriptString::AddString( const ScriptString& other ) const
{
	return *this + other;
}

template <typename T>
ScriptString ScriptString::AddNumber( T other ) const
{
	return *this + CNumStr( other );
}

template <typename T>
ScriptString ScriptString::PrependNumber( T other ) const
{
	return CNumStr( other ) + *this;
}

ScriptString ScriptString::Replace( const ScriptString& pszFrom, const ScriptString& pszTo ) const
{
	return Replace( pszFrom.Get(), pszTo.Get() );
}

uint32 ScriptString::Find( const ScriptString& pattern, uint32 skip ) const
{
	const uint len    = Length();
	const uint patLen = pattern.Length();
	const char* pat = pattern;
	const char* f   = Get();

	if ( patLen > len || skip > len - patLen )
		return static_cast<size_t>( -1 );

	if ( patLen == 0 )
		return skip;

	const auto _Possible_matches_end = f + ( len - patLen ) + 1;
	for ( auto _Match_try = f + skip;; ++_Match_try )
	{
		_Match_try = std::char_traits<char>::find( _Match_try, static_cast<size_t>( _Possible_matches_end - _Match_try ), *pat );
		if ( !_Match_try )
			return static_cast<size_t>( -1 );

		if ( std::char_traits<char>::compare( _Match_try, pat, patLen ) == 0 )
			return static_cast<size_t>( _Match_try - f );
	}
}

bool ScriptString::IsAlpha() const
{
	const int len = Length();
	const char* str = Get();
	for ( int i = 0; i < len; i++ )
		if ( !isalpha( str[i] ) )
			return false;
	return len != 0;
}

bool ScriptString::IsNumeric() const
{
	const int len = Length();
	const char* str = Get();
	for ( int i = 0; i < len; i++ )
		if ( !isdigit( str[i] ) )
			return false;
	return len != 0;
}

bool ScriptString::IsAlphaNumerical() const
{
	const int len = Length();
	const char* str = Get();
	for ( int i = 0; i < len; i++ )
		if ( !isalnum( str[i] ) )
			return false;
	return len != 0;
}

static CScriptArray* StringSplit( const ScriptString& delim, const ScriptString& str )
{
	// Obtain a pointer to the engine
	asIScriptContext* ctx = asGetActiveContext();
	asIScriptEngine* engine = ctx->GetEngine();

	// TODO: This should only be done once
	// TODO: This assumes that CScriptArray was already registered
	asITypeInfo* arrayType = engine->GetTypeInfoByDecl( "array<string>" );

	// Create the array object
	auto array = static_cast<CScriptArrayT<ScriptString>*>( CScriptArray::Create( arrayType ) );

	// Find the existence of the delimiter in the input string
	int pos = 0, prev = 0, count = 0;
	while ( ( pos = str.Find( delim, prev ) ) != -1 )
	{
		// Add the part to the array
		array->Resize( array->GetSize() + 1 );
		Construct( array->At( count ), &str.Get()[prev], pos - prev );

		// Find the next part
		++count;
		prev = pos + delim.Length();
	}

	// Add the remaining part
	array->Resize( array->GetSize() + 1 );
	Construct( array->At( count ), &str.Get()[prev] );

	return array;
}

static ScriptString StringJoin( const CScriptArrayT<ScriptString>& array, const ScriptString& delim )
{
	// Create the new string
	ScriptString str;
	if ( array.GetSize() )
	{
		size_t n;
		for ( n = 0; n < array.GetSize() - 1; n++ )
		{
			str += *array.At( n );
			str += delim;
		}

		// Add the last part
		str += *array.At( n );
	}

	return str;
}

#include "tier0/memdbgoff.h"

static void ConstructString( ScriptString* thisPtr )
{
	new ( thisPtr ) ScriptString;
}

static void ConstructStringCopy( ScriptString* thisPtr, const ScriptString& other )
{
	new ( thisPtr ) ScriptString( other );
}

template <typename T>
static void ConstructStringFromNumber( ScriptString* thisPtr, T other )
{
	new ( thisPtr ) ScriptString( CNumStr( other ) );
}

BEGIN_AS_NAMESPACE

void RegisterString( asIScriptEngine* engine )
{
	int r;

	r = engine->RegisterObjectType( "string", sizeof( ScriptString ), asOBJ_VALUE | asGetTypeTraits<ScriptString>() ); Assert( r >= 0 );

	// register the string factory
	r = engine->RegisterStringFactory( "string", &stringFactory ); Assert( r >= 0 );

	// register object behaviours
	r = engine->RegisterObjectBehaviour( "string", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION( ConstructString ), asCALL_CDECL_OBJFIRST ); Assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "string", asBEHAVE_DESTRUCT, "void f()", asFUNCTION( Destruct<ScriptString> ), asCALL_CDECL_OBJFIRST ); Assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "string", asBEHAVE_CONSTRUCT, "void f(const string &in other)", asFUNCTION( ConstructStringCopy ), asCALL_CDECL_OBJFIRST ); Assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "string", asBEHAVE_CONSTRUCT, "void f(bool num)", asFUNCTION( ConstructStringFromNumber<bool> ), asCALL_CDECL_OBJFIRST ); Assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "string", asBEHAVE_CONSTRUCT, "void f(int32 num)", asFUNCTION( ConstructStringFromNumber<int32> ), asCALL_CDECL_OBJFIRST ); Assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "string", asBEHAVE_CONSTRUCT, "void f(uint32 num)", asFUNCTION( ConstructStringFromNumber<uint32> ), asCALL_CDECL_OBJFIRST ); Assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "string", asBEHAVE_CONSTRUCT, "void f(int64 num)", asFUNCTION( ConstructStringFromNumber<int64> ), asCALL_CDECL_OBJFIRST ); Assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "string", asBEHAVE_CONSTRUCT, "void f(uint64 num)", asFUNCTION( ConstructStringFromNumber<uint64> ), asCALL_CDECL_OBJFIRST ); Assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "string", asBEHAVE_CONSTRUCT, "void f(float num)", asFUNCTION( ConstructStringFromNumber<float> ), asCALL_CDECL_OBJFIRST ); Assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "string", asBEHAVE_CONSTRUCT, "void f(double num)", asFUNCTION( ConstructStringFromNumber<double> ), asCALL_CDECL_OBJFIRST ); Assert( r >= 0 );

#ifdef ENABLE_STRING_IMPLICIT_CASTS
	r = engine->RegisterObjectMethod( "string", "int64 opImplConv() const", asMETHOD( ScriptString, ToInt64 ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "uint64 opImplConv() const", asMETHOD( ScriptString, ToUInt64 ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "float opImplConv() const", asMETHOD( ScriptString, ToFloat ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "double opImplConv() const", asMETHOD( ScriptString, ToDouble ), asCALL_THISCALL ); Assert( r >= 0 );
#endif

	// register object methods

	// assignments
	r = engine->RegisterObjectMethod( "string", "string &opAssign(const string &in str)", asMETHOD( ScriptString, AssignString ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string &opAssign(int64 num)", asMETHOD( ScriptString, AssignNumber<int64> ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string &opAssign(uint64 num)", asMETHOD( ScriptString, AssignNumber<uint64> ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string &opAssign(double num)", asMETHOD( ScriptString, AssignNumber<double> ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string &opAssign(float num)", asMETHOD( ScriptString, AssignNumber<float> ), asCALL_THISCALL ); Assert( r >= 0 );

	// register the index operator, both as a mutator and as an inspector
	r = engine->RegisterObjectMethod( "string", "uint8 &opIndex(uint idx)", asMETHODPR( ScriptString, CharAt, ( uint ), char* ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "const uint8 &opIndex(uint idx) const", asMETHODPR( ScriptString, CharAt, ( uint ) const, const char* ), asCALL_THISCALL ); Assert( r >= 0 );

	// +=
	r = engine->RegisterObjectMethod( "string", "string &opAddAssign(const string &in)", asMETHOD( ScriptString, AppendString ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string &opAddAssign(int64)", asMETHOD( ScriptString, AppendNumber<int64> ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string &opAddAssign(uint64)", asMETHOD( ScriptString, AppendNumber<uint64> ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string &opAddAssign(double)", asMETHOD( ScriptString, AppendNumber<double> ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string &opAddAssign(float)", asMETHOD( ScriptString, AppendNumber<float> ), asCALL_THISCALL ); Assert( r >= 0 );

	// +
	r = engine->RegisterObjectMethod( "string", "string opAdd(const string &in) const", asMETHOD( ScriptString, AddString ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string opAdd(int64) const", asMETHOD( ScriptString, AddNumber<int64> ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string opAdd(uint64) const", asMETHOD( ScriptString, AddNumber<uint64> ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string opAdd_r(int64) const", asMETHOD( ScriptString, PrependNumber<int64> ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string opAdd_r(uint64) const", asMETHOD( ScriptString, PrependNumber<uint64> ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string opAdd(double) const", asMETHOD( ScriptString, AddNumber<double> ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string opAdd_r(double) const", asMETHOD( ScriptString, PrependNumber<double> ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string opAdd(float) const", asMETHOD( ScriptString, AddNumber<float> ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string opAdd_r(float) const", asMETHOD( ScriptString, PrependNumber<float> ), asCALL_THISCALL ); Assert( r >= 0 );

	// ==
	r = engine->RegisterObjectMethod( "string", "bool opEquals(const string &in) const", asMETHODPR( ScriptString, operator==, ( const ScriptString& ) const, bool ), asCALL_THISCALL ); Assert( r >= 0 );

	r = engine->RegisterObjectMethod( "string", "uint len() const", asMETHOD( ScriptString, Length ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "uint length() const", asMETHOD( ScriptString, Length ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "uint resize() const", asMETHOD( ScriptString, SetLength ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "bool empty() const", asMETHOD( ScriptString, IsEmpty ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string tolower() const", asMETHOD( ScriptString, ToLower ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string toupper() const", asMETHOD( ScriptString, ToUpper ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string trim() const", asMETHOD( ScriptString, Trim ), asCALL_THISCALL ); Assert( r >= 0 );

	r = engine->RegisterObjectMethod( "string", "int64 toInt() const", asMETHOD( ScriptString, ToInt64 ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "float toFloat() const", asMETHOD( ScriptString, ToFloat ), asCALL_THISCALL ); Assert( r >= 0 );

	r = engine->RegisterObjectMethod( "string", "uint locate(const string &in, const uint = 0) const", asMETHOD( ScriptString, Find ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string substr(const int start, const int length) const", asMETHODPR( ScriptString, Slice, ( int32, int32 ) const, ScriptString ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string subString(const int start, const int length) const", asMETHODPR( ScriptString, Slice, ( int32, int32 ) const, ScriptString ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string substr(const int start) const", asMETHODPR( ScriptString, Slice, ( int32 ) const, ScriptString ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "string subString(const int start) const", asMETHODPR( ScriptString, Slice, ( int32 ) const, ScriptString ), asCALL_THISCALL ); Assert( r >= 0 );

	r = engine->RegisterObjectMethod( "string", "string replace(const string &in search, const string &in replace) const", asMETHODPR( ScriptString, Replace, ( const ScriptString&, const ScriptString& ) const, ScriptString ), asCALL_THISCALL ); Assert( r >= 0 );

	r = engine->RegisterObjectMethod( "string", "bool isAlpha() const", asMETHOD( ScriptString, IsAlpha ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "bool isNumerical() const", asMETHOD( ScriptString, IsNumeric ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "bool isNumeric() const", asMETHOD( ScriptString, IsNumeric ), asCALL_THISCALL ); Assert( r >= 0 );
	r = engine->RegisterObjectMethod( "string", "bool isAlphaNumerical() const", asMETHOD( ScriptString, IsAlphaNumerical ), asCALL_THISCALL ); Assert( r >= 0 );

	r = engine->RegisterObjectMethod( "string", "array<string>@ split(const string &in) const", asFUNCTION( StringSplit ), asCALL_CDECL_OBJLAST ); Assert( r >= 0 );
	r = engine->RegisterGlobalFunction( "string join(const array<string> &in, const string &in)", asFUNCTION( StringJoin ), asCALL_CDECL ); Assert( r >= 0 );

	NOTE_UNUSED( r );
}

END_AS_NAMESPACE