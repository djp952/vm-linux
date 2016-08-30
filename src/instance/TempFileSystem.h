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

#ifndef __TEMPFILESYSTEM_H_
#define __TEMPFILESYSTEM_H_
#pragma once

#include <memory>
#include <text.h>

#include "IndexPool.h"
#include "VirtualMachine.h"

#pragma warning(push, 4)

// FORWARD DECLARATIONS
//
class MountOptions;

// CreateTempFileSystem
//
// VirtualMachine::CreateFileSystem function for TempFileSystem
std::unique_ptr<VirtualMachine::FileSystem> CreateTempFileSystem(char_t const* source, VirtualMachine::MountFlags flags, void const* data, size_t datalength);

//-----------------------------------------------------------------------------
// Class TempFileSystem
//
// TempFileSystem implements an in-memory file system.  Rather than using a virtual 
// block device constructed on raw virtual memory, this uses a private Windows heap 
// to store the file system data.  There are a number of challenges with the virtual 
// block device method that can be easily overcome by doing it this way; let the 
// operating system do the heavy lifting
//
// Supported mount options:
//
//	TODO
//
//	size=nnn[K|k|M|m|G|g|%]			- Defines the maximum file system size
//	nr_blocks=nnn[K|k|M|m|G|g|%]	- Defines the maximum number of blocks
//	nr_inodes=nnn[K|k|M|m|G|g|%]	- Defines the maximum number of inodes
//	mode=nnn						- Defines the permissions of the root directory
//	uid=nnn							- Defines the owner user id of the root directory
//	gid=nnn							- Defines the owner group id of the root directory
//	
// Supported remount options:
//
//	TODO

class TempFileSystem : public VirtualMachine::FileSystem
{
public:

	// Instance Constructor
	//
	TempFileSystem(char_t const* source, VirtualMachine::MountFlags flags, void const* data, size_t datalength);

	// Destructor
	//
	~TempFileSystem();

	//-----------------------------------------------------------------------------
	// Member Functions

private:

	TempFileSystem(TempFileSystem const&)=delete;
	TempFileSystem& operator=(TempFileSystem const&)=delete;

	//-------------------------------------------------------------------------
	// Private Member Functions

	// ParseScaledInteger (static)
	//
	// Parses a scaled integer value (K/M/G)
	static size_t ParseScaledInteger(std::string const& str);

	//-------------------------------------------------------------------------
	// Member Variables

	HANDLE						m_heap;				// Private heap handle
	size_t						m_size;				// Current file system size
	size_t						m_maxsize;			// Maximum file system size
	size_t						m_nodes;			// Current number of nodes
	size_t						m_maxnodes;			// Maximum number of nodes
	IndexPool<intptr_t>			m_indexpool;		// Pool of node index numbers

	static size_t				s_maxmemory;		// Maximum available memory
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __TEMPFILESYSTEM_H_
