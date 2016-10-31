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

//-----------------------------------------------------------------------------
// GetNormalizedPath (local)
//
// Gets the normalized path for a Windows file system handle
//
// Arguments:
//
//	handle		- Windows file system handle to get the path for

windows_path GetNormalizedPath(HANDLE const handle)
{
	std::unique_ptr<wchar_t[]>	pathstr;				// Normalized file system path
	DWORD						cch, pathlen = 0;		// Path string lengths

	_ASSERTE((handle) && (handle != INVALID_HANDLE_VALUE));
	if((handle == nullptr) || (handle == INVALID_HANDLE_VALUE)) throw LinuxException(UAPI_EINVAL);

	// There is a possibility that the file system object could be renamed externally between calls to
	// GetFinalPathNameByHandle so this must be done in a loop to ensure that it ultimately succeeds
	do {

		// If the buffer is too small, this will return the required size including the null terminator
		// otherwise it will return the number of characters copied into the output buffer
		cch = GetFinalPathNameByHandleW(handle, pathstr.get(), pathlen, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
		if(cch == 0) throw MapHostException(GetLastError());

		if(cch > pathlen) {

			// The buffer is too small for the current object path, reallocate it
			pathstr = std::make_unique<wchar_t[]>(pathlen = cch);
			if(!pathstr) throw LinuxException(UAPI_ENOMEM);
		}

	} while(cch >= pathlen);

	// Use (cch + 1) as the path buffer length rather than pathlen here, if the
	// object was renamed the buffer may actually be longer than necessary
	return windows_path(std::move(pathstr), cch + 1);
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
	bool sandbox = true;						// Flag to sandbox the virtual file system

	// Source is ignored, but has to be specified by contract
	if(source == nullptr) throw LinuxException(UAPI_EFAULT);

	// Convert the specified options into MountOptions to process the custom parameters
	MountOptions options(flags, data, datalength);

	// Verify that the specified flags are supported for a creation operation
	if(options.Flags & ~HostFileSystem::MOUNT_FLAGS) throw LinuxException(UAPI_EINVAL);

	try {

		// sandbox
		//
		// Sets the option to check all nodes are within the base mounting path
		if(options.Arguments.Contains("sandbox")) sandbox = true;

		// nosandbox
		//
		// Clears the option to check all nodes are within the base mounting path
		if(options.Arguments.Contains("nosandbox")) sandbox = false;
	}

	catch(...) { throw LinuxException(UAPI_EINVAL); }

	// Construct the shared file system instance
	auto fs = std::make_shared<HostFileSystem>(options.Flags & ~UAPI_MS_PERMOUNT_MASK, sandbox);

	// Construct the root directory instance as a path-only directory handle
	std::wstring wpath = std::to_wstring(source);
	auto rootdir = std::make_shared<HostFileSystem::Directory>(fs, wpath.c_str(), UAPI_O_DIRECTORY | UAPI_O_PATH);

	// Create and return the mount point instance to the caller
	return std::make_unique<HostFileSystem::Mount>(fs, rootdir, options.Flags & UAPI_MS_PERMOUNT_MASK);
}

//---------------------------------------------------------------------------
// HostFileSystem Constructor
//
// Arguments:
//
//	flags		- Initial file system level flags
//	sandbox		- Initial [no]sandbox option indicator

HostFileSystem::HostFileSystem(uint32_t flags, bool sandbox) : Flags(flags), Sandbox(sandbox)
{
	// The specified flags should not include any that apply to the mount point
	_ASSERTE((flags & UAPI_MS_PERMOUNT_MASK) == 0);
	if((flags & UAPI_MS_PERMOUNT_MASK) != 0) throw LinuxException(UAPI_EINVAL);
}

//
// HOSTFILESYSTEM::DIRECTORY IMPLEMENTATION
//

//---------------------------------------------------------------------------
// HostFileSystem::Directory Constructor
//
// Arguments:
//
//	fs			- Shared file system instance
//	path		- Path to the host operating system object
//	flags		- Instance-specific handle flags

HostFileSystem::Directory::Directory(std::shared_ptr<HostFileSystem> const& fs, wchar_t const* path, uint32_t flags) : Node(fs, OpenHandle(path, flags), flags)
{
	_ASSERTE(m_handle);
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory Constructor
//
// Arguments:
//
//	rhs			- Existing File instance to duplicate
//	flags		- Instance-specific handle flags

HostFileSystem::Directory::Directory(Directory const& rhs, uint32_t flags) : Node(rhs.m_fs, OpenHandle(rhs.m_path, flags), flags)
{
	// todo: this could be more efficient by using ReOpenFile and copying the windows_path
	// but that requires some additional support that isn't entirely necessary right now
	_ASSERTE(m_handle);
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

void HostFileSystem::Directory::CreateDirectory(VirtualMachine::Mount const* mount, char_t const* name, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	UNREFERENCED_PARAMETER(mode);
	UNREFERENCED_PARAMETER(uid);
	UNREFERENCED_PARAMETER(gid);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// Attempt to create the directory on the host file system
	if(!::CreateDirectoryW(m_path.append(name), nullptr)) throw MapHostException(GetLastError());
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

void HostFileSystem::Directory::CreateFile(VirtualMachine::Mount const* mount, char_t const* name, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	UNREFERENCED_PARAMETER(mode);
	UNREFERENCED_PARAMETER(uid);
	UNREFERENCED_PARAMETER(gid);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// Attempt to create the new file on the host file system
	HANDLE handle = ::CreateFileW(m_path.append(name), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_FLAG_POSIX_SEMANTICS | FILE_ATTRIBUTE_NORMAL, nullptr);
	if(handle == INVALID_HANDLE_VALUE) throw MapHostException(GetLastError());

	// This function does not return a handle/node type, close the OS handle
	CloseHandle(handle);
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

void HostFileSystem::Directory::CreateSymbolicLink(VirtualMachine::Mount const* mount, char_t const* name, char_t const* target, uapi_uid_t uid, uapi_gid_t gid)
{
	UNREFERENCED_PARAMETER(mount);
	UNREFERENCED_PARAMETER(name);
	UNREFERENCED_PARAMETER(target);
	UNREFERENCED_PARAMETER(uid);
	UNREFERENCED_PARAMETER(gid);

	// HostFileSystem does not support creation of symbolic links
	throw LinuxException(UAPI_EPERM);
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::Duplicate
//
// Duplicates this node handle
//
// Arguments:
//
//	flags		- New flags to be applied to the duplicate handle

std::unique_ptr<VirtualMachine::Node> HostFileSystem::Directory::Duplicate(uint32_t flags) const
{
	// todo - this should duplicate the handle or use ReOpenFile - the file pointer
	// should remain shared among all duplicates
	return std::unique_ptr<Directory>(new Directory(*this, flags));
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
	// todo
	throw LinuxException(UAPI_EPERM);
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::LinkNode
//
// Links an existing node as a child of this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	node		- Node to be linked into this directory
//	name		- Name to assign to the new link

void HostFileSystem::Directory::LinkNode(VirtualMachine::Mount const* mount, VirtualMachine::Node const* node, char_t const* name)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// HostFileSystem does not support hard links
	throw LinuxException(UAPI_EPERM);
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
// HostFileSystem::Directory::OpenHandle (private, static)
//
// Opens the Win32 handle for the directory object
//
// Arguments:
//
//	path			- Host path to the directory node
//	flags			- Handle flags and attributes

HANDLE HostFileSystem::Directory::OpenHandle(wchar_t const* path, uint32_t flags)
{
	// Directories can only be opened in read-only mode
	if((flags & UAPI_O_ACCMODE) != UAPI_O_RDONLY) throw LinuxException(UAPI_EISDIR);

	// Verify that only valid flags for use with directories have been specified, even if they are ignored
	if((flags & ~UAPI_O_ACCMODE) & ~(UAPI_O_CLOEXEC | UAPI_O_DIRECTORY | UAPI_O_NOATIME | UAPI_O_PATH)) throw LinuxException(UAPI_EINVAL);

	// Open a query-only handle against the target directory node, which must already exist
	HANDLE handle = ::CreateFile(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_POSIX_SEMANTICS | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if(handle == INVALID_HANDLE_VALUE) throw MapHostException(GetLastError());

	try {

		// Get the basic information about the handle to ensure that it represents a directory node
		FILE_BASIC_INFO info;
		if(!GetFileInformationByHandleEx(handle, FileBasicInfo, &info, sizeof(FILE_BASIC_INFO))) throw MapHostException(GetLastError());
		if((info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY) throw LinuxException(UAPI_ENOTDIR);
	}

	catch(...) { CloseHandle(handle); throw; }

	return handle;
}

//-----------------------------------------------------------------------------
// HostFileSystem::Directory::OpenNode
//
// Opens a child node of this directory by name
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name of the child node to be looked up
//	flags		- Open operation flags
//	mode		- Permission mask to assign to the node if created
//	uid			- Owner user id to assign to the node if created
//	gid			- Owner group id to assign to the node if created

std::unique_ptr<VirtualMachine::Node> HostFileSystem::Directory::OpenNode(VirtualMachine::Mount const* mount, char_t const* name, uint32_t flags,
	uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) 
{
	std::unique_ptr<VirtualMachine::Node>		node;		// Resultant Node instance

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check the provided mount and ensure the file system/handle is not read only
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// Combine the requested name with the normalized directory path
	auto hostpath = m_path.append(name);

	// Determine if the object exists and what kind of node needs to be created
	DWORD attributes = GetFileAttributes(hostpath);
	if(attributes == INVALID_FILE_ATTRIBUTES) throw LinuxException(UAPI_ENOENT);

	// Generate the flags required to open the node handle; directories require BACKUP_SEMANTICS
	//////DWORD flags = FILE_FLAG_POSIX_SEMANTICS;
	///////if((attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) flags |= FILE_FLAG_BACKUP_SEMANTICS;

	// Attempt to open a query-only handle against the host file system object
	//HANDLE handle = ::CreateFile(hostpath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, flags, nullptr);
	//if(handle == INVALID_HANDLE_VALUE) throw MapHostException(GetLastError());

	///////// TODO TESTING
	///auto nodeptr = std::make_shared<directory_node_t>(m_handle->node->fs, hostpath);
	/*auto handleptr = std::make_shared<directory_handle_t>(nodeptr);*/
	////return std::make_unique<Directory>(nodeptr, 0);
	return nullptr;

	//////// Create the appropriate node type to return to the caller
	//////if((attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) node = std::make_unique<Directory>(std::make_shared<directory_node_t>(m_node->fs, handle));
	//////else node = std::make_unique<File>(std::make_shared<file_node_t>(m_handle->node->fs, handle));

	//////// todo: check sandbox option against the generated node before returning it

	//////return node;
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::Seek
//
// Changes the file position
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	offset		- Delta from specified whence position
//	whence		- Position from which to apply the specified delta

size_t HostFileSystem::Directory::Seek(VirtualMachine::Mount const* mount, ssize_t offset, int whence)
{
	// todo
	return 0;
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::Sync
//
// Synchronizes all metadata and data associated with the file to storage
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation

void HostFileSystem::Directory::Sync(VirtualMachine::Mount const* mount) const
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// no-op
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::SyncData
//
// Synchronizes all data associated with the file to storage, not metadata
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation

void HostFileSystem::Directory::SyncData(VirtualMachine::Mount const* mount) const
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// no-op
}

//---------------------------------------------------------------------------
// HostFileSystem::Directory::UnlinkNode
//
// Unlinks a child node from this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name of the node to be unlinked

void HostFileSystem::Directory::UnlinkNode(VirtualMachine::Mount const* mount, char_t const* name)
{
	// todo
	throw LinuxException(UAPI_EPERM);
}

//
// HOSTFILESYSTEM::FILE IMPLEMENTATION
//

//---------------------------------------------------------------------------
// HostFileSystem::File Constructor
//
// Arguments:
//
//	fs			- Shared file system instance
//	path		- Path to the host operating system object
//	flags		- Instance-specific handle flags

HostFileSystem::File::File(std::shared_ptr<HostFileSystem> const& fs, wchar_t const* path, uint32_t flags) : Node(fs, OpenHandle(path, flags), flags)
{
	_ASSERTE(m_handle);
}

//---------------------------------------------------------------------------
// HostFileSystem::File Constructor
//
// Arguments:
//
//	rhs			- Existing File instance to duplicate
//	flags		- Instance-specific handle flags

HostFileSystem::File::File(File const& rhs, uint32_t flags) : Node(rhs.m_fs, OpenHandle(rhs.m_path, flags), flags)
{
	// todo: this could be more efficient by using ReOpenFile and copying the windows_path
	// but that requires some additional support that isn't entirely necessary right now
	_ASSERTE(m_handle);
}

//---------------------------------------------------------------------------
// HostFileSystem::File::Duplicate
//
// Duplicates this node handle
//
// Arguments:
//
//	flags		- New flags to be applied to the duplicate handle

std::unique_ptr<VirtualMachine::Node> HostFileSystem::File::Duplicate(uint32_t flags) const
{
	return std::unique_ptr<File>(new File(*this, flags));
}

//---------------------------------------------------------------------------
// HostFileSystem::File::getLength
//
// Gets the length of the file

size_t HostFileSystem::File::getLength(void) const
{
	LARGE_INTEGER size;
	if(!GetFileSizeEx(m_handle, &size)) throw MapHostException(GetLastError());

	return (size.QuadPart > std::numeric_limits<size_t>::max()) ? std::numeric_limits<size_t>::max() : static_cast<size_t>(size.QuadPart);
}

//---------------------------------------------------------------------------
// HostFileSystem::File::getMode
//
// Gets the type and permissions mask for the node

uapi_mode_t HostFileSystem::File::getMode(void) const
{
	return (UAPI_S_IFREG | 0777);
}

//---------------------------------------------------------------------------
// HostFileSystem::File::OpenHandle (private, static)
//
// Opens the handle against the native operating system object
//
// Arguments:
//
//	path			- Host path to the file node
//	flags			- Handle flags and attributes

HANDLE HostFileSystem::File::OpenHandle(wchar_t const* path, uint32_t flags)
{
	DWORD			access = 0;						// Win32 CreateFile() access mask
	DWORD			disposition = OPEN_EXISTING;	// Win32 CreateFile() creation disposition
	DWORD			attributes = 0;					// Win32 CreateFile() flags and attributes

	// O_DIRECTORY and O_TMPFILE are not compatible with the file handle implementation
	if((flags & UAPI_O_DIRECTORY) == UAPI_O_DIRECTORY) throw LinuxException(UAPI_ENOTDIR);
	if((flags & UAPI_O_TMPFILE) == UAPI_O_TMPFILE) throw LinuxException(UAPI_EOPNOTSUPP);

	// Determine the access mask to specify for the file handle
	switch(flags & UAPI_O_ACCMODE) {

		case UAPI_O_RDONLY: access = FILE_GENERIC_READ;
		case UAPI_O_WRONLY: access = FILE_GENERIC_WRITE;
		case UAPI_O_RDWR: access = FILE_GENERIC_READ | FILE_GENERIC_WRITE;
		default: throw LinuxException(UAPI_EINVAL);
	}

	// Determine the creation disposition to use for the file
	switch(flags & (UAPI_O_CREAT | UAPI_O_EXCL | UAPI_O_TRUNC)) {

		case 0: disposition = OPEN_EXISTING;
		case UAPI_O_CREAT: disposition = OPEN_ALWAYS;
		case UAPI_O_CREAT | UAPI_O_EXCL: disposition = CREATE_ALWAYS;
		case UAPI_O_TRUNC: disposition = TRUNCATE_EXISTING;
		default: throw LinuxException(UAPI_EINVAL);
	}

	// O_DIRECT, O_DSYNC, O_SYNC -- A write-through handle is a reasonable approximation
	if((flags & UAPI_O_DIRECT) == UAPI_O_DIRECT) attributes |= FILE_FLAG_WRITE_THROUGH;
	if((flags & UAPI_O_DSYNC) == UAPI_O_DSYNC) attributes |= FILE_FLAG_WRITE_THROUGH;
	if((flags & UAPI_O_SYNC) == UAPI_O_SYNC) attributes |= FILE_FLAG_WRITE_THROUGH;

	// Open a handle against the target file node using the desired access and attributes
	HANDLE handle = ::CreateFile(path, (flags & UAPI_O_PATH) ? 0 : access, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, disposition, 
		FILE_FLAG_POSIX_SEMANTICS | attributes, nullptr);
	if(handle == INVALID_HANDLE_VALUE) throw MapHostException(GetLastError());

	try {

		// Get the basic information about the handle to ensure that it does not represent a directory node
		FILE_BASIC_INFO	info;
		if(!GetFileInformationByHandleEx(handle, FileBasicInfo, &info, sizeof(FILE_BASIC_INFO))) throw MapHostException(GetLastError());
		if((info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) throw LinuxException(UAPI_EISDIR);

		// O_NOATIME is handled by Windows via a special call to SetFileTime
		if((flags & UAPI_O_NOATIME) == UAPI_O_NOATIME) {

			FILETIME noatime{ 0xFFFFFFFF, 0xFFFFFFFF };
			SetFileTime(handle, nullptr, &noatime, nullptr);
		}
	}

	catch(...) { CloseHandle(handle); throw; }

	return handle;
}

//---------------------------------------------------------------------------
// HostFileSystem::File::Read
//
// Synchronously reads data from the underlying node into a buffer
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	buffer		- Destination data buffer
//	count		- Maximum number of bytes to read into the buffer

size_t HostFileSystem::File::Read(VirtualMachine::Mount const* mount, void* buffer, size_t count)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Verify the provided mount point and ensure that the handle is not write-only
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) throw LinuxException(UAPI_EACCES);

	// ReadFile() can only read up to MAXDWORD bytes from the underlying file
	if(count == 0) return 0;
	if(count >= MAXDWORD) throw LinuxException(UAPI_EINVAL);

	// Attempt to read the specified number of bytes from the file into the buffer
	DWORD read = static_cast<DWORD>(count);
	if(!ReadFile(m_handle, buffer, read, &read, nullptr)) throw MapHostException(GetLastError());

	return static_cast<size_t>(read);
}

//---------------------------------------------------------------------------
// HostFileSystem::File::ReadAt
//
// Synchronously reads data from the underlying node into a buffer
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	offset		- Delta from specified starting position
//	whence		- Starting position from which to apply the delta
//	buffer		- Destination data buffer
//	count		- Maximum number of bytes to read into the buffer

size_t HostFileSystem::File::ReadAt(VirtualMachine::Mount const* mount, ssize_t offset, int whence, void* buffer, size_t count)
{
	// todo - needs offset/whence functionality
	return 0;
}

//---------------------------------------------------------------------------
// HostFileSystem::File::Seek
//
// Changes the file position
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	offset		- Delta from specified whence position
//	whence		- Position from which to apply the specified delta

size_t HostFileSystem::File::Seek(VirtualMachine::Mount const* mount, ssize_t offset, int whence)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);

	LARGE_INTEGER delta;
	delta.QuadPart = offset;

	if(!SetFilePointerEx(m_handle, delta, &delta, static_cast<DWORD>(whence))) throw MapHostException(GetLastError());
	return static_cast<size_t>(delta.QuadPart);
}

//---------------------------------------------------------------------------
// HostFileSystem::File::Sync
//
// Synchronizes all metadata and data associated with the file to storage
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation

void HostFileSystem::File::Sync(VirtualMachine::Mount const* mount) const
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	FlushFileBuffers(m_handle);			// Do not throw exception on failure
}

//---------------------------------------------------------------------------
// HostFileSystem::File::SyncData
//
// Synchronizes all data associated with the file to storage, not metadata
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation

void HostFileSystem::File::SyncData(VirtualMachine::Mount const* mount) const
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	FlushFileBuffers(m_handle);			// Do not throw exception on failure
}
		
//---------------------------------------------------------------------------
// HostFileSystem::File::SetLength
//
// Sets the length of the file
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	length		- New length to assign to the file

size_t HostFileSystem::File::SetLength(VirtualMachine::Mount const* mount, size_t length)
{
	LARGE_INTEGER current, delta;
	current.QuadPart = delta.QuadPart = 0;

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// On Windows, this is unfortunately a multi-step operation that can't be accomplished atomically
	if(!SetFilePointerEx(m_handle, current, &current, FILE_CURRENT)) throw MapHostException(GetLastError());
	if(!SetFilePointerEx(m_handle, delta, &delta, FILE_BEGIN)) throw MapHostException(GetLastError());
	if(!SetEndOfFile(m_handle)) throw MapHostException(GetLastError());
	if(!SetFilePointerEx(m_handle, current, &current, FILE_BEGIN)) throw MapHostException(GetLastError());

	return length;
}

//---------------------------------------------------------------------------
// HostFileSystem::File::Write
//
// Synchronously writes data from a buffer to the underlying node
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	buffer		- Source data buffer
//	count		- Maximum number of bytes to write from the buffer

size_t HostFileSystem::File::Write(VirtualMachine::Mount const* mount, const void* buffer, size_t count)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Verify the mount point and file system/handle level write flags
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);

	// WriteFile() can only write up to MAXDWORD bytes from the underlying file
	if(count == 0) return 0;
	if(count >= MAXDWORD) throw LinuxException(UAPI_EINVAL);

	// Attempt to write the specified number of bytes into the file into the buffer
	DWORD written = static_cast<DWORD>(count);
	if(!WriteFile(m_handle, buffer, written, &written, nullptr)) throw MapHostException(GetLastError());

	return static_cast<size_t>(written);
}

//---------------------------------------------------------------------------
// HostFileSystem::File::WriteAt
//
// Synchronously writes data from a buffer to the underlying node, does not
// modify the file pointer
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	offset		- Delta from the specifieed whence position
//	whence		- Position from which to apply the delta
//	buffer		- Source data buffer
//	count		- Maximum number of bytes to write from the buffer

size_t HostFileSystem::File::WriteAt(VirtualMachine::Mount const* mount, ssize_t offset, int whence, const void* buffer, size_t count)
{
	// todo - needs offset/whence functionality
	return 0;
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

HostFileSystem::Mount::Mount(std::shared_ptr<HostFileSystem> const& fs, std::shared_ptr<Directory> const& rootdir, uint32_t flags) : 
	m_fs(fs), m_rootdir(rootdir), m_flags(flags)
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

VirtualMachine::Node const* HostFileSystem::Mount::getRootNode(void) const
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
//	fs			- Shared file system instance
//	handle		- Native Win32 object handle
//	flags		- Handle-level flags applicable to this instance

template <class _interface>
HostFileSystem::Node<_interface>::Node(std::shared_ptr<HostFileSystem> const& fs, HANDLE handle, uint32_t flags) : 
	m_fs(fs), m_handle(handle), m_path(GetNormalizedPath(handle)), m_flags(flags)
{
	_ASSERTE(m_fs);
	_ASSERTE((m_handle) && (m_handle != INVALID_HANDLE_VALUE));
	_ASSERTE(m_path);
}

//---------------------------------------------------------------------------
// HostFileSystem::Node::getAccessTime
//
// Gets the access time of the node

template <class _interface>
uapi_timespec HostFileSystem::Node<_interface>::getAccessTime(void) const
{
	FILE_BASIC_INFO				info;				// Basic file information

	if(!GetFileInformationByHandleEx(m_handle, FileBasicInfo, &info, sizeof(FILE_BASIC_INFO))) throw MapHostException(GetLastError());
	return convert<uapi_timespec>(info.LastAccessTime);
}

//---------------------------------------------------------------------------
// HostFileSystem::Node::getChangeTime
//
// Gets the change time of the node

template <class _interface>
uapi_timespec HostFileSystem::Node<_interface>::getChangeTime(void) const
{
	FILE_BASIC_INFO				info;				// Basic file information

	if(!GetFileInformationByHandleEx(m_handle, FileBasicInfo, &info, sizeof(FILE_BASIC_INFO))) throw MapHostException(GetLastError());
	return convert<uapi_timespec>(info.ChangeTime);
}
		
//---------------------------------------------------------------------------
// HostFileSystem::Node::getFlags
//
// Gets the handle-level flags applied to this instance

template <class _interface>
uint32_t HostFileSystem::Node<_interface>::getFlags(void) const
{
	return m_flags;
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
	BY_HANDLE_FILE_INFORMATION		info;			// File information

	if(!GetFileInformationByHandle(m_handle, &info)) throw MapHostException(GetLastError());

#ifndef _M_X64
	return intptr_t(info.nFileIndexLow);
#else
	return intptr_t(info.nFileIndexHigh) << 32 | info.nFileIndexLow;
#endif
}

//---------------------------------------------------------------------------
// HostFileSystem::Node::getModificationTime
//
// Gets the modification time of the node

template <class _interface>
uapi_timespec HostFileSystem::Node<_interface>::getModificationTime(void) const
{
	FILE_BASIC_INFO				info;				// Basic file information

	if(!GetFileInformationByHandleEx(m_handle, FileBasicInfo, &info, sizeof(FILE_BASIC_INFO))) throw MapHostException(GetLastError());
	return convert<uapi_timespec>(info.LastWriteTime);
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
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	FILETIME accesstime = convert<FILETIME>(atime);
	if(!SetFileTime(m_handle, nullptr, &accesstime, nullptr)) throw MapHostException(GetLastError());

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
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	return 0;
}

//---------------------------------------------------------------------------
// HostFileSystem::Node::SetMode
//
// Changes the mode flags for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	mode		- New permission flags to be set

template <class _interface>
uapi_mode_t HostFileSystem::Node<_interface>::SetMode(VirtualMachine::Mount const* mount, uapi_mode_t mode)
{
	UNREFERENCED_PARAMETER(mode);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	return (UAPI_S_IFDIR | 0777);
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
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	FILETIME modificationtime = convert<FILETIME>(mtime);
	if(!SetFileTime(m_handle, nullptr, nullptr, &modificationtime)) throw MapHostException(GetLastError());

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
	if(mount->FileSystem != m_fs.get()) throw LinuxException(UAPI_EXDEV);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	return 0;
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
