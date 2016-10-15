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

#ifndef __ROOTFILESYSTEM_H_
#define __ROOTFILESYSTEM_H_
#pragma once

#include <atomic>
#include <datetime.h>
#include <memory>
#include <text.h>

#include "VirtualMachine.h"

#pragma warning(push, 4)

// MountRootFileSystem
//
// Creates an instance of RootFileSystem
std::unique_ptr<VirtualMachine::Mount> MountRootFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength);

//-----------------------------------------------------------------------------
// Class RootFileSystem
//
// RootFileSystem implements a virtual single directory node file system in which
// no child nodes can be created
//
// Supported mount options:
//
//	MS_KERNMOUNT
//	MS_NOATIME
//	MS_NODIRATIME
//	MS_RDONLY
//	MS_RELATIME
//	MS_SILENT
//	MS_STRICTATIME
//
//	mode=nnn	- Sets the permissions of the directory node
//	uid=nnn		- Sets the owner user id of the directory node
//	gid=nnn		- Sets the owner group id of the directory node
//	
//	(MS_NODEV, MS_NOEXEC and MS_NOSUID are always set)
//
// Supported remount options:
//
//	MS_RDONLY

class RootFileSystem : public VirtualMachine::FileSystem
{
	// MOUNT_FLAGS
	//
	// Supported creation/mount operation flags
	static const uint32_t MOUNT_FLAGS = UAPI_MS_KERNMOUNT | UAPI_MS_NOATIME | UAPI_MS_NODIRATIME | UAPI_MS_RDONLY | UAPI_MS_RELATIME | 
		UAPI_MS_SILENT | UAPI_MS_STRICTATIME;
	
	// REMOUNT_FLAGS
	//
	// Supported remount operation flags
	static const uint32_t REMOUNT_FLAGS = UAPI_MS_REMOUNT | UAPI_MS_RDONLY;

	// MountRootFileSystem (friend)
	//
	// Creates an instance of RootFileSystem
	friend std::unique_ptr<VirtualMachine::Mount> MountRootFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength);

	// FORWARD DECLARATIONS
	//
	class Directory;
	class Mount;

public:

	// Instance Constructor
	//
	RootFileSystem(uint32_t flags);

	// Destructor
	//
	virtual ~RootFileSystem()=default;

	//-----------------------------------------------------------------------------
	// Fields

	// Flags
	//
	// File system specific flags
	std::atomic<uint32_t> Flags = 0;

private:

	RootFileSystem(RootFileSystem const&)=delete;
	RootFileSystem& operator=(RootFileSystem const&)=delete;

	// node_t
	//
	// Internal file system node representation
	class node_t
	{
	public:

		// Instance Constructor
		//
		node_t(std::shared_ptr<RootFileSystem> const& filesystem, intptr_t nodeindex, uapi_mode_t nodemode, uapi_uid_t userid, uapi_gid_t groupid);

		// Destructor
		//
		virtual ~node_t()=default;

		//-------------------------------------------------------------------
		// Fields

		// atime
		//
		// Date/time that the node was last accessed
		datetime atime;

		// ctime
		//
		// Date/time that the node metadata was last changed
		datetime ctime;

		// fs
		//
		// Shared pointer to the parent file system
		std::shared_ptr<RootFileSystem> const fs;

		// gid
		//
		// Node owner group identifier
		std::atomic<uapi_gid_t> gid;

		// index
		//
		// The node index value
		intptr_t const index;

		// mode
		//
		// The node type and permission flags
		std::atomic<uapi_mode_t> mode;

		// mtime
		//
		// Date/time that the node data was last changed
		datetime mtime;

		// uid
		//
		// Node owner user identifier
		std::atomic<uapi_uid_t> uid;

	private:

		node_t(node_t const&)=delete;
		node_t& operator=(node_t const&)=delete;
	};

	// Directory
	//
	// Implements a directory node for this file system
	class Directory : public VirtualMachine::Directory
	{
	public:

		// Instance Constructor
		//
		Directory(std::shared_ptr<node_t> const& node);

		// Destructor
		//
		virtual ~Directory()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// CreateDirectory (VirtualMachine::Directory)
		//
		// Creates or opens a directory node as a child of this directory
		virtual std::unique_ptr<VirtualMachine::Directory> CreateDirectory(VirtualMachine::Mount const* mount, char_t const* name, uint32_t flags, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) override;

		// CreateFile (VirtualMachine::Directory)
		//
		// Creates or opens a regular file node as a child of this directory
		virtual std::unique_ptr<VirtualMachine::File> CreateFile(VirtualMachine::Mount const* mount, char_t const* name, uint32_t flags, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) override;

		// Lookup (VirtualMachine::Directory)
		//
		// Accesses a child node of this directory by name
		virtual std::unique_ptr<VirtualMachine::Node> Lookup(VirtualMachine::Mount const* mount, char_t const* name) const override;

		// OpenHandle (VirtualMachine::Node)
		//
		// Opens a handle against this node
		virtual std::unique_ptr<VirtualMachine::Handle> OpenHandle(VirtualMachine::Mount const* mount, uint32_t flags) override;

		// SetGroupId (VirtualMachine::Node)
		//
		// Changes the owner group id for this node
		virtual uapi_gid_t SetGroupId(VirtualMachine::Mount const* mount, uapi_gid_t gid) override;

		// SetMode (VirtualMachine::Node)
		//
		// Changes the mode flags for this node
		virtual uapi_mode_t SetMode(VirtualMachine::Mount const* mount, uapi_mode_t mode) override;

		// SetUserId (VirtualMachine::Node)
		//
		// Changes the owner user id for this node
		virtual uapi_uid_t SetUserId(VirtualMachine::Mount const* mount, uapi_uid_t uid) override;

		//---------------------------------------------------------------------
		// Properties

		// GroupId (VirtualMachine::Node)
		//
		// Gets the node owner group identifier
		__declspec(property(get=getGroupId)) uapi_gid_t GroupId;
		virtual uapi_gid_t getGroupId(void) const override;

		// Index  (VirtualMachine::Node)
		//
		// Gets the node index within the file system (inode number)
		__declspec(property(get=getIndex)) intptr_t Index;
		virtual intptr_t getIndex(void) const override;

		// Mode (VirtualMachine::Node)
		//
		// Gets the node type and permission mask for the node
		__declspec(property(get=getMode)) uapi_mode_t Mode;
		virtual uapi_mode_t getMode(void) const override;

		// UserId (VirtualMachine::Node)
		//
		// Gets the node owner user identifier 
		__declspec(property(get=getUserId)) uapi_uid_t UserId;
		virtual uapi_uid_t getUserId(void) const override;

	private:

		Directory(Directory const&)=delete;
		Directory& operator=(Directory const&)=delete;

		//---------------------------------------------------------------------
		// Member Variables

		std::shared_ptr<node_t>			m_node;		// Shared node instance
	};

	// Mount
	//
	// Implements VirtualMachine::Mount
	class Mount : public VirtualMachine::Mount
	{
	public:

		// Instance Constructor
		//
		Mount(std::shared_ptr<RootFileSystem> const& fs, std::shared_ptr<node_t> const& rootdir, uint32_t flags);

		// Copy Constructor
		//
		Mount(Mount const& rhs);

		// Destructor
		//
		~Mount()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// Duplicate (VirtualMachine::Mount)
		//
		// Duplicates this mount instance
		virtual std::unique_ptr<VirtualMachine::Mount> Duplicate(void) const override;

		// GetRootNode (VirtualMachine::Mount)
		//
		// Gets the root node of the mount point
		virtual std::unique_ptr<VirtualMachine::Node> GetRootNode(void) const override;

		//-------------------------------------------------------------------
		// Properties

		// FileSystem (VirtualMachine::Mount)
		//
		// Accesses the underlying file system instance
		__declspec(property(get=getFileSystem)) VirtualMachine::FileSystem* FileSystem;
		virtual VirtualMachine::FileSystem* getFileSystem(void) const override;

		// Flags (VirtualMachine::Mount)
		//
		// Gets the mount point flags
		__declspec(property(get=getFlags)) uint32_t Flags;
		virtual uint32_t getFlags(void) const override;

	private:

		Mount& operator=(Mount const&)=delete;

		//---------------------------------------------------------------------
		// Member Variables

		std::shared_ptr<RootFileSystem>		m_fs;		// File system instance
		std::shared_ptr<node_t>				m_rootdir;	// Root node instance
		std::atomic<uint32_t>				m_flags;	// Mount-level flags
	};
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __ROOTFILESYSTEM_H_
