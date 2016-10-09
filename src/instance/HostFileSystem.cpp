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

#include "LinuxException.h"
#include "MountOptions.h"

#pragma warning(push, 4)

// Invariants
//
static_assert(FILE_BEGIN == UAPI_SEEK_SET, "FILE_BEGIN must be the same value as UAPI_SEEK_SET");
static_assert(FILE_CURRENT == UAPI_SEEK_CUR, "FILE_CURRENT must be the same value as UAPI_SEEK_CUR");
static_assert(FILE_END == UAPI_SEEK_END, "FILE_END must be the same value as UAPI_SEEK_END");

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
	auto fs = std::make_shared<HostFileSystem>(options.Flags & ~UAPI_MS_PERMOUNT_MASK);

	// Create and return the mount point instance to the caller
	return std::make_unique<HostFileSystem::Mount>(fs, options.Flags & UAPI_MS_PERMOUNT_MASK);
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
// HOSTFILESYSTEM::MOUNT IMPLEMENTATION
//

//-----------------------------------------------------------------------------
// HostFileSystem::Mount Constructor
//
// Arguments:
//
//	fs			- Shared file system instance
//	flags		- Mount-specific flags

HostFileSystem::Mount::Mount(std::shared_ptr<HostFileSystem> const& fs, uint32_t flags) : m_fs(fs), m_flags(flags)
{
	_ASSERTE(m_fs);

	// The specified flags should not include any that apply to the file system
	_ASSERTE((flags & ~UAPI_MS_PERMOUNT_MASK) == 0);
	if((flags & ~UAPI_MS_PERMOUNT_MASK) != 0) throw LinuxException(UAPI_EINVAL);
}

//---------------------------------------------------------------------------
// HostFileSystem::Mount Copy Constructor
//
// Arguments:
//
//	rhs		- Existing Mount instance to create a copy of

HostFileSystem::Mount::Mount(Mount const& rhs) : m_fs(rhs.m_fs), m_flags(static_cast<uint32_t>(rhs.m_flags))
{
	// A copy of a mount references the same shared file system and copies the flags
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
// HostFileSystem::Mount::getRootNode
//
// Gets the root node of the mount point

VirtualMachine::Node* HostFileSystem::Mount::getRootNode(void) const
{
	// todo: placeholder
	return nullptr;
}

//---------------------------------------------------------------------------

#pragma warning(pop)
