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

#include <convert.h>

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
	fs(filesystem), index(nodeindex), mode(nodemode), uid(userid), gid(groupid)
{
	_ASSERTE(fs);
	
	// Set the access time, change time and modification time to now
	uapi_timespec now = convert<uapi_timespec>(datetime::now());
	atime = ctime = mtime = now;
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

RootFileSystem::Directory::Directory(std::shared_ptr<node_t> const& node) : Node(node)
{
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
//	uid			- Initial owner user id to assign to the node
//	gid			- Initial owner group id to assign to the node

std::unique_ptr<VirtualMachine::Directory> RootFileSystem::Directory::CreateDirectory(VirtualMachine::Mount const* mount, char_t const* name,
	uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	UNREFERENCED_PARAMETER(mode);
	UNREFERENCED_PARAMETER(uid);
	UNREFERENCED_PARAMETER(gid);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
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
//	mode		- Initial permissions to assign to the node
//	uid			- Initial owner user id to assign to the node
//	gid			- Initial owner group id to assign to the node

std::unique_ptr<VirtualMachine::File> RootFileSystem::Directory::CreateFile(VirtualMachine::Mount const* mount, char_t const* name,
	uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	UNREFERENCED_PARAMETER(mode);
	UNREFERENCED_PARAMETER(uid);
	UNREFERENCED_PARAMETER(gid);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	
	// RootFileSystem does not allow creation of child nodes
	throw LinuxException(UAPI_EPERM);
}

//---------------------------------------------------------------------------
// RootFileSystem::Directory::CreateSymbolicLink
//
// Creates or opens a symbolic link as a child of this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name to assign to the new node
//	target		- Target to assign to the symbolic link
//	uid			- Initial owner user id to assign to the node
//	gid			- Initial owner group id to assign to the node

std::unique_ptr<VirtualMachine::SymbolicLink> RootFileSystem::Directory::CreateSymbolicLink(VirtualMachine::Mount const* mount, 
	char_t const* name, char_t const* target, uapi_uid_t uid, uapi_uid_t gid)
{
	UNREFERENCED_PARAMETER(uid);
	UNREFERENCED_PARAMETER(gid);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);
	if(target == nullptr) throw LinuxException(UAPI_EFAULT);
	
	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	
	// RootFileSystem does not allow creation of child nodes
	throw LinuxException(UAPI_EPERM);
}

//---------------------------------------------------------------------------
// RootFileSystem::Directory::Enumerate
//
// Enumerates all of the entries in this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	func		- Callback function to invoke for each entry; return false to stop

void RootFileSystem::Directory::Enumerate(VirtualMachine::Mount const* mount, std::function<bool(VirtualMachine::DirectoryEntry const&)> func) const
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(func == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Nothing to do -- there are no child entries in a RootFileSystem directory
}

//-----------------------------------------------------------------------------
// RootFileSystem::Directory::getLength
//
// Gets the length of the node data

size_t RootFileSystem::Directory::getLength(void) const
{
	// There is always just . and .. available to be read
	return sizeof(uapi_linux_dirent64) * 2;
}

//---------------------------------------------------------------------------
// RootFileSystem::Directory::LinkNode
//
// Links an existing node as a child of this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	node		- Node to be linked into this directory
//	name		- Name to assign to the new link

void RootFileSystem::Directory::LinkNode(VirtualMachine::Mount const* mount, VirtualMachine::Node const* node, char_t const* name)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(node == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);
	
	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	
	// RootFileSystem does not allow linking nodes into a directory
	throw LinuxException(UAPI_EPERM);
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
// RootFileSystem::Directory::Read
//
// Reads data from the underlying node into a buffer
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	offset		- Offset from which to begin reading the data
//	buffer		- Destination data output buffer
//	count		- Maximum number of bytes to read into the buffer

size_t RootFileSystem::Directory::Read(VirtualMachine::Mount const* mount, size_t offset, void* buffer, size_t count)
{
	UNREFERENCED_PARAMETER(offset);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	throw LinuxException(UAPI_EISDIR);
}

//---------------------------------------------------------------------------
// RootFileSystem::Directory::SetLength
//
// Sets the length of the node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	length		- New length to assign to the file

size_t RootFileSystem::Directory::SetLength(VirtualMachine::Mount const* mount, size_t length)
{
	UNREFERENCED_PARAMETER(length);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	throw LinuxException(UAPI_EISDIR);
}

//---------------------------------------------------------------------------
// RootFileSystem::Directory::UnlinkNode
//
// Unlinks a child node from this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name of the node to be unlinked

void RootFileSystem::Directory::UnlinkNode(VirtualMachine::Mount const* mount, char_t const* name)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);
	
	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// RootFileSystem does not allow unlinking of child nodes
	throw LinuxException(UAPI_EPERM);
}

//---------------------------------------------------------------------------
// RootFileSystem::Directory::Write
//
// Writes data into the node at the specified position
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	offset		- Offset from which to begin writing the data
//	buffer		- Source data input buffer
//	count		- Maximum number of bytes to write from the buffer

size_t RootFileSystem::Directory::Write(VirtualMachine::Mount const* mount, size_t offset, void const* buffer, size_t count)
{
	UNREFERENCED_PARAMETER(offset);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	
	throw LinuxException(UAPI_EISDIR);
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

//
// ROOTFILESYSTEM::NODE IMPLEMENTATION
//

//---------------------------------------------------------------------------
// RootFileSystem::Node Constructor
//
// Arguments:
//
//	node		- Shared Node instance

template <class _interface, typename _node_type>
RootFileSystem::Node<_interface, _node_type>::Node(std::shared_ptr<_node_type> const& node) : m_node(node)
{
	_ASSERTE(m_node);
}

//---------------------------------------------------------------------------
// RootFileSystem::Node::getAccessTime
//
// Gets the access time of the node

template <class _interface, typename _node_type>
uapi_timespec RootFileSystem::Node<_interface, _node_type>::getAccessTime(void) const
{
	return m_node->atime;
}

//---------------------------------------------------------------------------
// RootFileSystem::Node::getChangeTime
//
// Gets the change time of the node

template <class _interface, typename _node_type>
uapi_timespec RootFileSystem::Node<_interface, _node_type>::getChangeTime(void) const
{
	return m_node->ctime;
}
		
//-----------------------------------------------------------------------------
// RootFileSystem::Node::getGroupId
//
// Gets the currently set owner group identifier for the directory

template <class _interface, typename _node_type>
uapi_gid_t RootFileSystem::Node<_interface, _node_type>::getGroupId(void) const
{
	return m_node->gid;
}

//---------------------------------------------------------------------------
// RootFileSystem::Node::getIndex
//
// Gets the node index within the file system (inode number)

template <class _interface, typename _node_type>
intptr_t RootFileSystem::Node<_interface, _node_type>::getIndex(void) const
{
	return m_node->index;
}

//---------------------------------------------------------------------------
// RootFileSystem::Node::getMode
//
// Gets the type and permissions mask for the node

template <class _interface, typename _node_type>
uapi_mode_t RootFileSystem::Node<_interface, _node_type>::getMode(void) const
{
	return m_node->mode;
}

//---------------------------------------------------------------------------
// RootFileSystem::Node::getModificationTime
//
// Gets the modification time of the node

template <class _interface, typename _node_type>
uapi_timespec RootFileSystem::Node<_interface, _node_type>::getModificationTime(void) const
{
	return m_node->mtime;
}
		
//---------------------------------------------------------------------------
// RootFileSystem::Node::SetAccessTime
//
// Changes the access time of this node
//
// Arugments:
//
//	mount		- Mount point on which to perform this operation
//	atime		- New access time to be set

template <class _interface, typename _node_type>
uapi_timespec RootFileSystem::Node<_interface, _node_type>::SetAccessTime(VirtualMachine::Mount const* mount, uapi_timespec atime)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// If the mount point prevents changing the access time there is nothing to do
	if((mount->Flags & UAPI_MS_NOATIME) == UAPI_MS_NOATIME) return m_node->atime;

	bool		update = false;							// Flag to actually update atime
	datetime	newatime = convert<datetime>(atime);	// New access time as a datetime

	// Update atime if previous atime is more than 24 hours in the past (see mount(2))
	if(newatime > (convert<datetime>(m_node->atime.load()) + timespan::days(1))) update = true;

	// MS_STRICTATIME - update atime
	else if((mount->Flags & UAPI_MS_STRICTATIME) == UAPI_MS_STRICTATIME) update = true;

	// Default (MS_RELATIME) - update atime if it is more recent than ctime or mtime
	else if((newatime >= convert<datetime>(m_node->ctime.load())) || (newatime >= convert<datetime>(m_node->mtime.load()))) update = true;

	if(update) m_node->atime = atime;				// Update access time
	return atime;									// Return new access time
}

//---------------------------------------------------------------------------
// RootFileSystem::Node::SetChangeTime
//
// Changes the change time of this node
//
// Arugments:
//
//	mount		- Mount point on which to perform this operation
//	ctime		- New change time to be set

template <class _interface, typename _node_type>
uapi_timespec RootFileSystem::Node<_interface, _node_type>::SetChangeTime(VirtualMachine::Mount const* mount, uapi_timespec ctime)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_node->ctime = ctime;
	return ctime;
}

//---------------------------------------------------------------------------
// RootFileSystem::Node::SetGroupId
//
// Changes the owner group id for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	gid			- New owner group id to be set

template <class _interface, typename _node_type>
uapi_gid_t RootFileSystem::Node<_interface, _node_type>::SetGroupId(VirtualMachine::Mount const* mount, uapi_gid_t gid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_node->gid = gid;
	return gid;
}

//---------------------------------------------------------------------------
// RootFileSystem::Node::SetMode
//
// Changes the mode flags for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	mode		- New mode flags to be set

template <class _interface, typename _node_type>
uapi_mode_t RootFileSystem::Node<_interface, _node_type>::SetMode(VirtualMachine::Mount const* mount, uapi_mode_t mode)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// Strip out all but the permissions from the provided mode; the type
	// cannot be changed after a node has been created
	mode = ((mode & UAPI_S_IALLUGO) | (m_node->mode & ~UAPI_S_IALLUGO));

	m_node->mode = mode;
	return mode;
}

//---------------------------------------------------------------------------
// RootFileSystem::Node::SetModificationTime
//
// Changes the modification time of this node
//
// Arugments:
//
//	mount		- Mount point on which to perform this operation
//	mtime		- New modification time to be set

template <class _interface, typename _node_type>
uapi_timespec RootFileSystem::Node<_interface, _node_type>::SetModificationTime(VirtualMachine::Mount const* mount, uapi_timespec mtime)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_node->mtime = m_node->ctime = mtime;
	return mtime;
}
		
//---------------------------------------------------------------------------
// RootFileSystem::Node::SetUserId
//
// Changes the owner user id for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	uid			- New owner user id to be set

template <class _interface, typename _node_type>
uapi_uid_t RootFileSystem::Node<_interface, _node_type>::SetUserId(VirtualMachine::Mount const* mount, uapi_uid_t uid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_node->uid = uid;
	return uid;
}

//---------------------------------------------------------------------------
// RootFileSystem::Node::Sync
//
// Synchronizes all metadata and data associated with the file to storage
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation

template <class _interface, typename _node_type>
void RootFileSystem::Node<_interface, _node_type>::Sync(VirtualMachine::Mount const* mount) const
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// This is a no-operation in RootFileSystem
}

//---------------------------------------------------------------------------
// RootFileSystem::Node::SyncData
//
// Synchronizes all data associated with the file to storage, not metadata
//
// Arguments:
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation

template <class _interface, typename _node_type>
void RootFileSystem::Node<_interface, _node_type>::SyncData(VirtualMachine::Mount const* mount) const
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// This is a no-operation in RootFileSystem
}

//-----------------------------------------------------------------------------
// RootFileSystem::Node::getUserId
//
// Gets the currently set owner user identifier for the directory

template <class _interface, typename _node_type>
uapi_uid_t RootFileSystem::Node<_interface, _node_type>::getUserId(void) const
{
	return m_node->uid;
}

//---------------------------------------------------------------------------

#pragma warning(pop)
