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

#include <datetime.h>
#include <memory>
#include <sync.h>
#include <text.h>
#include <timespan.h>
#include <unordered_map>

#include "IndexPool.h"
#include "VirtualMachine.h"

#pragma warning(push, 4)

// MountTempFileSystem
//
// Creates an instance of TempFileSystem
std::unique_ptr<VirtualMachine::Mount> MountTempFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength);

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
//	MS_DIRSYNC
//	MS_I_VERSION
//	MS_KERNMOUNT
//	MS_LAZYTIME
//	MS_MANDLOCK
//	MS_NOATIME
//	MS_NODEV
//	MS_NODIRATIME
//	MS_NOEXEC
//	MS_NOSUID
//	MS_RDONLY
//	MS_RELATIME
//	MS_SILENT
//	MS_STRICTATIME
//	MS_SYNCHRONOUS
//	
//	size=nnn[K|k|M|m|G|g|%]			- Defines the maximum file system size
//	nr_blocks=nnn[K|k|M|m|G|g]		- Defines the maximum number of blocks
//	nr_blocks=nnn[K|k|M|m|G|g]		- Defines the maximum number of inodes
//	mode=nnn						- Defines the permissions of the root directory
//	uid=nnn							- Defines the owner user id of the root directory
//	gid=nnn							- Defines the owner group id of the root directory
//	
// Supported remount options:
//
//	MS_I_VERSION
//	MS_LAZYTIME
//	MS_MANDLOCK
//	MS_RDONLY
//	MS_SYNCHRONOUS
//
//	size=nnn[K|k|M|m|G|g|%]			- See above
//	nr_blocks=nnn[K|k|M|m|G|g]		- See above
//	nr_blocks=nnn[K|k|M|m|G|g]		- See above

class TempFileSystem : public VirtualMachine::FileSystem
{
	// MOUNT_FLAGS
	//
	// Supported creation/mount operation flags
	static const uint32_t MOUNT_FLAGS = UAPI_MS_RDONLY | UAPI_MS_NOSUID | UAPI_MS_NODEV | UAPI_MS_NOEXEC | UAPI_MS_SYNCHRONOUS |
		UAPI_MS_MANDLOCK | UAPI_MS_DIRSYNC | UAPI_MS_NOATIME | UAPI_MS_NODIRATIME | UAPI_MS_RELATIME | UAPI_MS_SILENT | UAPI_MS_STRICTATIME | 
		UAPI_MS_LAZYTIME | UAPI_MS_I_VERSION | UAPI_MS_KERNMOUNT;

	// REMOUNT_FLAGS
	//
	// Supported remount operation flags
	static const uint32_t REMOUNT_FLAGS = UAPI_MS_REMOUNT | UAPI_MS_RDONLY | UAPI_MS_SYNCHRONOUS | UAPI_MS_MANDLOCK | UAPI_MS_I_VERSION | UAPI_MS_LAZYTIME;

	// MountTempFileSystem (friend)
	//
	// Creates an instance of TempFileSystem
	friend std::unique_ptr<VirtualMachine::Mount> MountTempFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength);

	// FORWARD DECLARATIONS
	//
	class Directory;
	class Mount;

public:

	// Instance Constructor
	//
	TempFileSystem(uint32_t flags);

	// Destructor
	//
	virtual ~TempFileSystem();

	//-----------------------------------------------------------------------------
	// Fields

	// Flags
	//
	// File system specific flags
	std::atomic<uint32_t> Flags = 0;

	// NodeIndexPool
	//
	// Lock-free pool of node index numbers
	IndexPool<intptr_t>	NodeIndexPool;

	// MaximumNodes
	//
	// Maximum allowed number of file system nodes
	std::atomic<size_t> MaximumNodes = 0;

	// MaximumSize
	//
	// Maximum allowed size of the private heap
	std::atomic<size_t> MaximumSize = 0;

	//-----------------------------------------------------------------------
	// Member Functions

	// Allocate
	//
	// Allocates memory from the private heap
	void* Allocate(size_t bytecount, bool zeroinit = false);

	// Reallocate
	//
	// Reallocates memory in the private heap
	void* Reallocate(void* ptr, size_t bytecount, bool zeroinit = false);

	// Release
	//
	// Releases memory from the private heap
	void Release(void* ptr);
	
private:

	TempFileSystem(TempFileSystem const&)=delete;
	TempFileSystem& operator=(TempFileSystem const&)=delete;

	// Directory
	//
	// Implements a directory node for this file system
	class Directory : public VirtualMachine::Directory
	{
	public:

		// Instance Constructor
		//
		Directory(std::shared_ptr<TempFileSystem> const& fs, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid);

		// Destructor
		//
		virtual ~Directory()=default;

		//---------------------------------------------------------------------
		// Member Functions

		// OwnerGroupId (VirtualMachine::Node)
		//
		// Gets the node owner group identifier
		__declspec(property(get=getOwnerGroupId)) uapi_gid_t OwnerGroupId;
		virtual uapi_gid_t getOwnerGroupId(void) const override;

		// OwnerUserId (VirtualMachine::Node)
		//
		// Gets the node owner user identifier 
		__declspec(property(get=getOwnerUserId)) uapi_uid_t OwnerUserId;
		virtual uapi_uid_t getOwnerUserId(void) const override;

		// Permissions (VirtualMachine::Node)
		//
		// Gets the node permissions mask
		__declspec(property(get=getPermissions)) uapi_mode_t Permissions;
		virtual uapi_mode_t getPermissions(void) const override;

	private:

		Directory(Directory const&)=delete;
		Directory& operator=(Directory const&)=delete;

		//---------------------------------------------------------------------
		// Member Variables

		std::shared_ptr<TempFileSystem>		m_fs;		// File system instance
		std::atomic<uapi_mode_t>			m_mode;		// Permission mask
		std::atomic<uapi_uid_t>				m_uid;		// Owner id
		std::atomic<uapi_gid_t>				m_gid;		// Owner group id

		//datetime m_atime;		// <--- add these in later, probably in a base class with mode, uid and gid
		//datetime m_ctime;
		//datetime m_mtime;
	};

	// Mount
	//
	// Implements VirtualMachine::Mount
	class Mount : public VirtualMachine::Mount
	{
	public:

		// Instance Constructor
		//
		Mount(std::shared_ptr<TempFileSystem> const& fs, uint32_t flags);

		// Destructor
		//
		~Mount()=default;

	private:

		//---------------------------------------------------------------------
		// Member Variables

		std::shared_ptr<TempFileSystem>		m_fs;		// File system instance
		std::atomic<uint32_t>				m_flags;	// Mount-specific flags
	};

	//-------------------------------------------------------------------------
	// Member Variables

	HANDLE							m_heap;			// Private heap handle
	size_t							m_heapsize;		// Currently allocated heap size
	sync::critical_section			m_heaplock;		// Heap synchronization object
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __TEMPFILESYSTEM_H_
