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

#include "Capability.h"
#include "LinuxException.h"
#include "MountOptions.h"

#pragma warning(push, 4)

// Invariants
//
static_assert(FILE_BEGIN == UAPI_SEEK_SET, "FILE_BEGIN must be the same value as UAPI_SEEK_SET");
static_assert(FILE_CURRENT == UAPI_SEEK_CUR, "FILE_CURRENT must be the same value as UAPI_SEEK_CUR");
static_assert(FILE_END == UAPI_SEEK_END, "FILE_END must be the same value as UAPI_SEEK_END");

//---------------------------------------------------------------------------
// CreateHostFileSystem
//
// Creates an instance of the HostFileSystem file system
//
// Arguments:
//
//	source		- Source device string
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

std::unique_ptr<VirtualMachine::FileSystem> CreateHostFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength)
{
	return std::make_unique<HostFileSystem>(source, flags, data, datalength);
}

//---------------------------------------------------------------------------
// HostFileSystem Constructor
//
// Arguments:
//
//	source		- Source device string
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

HostFileSystem::HostFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength)
{
	bool sandbox = true;						// Flag to sandbox the virtual file system

	Capability::Demand(UAPI_CAP_SYS_ADMIN);

	// Source is ignored, but has to be specified by contract
	if(source == nullptr) throw LinuxException(UAPI_EFAULT);

	// Convert the specified options into MountOptions to process the custom parameters
	MountOptions options(flags, data, datalength);

	// Verify that the specified flags are supported for a creation operation
	if(options.Flags & ~MOUNT_FLAGS) throw LinuxException(UAPI_EINVAL);

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

	// Create the shared file system state and assign the file system level flags
	m_fs = std::make_shared<hostfs_t>(); 
	m_fs->flags = options.Flags & ~UAPI_MS_PERMOUNT_MASK;
}

//---------------------------------------------------------------------------
// HostFileSystem::Mount
//
// Mounts the file system
//
// Arguments:
//
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

std::unique_ptr<VirtualMachine::Mount> HostFileSystem::Mount(uint32_t flags, void const* data, size_t datalength)
{
	Capability::Demand(UAPI_CAP_SYS_ADMIN);

	// Convert the flags and data parameters into a MountOptions and check for invalid flags
	MountOptions options(flags, data, datalength);
	if(options.Flags & ~MOUNT_FLAGS) throw LinuxException(UAPI_EINVAL);

	// Construct the mount instance, passing only the mount specific flags (MS_NODEV and MS_NOSUID are always set)
	std::unique_ptr<VirtualMachine::Mount> mount = 
		std::make_unique<class Mount>(m_fs, (options.Flags & UAPI_MS_PERMOUNT_MASK) | (UAPI_MS_NODEV | UAPI_MS_NOSUID));

	// Reapply the file system level flags to the shared state instance after the mount operation
	m_fs->flags = options.Flags & ~UAPI_MS_PERMOUNT_MASK;

	return mount;
}

//
// HOSTFILESYSTEM::MOUNT IMPLEMENTATION
//

//-----------------------------------------------------------------------------
// HostFileSystem::Mount Constructor
//
// Arguments:
//
//	fs			- Shared file system state instance
//	flags		- Mount-specific flags

HostFileSystem::Mount::Mount(std::shared_ptr<hostfs_t> const& fs, uint32_t flags) : m_fs(fs), m_flags(flags)
{
	_ASSERTE(fs);

	// The specified flags should not include any that apply to the file system
	_ASSERTE((flags & ~UAPI_MS_PERMOUNT_MASK) == 0);
	if((flags & ~UAPI_MS_PERMOUNT_MASK) != 0) throw LinuxException(UAPI_EINVAL);
}

//---------------------------------------------------------------------------

#pragma warning(pop)
