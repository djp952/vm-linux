//-----------------------------------------------------------------------------
// Copyright (c) 2016 Michael G. Brehm
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "HostFile.h"

#include <Win32Exception.h>

#pragma warning(push, 4)				

//-----------------------------------------------------------------------------
// HostFile Constructor
//
// Arguments:
//
//	path			- Path to the file

HostFile::HostFile(tchar_t const* path) : HostFile(path, GENERIC_READ | GENERIC_WRITE, 0, FILE_ATTRIBUTE_NORMAL)
{
}

//-----------------------------------------------------------------------------
// HostFile Constructor
//
// Arguments:
//
//	path			- Path to the file
//	access			- File access mask

HostFile::HostFile(tchar_t const* path, uint32_t access) : HostFile(path, access, 0, FILE_ATTRIBUTE_NORMAL)
{
}

//-----------------------------------------------------------------------------
// HostFile Constructor
//
// Arguments:
//
//	path			- Path to the file
//	access			- File access mask
//	share			- File sharing flags

HostFile::HostFile(tchar_t const* path, uint32_t access, uint32_t share) : HostFile(path, access, share, FILE_ATTRIBUTE_NORMAL)
{
}

//-----------------------------------------------------------------------------
// HostFile Constructor
//
// Arguments:
//
//	path			- Path to the file
//	access			- File access mask
//	share			- File sharing flags
//	flags			- File flags and attributes

HostFile::HostFile(tchar_t const* path, uint32_t access, uint32_t share, uint32_t flags)
{
	if(!path || !(*path)) throw Win32Exception(ERROR_INVALID_PARAMETER);

	m_handle = CreateFile(path, access, share, NULL, OPEN_EXISTING, flags, NULL);
	if(m_handle == INVALID_HANDLE_VALUE) throw Win32Exception();
}

//-----------------------------------------------------------------------------
// HostFile Destructor

HostFile::~HostFile()
{
	if(m_handle != INVALID_HANDLE_VALUE) CloseHandle(m_handle);
}

//-----------------------------------------------------------------------------
// HostFile HANDLE conversion operator

HostFile::operator HANDLE() const
{
	return m_handle;
}

//-----------------------------------------------------------------------------
// HostFile::Exists (static)
//
// Determines if the specifed file exists, will return false if the object
// exists and is not a file (or a link to a file)
//
// Arguments:
//
//	path		- Path to the file to check

bool HostFile::Exists(tchar_t const* path)
{
	WIN32_FILE_ATTRIBUTE_DATA fileinfo;
	if(!GetFileAttributesEx(path, GetFileExInfoStandard, &fileinfo)) return false;

	// Check for devices, directories, and offline files.  Reparse points that 
	// aren't files will also have these flags set, so this should catch those
	if(fileinfo.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) return false;
	else if(fileinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return false;
	else if(fileinfo.dwFileAttributes & FILE_ATTRIBUTE_OFFLINE) return false;

	return true;
}

//-----------------------------------------------------------------------------
// HostFile::getHandle
//
// Gets the underlying Windows handle for the file

HANDLE HostFile::getHandle(void) const
{
	return m_handle;
}

//-----------------------------------------------------------------------------
// HostFile::getSize
//
// Gets the size of the file

size_t HostFile::getSize(void) const
{
	LARGE_INTEGER			size;				// Size of the file

	// Attempt to get the size of the file as a LARGE_INTEGER
	if(!GetFileSizeEx(m_handle, &size)) throw Win32Exception();

#ifdef _M_X64
	return static_cast<size_t>(size.QuadPart);
#else
	// Can only return up to INT32_MAX with a 32-bit size_t
	return (size.HighPart) ? INT32_MAX : static_cast<size_t>(size.LowPart);
#endif
}

//-----------------------------------------------------------------------------

#pragma warning(pop)
