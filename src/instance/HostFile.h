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

#ifndef __HOSTFILE_H_
#define __HOSTFILE_H_
#pragma once

#include <memory>
#include <text.h>

#pragma warning(push, 4)				

//-----------------------------------------------------------------------------
// Class HostFile
//
// Wrapper around a windows file object

class HostFile
{
public:

	// Instance Constructors
	//
	HostFile(tchar_t const* path);
	HostFile(tchar_t const* path, uint32_t access);
	HostFile(tchar_t const* path, uint32_t access, uint32_t share);
	HostFile(tchar_t const* path, uint32_t access, uint32_t share, uint32_t flags);

	// Destructor
	//
	~HostFile();

	//-------------------------------------------------------------------------
	// Overloaded Operators
	
	// HANDLE
	//
	operator HANDLE() const;

	//-------------------------------------------------------------------------
	// Member Functions

	// Exists (static)
	//
	// Determines if the specified file exists
	static bool Exists(tchar_t const* path);

	//-------------------------------------------------------------------------
	// Properties

	// Handle
	//
	// Gets the underlying handle for the file
	__declspec(property(get=getHandle)) HANDLE Handle;
	HANDLE getHandle(void) const;

	// Size
	//
	// Gets the size of the file
	__declspec(property(get=getSize)) size_t Size;
	size_t getSize(void) const;

private:

	HostFile(HostFile const&)=delete;
	HostFile& operator=(HostFile const&)=delete;

	//-------------------------------------------------------------------------
	// Member Variables

	HANDLE				m_handle;			// Contained file handle
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __HOSTFILE_H_
