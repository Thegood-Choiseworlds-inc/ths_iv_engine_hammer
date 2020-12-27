//====== Copyright c 1996-2007, Valve Corporation, All rights reserved. =======//
//
// Purpose:
//
// $NoKeywords: $
//
//=============================================================================//

#include "stdafx.h"
#include "platform.h"
#include "utlvector.h"
#include "utlbuffer.h"
#include "chunkfile.h"
#include "fgdlib/wckeyvalues.h"
#include "vmfmeshdatasupport.h"


namespace KvEncoder
{
	inline char DecodeChar( char ch )
	{
		if ( ch == '-' )
			return 0;
		if ( ch >= 'a' && ch <= '{' )
			return 1 + ch - 'a';
		if ( ch >= 'A' && ch <= '[' )
			return 28 + ch - 'A';
		if ( ch >= '0' && ch <= '9' )
			return 55 + ch - '0';
		return -1;
	}

	inline char EncodeChar( char ch )
	{
		if ( ch == 0 )
			return '-';
		if ( ch >= 1 && ch <= 27 )
			return 'a' + ch - 1;
		if ( ch >= 28 && ch <= 54 )
			return 'A' + ch - 28;
		if ( ch >= 55 && ch <= 64 )
			return '0' + ch - 55;
		return -1;
	}

	inline unsigned long EncodeByte( unsigned char x )
	{
		unsigned int iSegment = x / 64;
		unsigned int iOffset = x % 64;
		return (
			( unsigned long ) ( ( iOffset ) & 0xFFFF ) |
			( unsigned long ) ( ( ( iSegment ) & 0xFFFF ) << 16 )
			);
	}

	inline unsigned char DecodeByte( int iSegment, int iOffset )
	{
		return iSegment * 64 + iOffset;
	}

	inline int GuessEncodedLength( int nLength )
	{
		return 4 * ( ( nLength + 2 ) / 3 );
	}

	inline int GuessDecodedLength( int nLength )
	{
		return 3 * ( ( nLength + 3 ) / 4 );
	}

	inline bool Encode( CUtlBuffer &src, CUtlBuffer &dst )
	{
		int numBytes = dst.Size();
		int nReqLen = GuessEncodedLength( src.TellPut() );
		if ( numBytes < nReqLen )
			return false;

		char *pBase = (char *) dst.Base();
		char *pSrc = (char *) src.Base();
		int srcBytes = src.TellPut();
		while ( srcBytes > 0 )
		{
			char *pSegs = pBase;
			char *pOffs = pBase + 1;
			int flags = 0;

			for ( int k = 0; k < 3; ++ k )
			{
				if ( srcBytes -- > 0 )
				{
					unsigned long enc = EncodeByte( *pSrc ++ );
					*( pOffs ++ ) = EncodeChar( enc & 0xFFFF );
					flags |= ( enc >> 16 ) << ( 2 * k );
				}
				else
				{
					*( pOffs ++ ) = EncodeChar( 64 );
				}
			}

			*pSegs = EncodeChar( flags );
			pBase = pOffs;
		}

		dst.SeekPut( CUtlBuffer::SEEK_HEAD, nReqLen );
		return true;
	}

	inline bool Decode( CUtlBuffer &src, CUtlBuffer &dst )
	{
		int numBytesLimit = dst.Size();

		char *pBase = (char *) src.Base();
		char *pData = (char *) dst.Base();
		char *pBaseEnd = pBase + src.TellPut();

		while ( pBase < pBaseEnd )
		{
			int flags = DecodeChar( *( pBase ++ ) );
			if ( -1 == flags )
				return false;

			for ( int k = 0; k < 3 && pBase < pBaseEnd; ++ k )
			{
				int off = DecodeChar( *( pBase ++ ) );
				if ( off == -1 )
					return false;
				if ( off == 64 )
					continue;

				int seg = flags >> ( 2 * k );
				seg %= 4;

				if ( numBytesLimit --> 0 )
					*( pData ++ ) = DecodeByte( seg, off );
				else
					return false;
			}
		}

		int numBytes = dst.Size() - numBytesLimit;
		dst.SeekPut( CUtlBuffer::SEEK_HEAD, numBytes );

		return true;
	}

}; // namespace KvEncoder


CVmfMeshDataSupport_SaveLoadHandler::CVmfMeshDataSupport_SaveLoadHandler()
{
}

CVmfMeshDataSupport_SaveLoadHandler::~CVmfMeshDataSupport_SaveLoadHandler()
{
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::WriteDataChunk( CChunkFile *pFile, char const *szHash )
{
	ChunkFileResult_t eResult;
	eResult = pFile->BeginChunk( GetCustomSectionName() );
	if ( eResult != ChunkFile_Ok )
		return eResult;

	// Write out our data version
	char szModelDataVer[ 16 ] = {0};
	sprintf( szModelDataVer, "%d", GetCustomSectionVer() );
	eResult = pFile->WriteKeyValue( "version", szModelDataVer );
	if ( eResult != ChunkFile_Ok )
		return eResult;

	// Write our hash
	eResult = pFile->WriteKeyValue( "hash", szHash );
	if ( eResult != ChunkFile_Ok )
		return eResult;

	// Write out additional data
	eResult = OnFileDataWriting( pFile, szHash );
	if ( eResult != ChunkFile_Ok )
		return eResult;

	return pFile->EndChunk();
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::OnFileDataWriting( CChunkFile *pFile, char const *szHash )
{
	return ChunkFile_Ok;
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::WriteBufferData( CChunkFile *pFile, CUtlBuffer &bufData, char const *szPrefix )
{
	int numEncBytes = KvEncoder::GuessEncodedLength( bufData.TellPut() );
	CUtlBuffer bufEncoded;
	bufEncoded.EnsureCapacity( numEncBytes );

	if ( !KvEncoder::Encode( bufData, bufEncoded ) )
		return ChunkFile_Fail;
	numEncBytes = bufEncoded.TellPut();

	// Now we have the encoded data, split it into blocks
	int numBytesPerLine = KEYVALUE_MAX_VALUE_LENGTH - 2;
	int numLines = (numEncBytes + numBytesPerLine - 1 ) / numBytesPerLine;
	int numLastLineBytes = numEncBytes % numBytesPerLine;
	if ( !numLastLineBytes )
		numLastLineBytes = numBytesPerLine;

	// Key buffer
	char chKeyBuf[ KEYVALUE_MAX_KEY_LENGTH ] = {0};
	char chKeyValue[ KEYVALUE_MAX_VALUE_LENGTH ] = {0};

	// Write the data
	ChunkFileResult_t eResult;

	sprintf( chKeyBuf, "%s_ebytes", szPrefix );
	eResult = pFile->WriteKeyValueInt( chKeyBuf, numEncBytes );
	if ( eResult != ChunkFile_Ok )
		return eResult;

	sprintf( chKeyBuf, "%s_rbytes", szPrefix );
	eResult = pFile->WriteKeyValueInt( chKeyBuf, bufData.TellPut() );
	if ( eResult != ChunkFile_Ok )
		return eResult;

	sprintf( chKeyBuf, "%s_lines", szPrefix );
	eResult = pFile->WriteKeyValueInt( chKeyBuf, numLines );
	if ( eResult != ChunkFile_Ok )
		return eResult;

	for ( int ln = 0; ln < numLines; ++ ln )
	{
		int lnLen = ( ln < (numLines - 1) ) ? numBytesPerLine : numLastLineBytes;

		sprintf( chKeyBuf, "%s_ln_%d", szPrefix, ln );
		sprintf( chKeyValue, "%.*s", lnLen, ( char * ) bufEncoded.Base() + ln * numBytesPerLine );

		eResult = pFile->WriteKeyValue( chKeyBuf, chKeyValue );
		if ( eResult != ChunkFile_Ok )
			return eResult;
	}

	return ChunkFile_Ok;

}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::LoadKeyValueBegin( CChunkFile *pFile )
{
	m_eLoadState = LOAD_VERSION;
	m_iLoadVer = 0;
	m_hLoadHeader.sHash[0] = 0;
	LoadInitHeader();

	return ChunkFile_Ok;
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::LoadKeyValue( const char *szKey, const char *szValue )
{
	switch ( m_eLoadState )
	{
	case LOAD_VERSION:
		return LoadKeyValue_Hdr( szKey, szValue );

	default:
		switch ( m_iLoadVer )
		{
		case 1:
			return LoadKeyValue_Ver1( szKey, szValue );
		default:
			return ChunkFile_Fail;
		}
	}
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::LoadKeyValue_Hdr( const char *szKey, const char *szValue )
{
	switch ( m_eLoadState )
	{
	case LOAD_VERSION:
		if ( stricmp( szKey, "version" ) )
			return ChunkFile_Fail;
		m_iLoadVer = atoi( szValue );

		switch ( m_iLoadVer )
		{
		case 1:
			m_eLoadState = LOAD_HASH;
			return ChunkFile_Ok;
		default:
			return ChunkFile_Fail;
		}

	default:
		return ChunkFile_Fail;
	}
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::LoadKeyValue_Ver1( const char *szKey, const char *szValue )
{
	const char *szKeyName = szKey;
	const int nPrefixLen = 3;

	switch ( m_eLoadState )
	{
	case LOAD_HASH:
		if ( stricmp( szKey, "hash" ) )
			return ChunkFile_Fail;
		strncpy( m_hLoadHeader.sHash, szValue, sizeof( m_hLoadHeader.sHash ) );
		m_eLoadState = LOAD_PREFIX;
		break;

	case LOAD_PREFIX:
		sprintf( m_hLoadHeader.sPrefix, "%.*s", nPrefixLen, szKey );
		if ( strlen( szKey ) < 4 )
			return ChunkFile_Fail;
		if ( szKey[3] != '_' )
			return ChunkFile_Fail;
		m_eLoadState = LOAD_HEADER;
		// fall-through

	case LOAD_HEADER:
		if ( strnicmp( m_hLoadHeader.sPrefix, szKey, nPrefixLen ) )
			return ChunkFile_Fail;
		if ( szKey[3] != '_' )
			return ChunkFile_Fail;

		szKeyName = szKey + 4;
		if ( !stricmp( szKeyName, "ebytes" ) )
			m_hLoadHeader.numEncBytes = atoi( szValue );
		else if ( !stricmp( szKeyName, "rbytes" ) )
			m_hLoadHeader.numBytes = atoi( szValue );
		else if ( !stricmp( szKeyName, "lines" ) )
			m_hLoadHeader.numLines = atoi( szValue );
		else
			return ChunkFile_Fail;

		if ( !LoadHaveHeader() )
			break;

		m_eLoadState = LOAD_DATA;
		return LoadHaveLines( 0 );

	case LOAD_DATA:
		if ( strnicmp( m_hLoadHeader.sPrefix, szKey, nPrefixLen ) )
			return ChunkFile_Fail;
		if ( szKey[3] != '_' )
			return ChunkFile_Fail;

		szKeyName = szKey + 4;
		if ( strnicmp( szKeyName, "ln", 2 ) || szKeyName[2] != '_' )
			return ChunkFile_Fail;

		szKeyName += 3;
		{
			int iLineNum = atoi( szKeyName );
			if ( iLineNum != m_hLoadHeader.numHaveLines )
				return ChunkFile_Fail;

			m_bufLoadData.Put( szValue, strlen( szValue ) );

			return LoadHaveLines( iLineNum + 1 );
		}

	default:
		return ChunkFile_Fail;
	}

	return ChunkFile_Ok;
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::LoadKeyValueEnd( CChunkFile *pFile, ChunkFileResult_t eLoadResult )
{
	if ( eLoadResult != ChunkFile_Ok )
		return eLoadResult;

	if ( m_eLoadState == LOAD_VERSION )
		return ChunkFile_Ok;

	switch ( m_iLoadVer )
	{
	case 1:
		switch ( m_eLoadState )
		{
		case LOAD_HASH:
		case LOAD_PREFIX:
			return ChunkFile_Ok;

		default:
			return ChunkFile_Fail;
		}

	default:
		return ChunkFile_Fail;
	}
}

void CVmfMeshDataSupport_SaveLoadHandler::LoadInitHeader()
{
	m_hLoadHeader.sPrefix[0] = 0;
	m_hLoadHeader.numLines = -1;
	m_hLoadHeader.numBytes = -1;
	m_hLoadHeader.numEncBytes = -1;
	m_hLoadHeader.numHaveLines = 0;
}

bool CVmfMeshDataSupport_SaveLoadHandler::LoadHaveHeader()
{
	return m_hLoadHeader.numLines >= 0 && m_hLoadHeader.numBytes >= 0 && m_hLoadHeader.numEncBytes >= 0;
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::LoadHaveLines( int numHaveLines )
{
	if ( !numHaveLines )
	{
		m_bufLoadData.EnsureCapacity( m_hLoadHeader.numEncBytes );
		m_bufLoadData.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
	}

	m_hLoadHeader.numHaveLines = numHaveLines;
	if ( m_hLoadHeader.numHaveLines < m_hLoadHeader.numLines )
		return ChunkFile_Ok;
	if ( m_hLoadHeader.numHaveLines > m_hLoadHeader.numLines )
		return ChunkFile_Fail;

	ChunkFileResult_t eRes = LoadSaveFullData();
	if ( eRes != ChunkFile_Ok )
		return eRes;

	LoadInitHeader();
	m_eLoadState = LOAD_PREFIX;
	return ChunkFile_Ok;
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::LoadSaveFullData()
{
	// The filename
	CUtlBuffer bufBytes;
	bufBytes.EnsureCapacity( m_hLoadHeader.numBytes + 0x10 );
	if ( !KvEncoder::Decode( m_bufLoadData, bufBytes ) )
		return ChunkFile_Fail;

	return OnFileDataLoaded( bufBytes );
}
