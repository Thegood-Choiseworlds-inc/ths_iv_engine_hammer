#ifndef CONNECTION_H
#define CONNECTION_H

#pragma once

#include <cstddef>

// This is to wrap the platform specific kinds of connect/read/write.

// not really connectiony, but need per-platform
int GetProcessId();

struct BaseConnection
{
	BaseConnection()
	{
		isOpen = false;
	}

	static BaseConnection* Create();
	static void            Destroy(BaseConnection*&);
	bool                   Open();
	bool                   Close();
	bool                   Write( const void* data, size_t length );
	bool                   Read( void* data, size_t length );

	bool                   isOpen;
};


#endif // CONNECTION_H