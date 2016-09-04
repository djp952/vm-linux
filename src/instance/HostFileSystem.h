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

#ifndef __HOSTFILESYSTEM_H_
#define __HOSTFILESYSTEM_H_
#pragma once

#include <memory>
#include <text.h>

#include "VirtualMachine.h"

#pragma warning(push, 4)

// CreateHostFileSystem
//
// VirtualMachine::CreateFileSystem function for HostFileSystem
std::unique_ptr<VirtualMachine::FileSystem> CreateHostFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength);

//-----------------------------------------------------------------------------
// Class HostFileSystem
//
// HostFileSystem implements a pass-through file system that operates against
// a directory accessible to the host operating system
//
// Supported mount options:
//
//	MS_DIRSYNC
//	MS_KERNMOUNT
//	MS_NODEV		(Always set)
//	MS_NOEXEC
//	MS_NOSUID		(Always set)
//	MS_RDONLY
//	MS_SILENT
//	MS_SYNCHRONOUS
//
//	[no]sandbox		- Controls sandboxing of the virtual file system (see below)
//	
// Supported remount options:
//
//	MS_RDONLY
//	MS_SYNCHRONOUS

class HostFileSystem : public VirtualMachine::FileSystem
{
	// MOUNT_FLAGS
	//
	// Supported creation/mount operation flags
	static const uint32_t MOUNT_FLAGS = UAPI_MS_RDONLY | UAPI_MS_NOSUID | UAPI_MS_NODEV | UAPI_MS_NOEXEC | UAPI_MS_SYNCHRONOUS |
		UAPI_MS_DIRSYNC | UAPI_MS_SILENT | UAPI_MS_KERNMOUNT;

	// REMOUNT_FLAGS
	//
	// Supported remount operation flags
	static const uint32_t REMOUNT_FLAGS = UAPI_MS_REMOUNT | UAPI_MS_RDONLY | UAPI_MS_SYNCHRONOUS;

public:

	// Instance Constructor
	//
	HostFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength);

	// Destructor
	//
	~HostFileSystem()=default;

	// hostfs_t
	//
	// Internal shared file system state
	struct hostfs_t
	{
		// flags
		//
		// Filesystem-level flags
		std::atomic<uint32_t> flags = 0;
	};

	// Mount
	//
	// Implements VirtualMachine::Mount
	class Mount : public VirtualMachine::Mount
	{
	public:

		// Instance Constructor
		//
		Mount(std::shared_ptr<hostfs_t> const& fs, uint32_t flags);

		// Destructor
		//
		~Mount()=default;

		//---------------------------------------------------------------------
		// Member Functions

	private:

		//---------------------------------------------------------------------
		// Member Variables

		std::shared_ptr<hostfs_t>	m_fs;		// Shared file system instance
		uint32_t					m_flags;	// Mount-specific flags
	};

	//-----------------------------------------------------------------------------
	// Member Functions

	// Mount (FileSystem)
	//
	// Mounts the file system
	virtual std::unique_ptr<VirtualMachine::Mount> Mount(uint32_t flags, void const* data, size_t datalength);

private:

	HostFileSystem(HostFileSystem const&)=delete;
	HostFileSystem& operator=(HostFileSystem const&)=delete;

	//-------------------------------------------------------------------------
	// Member Variables

	std::shared_ptr<hostfs_t>		m_fs;			// Shared file system state
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __HOSTFILESYSTEM_H_
