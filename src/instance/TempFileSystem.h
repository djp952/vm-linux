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

// FORWARD DECLARATIONS
//
class MountOptions;

// CreateTempFileSystem
//
// VirtualMachine::CreateFileSystem function for TempFileSystem
std::unique_ptr<VirtualMachine::FileSystem> CreateTempFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength);

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

public:

	// Instance Constructor
	//
	TempFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength);

	// Destructor
	//
	virtual ~TempFileSystem()=default;

	//-----------------------------------------------------------------------------
	// Member Functions

	// Mount (FileSystem)
	//
	// Mounts the file system
	virtual std::unique_ptr<VirtualMachine::Mount> Mount(uint32_t flags, void const* data, size_t datalength);

private:

	TempFileSystem(TempFileSystem const&)=delete;
	TempFileSystem& operator=(TempFileSystem const&)=delete;

	// node_t
	//
	// Internal node implementation
	struct node_t
	{
		// atime
		//
		// Date/time at which the node was accessed
		datetime atime;

		// ctime
		//
		// Date/time at which the node metadata was changed
		datetime ctime;

		// crtime
		//
		// Date/time at which the node was created
		datetime crtime;

		// gid
		//
		// Node owner group identifier
		uapi_gid_t	gid;

		// mode
		//
		// Node permission mask
		uapi_mode_t mode;

		// mtime
		//
		// Date/time at which the node data was modified
		datetime mtime;
		
		// uid
		//
		// Node owner identifier
		uapi_uid_t uid;
	};

	// tempfs_t
	//
	// Internal shared file system instance
	class tempfs_t
	{
	public:

		// Instance Constructor
		//
		tempfs_t();

		// Destructor
		//
		~tempfs_t();

		//---------------------------------------------------------------------
		// Fields

		// flags
		//
		// Filesystem-specific flags
		std::atomic<uint32_t> flags = 0;

		// indexpool
		//
		// Lock-free pool of node index numbers
		IndexPool<intptr_t>	indexpool;
											
		// maxnodes
		//
		// Maximum allowed number of nodes
		std::atomic<size_t> maxnodes = 0;

		// maxsize
		//
		// Maximum allowed block storage size
		std::atomic<size_t> maxsize = 0;

		// nodecount
		//
		// Number of allocated nodes
		std::atomic<size_t> nodecount = 0;

		// nodes
		//
		// Collection of active node instances
		std::unordered_map<int32_t, std::shared_ptr<node_t>> nodes;

		// nodeslock
		//
		// Nodes collection synchronization object
		sync::reader_writer_lock nodeslock;

		//---------------------------------------------------------------------
		// Member Functions

		// allocate
		//
		// Allocates blocks from the private heap, throws if max size has been exceeded
		uintptr_t allocate(size_t count, bool zeroinit = false);

		// release
		//
		// Releases memory from the private heap
		void release(uintptr_t base);

	private:

		//---------------------------------------------------------------------
		// Member Variables

		HANDLE						m_heap;			// Private heap handle
		size_t						m_heapsize;		// Currently allocated heap size
		sync::critical_section		m_heaplock;		// Heap synchronization object
	};

	// Directory
	//
	// Implements a directory node for this file system
	class Directory : public VirtualMachine::Directory
	{
	public:

		// Instance Constructor
		//
		Directory(std::shared_ptr<tempfs_t> const& fs, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid);

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

		std::shared_ptr<tempfs_t>		m_fs;		// Shared file system state
		std::atomic<uapi_mode_t>		m_mode;		// Permission mask
		std::atomic<uapi_uid_t>			m_uid;		// Owner id
		std::atomic<uapi_gid_t>			m_gid;		// Owner group id
	};

	// Mount
	//
	// Implements VirtualMachine::Mount
	class Mount : public VirtualMachine::Mount
	{
	public:

		// Instance Constructor
		//
		Mount(std::shared_ptr<tempfs_t> const& fs, uint32_t flags);

		// Destructor
		//
		~Mount()=default;

		//---------------------------------------------------------------------
		// Member Functions

	private:

		//---------------------------------------------------------------------
		// Member Variables

		std::shared_ptr<tempfs_t>	m_fs;		// Shared file system instance
		uint32_t					m_flags;	// Mount-specific flags
	};

	//-------------------------------------------------------------------------
	// Private Member Functions

	// ParseScaledInteger (static)
	//
	// Parses a scaled integer value (K/M/G)
	static size_t ParseScaledInteger(std::string const& str);

	//-------------------------------------------------------------------------
	// Member Variables

	std::shared_ptr<tempfs_t>		m_fs;			// Shared file system instance
	std::unique_ptr<Directory>		m_rootnode;		// Root directory node
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __TEMPFILESYSTEM_H_
