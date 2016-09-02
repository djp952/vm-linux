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
// CreateRootFileSystem
//
// Creates an instance of the RootFileSystem file system
//
// Arguments:
//
//	source		- Source device string
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

std::unique_ptr<VirtualMachine::FileSystem> CreateRootFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength)
{
	return std::make_unique<RootFileSystem>(source, flags, data, datalength);
}

//---------------------------------------------------------------------------
// RootFileSystem Constructor (private)
//
// Arguments:
//
//	source		- Source device string
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

RootFileSystem::RootFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength)
{
	// Source is ignored but must be specified by contract
	if(source == nullptr) throw LinuxException(UAPI_EFAULT);

	// Verify that the specified flags are supported for a creation operation
	if(flags & ~MOUNT_FLAGS) throw LinuxException(UAPI_EINVAL);

	// Convert the specified options into MountOptions to process the custom parameters
	MountOptions options(flags, data, datalength);

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

	// Create the shared file system state and assign the file system level flags
	m_fs = std::make_shared<rootfs_t>();
	m_fs->flags = flags & ~UAPI_MS_PERMOUNT_MASK;

	// Construct the root directory node instance
	m_fs->rootnode = std::make_unique<Directory>(m_fs, mode, uid, gid);
}

//---------------------------------------------------------------------------
// RootFileSystem::Mount
//
// Mounts the file system
//
// Arguments:
//
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

std::unique_ptr<VirtualMachine::Mount> RootFileSystem::Mount(uint32_t flags, void const* data, size_t datalength)
{
	// This operation does not support any custom parameters, ignore them
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(datalength);

	// Verify that the specified flags are supported for a mount operation
	if(flags & ~MOUNT_FLAGS) throw LinuxException(UAPI_EINVAL);

	// Construct the mount instance, passing only the mount specific flags
	std::unique_ptr<VirtualMachine::Mount> mount = std::make_unique<class Mount>(m_fs, flags & UAPI_MS_PERMOUNT_MASK);

	// Reapply the file system level flags to the shared state instance after the mount operation
	m_fs->flags = flags & ~UAPI_MS_PERMOUNT_MASK;

	return mount;
}

//
// ROOTFILESYSTEM::DIRECTORY IMPLEMENTATION
//

//---------------------------------------------------------------------------
// RootFileSystem::Directory Constructor
//
// Arguments:
//
//	fs			- Shared file system state instance
//	mode		- Permission flags to assign to the directory node
//	uid			- Owner UID of the directory node
//	gid			- Owner GID of the directory node

RootFileSystem::Directory::Directory(std::shared_ptr<rootfs_t> const& fs, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) 
	: m_fs(fs), m_mode(mode), m_uid(uid), m_gid(gid)
{
	_ASSERTE(fs);
}

//-----------------------------------------------------------------------------
// RootFileSystem::Directory::getOwnerGroupId
//
// Gets the currently set owner group identifier for the directory

uapi_gid_t RootFileSystem::Directory::getOwnerGroupId(void) const
{
	return m_gid;
}

//-----------------------------------------------------------------------------
// RootFileSystem::Directory::getOwnerUserId
//
// Gets the currently set owner user identifier for the directory

uapi_uid_t RootFileSystem::Directory::getOwnerUserId(void) const
{
	return m_uid;
}

//-----------------------------------------------------------------------------
// RootFileSystem::Directory::getPermissions
//
// Gets the currently set permissions mask for the directory

uapi_mode_t RootFileSystem::Directory::getPermissions(void) const
{
	return m_mode;
}

//
// ROOTFILESYSTEM::MOUNT IMPLEMENTATION
//

//-----------------------------------------------------------------------------
// RootFileSystem::Mount Constructor
//
// Arguments:
//
//	fs			- Shared file system state instance
//	flags		- Mount-specific flags

RootFileSystem::Mount::Mount(std::shared_ptr<rootfs_t> const& fs, uint32_t flags) : m_fs(fs), m_flags(flags)
{
	_ASSERTE(fs);

	// The specified flags should not include any that apply to the file system
	_ASSERTE((flags & ~UAPI_MS_PERMOUNT_MASK) == 0);
	if((flags & ~UAPI_MS_PERMOUNT_MASK) != 0) throw LinuxException(UAPI_EINVAL);
}

//---------------------------------------------------------------------------

#pragma warning(pop)
