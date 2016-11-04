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
#include "HostFileSystem.h"

#include <convert.h>

#include "LinuxException.h"
#include "MountOptions.h"
#include "Win32Exception.h"

#pragma warning(push, 4)

// Invariants
//
static_assert(FILE_BEGIN == UAPI_SEEK_SET,		"HostFileSystem: FILE_BEGIN must be the same value as UAPI_SEEK_SET");
static_assert(FILE_CURRENT == UAPI_SEEK_CUR,	"HostFileSystem: FILE_CURRENT must be the same value as UAPI_SEEK_CUR");
static_assert(FILE_END == UAPI_SEEK_END,		"HostFileSystem: FILE_END must be the same value as UAPI_SEEK_END");

//-----------------------------------------------------------------------------
// MapHostException (local)
//
// Converts a Win32 error code into a representative LinuxException instance
//
// Arguments:
//
//	code		- Win32 error code to be mapped

static LinuxException MapHostException(DWORD code)
{
	int linuxcode = UAPI_EIO;		// Use EIO as the default linux error code

	// Try to map the Win32 error code to something that makes sense
	switch(code) {

		case ERROR_ACCESS_DENIED:		linuxcode = UAPI_EACCES; break;
		case ERROR_FILE_NOT_FOUND:		linuxcode = UAPI_ENOENT; break;
		case ERROR_PATH_NOT_FOUND:		linuxcode = UAPI_ENOENT; break;
		case ERROR_FILE_EXISTS:			linuxcode = UAPI_EEXIST; break;
		case ERROR_INVALID_PARAMETER:	linuxcode = UAPI_EINVAL; break;
		case ERROR_ALREADY_EXISTS:		linuxcode = UAPI_EEXIST; break;
		case ERROR_NOT_ENOUGH_MEMORY:	linuxcode = UAPI_ENOMEM; break;
	}

	// Generate a LinuxException with the mapped code and provide the underlying Win32
	// error as an inner Win32Exception instance
	return LinuxException(linuxcode, Win32Exception(code));
}

//---------------------------------------------------------------------------
// MountHostFileSystem
//
// Creates an instance of HostFileSystem
//
// Arguments:
//
//	source		- Source device string
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

std::unique_ptr<VirtualMachine::Mount> MountHostFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength)
{
	// Source is ignored, but has to be specified by contract
	if(source == nullptr) throw LinuxException(UAPI_EFAULT);

	// Convert the specified options into MountOptions to process the custom parameters
	MountOptions options(flags, data, datalength);

	// Verify that the specified flags are supported for a creation operation
	if(options.Flags & ~HostFileSystem::MOUNT_FLAGS) throw LinuxException(UAPI_EINVAL);

	// Construct the shared file system instance and root node instance
	auto fs = std::make_shared<HostFileSystem>(options.Flags & ~UAPI_MS_PERMOUNT_MASK);
	auto rootnode = std::make_shared<HostFileSystem::node_t>(fs, windows_path(std::to_wstring(source).c_str()));

	// Create and return the mount point instance to the caller, wrapping the root node into a Directory
	return std::make_unique<HostFileSystem::Mount>(fs, std::make_unique<HostFileSystem::Directory>(rootnode), options.Flags & UAPI_MS_PERMOUNT_MASK);
}

//---------------------------------------------------------------------------
// HostFileSystem Constructor
//
// Arguments:
//
//	flags		- Initial file system level flags

HostFileSystem::HostFileSystem(uint32_t flags) : Flags(flags)
{
	// The specified flags should not include any that apply to the mount point
	_ASSERTE((flags & UAPI_MS_PERMOUNT_MASK) == 0);
	if((flags & UAPI_MS_PERMOUNT_MASK) != 0) throw LinuxException(UAPI_EINVAL);
}

//
// HOSTFILESYSTEM::NODE_T IMPLEMENTATION
//

//---------------------------------------------------------------------------
// HostFileSystem:node_t Constructor
//
// Arguments:
//
//	filesystem		- Shared file system instance
//	hostpath		- Path to the node on the host file system

HostFileSystem::node_t::node_t(std::shared_ptr<HostFileSystem> const& filesystem, windows_path&& hostpath) : 
	node_t(filesystem, std::move(hostpath), GetFileAttributes(path))
{
}

//---------------------------------------------------------------------------
// HostFileSystem:node_t Constructor
//
// Arguments:
//
//	filesystem		- Shared file system instance
//	hostpath		- Path to the node on the host file system
//	attributes		- Attributes of the target node on the host file system

HostFileSystem::node_t::node_t(std::shared_ptr<HostFileSystem> const& filesystem, windows_path&& hostpath, DWORD nodeattrs) :
	fs(filesystem), path(hostpath), attributes(nodeattrs)
{
	_ASSERTE(fs);

	// Ensure that the target node attributes are valid, the object may not actually exist
	if(attributes == INVALID_FILE_ATTRIBUTES) throw LinuxException(UAPI_ENOENT);
}

//
// HOSTFILESYSTEM::DIRECTORY IMPLEMENTATION
//

//---------------------------------------------------------------------------
// HostFileSystem::Directory Constructor
//
// Arguments:
//
//	node		- Shared node_t instance

HostFileSystem::Directory::Directory(std::shared_ptr<node_t> const& node) : Node(node)
{
	// The supplied node_t should be a directory object
	_ASSERTE((m_node->attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::CreateDirectory
//
// Creates or opens a directory node as a child of this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name to assign to the new directory
//	mode		- Initial permissions to assign to the node
//	uid			- Initial owner user id to assign to the node
//	gid			- Initial owner group id to assign to the node

std::unique_ptr<VirtualMachine::Node> HostFileSystem::Directory::CreateDirectory(VirtualMachine::Mount const* mount, char_t const* name, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	UNREFERENCED_PARAMETER(mode);
	UNREFERENCED_PARAMETER(uid);
	UNREFERENCED_PARAMETER(gid);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// Attempt to create the directory object on the host file system
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);
	auto path = m_node->path.append(name);

	if(!::CreateDirectory(path, nullptr)) throw MapHostException(GetLastError());

	// Wrap the path to the object into a node_t and return it as a Directory node
	return std::make_unique<Directory>(std::make_shared<node_t>(m_node->fs, std::move(path)));
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::CreateFile
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

std::unique_ptr<VirtualMachine::Node> HostFileSystem::Directory::CreateFile(VirtualMachine::Mount const* mount, char_t const* name, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	UNREFERENCED_PARAMETER(mode);
	UNREFERENCED_PARAMETER(uid);
	UNREFERENCED_PARAMETER(gid);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// Attempt to create the file object on the host file system
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);
	auto path = m_node->path.append(name);

	HANDLE handle = ::CreateFile(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_FLAG_POSIX_SEMANTICS | FILE_ATTRIBUTE_NORMAL, nullptr);
	if(handle == INVALID_HANDLE_VALUE) throw MapHostException(GetLastError());

	CloseHandle(handle);					// Always close the handle

	// Wrap the path to the object into a node_t and return it as a File node
	return std::make_unique<File>(std::make_shared<node_t>(m_node->fs, std::move(path)));
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::CreateHandle
//
// Opens a Handle instance against this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	flags		- Requested handle flags

std::unique_ptr<VirtualMachine::Handle> HostFileSystem::Directory::CreateHandle(VirtualMachine::Mount const* mount, uint32_t flags) const
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Translate the mount level MS_NODIRATIME/MS_NOATIME into O_NOATIME, the handle does not have access
	// access to the mount specific flags, only the file system level flags
	if(((mount->Flags & UAPI_MS_NODIRATIME) == UAPI_MS_NODIRATIME) || ((mount->Flags & UAPI_MS_NOATIME) == UAPI_MS_NOATIME)) flags |= UAPI_O_NOATIME;

	return std::make_unique<DirectoryHandle>(m_node, OpenHandle(flags), flags);
}
		
//---------------------------------------------------------------------------
// HostFileSystem::Directory::CreateSymbolicLink
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

std::unique_ptr<VirtualMachine::Node> HostFileSystem::Directory::CreateSymbolicLink(VirtualMachine::Mount const* mount, char_t const* name, char_t const* target, uapi_uid_t uid, uapi_gid_t gid)
{
	UNREFERENCED_PARAMETER(name);
	UNREFERENCED_PARAMETER(target);
	UNREFERENCED_PARAMETER(uid);
	UNREFERENCED_PARAMETER(gid);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	throw LinuxException(UAPI_EPERM);			// Not supported
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::Duplicate
//
// Duplicates this node instance
//
// Arguments:
//
//	NONE

std::unique_ptr<VirtualMachine::Node> HostFileSystem::Directory::Duplicate(void) const
{
	return std::make_unique<Directory>(m_node);
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::Enumerate
//
// Enumerates all of the entries in this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	func		- Callback function to invoke for each entry; return false to stop

void HostFileSystem::Directory::Enumerate(VirtualMachine::Mount const* mount, std::function<bool(VirtualMachine::DirectoryEntry const&)> func)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// todo
	throw LinuxException(UAPI_EPERM);
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::Link
//
// Links an existing node as a child of this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	node		- Node to be linked into this directory
//	name		- Name to assign to the new link

void HostFileSystem::Directory::Link(VirtualMachine::Mount const* mount, VirtualMachine::Node const* node, char_t const* name)
{
	UNREFERENCED_PARAMETER(node);
	UNREFERENCED_PARAMETER(name);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	throw LinuxException(UAPI_EPERM);			// Not supported
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::getMode
//
// Gets the type and permissions mask for the node

uapi_mode_t HostFileSystem::Directory::getMode(void) const
{
	return (UAPI_S_IFDIR | 0777);
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::OpenHandle (protected)
//
// Opens a host operating system handle against the node
//
// Arguments:
//
//	flags		- Standard O_XXXX handle flags

HANDLE HostFileSystem::Directory::OpenHandle(uint32_t flags) const
{
	DWORD				access = 0;				// Default to query-only access (O_PATH)

	// Check for flags that aren't compatible with directory objects
	if(flags & (UAPI_O_APPEND | UAPI_FASYNC | UAPI_O_CREAT | UAPI_O_EXCL | UAPI_O_TMPFILE | UAPI_O_TRUNC)) throw LinuxException(UAPI_EINVAL);

	// Determine the access rights required for the directory handle
	if((flags & UAPI_O_PATH) == 0) {

		switch(flags & UAPI_O_ACCMODE) {

			case UAPI_O_RDONLY: access = GENERIC_READ; break;
			case UAPI_O_WRONLY: access = GENERIC_WRITE; break;
			case UAPI_O_RDWR: access = GENERIC_READ | GENERIC_WRITE; break;
		
			default: throw LinuxException(UAPI_EINVAL);
		}
	}

	// Attempt to open the Win32 handle against the directory, the only valid disposition is OPEN_EXISTING
	HANDLE handle = ::CreateFile(m_node->path, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING, FILE_FLAG_POSIX_SEMANTICS | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if(handle == INVALID_HANDLE_VALUE) throw MapHostException(GetLastError());

	// O_NOATIME - Set the handle to not track access times for this object
	if((flags & UAPI_O_NOATIME) == UAPI_O_NOATIME) {

		FILETIME noatime{ 0xFFFFFFFF, 0xFFFFFFFF };
		SetFileTime(handle, nullptr, &noatime, nullptr);
	}

	return handle;
}

//-----------------------------------------------------------------------------
// HostFileSystem::Directory::Lookup
//
// Opens a child node of this directory by name
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name of the child node to be looked up

std::unique_ptr<VirtualMachine::Node> HostFileSystem::Directory::Lookup(VirtualMachine::Mount const* mount, char_t const* name) 
{
	// Check the provided mount and ensure the file system is not read only
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// Combine the requested name with the normalized directory path
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);
	auto path = m_node->path.append(name);

	// Determine if the object exists and what kind of node needs to be created
	DWORD attributes = GetFileAttributes(path);
	if(attributes == INVALID_FILE_ATTRIBUTES) throw LinuxException(UAPI_ENOENT);

	// Construct a node_t around the path and attributes and create the Node instance
	auto node = std::make_shared<node_t>(m_node->fs, std::move(path), attributes);
	if((attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) return std::make_unique<Directory>(node);
	else return std::make_unique<File>(node);
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::SetMode
//
// Changes the mode flags for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	mode		- New mode flags to be set on the node

uapi_mode_t HostFileSystem::Directory::SetMode(VirtualMachine::Mount const* mount, uapi_mode_t mode)
{
	UNREFERENCED_PARAMETER(mode);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	return (S_IFDIR | 0777);
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::Stat
//
// Gets statistical information about this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	stat		- Generic stat structure to receive the results

void HostFileSystem::Directory::Stat(VirtualMachine::Mount const* mount, uapi_stat3264* stat)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// todo
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::Unlink
//
// Unlinks a child node from this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name of the node to be unlinked

void HostFileSystem::Directory::Unlink(VirtualMachine::Mount const* mount, char_t const* name)
{
	BOOL				result;				// Result from the unlink operation

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// Create the path to the child node based on this node's path
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);
	auto path = m_node->path.append(name);

	// Access the attributes of the child node to find out what kind of node it is
	DWORD attributes = GetFileAttributes(path);
	if(attributes == INVALID_FILE_ATTRIBUTES) throw LinuxException(UAPI_ENOENT);

	// Attempt to clear any read-only flag on the node prior to deletion
	if((attributes & FILE_ATTRIBUTE_READONLY) == FILE_ATTRIBUTE_READONLY) SetFileAttributes(path, FILE_ATTRIBUTE_NORMAL);

	// Call RemoveDirectory or DeleteFile as appropriate to unlink the target node
	result = ((attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) ? RemoveDirectory(path) : DeleteFile(path);
	if(!result) throw MapHostException(GetLastError());
}

//
// HOSTFILESYSTEM::DIRECTORYHANDLE IMPLEMENTATION
//

//-----------------------------------------------------------------------------
// HostFileSystem::DirectoryHandle Constructor
//
// Arguments:
//
//	node		- Shared node_t instance
//	handle		- Native object handle
//	flags		- Handle instance flags

HostFileSystem::DirectoryHandle::DirectoryHandle(std::shared_ptr<node_t> const& node, HANDLE handle, uint32_t flags) : m_node(node), m_handle(handle), m_flags(flags)
{
	_ASSERTE(node);
	_ASSERTE((m_handle) && (m_handle != INVALID_HANDLE_VALUE));
	_ASSERTE((node->attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);
}

//-----------------------------------------------------------------------------
// HostFileSystem::DirectoryHandle::Duplicate
//
// Duplicates this handle instance
//
// Arguments:
//
//	flags		- Flags to apply to the duplicate handle instance

std::unique_ptr<VirtualMachine::Handle> HostFileSystem::DirectoryHandle::Duplicate(uint32_t flags) const
{
	// todo
	return nullptr;
}

//-----------------------------------------------------------------------------
// HostFileSystem::DirectoryHandle::getFlags
//
// Gets the currently set of handle flags for this instance

uint32_t HostFileSystem::DirectoryHandle::getFlags(void) const
{
	return m_flags;
}

//---------------------------------------------------------------------------
// HostFileSystem::DirectoryHandle::getLength
//
// Gets the length of the file

size_t HostFileSystem::DirectoryHandle::getLength(void) const
{
	// todo: this may be valid
	throw LinuxException(UAPI_EISDIR);
}

//---------------------------------------------------------------------------
// HostFileSystem::DirectoryHandle::Read
//
// Synchronously reads data from the underlying node into a buffer
//
// Arguments:
//
//	buffer		- Destination data buffer
//	count		- Maximum number of bytes to read into the buffer

size_t HostFileSystem::DirectoryHandle::Read(void* buffer, size_t count)
{
	UNREFERENCED_PARAMETER(buffer);
	UNREFERENCED_PARAMETER(count);

	// todo: this may be valid
	throw LinuxException(UAPI_EISDIR);
}

//---------------------------------------------------------------------------
// HostFileSystem::DirectoryHandle::ReadAt
//
// Synchronously reads data from the underlying node into a buffer
//
// Arguments:
//
//	offset		- Delta from specified starting position
//	buffer		- Destination data buffer
//	count		- Maximum number of bytes to read into the buffer

size_t HostFileSystem::DirectoryHandle::ReadAt(size_t offset, void* buffer, size_t count)
{
	UNREFERENCED_PARAMETER(offset);
	UNREFERENCED_PARAMETER(buffer);
	UNREFERENCED_PARAMETER(count);

	throw LinuxException(UAPI_EISDIR);
}

//---------------------------------------------------------------------------
// HostFileSystem::DirectoryHandle::Seek
//
// Changes the file position
//
// Arguments:
//
//	offset		- Delta from specified whence position
//	whence		- Position from which to apply the specified delta

size_t HostFileSystem::DirectoryHandle::Seek(ssize_t offset, int whence)
{
	UNREFERENCED_PARAMETER(offset);
	UNREFERENCED_PARAMETER(whence);

	// todo: this may be valid
	throw LinuxException(UAPI_EISDIR);
}

//---------------------------------------------------------------------------
// HostFileSystem::DirectoryHandle::SetLength
//
// Sets the length of the file
//
// Arguments:
//
//	length		- New length to assign to the file

size_t HostFileSystem::DirectoryHandle::SetLength(size_t length)
{
	UNREFERENCED_PARAMETER(length);
	throw LinuxException(UAPI_EISDIR);
}

//---------------------------------------------------------------------------
// HostFileSystem::DirectoryHandle::Sync
//
// Synchronizes all data associated with the file to storage
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation

void HostFileSystem::DirectoryHandle::Sync(void) const
{
	// Ensure that the file system isn't read-only and the handle isn't read-only
	if((m_node->fs->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) throw LinuxException(UAPI_EACCES);

	// no-operation
}

//---------------------------------------------------------------------------
// HostFileSystem::DirectoryHandle::Write
//
// Synchronously writes data from a buffer to the underlying node
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	buffer		- Source data buffer
//	count		- Maximum number of bytes to write from the buffer

size_t HostFileSystem::DirectoryHandle::Write(const void* buffer, size_t count)
{
	UNREFERENCED_PARAMETER(buffer);
	UNREFERENCED_PARAMETER(count);

	throw LinuxException(UAPI_EISDIR);
}

//---------------------------------------------------------------------------
// HostFileSystem::DirectoryHandle::WriteAt
//
// Synchronously writes data from a buffer to the underlying node
//
// Arguments:
//
//	offset		- Delta from the specifieed whence position
//	buffer		- Source data buffer
//	count		- Maximum number of bytes to write from the buffer

size_t HostFileSystem::DirectoryHandle::WriteAt(size_t offset, const void* buffer, size_t count)
{
	UNREFERENCED_PARAMETER(offset);
	UNREFERENCED_PARAMETER(buffer);
	UNREFERENCED_PARAMETER(count);

	throw LinuxException(UAPI_EISDIR);
}

//
// HOSTFILESYSTEM::FILE IMPLEMENTATION
//

//---------------------------------------------------------------------------
// HostFileSystem::File Constructor
//
// Arguments:
//
//	node		- Shared node_t instance

HostFileSystem::File::File(std::shared_ptr<node_t> const& node) : Node(node)
{
	// The supplied node_t should not be a directory object
	_ASSERTE((m_node->attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

//---------------------------------------------------------------------------
// HostFileSystem::File::CreateHandle
//
// Opens a Handle instance against this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	flags		- Requested handle flags

std::unique_ptr<VirtualMachine::Handle> HostFileSystem::File::CreateHandle(VirtualMachine::Mount const* mount, uint32_t flags) const
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Ensure that the mount-level MS_NOEXEC flag isn't set in conjunction with O_EXEC
	if(((mount->Flags & UAPI_MS_NOEXEC) == UAPI_MS_NOEXEC) && ((flags & UAPI_O_EXEC) == UAPI_O_EXEC)) throw LinuxException(UAPI_EACCES);

	// Translate the mount level MS_NOATIME into O_NOATIME, the handle does not have access
	// to the mount specific flags, only file system level flags
	if((mount->Flags & UAPI_MS_NOATIME) == UAPI_MS_NOATIME) flags |= UAPI_O_NOATIME;

	return std::make_unique<FileHandle>(m_node, OpenHandle(flags), flags);
}
		
//---------------------------------------------------------------------------
// HostFileSystem::File::Duplicate
//
// Duplicates this node instance
//
// Arguments:
//
//	NONE

std::unique_ptr<VirtualMachine::Node> HostFileSystem::File::Duplicate(void) const
{
	return std::make_unique<File>(m_node);
}

//---------------------------------------------------------------------------
// HostFileSystem::File::getMode
//
// Gets the type and permission masks from the node

uapi_mode_t HostFileSystem::File::getMode(void) const
{
	return (S_IFREG | 0777);
}

//---------------------------------------------------------------------------
// HostFileSystem::File::OpenHandle (protected)
//
// Opens a host operating system handle against the node
//
// Arguments:
//
//	flags		- Standard O_XXXX handle flags

HANDLE HostFileSystem::File::OpenHandle(uint32_t flags) const
{
	DWORD		access = 0;									// Default to query-only access (O_PATH)
	DWORD		disposition = OPEN_EXISTING;				// Default to opening an existing file
	DWORD		attributes = FILE_FLAG_POSIX_SEMANTICS;		// Default handle attributes

	// Check for flags that aren't compatible with file objects
	if(flags & (UAPI_O_DIRECTORY | UAPI_FASYNC | UAPI_O_TMPFILE)) throw LinuxException(UAPI_EINVAL);

	// O_PATH, O_RDONLY, O_WRONLY, O_RDWR, O_EXEC - Use appropriate access rights
	if((flags & UAPI_O_PATH) == 0) {

		switch(flags & UAPI_O_ACCMODE) {

			case UAPI_O_RDONLY: access = GENERIC_READ; break;
			case UAPI_O_WRONLY: access = GENERIC_WRITE; break;
			case UAPI_O_RDWR: access = GENERIC_READ | GENERIC_WRITE; break;
		
			default: throw LinuxException(UAPI_EINVAL);
		}

		// O_EXEC is a special flag not included in standard Linux, but using O_PATH or adding a special
		// method call just to add EXECUTE rights to the handle seems like overkill
		if((flags & UAPI_O_EXEC) == UAPI_O_EXEC) access |= GENERIC_EXECUTE;
	}

	// O_CREAT, O_EXCL, O_TRUNC - Use an appropriate handle disposition flag
	switch(flags & (UAPI_O_CREAT | UAPI_O_EXCL | UAPI_O_TRUNC)) {

		case 0: disposition = OPEN_EXISTING;
		case UAPI_O_CREAT: disposition = OPEN_ALWAYS;
		case UAPI_O_CREAT | UAPI_O_EXCL: disposition = CREATE_ALWAYS;
		case UAPI_O_TRUNC: disposition = TRUNCATE_EXISTING;
		default: throw LinuxException(UAPI_EINVAL);
	}

	// O_DIRECT, O_DSYNC, O_SYNC -- A write-through handle is a reasonable approximation for these
	if((flags & UAPI_O_DIRECT) == UAPI_O_DIRECT) attributes |= FILE_FLAG_WRITE_THROUGH;
	if((flags & UAPI_O_DSYNC) == UAPI_O_DSYNC) attributes |= FILE_FLAG_WRITE_THROUGH;
	if((flags & UAPI_O_SYNC) == UAPI_O_SYNC) attributes |= FILE_FLAG_WRITE_THROUGH;

	// Attempt to open the Win32 handle against the file using the generated access and disposition
	HANDLE handle = ::CreateFile(m_node->path, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 
		nullptr, disposition, attributes, nullptr);
	if(handle == INVALID_HANDLE_VALUE) throw MapHostException(GetLastError());

	// O_NOATIME - Set the handle to not track access times for this object
	if((flags & UAPI_O_NOATIME) == UAPI_O_NOATIME) {

		FILETIME noatime{ 0xFFFFFFFF, 0xFFFFFFFF };
		SetFileTime(handle, nullptr, &noatime, nullptr);
	}

	return handle;
}

//---------------------------------------------------------------------------
// HostFileSystem::File::SetMode
//
// Changes the mode flags for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	mode		- New mode flags to be set on the node

uapi_mode_t HostFileSystem::File::SetMode(VirtualMachine::Mount const* mount, uapi_mode_t mode)
{
	UNREFERENCED_PARAMETER(mode);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	return (S_IFREG | 0777);
}

//---------------------------------------------------------------------------
// HostFileSystem::File::Stat
//
// Gets statistical information about this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	stat		- Generic stat structure to receive the results

void HostFileSystem::File::Stat(VirtualMachine::Mount const* mount, uapi_stat3264* stat)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// todo
}

//
// HOSTFILESYSTEM::FILEHANDLE IMPLEMENTATION
//

//-----------------------------------------------------------------------------
// HostFileSystem::FileHandle Constructor
//
// Arguments:
//
//	node		- Shared node_t instance
//	handle		- Native object handle
//	flags		- Handle instance flags

HostFileSystem::FileHandle::FileHandle(std::shared_ptr<node_t> const& node, HANDLE handle, uint32_t flags) : m_node(node), m_handle(handle), m_flags(flags)
{
	_ASSERTE(node);
	_ASSERTE((m_handle) && (m_handle != INVALID_HANDLE_VALUE));
	_ASSERTE((node->attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

//-----------------------------------------------------------------------------
// HostFileSystem::FileHandle::Duplicate
//
// Duplicates this handle instance
//
// Arguments:
//
//	flags		- Flags to apply to the duplicate handle instance

std::unique_ptr<VirtualMachine::Handle> HostFileSystem::FileHandle::Duplicate(uint32_t flags) const
{
	// todo
	return nullptr;
}

//-----------------------------------------------------------------------------
// HostFileSystem::FileHandle::getFlags
//
// Gets the currently set of handle flags for this instance

uint32_t HostFileSystem::FileHandle::getFlags(void) const
{
	return m_flags;
}

//---------------------------------------------------------------------------
// HostFileSystem::FileHandle::getLength
//
// Gets the length of the file

size_t HostFileSystem::FileHandle::getLength(void) const
{
	LARGE_INTEGER size;
	if(!GetFileSizeEx(m_handle, &size)) throw MapHostException(GetLastError());

#ifdef _M_X64
	// 64-bit size_t can handle all of a LONGLONG
	return size.QuadPart;
#else
	// 32-bit size_t can't handle all of a LONGLONG, apply a ceiling to the value
	return (size.QuadPart > std::numeric_limits<size_t>::max()) ? std::numeric_limits<size_t>::max() : static_cast<size_t>(size.QuadPart);
#endif
}

//---------------------------------------------------------------------------
// HostFileSystem::FileHandle::Read
//
// Synchronously reads data from the underlying node into a buffer
//
// Arguments:
//
//	buffer		- Destination data buffer
//	count		- Maximum number of bytes to read into the buffer

size_t HostFileSystem::FileHandle::Read(void* buffer, size_t count)
{
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Ensure that the handle was not opened in write-only mode
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) throw LinuxException(UAPI_EACCES);

	// If no data is being requested there is no reason to bother
	if(count == 0) return 0;

	// ReadFile() can only read up to MAXDWORD bytes from the underlying file
	if(count >= MAXDWORD) throw LinuxException(UAPI_EINVAL);

	// Attempt to read the specified number of bytes from the file into the buffer
	DWORD read = static_cast<DWORD>(count);
	if(!ReadFile(m_handle, buffer, read, &read, nullptr)) throw MapHostException(GetLastError());

	return static_cast<size_t>(read);
}

//---------------------------------------------------------------------------
// HostFileSystem::FileHandle::ReadAt
//
// Synchronously reads data from the underlying node into a buffer
//
// Arguments:
//
//	offset		- Delta from specified starting position
//	buffer		- Destination data buffer
//	count		- Maximum number of bytes to read into the buffer

size_t HostFileSystem::FileHandle::ReadAt(size_t offset, void* buffer, size_t count)
{
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Ensure that the handle was not opened in write-only mode
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) throw LinuxException(UAPI_EACCES);

	// If no data is being requested there is no reason to bother
	if(count == 0) return 0;

	// ReadFile() can only read up to MAXDWORD bytes from the underlying file
	if(count >= MAXDWORD) throw LinuxException(UAPI_EINVAL);

	// OVERLAPPED structure can be used to read from a specific position
	OVERLAPPED overlapped = { 0 };
	overlapped.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
#ifdef _M_X64
	overlapped.OffsetHigh = (offset >> 32);
#else
	overlapped.OffsetHigh = 0;
#endif

	// Attempt to read the specified number of bytes from the file into the buffer
	DWORD read = static_cast<DWORD>(count);
	if(!ReadFile(m_handle, buffer, read, &read, &overlapped)) throw MapHostException(GetLastError());

	return static_cast<size_t>(read);
}

//---------------------------------------------------------------------------
// HostFileSystem::FileHandle::Seek
//
// Changes the file position
//
// Arguments:
//
//	offset		- Delta from specified whence position
//	whence		- Position from which to apply the specified delta

size_t HostFileSystem::FileHandle::Seek(ssize_t offset, int whence)
{
	LARGE_INTEGER delta;
	delta.QuadPart = offset;

	if(!SetFilePointerEx(m_handle, delta, &delta, static_cast<DWORD>(whence))) throw MapHostException(GetLastError());
	return static_cast<size_t>(delta.QuadPart);
}

//---------------------------------------------------------------------------
// HostFileSystem::FileHandle::SetLength
//
// Sets the length of the file
//
// Arguments:
//
//	length		- New length to assign to the file

size_t HostFileSystem::FileHandle::SetLength(size_t length)
{
	LARGE_INTEGER current, delta;
	current.QuadPart = delta.QuadPart = 0;

	// Ensure that the file system isn't read-only and the handle isn't read-only
	if((m_node->fs->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) throw LinuxException(UAPI_EACCES);

	// Get the current file pointer so that it can be restored and then move it to the desired end point
	if(!SetFilePointerEx(m_handle, current, &current, FILE_CURRENT) || !SetFilePointerEx(m_handle, delta, &delta, FILE_BEGIN)) throw MapHostException(GetLastError());
	
	// Attempt to truncate/expand the length of the file by setting EOF to the current position
	if(!SetEndOfFile(m_handle)) throw MapHostException(GetLastError());

	// Attempt to restore the original file pointer
	SetFilePointerEx(m_handle, current, &current, FILE_BEGIN);

	return length;
}

//---------------------------------------------------------------------------
// HostFileSystem::FileHandle::Sync
//
// Synchronizes all data associated with the file to storage
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation

void HostFileSystem::FileHandle::Sync(void) const
{
	// Ensure that the file system isn't read-only and the handle isn't read-only
	if((m_node->fs->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) throw LinuxException(UAPI_EACCES);

	FlushFileBuffers(m_handle);
}

//---------------------------------------------------------------------------
// HostFileSystem::FileHandle::Write
//
// Synchronously writes data from a buffer to the underlying node
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	buffer		- Source data buffer
//	count		- Maximum number of bytes to write from the buffer

size_t HostFileSystem::FileHandle::Write(const void* buffer, size_t count)
{
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Ensure that the file system isn't read-only and the handle isn't read-only
	if((m_node->fs->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) throw LinuxException(UAPI_EACCES);

	// If no data is being written, there is no reason to bother calling WriteFile
	if(count == 0) return 0;

	// WriteFile() can only write up to MAXDWORD bytes from the underlying file
	if(count >= MAXDWORD) throw LinuxException(UAPI_EINVAL);

	// O_APPEND - Move the pointer to the end of the file before every write
	if((m_flags & UAPI_O_APPEND) == UAPI_O_APPEND) SetFilePointerEx(m_handle, { 0, 0 }, nullptr, FILE_END);

	// Attempt to write the specified number of bytes into the file into the buffer
	DWORD written = static_cast<DWORD>(count);
	if(!WriteFile(m_handle, buffer, written, &written, nullptr)) throw MapHostException(GetLastError());

	return static_cast<size_t>(written);
}

//---------------------------------------------------------------------------
// HostFileSystem::FileHandle::WriteAt
//
// Synchronously writes data from a buffer to the underlying node
//
// Arguments:
//
//	offset		- Delta from the specifieed whence position
//	buffer		- Source data buffer
//	count		- Maximum number of bytes to write from the buffer

size_t HostFileSystem::FileHandle::WriteAt(size_t offset, const void* buffer, size_t count)
{
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Ensure that the file system isn't read-only and the handle isn't read-only
	if((m_node->fs->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) throw LinuxException(UAPI_EACCES);

	// If no data is being written, there is no reason to bother calling WriteFile
	if(count == 0) return 0;

	// WriteFile() can only write up to MAXDWORD bytes from the underlying file
	if(count >= MAXDWORD) throw LinuxException(UAPI_EINVAL);

	// OVERLAPPED structure can be used to write to a specific position
	OVERLAPPED overlapped = { 0 };
	overlapped.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
#ifdef _M_X64
	overlapped.OffsetHigh = (offset >> 32);
#else
	overlapped.OffsetHigh = 0;
#endif

	// Attempt to write the specified number of bytes from the buffer into the file
	DWORD written = static_cast<DWORD>(count);
	if(!WriteFile(m_handle, buffer, written, &written, &overlapped)) throw MapHostException(GetLastError());

	return static_cast<size_t>(written);
}

//
// HOSTFILESYSTEM::MOUNT IMPLEMENTATION
//

//-----------------------------------------------------------------------------
// HostFileSystem::Mount Constructor
//
// Arguments:
//
//	fs			- Shared file system instance
//	rootdir		- Root directory node instance
//	flags		- Mount-specific flags

HostFileSystem::Mount::Mount(std::shared_ptr<HostFileSystem> const& fs, std::unique_ptr<Directory>&& rootdir, uint32_t flags) : 
	m_fs(fs), m_rootdir(std::move(rootdir)), m_flags(flags)
{
	_ASSERTE(m_fs);
	_ASSERTE(m_rootdir);

	// The specified flags should not include any that apply to the file system
	_ASSERTE((flags & ~UAPI_MS_PERMOUNT_MASK) == 0);
	if((flags & ~UAPI_MS_PERMOUNT_MASK) != 0) throw LinuxException(UAPI_EINVAL);
}

//---------------------------------------------------------------------------
// HostFileSystem::Mount Constructor
//
// Arguments:
//
//	rhs		- Existing Mount instance to create a copy of

HostFileSystem::Mount::Mount(Mount const& rhs) : m_fs(rhs.m_fs), m_rootdir(rhs.m_rootdir), m_flags(static_cast<uint32_t>(rhs.m_flags))
{
	_ASSERTE(m_fs);
	_ASSERTE(m_rootdir);
}

//---------------------------------------------------------------------------
// HostFileSystem::Mount::Duplicate
//
// Duplicates this mount instance
//
// Arguments:
//
//	NONE

std::unique_ptr<VirtualMachine::Mount> HostFileSystem::Mount::Duplicate(void) const
{
	return std::make_unique<HostFileSystem::Mount>(*this);
}

//---------------------------------------------------------------------------
// HostFileSystem::Mount::getFileSystem
//
// Accesses the underlying file system instance

VirtualMachine::FileSystem* HostFileSystem::Mount::getFileSystem(void) const
{
	return m_fs.get();
}

//---------------------------------------------------------------------------
// HostFileSystem::Mount::getFlags
//
// Gets the mount point flags

uint32_t HostFileSystem::Mount::getFlags(void) const
{
	// Combine the mount flags with those of the underlying file system
	return m_fs->Flags | m_flags;
}

//---------------------------------------------------------------------------
// HostFileSystem::Mount::GetRootNode
//
// Gets the root node of the mount point
//
// Arguments:
//
//	NONE

VirtualMachine::Node* HostFileSystem::Mount::getRootNode(void) const
{
	return m_rootdir.get();
}

//
// HOSTFILESYSTEM::NODE IMPLEMENTATION
//

//---------------------------------------------------------------------------
// HostFileSystem::Node Constructor
//
// Arguments:
//
//	node		- Shared node_t instance

template <class _interface>
HostFileSystem::Node<_interface>::Node(std::shared_ptr<node_t> const& node) : m_node(node)
{
	_ASSERTE(m_node);
}

//---------------------------------------------------------------------------
// HostFileSystem::Node::getAccessTime
//
// Gets the access time of the node

template <class _interface>
uapi_timespec HostFileSystem::Node<_interface>::getAccessTime(void) const
{
	WIN32_FILE_ATTRIBUTE_DATA		data;		// File attributes

	if(!GetFileAttributesEx(m_node->path, GetFileExInfoStandard, &data)) throw MapHostException(GetLastError());
	return convert<uapi_timespec>(data.ftLastAccessTime);
}

//---------------------------------------------------------------------------
// HostFileSystem::Node::getChangeTime
//
// Gets the change time of the node

template <class _interface>
uapi_timespec HostFileSystem::Node<_interface>::getChangeTime(void) const
{
	WIN32_FILE_ATTRIBUTE_DATA		data;		// File attributes

	if(!GetFileAttributesEx(m_node->path, GetFileExInfoStandard, &data)) throw MapHostException(GetLastError());
	return convert<uapi_timespec>(data.ftLastWriteTime);
}
		
//---------------------------------------------------------------------------
// HostFileSystem::Node::getGroupId
//
// Gets the currently set owner group identifier for the file

template <class _interface>
uapi_gid_t HostFileSystem::Node<_interface>::getGroupId(void) const
{
	return 0;
}

//---------------------------------------------------------------------------
// HostFileSystem::Node::getIndex
//
// Gets the node index within the file system (inode number)

template <class _interface>
intptr_t HostFileSystem::Node<_interface>::getIndex(void) const
{
	// TODO: Trying this instead .... inode numbers are important
	// but they also need to be fast for Path to work right
	return intptr_t(m_node.get());

////	BY_HANDLE_FILE_INFORMATION		info;			// File information
////
////	// Accessing the node index requires a query handle to the node
////	HANDLE handle = OpenHandle(UAPI_O_PATH);
////
////	try {
////		
////		// Attempt to access the BY_HANDLE_FILE_INFORMATION for this node
////		if(!GetFileInformationByHandle(handle, &info)) throw MapHostException(GetLastError());
////		CloseHandle(handle);
////	}
////	catch(...) { CloseHandle(handle); throw; }
////
////#ifndef _M_X64
////	// 32-bit builds can't handle the entire index, use the lower 32 bits only
////	return intptr_t(info.nFileIndexLow);
////#else
////	// 64-bit builds can handle the entire 64-bit index value, combine them
////	return intptr_t(info.nFileIndexHigh) << 32 | info.nFileIndexLow;
////#endif
}

//---------------------------------------------------------------------------
// HostFileSystem::Node::getModificationTime
//
// Gets the modification time of the node

template <class _interface>
uapi_timespec HostFileSystem::Node<_interface>::getModificationTime(void) const
{
	WIN32_FILE_ATTRIBUTE_DATA		data;		// File attributes

	if(!GetFileAttributesEx(m_node->path, GetFileExInfoStandard, &data)) throw MapHostException(GetLastError());
	return convert<uapi_timespec>(data.ftLastWriteTime);
}
		
//---------------------------------------------------------------------------
// HostFileSystem::Node::SetAccessTime
//
// Changes the access time of this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	atime		- New access time to be set

template <class _interface>
uapi_timespec HostFileSystem::Node<_interface>::SetAccessTime(VirtualMachine::Mount const* mount, uapi_timespec atime)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	FILETIME accesstime = convert<FILETIME>(atime);

	// Changing the access time requires a write handle to the node
	HANDLE handle = OpenHandle(UAPI_O_WRONLY);

	try {

		// Attempt to change the access time stamp on the node
		if(!SetFileTime(handle, nullptr, &accesstime, nullptr)) throw MapHostException(GetLastError());
		CloseHandle(handle);
	}
	catch(...) { CloseHandle(handle); throw; }

	return atime;
}

//---------------------------------------------------------------------------
// HostFileSystem::Node::SetChangeTime
//
// Changes the change time of this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	ctime		- New change time to be set

template <class _interface>
uapi_timespec HostFileSystem::Node<_interface>::SetChangeTime(VirtualMachine::Mount const* mount, uapi_timespec ctime)
{
	// Windows does not maintain change time, apply via modification time
	return SetModificationTime(mount, ctime);
}

//---------------------------------------------------------------------------
// HostFileSystem::Node::SetGroupId
//
// Changes the owner group id for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	gid			- New owner group id to be set

template <class _interface>
uapi_gid_t HostFileSystem::Node<_interface>::SetGroupId(VirtualMachine::Mount const* mount, uapi_gid_t gid)
{
	UNREFERENCED_PARAMETER(gid);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	return 0;
}

//---------------------------------------------------------------------------
// HostFileSystem::Node::SetModificationTime
//
// Changes the modification time of this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	mtime		- New modification time to be set

template <class _interface>
uapi_timespec HostFileSystem::Node<_interface>::SetModificationTime(VirtualMachine::Mount const* mount, uapi_timespec mtime)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	FILETIME modificationtime = convert<FILETIME>(mtime);

	// Changing the access time requires a write handle to the node
	HANDLE handle = OpenHandle(UAPI_O_WRONLY);

	try {

		// Attempt to change the access time stamp on the node
		if(!SetFileTime(handle, nullptr, nullptr, &modificationtime)) throw MapHostException(GetLastError());
		CloseHandle(handle);
	}
	catch(...) { CloseHandle(handle); throw; }

	return mtime;
}

//---------------------------------------------------------------------------
// HostFileSystem::Node::SetUserId
//
// Changes the owner user id for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	uid			- New owner user id to be set

template <class _interface>
uapi_uid_t HostFileSystem::Node<_interface>::SetUserId(VirtualMachine::Mount const* mount, uapi_uid_t uid)
{
	UNREFERENCED_PARAMETER(uid);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	return 0;
}

//---------------------------------------------------------------------------
// HostFileSystem::Node::Sync
//
// Synchronizes all metadata and data associated with the file to storage
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation

template <class _interface>
void HostFileSystem::Node<_interface>::Sync(VirtualMachine::Mount const* mount) const
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// no-op
}

//---------------------------------------------------------------------------
// HostFileSystem::Node::getUserId
//
// Gets the currently set owner user identifier for the file

template <class _interface>
uapi_uid_t HostFileSystem::Node<_interface>::getUserId(void) const
{
	return 0;
}

//---------------------------------------------------------------------------

#pragma warning(pop)
