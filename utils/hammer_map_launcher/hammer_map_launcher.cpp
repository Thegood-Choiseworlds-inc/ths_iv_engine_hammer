#define WIN32_LEAN_AND_MEAN
#define NOWINRES
#define NOSERVICE
#define NORESOURCE
#define NOMCX
#define NOIME
#define NOHELP
#define NOMINMAX
#define NOGDICAPMASKS
#include <Windows.h>
#include <processenv.h>
#include <direct.h>
#include <conio.h>

#include <charconv>
#include <iostream>
#include <string_view>
#include <vector>

#include "style.hpp"

namespace clr
{
	using namespace termcolor;
	static const auto red = _internal::ansi_color( color( 222, 12, 17 ) );
} // namespace clr

static constexpr const std::string_view base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static __forceinline bool is_base64( const unsigned char c ) { return ( isalnum( c ) || ( c == '+' ) || ( c == '/' ) ); }

static size_t base64_decode( const char* encoded_string, void* pOutput, const size_t nOutSize )
{
	size_t in_len = strlen( encoded_string );
	int i = 0;
	int in_ = 0;
	unsigned char char_array_4[4], char_array_3[3];
	std::vector<unsigned char> ret;
	ret.reserve( ( 3 * in_len ) / 4 - 2 );

	while ( in_len-- && ( encoded_string[in_] != '=' ) && is_base64( encoded_string[in_] ) )
	{
		char_array_4[i++] = encoded_string[in_];
		in_++;
		if ( i == 4 )
		{
			for ( i = 0; i < 4; i++ )
				char_array_4[i] = static_cast<unsigned char>( base64_chars.find( char_array_4[i] ) );

			char_array_3[0] = ( char_array_4[0] << 2 ) + ( ( char_array_4[1] & 0x30 ) >> 4 );
			char_array_3[1] = ( ( char_array_4[1] & 0xf ) << 4 ) + ( ( char_array_4[2] & 0x3c ) >> 2 );
			char_array_3[2] = ( ( char_array_4[2] & 0x3 ) << 6 ) + char_array_4[3];

			for ( i = 0; i < 3; i++ )
				ret.emplace_back( char_array_3[i] );
			i = 0;
		}
	}

	if ( i )
	{
		for ( int j = 0; j < i; j++ )
			char_array_4[j] = static_cast<unsigned char>( base64_chars.find( char_array_4[j] ) );

		char_array_3[0] = ( char_array_4[0] << 2 ) + ( ( char_array_4[1] & 0x30 ) >> 4 );
		char_array_3[1] = ( ( char_array_4[1] & 0xf ) << 4 ) + ( ( char_array_4[2] & 0x3c ) >> 2 );

		for ( int j = 0; j < i - 1; j++ )
			ret.emplace_back( char_array_3[j] );
	}

	if ( nOutSize < ret.size() )
	{
		std::cout << "Base64 decode: insufficient output buffer" << std::endl;
		memset( pOutput, 0, nOutSize );
		return 0;
	}

	memcpy( pOutput, ret.data(), ret.size() );
	return ret.size();
}

enum class CmdType : unsigned char
{
	Exec,
	ChangeDir,
	Copy,
	Delete,
	Rename,
	EnsureExists,

	Last
};

#pragma pack(push, 1)
#pragma warning(push)
#pragma warning(disable: 4200)

struct Cmd
{
	unsigned short size;
	CmdType type;
	char command[];
};

#pragma warning(pop)
#pragma pack(pop)

bool cancel = false;
static BOOL WINAPI CtrlHandler( DWORD signal )
{
	if ( signal == CTRL_C_EVENT )
	{
		cancel = true;
		return TRUE;
	}

	return FALSE;
}

static int mychdir( const char* pszDir )
{
	int curdrive = _getdrive();

	// changes to drive/directory
	if ( pszDir[1] == ':' && _chdrive( toupper( pszDir[0] ) - 'A' + 1 ) == -1 )
		return -1;
	if ( _chdir( pszDir ) == -1 )
	{
		// change back to original disk
		(void)_chdrive( curdrive );
		return -1;
	}

	return 0;
}

template <size_t N>
static void GetErrorString( char( &szBuf )[N] )
{
	FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, szBuf, N, NULL );
	if ( char* p = strchr( szBuf, '\r' ) ) p[0] = 0;	// get rid of \r\n
}

int main( int argc, const char* argv[] )
{
	if ( argc <= 2 || ( argc > 3 && argv[1][0] == '-' && !!strcmp( argv[1], "-WaitForKeypress" ) ) )
		return 0;
	{
		const HANDLE console = GetStdHandle( STD_OUTPUT_HANDLE );
		DWORD mode;
		GetConsoleMode( console, &mode );
		SetConsoleMode( console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING );
		SetConsoleCtrlHandler( CtrlHandler, true );
	}
	bool waitForKey = argv[1][0] == '-' && !strcmp( argv[1], "-WaitForKeypress" );
	const char* cmdLine = GetCommandLineA();
	cmdLine += strlen( argv[0] ) + 2 * ( cmdLine[0] == '"' ) + 1;
	cmdLine += waitForKey * 17;
	int nCmds = 0;
	{
		const char* const n = waitForKey ? argv[2] : argv[1];
		const size_t nL = strlen( n );
		std::from_chars( n, n + nL, nCmds );
		cmdLine += nL + 1;
	}
	if ( nCmds <= 0 )
		return 0;

	HANDLE stdOut = nullptr;
	HANDLE stdErr = nullptr;
	HANDLE stdIn = nullptr;
	if ( !DuplicateHandle( GetCurrentProcess(), GetStdHandle( STD_OUTPUT_HANDLE ), GetCurrentProcess(), &stdOut, 0, TRUE, DUPLICATE_SAME_ACCESS ) )
		return -1;
	if ( !DuplicateHandle( GetCurrentProcess(), GetStdHandle( STD_ERROR_HANDLE ), GetCurrentProcess(), &stdErr, 0, TRUE, DUPLICATE_SAME_ACCESS ) )
		return -1;
	if ( !DuplicateHandle( GetCurrentProcess(), GetStdHandle( STD_INPUT_HANDLE ), GetCurrentProcess(), &stdIn, 0, TRUE, DUPLICATE_SAME_ACCESS ) )
		return -1;

	char cmdBuffer[4096] = { 0 };
	const auto cmdLen = base64_decode( cmdLine, cmdBuffer, sizeof( cmdBuffer ) );
	if ( cmdLen == 0 )
		return -1;
	char* cmdPtr = cmdBuffer;
	char* const cmdEnd = cmdBuffer + cmdLen;
	while ( cmdEnd > cmdPtr && nCmds-- > 0 && !cancel )
	{
		Cmd* cmd = reinterpret_cast<Cmd*>( cmdPtr );
		cmdPtr += sizeof( Cmd ) + cmd->size;

		if ( cmd->type == CmdType::Exec )
		{
			std::cout << clr::yellow << "----------------------------------" << clr::reset << std::endl;
			std::cout << clr::yellow << "Running command: " << cmd->command << clr::reset << std::endl;
			std::cout << clr::yellow << "----------------------------------" << clr::reset << std::endl;

			STARTUPINFOA si;
			memset( &si, 0, sizeof( si ) );
			si.cb = sizeof( si );
			si.dwFlags = STARTF_USESTDHANDLES;
			si.hStdInput = stdIn;
			si.hStdError = stdErr;
			si.hStdOutput = stdOut;
			PROCESS_INFORMATION pi;
			if ( CreateProcessA( nullptr, cmd->command, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi ) )
			{
				WaitForSingleObject( pi.hProcess, INFINITE );

				DWORD exitCode = 0;
				if ( GetExitCodeProcess( pi.hProcess, &exitCode ) && exitCode != 0 )
				{
					std::cout << clr::red << "----------------------------------" << clr::reset << std::endl;
					std::cout << clr::red << "process returned " << exitCode << ", exiting" << clr::reset << std::endl;
					std::cout << clr::red << "----------------------------------" << clr::reset << std::endl;
					cancel = true;
				}

				CloseHandle( pi.hProcess );
				CloseHandle( pi.hThread );
			}

			continue;
		}

		if ( cmd->type >= CmdType::Last )
		{
			std::cout << clr::red << "----------------------------------" << clr::reset << std::endl;
			std::cout << clr::red << "unknown command id " << static_cast<int>( cmd->type ) << ", skipping" << clr::reset << std::endl;
			std::cout << clr::red << "----------------------------------" << clr::reset << std::endl;
			continue;
		}

		bool fail = false;
		char errMsg[256] = { 0 };
		if ( cmd->type == CmdType::ChangeDir || cmd->type == CmdType::Delete || cmd->type == CmdType::EnsureExists ) // 1 arg
		{
			if ( cmd->type == CmdType::ChangeDir )
			{
				std::cout << clr::yellow << "----------------------------------" << clr::reset << std::endl;
				std::cout << clr::yellow << "Changing current directory to: " << clr::reset << cmd->command << std::endl;
				std::cout << clr::yellow << "----------------------------------" << clr::reset << std::endl;
				fail = mychdir( cmd->command ) == -1;
				if ( fail )
					strerror_s( errMsg, errno );
			}
			else if ( cmd->type == CmdType::EnsureExists )
			{
				std::cout << clr::green << "----------------------------------" << clr::reset << std::endl;
				std::cout << clr::green << "File check: \"" << cmd->command << "\"" << clr::reset << std::endl;
				std::cout << clr::green << "----------------------------------" << clr::reset << std::endl;

				fail = GetFileAttributesA( cmd->command ) == 0xFFFFFFFF;
				if ( fail )
					strcpy_s( errMsg, "File is missing!" );
			}
			else
			{
				std::cout << clr::yellow << "----------------------------------" << clr::reset << std::endl;
				std::cout << clr::yellow << "Deleting: " << cmd->command << clr::reset << std::endl;
				std::cout << clr::yellow << "----------------------------------" << clr::reset << std::endl;
				fail = DeleteFileA( cmd->command ) != TRUE;
				if ( fail )
					GetErrorString( errMsg );
			}
		}
		else if ( cmd->type == CmdType::Copy || cmd->type == CmdType::Rename ) // 2 args
		{
			char* const from = cmd->command;
			char* const to = strchr( from, '|' );
			*to = 0;
			if ( cmd->type == CmdType::Rename )
			{
				std::cout << clr::yellow << "----------------------------------" << clr::reset << std::endl;
				std::cout << clr::yellow << "Renaming \"" << from << "\" to \"" << ( to + 1 ) << "\"" << clr::reset << std::endl;
				std::cout << clr::yellow << "----------------------------------" << clr::reset << std::endl;
				fail = rename( from, to + 1 ) != 0;
				if ( fail )
					strerror_s( errMsg, errno );
			}
			else
			{
				std::cout << clr::yellow << "----------------------------------" << clr::reset << std::endl;
				std::cout << clr::yellow << "Copying \"" << from << "\" to \"" << ( to + 1 ) << "\"" << clr::reset << std::endl;
				std::cout << clr::yellow << "----------------------------------" << clr::reset << std::endl;
				fail = CopyFileA( from, to + 1, FALSE ) != TRUE;
				if ( fail )
					GetErrorString( errMsg );
			}
		}

		if ( fail )
		{
			std::cout << clr::red << "----------------------------------" << clr::reset << std::endl;
			std::cout << clr::red << "Command failed! " << errMsg << clr::reset << std::endl;
			std::cout << clr::red << "----------------------------------" << clr::reset << std::endl;
			cancel = true;
		}
	}

	if ( !cancel && cmdEnd != cmdPtr )
		std::cout << clr::red << "warn: leftover data at end!" << clr::reset << std::endl;

	CloseHandle( stdOut );
	CloseHandle( stdErr );
	CloseHandle( stdIn );

	if ( waitForKey )
	{
		std::cout << "press any key to continue..." << std::endl;
		(void)_getch();
	}
	return 0;
}