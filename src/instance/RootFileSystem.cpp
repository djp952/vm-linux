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
// Creates an instance of the RootFileSystem file system
//
// Arguments:
//
//	source		- Source device string
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

std::unique_ptr<VirtualMachine::FileSystem> MountRootFileSystem(char_t const* source, VirtualMachine::MountFlags flags, void const* data, size_t datalength)
{
	// Source is ignored, but has to be specified by contract
	if(source == nullptr) throw LinuxException(UAPI_EFAULT);

	// Parse the provided mounting options and construct the file system
	return std::make_unique<RootFileSystem>(MountOptions(flags, data, datalength));
}

//---------------------------------------------------------------------------
// RootFileSystem Constructor (private)
//
// Arguments:
//
//	options		- Mounting options

RootFileSystem::RootFileSystem(MountOptions const& options)
{
	using MountFlags = VirtualMachine::MountFlags;

	// Default mode, uid and gid for the root directory node
	uapi_mode_t mode = UAPI_S_IRWXU | UAPI_S_IRWXG | UAPI_S_IROTH | UAPI_S_IXOTH;	// 0775
	uapi_uid_t uid = 0;
	uapi_gid_t gid = 0;

	// Break up the standard mounting options bitmask into file system and mount specific masks
	auto fsflags = options.Flags & (MountFlags(UAPI_MS_RDONLY | UAPI_MS_KERNMOUNT | UAPI_MS_STRICTATIME));
	auto mountflags = (options.Flags & MountFlags::PerMountMask) | MountFlags(UAPI_MS_NOEXEC | UAPI_MS_NODEV | UAPI_MS_NOSUID);

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

	// Construct the root directory node
	m_node = std::make_unique<DirectoryNode>(mode, uid, gid);
}

//
// ROOTFILESYSTEM::DIRECTORYNODE
//

//---------------------------------------------------------------------------
// RootFileSystem::DirectoryNode Constructor
//
// Arguments:
//
//	mode		- Permission flags to assign to the directory node
//	uid			- Owner UID of the directory node
//	gid			- Owner GID of the directory node

RootFileSystem::DirectoryNode::DirectoryNode(uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) : m_mode(mode), m_uid(uid), m_gid(gid)
{
}

//-----------------------------------------------------------------------------
// RootFileSystem::DirectoryNode::getOwnerGroupId
//
// Gets the currently set owner group identifier for the directory

uapi_gid_t RootFileSystem::DirectoryNode::getOwnerGroupId(void) const
{
	return m_gid;
}

//-----------------------------------------------------------------------------
// RootFileSystem::DirectoryNode::getOwnerUserId
//
// Gets the currently set owner user identifier for the directory

uapi_uid_t RootFileSystem::DirectoryNode::getOwnerUserId(void) const
{
	return m_uid;
}

//-----------------------------------------------------------------------------
// RootFileSystem::DirectoryNode::getPermissions
//
// Gets the currently set permissions mask for the directory

uapi_mode_t RootFileSystem::DirectoryNode::getPermissions(void) const
{
	return m_mode;
}

//---------------------------------------------------------------------------

#pragma warning(pop)
