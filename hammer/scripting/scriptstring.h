
#pragma once

#include "tier1/utlstring.h"
#include "tier1/utlmemory.h"

#include "angelscript.h"

#include "robin_hood.h"

class ScriptString
{
public:
	enum TUtlStringPattern
	{
		PATTERN_NONE      = 0x00000000,
		PATTERN_DIRECTORY = 0x00000001
	};

public:
	ScriptString() = default;
	ScriptString( const char* pString );
	ScriptString( const char* pString, int length );
	ScriptString( const ScriptString& string );

	ScriptString( ScriptString&& rhs ) noexcept
	{
		m_pString.Swap( rhs.m_pString );
	}

	ScriptString& operator=( ScriptString&& rhs ) noexcept
	{
		m_pString.Swap( rhs.m_pString );
		return *this;
	}

	~ScriptString() = default;

	const char* Get() const;
	void        Set( const char* pValue );
	operator const char*() const;
	explicit operator bool() const { return m_pString.Count() != 0; }
	bool operator!() const { return m_pString.Count() == 0; }

	void SetDirect( const char* pValue, int nChars );

	const char* String() const { return Get(); }

	int Length() const;
	bool IsEmpty() const;

	void  SetLength( int nLen );
	char* GetForModify();
	void  Clear();

	void ToLowerSelf();
	void ToUpperSelf();
	void Append( const char* pAddition, int nChars );

	void Append( const char* pchAddition );
	void Append( const char chAddition )
	{
		char temp[2] = { chAddition, 0 };
		Append( temp );
	}

	void StripTrailingSlash();
	void FixSlashes( char cSeparator = CORRECT_PATH_SEPARATOR );

	ScriptString& AssignString( const ScriptString& other );
	template <typename T>
	ScriptString& AssignNumber( T other );

	ScriptString& AppendString( const ScriptString& other );
	template <typename T>
	ScriptString& AppendNumber( T other );

	int64  ToInt64() const;
	uint64 ToUInt64() const;
	float  ToFloat() const;
	double ToDouble() const;

	char*       CharAt( uint32 );
	const char* CharAt( uint32 ) const;

	void         TrimLeft( char cTarget );
	void         TrimLeft( const char* szTargets = "\t\r\n " );
	void         TrimRight( char cTarget );
	void         TrimRight( const char* szTargets = "\t\r\n " );
	void         TrimSelf( char cTarget );
	void         TrimSelf( const char* szTargets = "\t\r\n " );

	ScriptString ToLower() const;
	ScriptString ToUpper() const;
	ScriptString Trim() const;

	bool IsEqual_CaseSensitive( const char* src ) const;
	bool IsEqual_CaseInsensitive( const char* src ) const;

	ScriptString& operator=( const ScriptString& src );
	ScriptString& operator=( const char* src );

	bool operator==( const ScriptString& src ) const;
	bool operator!=( const ScriptString& src ) const { return !operator==( src ); }

	ScriptString& operator+=( const ScriptString& rhs );
	ScriptString& operator+=( const char* rhs );
	ScriptString& operator+=( char c );

	ScriptString        operator+( const char* pOther ) const;
	ScriptString        operator+( const ScriptString& other ) const;
	ScriptString        operator+( char rhs ) const;
	friend ScriptString operator+( const char* lhs, const ScriptString& rhs )
	{
		ScriptString s = lhs;
		s += rhs;
		return s;
	}

	bool MatchesPattern( const ScriptString& Pattern, int nFlags = 0 ) const; // case SENSITIVE, use * for wildcard in pattern string

	char operator[]( int i ) const;

	FMTFUNCTION_WIN( 2, 3 ) int Format( PRINTF_FORMAT_STRING const char* pFormat, ... ) FMTFUNCTION( 2, 3 );
	int FormatV( PRINTF_FORMAT_STRING const char* pFormat, va_list marker );

	bool IsAlpha() const;
	bool IsNumeric() const;
	bool IsAlphaNumerical() const;

	ScriptString AddString( const ScriptString& other ) const;
	template <typename T>
	ScriptString AddNumber( T other ) const;
	template <typename T>
	ScriptString PrependNumber( T other ) const;

	ScriptString Slice( int32 nStart = 0 ) const { return Slice( nStart, INT_MAX ); }
	ScriptString Slice( int32 nStart, int32 nEnd ) const;
	ScriptString Replace( const ScriptString& pszFrom, const ScriptString& pszTo ) const;
	uint32       Find( const ScriptString& pattern, uint32 skip ) const;
	ScriptString Left( int32 nChars ) const;
	ScriptString Right( int32 nChars ) const;
	ScriptString Replace( char cFrom, char cTo ) const;
	ScriptString Replace( const char* pszFrom, const char* pszTo ) const;
	ScriptString AbsPath( const char* pStartingDir = nullptr ) const;
	ScriptString UnqualifiedFilename() const;
	ScriptString DirName() const;
	ScriptString StripExtension() const;
	ScriptString StripFilename() const;
	ScriptString GetBaseFilename() const;
	ScriptString GetExtension() const;

	static ScriptString PathJoin( const char* pStr1, const char* pStr2 );

	static int __cdecl SortCaseInsensitive( const ScriptString* pString1, const ScriptString* pString2 );
	static int __cdecl SortCaseSensitive( const ScriptString* pString1, const ScriptString* pString2 );

	static const ScriptString& GetEmptyString();

private:

	class Storage
	{
	public:
		~Storage() { Purge(); }

		void Swap( Storage& other );

		void Grow();
		void Purge();
		void ResizeTo( size_t size );

		int Count() const { return m_nAllocCount; }

		char& operator[]( size_t i ) { Assert( i < m_nAllocCount ); return m_pMem[i]; }
		const char& operator[]( size_t i ) const { Assert( i < m_nAllocCount ); return m_pMem[i]; }
		char* Base() { return m_pMem; }
		const char* Base() const { return m_pMem; }

	private:
		char* m_pMem = nullptr;
		size_t m_nAllocCount = 0;
	};

	Storage m_pString;
};

extern uint32 MurmurHash3_32( const void* key, size_t len, uint32 seed, bool bCaselessStringVariant = false );
namespace robin_hood
{
	template <>
	struct hash<ScriptString>
	{
		size_t operator()( ScriptString const& str ) const noexcept
		{
			return ::MurmurHash3_32( str.Get(), str.Length(), 1047 /*anything will do for a seed*/, false );
		}
	};
} // namespace robin_hood

BEGIN_AS_NAMESPACE

void RegisterString( asIScriptEngine* engine );

END_AS_NAMESPACE