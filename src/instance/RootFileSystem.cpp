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
#include "RootFileSystem.h"

#include "LinuxException.h"
#include "MountOptions.h"

#pragma warning(push, 4)

//---------------------------------------------------------------------------
// MountRootFileSystem
//
// Creates an instance of RootFileSystem
//
// Arguments:
//
//	source		- Source device string
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

std::unique_ptr<VirtualMachine::Mount> MountRootFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength)
{
	// Source is ignored but must be specified by contract
	if(source == nullptr) throw LinuxException(UAPI_EFAULT);

	// Convert the specified options into MountOptions to process the custom parameters
	MountOptions options(flags, data, datalength);

	// Verify that the specified flags are supported for a creation operation
	if(options.Flags & ~RootFileSystem::MOUNT_FLAGS) throw LinuxException(UAPI_EINVAL);

	// Default mode, uid and gid for the root directory node
	uapi_mode_t mode = UAPI_S_IRWXU | UAPI_S_IRWXG | UAPI_S_IROTH | UAPI_S_IXOTH;	// 0775
	uapi_uid_t uid = 0;
	uapi_gid_t gid = 0;

	try {

		// mode=
		//
		// Sets the permission flags to apply to the root directory
		if(options.Arguments.Contains("mode")) mode = static_cast<uapi_mode_t>(std::stoul(options.Arguments["mode"], 0, 0) & UAPI_S_IRWXUGO);

		// uid=
		//
		// Sets the owner UID to apply to the root directory
		if(options.Arguments.Contains("uid")) uid = static_cast<uapi_uid_t>(std::stoul(options.Arguments["uid"], 0, 0));

		// gid=
		//
		// Sets the owner GID to apply to the root directory
		if(options.Arguments.Contains("gid")) gid = static_cast<uapi_gid_t>(std::stoul(options.Arguments["gid"], 0, 0));
	}

	catch(...) { throw LinuxException(UAPI_EINVAL); }

	// Construct the shared file system instance and the root directory node instance
	auto fs = std::make_shared<RootFileSystem>(options.Flags & ~UAPI_MS_PERMOUNT_MASK);
	auto rootdir = std::make_shared<RootFileSystem::node_t>(fs, 1, (mode & ~UAPI_S_IFMT) | UAPI_S_IFDIR, uid, gid);

	// Create and return the mount point instance to return to the caller
	return std::make_unique<RootFileSystem::Mount>(fs, rootdir, options.Flags & UAPI_MS_PERMOUNT_MASK);
}

//---------------------------------------------------------------------------
// RootFileSystem Constructor (private)
//
// Arguments:
//
//	flags		- Initial file system level flags

RootFileSystem::RootFileSystem(uint32_t flags) : Flags(flags)
{
	// The specified flags should not include any that apply to the mount point
	_ASSERTE((flags & UAPI_MS_PERMOUNT_MASK) == 0);
	if((flags & UAPI_MS_PERMOUNT_MASK) != 0) throw LinuxException(UAPI_EINVAL);
}

//
// ROOTFILESYSTEM::NODE_T IMPLEMENTATION
//

//---------------------------------------------------------------------------
// RootFileSystem::node_t Constructor (protected)
//
// Arguments:
//
//	filesystem		- Shared file system instance
//	nodeindex		- Node index value
//	nodemode		- Initial type and permissions to assign to the node
//	userid			- Initial owner UID to assign to the node
//	groupid			- Initial owner GID to assign to the node

RootFileSystem::node_t::node_t(std::shared_ptr<RootFileSystem> const& filesystem, intptr_t nodeindex, uapi_mode_t nodemode, uapi_uid_t userid, uapi_gid_t groupid) : 
	fs(filesystem), index(nodeindex), atime(datetime::now()), ctime(atime), mtime(atime), mode(nodemode), uid(userid), gid(groupid)
{
	_ASSERTE(fs);
}

//
// ROOTFILESYSTEM::DIRECTORY IMPLEMENTATION
//

//---------------------------------------------------------------------------
// RootFileSystem::Directory Constructor
//
// Arguments:
//
//	node		- Shared Node instance

RootFileSystem::Directory::Directory(std::shared_ptr<node_t> const& node) : m_node(node)
{
	_ASSERTE(m_node);
}

//---------------------------------------------------------------------------
// RootFileSystem::Directory::CreateDirectory
//
// Creates or opens a directory node as a child of this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name to assign to the new node
//	mode		- Initial permissions to assign to the node
//	flags		- Flags to use when opening/creating the directory
//	uid			- Initial owner user id to assign to the node
//	gid			- Initial owner group id to assign to the node

std::unique_ptr<VirtualMachine::Directory> RootFileSystem::Directory::CreateDirectory(VirtualMachine::Mount const* mount, char_t const* name,
	uint32_t flags, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	UNREFERENCED_PARAMETER(mode);
	UNREFERENCED_PARAMETER(uid);
	UNREFERENCED_PARAMETER(gid);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the provided mount is part of the same file system instance
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Verify that the file system was not mounted read-only
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// RootFileSystem does not allow creation of child nodes
	throw LinuxException(UAPI_EPERM);
}

//---------------------------------------------------------------------------
// RootFileSystem::Directory::CreateFile
//
// Creates a new file node as a child of this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name to assign to the new node
//	flags		- Flags to use when opening/creating the file
//	mode		- Initial permissions to assign to the node
//	uid			- Initial owner user id to assign to the node
//	gid			- Initial owner group id to assign to the node

std::unique_ptr<VirtualMachine::File> RootFileSystem::Directory::CreateFile(VirtualMachine::Mount const* mount, char_t const* name,
	uint32_t flags, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	UNREFERENCED_PARAMETER(mode);
	UNREFERENCED_PARAMETER(uid);
	UNREFERENCED_PARAMETER(gid);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the provided mount is part of the same file system instance
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Verify that the file system was not mounted read-only
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// RootFileSystem does not allow creation of child nodes
	throw LinuxException(UAPI_EPERM);
}

//-----------------------------------------------------------------------------
// RootFileSystem::Directory::getGroupId
//
// Gets the currently set owner group identifier for the directory

uapi_gid_t RootFileSystem::Directory::getGroupId(void) const
{
	return m_node->gid;
}

//---------------------------------------------------------------------------
// RootFileSystem::Directory::getIndex
//
// Gets the node index within the file system (inode number)

intptr_t RootFileSystem::Directory::getIndex(void) const
{
	return m_node->index;
}

//-----------------------------------------------------------------------------
// RootFileSystem::Directory::Lookup
//
// Accesses a child node of this directory by name
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name of the child node to be looked up

std::unique_ptr<VirtualMachine::Node> RootFileSystem::Directory::Lookup(VirtualMachine::Mount const* mount, char_t const* name) const 
{
	UNREFERENCED_PARAMETER(mount);
	UNREFERENCED_PARAMETER(name);

	throw LinuxException(UAPI_ENOENT);
}

//---------------------------------------------------------------------------
// RootFileSystem::Directory::OpenHandle
//
// Opens a handle against this node
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	flags		- Handle flags

std::unique_ptr<VirtualMachine::Handle> RootFileSystem::Directory::OpenHandle(VirtualMachine::Mount const* mount, uint32_t flags)
{
	UNREFERENCED_PARAMETER(mount);
	UNREFERENCED_PARAMETER(flags);

	// todo - need DirectoryHandle object
	return nullptr;
}

//---------------------------------------------------------------------------
// RootFileSystem::Directory::getMode
//
// Gets the type and permissions mask for the node

uapi_mode_t RootFileSystem::Directory::getMode(void) const
{
	return m_node->mode;
}

//---------------------------------------------------------------------------
// RootFileSystem::Directory:SetGroupId
//
// Changes the owner group id for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	gid			- New owner group id to be set

uapi_gid_t RootFileSystem::Directory::SetGroupId(VirtualMachine::Mount const* mount, uapi_gid_t gid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_node->gid = gid;							// Update the node gid
	m_node->ctime = datetime::now();			// Update change time

	return gid;
}

//---------------------------------------------------------------------------
// RootFileSystem::Directory:SetMode
//
// Changes the mode flags for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	mode		- New mode flags to be set

uapi_mode_t RootFileSystem::Directory::SetMode(VirtualMachine::Mount const* mount, uapi_mode_t mode)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// Strip out all but the permissions from the provided mode; the type
	// cannot be changed after a node has been created
	mode = ((mode & UAPI_S_IALLUGO) | (m_node->mode & ~UAPI_S_IALLUGO));

	m_node->mode = mode;						// Update the node mode
	m_node->ctime = datetime::now();			// Update change time

	return mode;
}

//---------------------------------------------------------------------------
// RootFileSystem::Directory:SetUserId
//
// Changes the owner user id for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	uid			- New owner user id to be set

uapi_uid_t RootFileSystem::Directory::SetUserId(VirtualMachine::Mount const* mount, uapi_uid_t uid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_node->uid = uid;							// Update the node uid
	m_node->ctime = datetime::now();			// Update change time

	return uid;
}

//-----------------------------------------------------------------------------
// RootFileSystem::Directory::getUserId
//
// Gets the currently set owner user identifier for the directory

uapi_uid_t RootFileSystem::Directory::getUserId(void) const
{
	return m_node->uid;
}

//
// ROOTFILESYSTEM::MOUNT IMPLEMENTATION
//

//---------------------------------------------------------------------------
// RootFileSystem::Mount Constructor
//
// Arguments:
//
//	fs			- Shared file system instance
//	rootdir		- Root directory node instance
//	flags		- Mount-specific flags

RootFileSystem::Mount::Mount(std::shared_ptr<RootFileSystem> const& fs, std::shared_ptr<node_t> const& rootdir, uint32_t flags) : 
	m_fs(fs), m_rootdir(rootdir), m_flags(flags)
{
	_ASSERTE(m_fs);

	// The root directory node is converted from a unique to a shared pointer so it
	// can be shared among multiple cloned Mount instances
	_ASSERTE(m_rootdir);

	// The specified flags should not include any that apply to the file system
	_ASSERTE((flags & ~UAPI_MS_PERMOUNT_MASK) == 0);
	if((flags & ~UAPI_MS_PERMOUNT_MASK) != 0) throw LinuxException(UAPI_EINVAL);
}

//---------------------------------------------------------------------------
// RootFileSystem::Mount Copy Constructor
//
// Arguments:
//
//	rhs		- Existing Mount instance to create a copy of

RootFileSystem::Mount::Mount(Mount const& rhs) : m_fs(rhs.m_fs), m_rootdir(rhs.m_rootdir), m_flags(static_cast<uint32_t>(rhs.m_flags))
{
	// A copy of a mount references the same shared file system and root
	// directory instance as well as a copy of the mount flags
}

//---------------------------------------------------------------------------
// RootFileSystem::Mount::Duplicate
//
// Duplicates this mount instance
//
// Arguments:
//
//	NONE

std::unique_ptr<VirtualMachine::Mount> RootFileSystem::Mount::Duplicate(void) const
{
	return std::make_unique<RootFileSystem::Mount>(*this);
}

//---------------------------------------------------------------------------
// RootFileSystem::Mount::getFileSystem
//
// Accesses the underlying file system instance

VirtualMachine::FileSystem* RootFileSystem::Mount::getFileSystem(void) const
{
	return m_fs.get();
}

//---------------------------------------------------------------------------
// RootFileSystem::Mount::getFlags
//
// Gets the mount point flags

uint32_t RootFileSystem::Mount::getFlags(void) const
{
	// Combine the mount flags with those of the underlying file system
	return m_fs->Flags | m_flags;
}

//---------------------------------------------------------------------------
// RootFileSystem::Mount::GetRootNode
//
// Gets the root node of the mount point
//
// Arguments:
//
//	NONE

std::unique_ptr<VirtualMachine::Node> RootFileSystem::Mount::GetRootNode(void) const
{
	return std::make_unique<RootFileSystem::Directory>(m_rootdir);
}

//---------------------------------------------------------------------------

#pragma warning(pop)
