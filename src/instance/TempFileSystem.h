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
	TempFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength);

	// Destructor
	//
	~TempFileSystem()=default;

	//-----------------------------------------------------------------------------
	// Member Functions

	// Mount (FileSystem)
	//
	// Mounts the file system
	virtual std::unique_ptr<VirtualMachine::Mount> Mount(uint32_t flags, void const* data, size_t datalength);

private:

	TempFileSystem(TempFileSystem const&)=delete;
	TempFileSystem& operator=(TempFileSystem const&)=delete;

	// tempfs_t
	//
	// Internal shared file system instance
	class tempfs_t
	{
	public:

		// Instance Constructor
		//
		tempfs_t(uint32_t flags, size_t maxsize, size_t maxnodes, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid);

		// Destructor
		//
		~tempfs_t();

	private:

		//---------------------------------------------------------------------
		// Member Variables

		uint32_t	m_flags;		// File system specific flags
		HANDLE						m_heap;			// Private heap handle
		size_t						m_size;			// Current file system size
		size_t						m_maxsize;		// Maximum file system size
		size_t						m_nodes;		// Current number of nodes
		size_t						m_maxnodes;		// Maximum number of nodes
		IndexPool<intptr_t>			m_indexpool;	// Pool of node index numbers
	};

	// DirectoryNode
	//
	// Implements a directory node for this file system
	class DirectoryNode : public VirtualMachine::Directory
	{
	public:

		// Instance Constructor
		//
		DirectoryNode(uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid);

		// Destructor
		//
		virtual ~DirectoryNode()=default;

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

		DirectoryNode(DirectoryNode const&)=delete;
		DirectoryNode& operator=(DirectoryNode const&)=delete;

		//---------------------------------------------------------------------
		// Member Variables

		std::atomic<uapi_mode_t>		m_mode;		// Permission mask
		std::atomic<uapi_uid_t>			m_uid;		// Owner id
		std::atomic<uapi_gid_t>			m_gid;		// Owner group id
	};

	// MountPoint
	//
	// Implements VirtualMachine::Mount
	class MountPoint : public VirtualMachine::Mount
	{
	public:

		// Instance Constructor
		//
		MountPoint(std::shared_ptr<tempfs_t> const& fs, uint32_t flags);

		// Destructor
		//
		~MountPoint()=default;

	private:

		std::shared_ptr<tempfs_t>	m_fs;		// Shared file system instance
		uint32_t	m_flags;	// Mount-specific flags
	};

	//-------------------------------------------------------------------------
	// Private Member Functions

	// ParseScaledInteger (static)
	//
	// Parses a scaled integer value (K/M/G)
	static size_t ParseScaledInteger(std::string const& str);

	//-------------------------------------------------------------------------
	// Member Variables

	std::shared_ptr<tempfs_t>	m_fs;				// Shared file system instance

	static size_t				s_maxmemory;		// Maximum available memory
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __TEMPFILESYSTEM_H_
