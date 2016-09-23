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

#include "Capability.h"
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
	Capability::Demand(UAPI_CAP_SYS_ADMIN);

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

	// Construct the shared file system instance
	auto fs = std::make_shared<RootFileSystem>(options.Flags & ~UAPI_MS_PERMOUNT_MASK);

	// Create and return the mount point instance to the caller
	return std::make_unique<RootFileSystem::Mount>(fs, options.Flags & UAPI_MS_PERMOUNT_MASK);
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
// ROOTFILESYSTEM::DIRECTORY IMPLEMENTATION
//

//---------------------------------------------------------------------------
// RootFileSystem::Directory Constructor
//
// Arguments:
//
//	fs			- Shared file system instance
//	mode		- Permission flags to assign to the directory node
//	uid			- Owner UID of the directory node
//	gid			- Owner GID of the directory node

RootFileSystem::Directory::Directory(std::shared_ptr<RootFileSystem> const& fs, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) 
	: m_fs(fs), m_mode(mode), m_uid(uid), m_gid(gid)
{
	_ASSERTE(m_fs);
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

//---------------------------------------------------------------------------
// RootFileSystem::Directory::getType
//
// Gets the type of the node instance

VirtualMachine::NodeType RootFileSystem::Directory::getType(void) const
{
	return VirtualMachine::NodeType::Directory;
}

//
// ROOTFILESYSTEM::MOUNT IMPLEMENTATION
//

//-----------------------------------------------------------------------------
// RootFileSystem::Mount Constructor
//
// Arguments:
//
//	fs			- Shared file system instance
//	flags		- Mount-specific flags

RootFileSystem::Mount::Mount(std::shared_ptr<RootFileSystem> const& fs, uint32_t flags) : m_fs(fs), m_flags(flags)
{
	_ASSERTE(m_fs);

	// The specified flags should not include any that apply to the file system
	_ASSERTE((flags & ~UAPI_MS_PERMOUNT_MASK) == 0);
	if((flags & ~UAPI_MS_PERMOUNT_MASK) != 0) throw LinuxException(UAPI_EINVAL);
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

#pragma warning(pop)
