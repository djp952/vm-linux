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
#include "TempFileSystem.h"

#include <LinuxException.h>
#include <Win32Exception.h>
#include "Capability.h"
#include "MountOptions.h"
#include "SystemInformation.h"

#pragma warning(push, 4)

// g_maxmemory (local)
//
// The maximum amount of memory available to this process
static size_t g_maxmemory = []() -> size_t {

	// Determine the maximum size of the memory available to this process, which is the lesser of: physical memory, 
	// accessible virtual memory, and the maximum value that can be held by a size_t
	//
	uint64_t accessiblemem = std::min(SystemInformation::TotalPhysicalMemory, SystemInformation::TotalVirtualMemory);
	return static_cast<size_t>(std::min(accessiblemem, static_cast<uint64_t>(std::numeric_limits<size_t>::max())));
}();

//---------------------------------------------------------------------------
// CreateTempFileSystem
//
// Creates an instance of the TempFileSystem file system
//
// Arguments:
//
//	source		- Source device string
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

std::unique_ptr<VirtualMachine::FileSystem> CreateTempFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength)
{
	return std::make_unique<TempFileSystem>(source, flags, data, datalength);
}

//---------------------------------------------------------------------------
// TempFileSystem Constructor
//
// Arguments:
//
//	source		- Source device string
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

TempFileSystem::TempFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength)
{
	size_t				maxsize = 0;			// Maximum file system size in bytes
	size_t				maxnodes = 0;			// Maximum number of file system nodes

	Capability::Demand(UAPI_CAP_SYS_ADMIN);

	// Source is ignored, but has to be specified by contract
	if(source == nullptr) throw LinuxException(UAPI_EFAULT);

	// Convert the specified options into MountOptions to process the custom parameters
	MountOptions options(flags, data, datalength);

	// Verify that the specified flags are supported for a creation operation
	if(options.Flags & ~MOUNT_FLAGS) throw LinuxException(UAPI_EINVAL);

	// Default mode, uid and gid for the root directory node
	uapi_mode_t mode = UAPI_S_IRWXU | UAPI_S_IRWXG | UAPI_S_IROTH | UAPI_S_IXOTH;	// 0775
	uapi_uid_t uid = 0;
	uapi_gid_t gid = 0;

	try {

		// size=
		//
		// Sets the maximum size of the temporary file system
		if(options.Arguments.Contains("size")) {
		
			size_t size = 0;
			auto sizearg = options.Arguments["size"];

			// Size is special and can optionally end with a % character to indicate that the maximum
			// is based on the amount of available RAM rather than a specific length in bytes
			if(std::endswith(sizearg, '%')) size = static_cast<size_t>(g_maxmemory * (static_cast<double>(ParseScaledInteger(std::rtrim(sizearg, '%'))) / 100));
			else size = ParseScaledInteger(sizearg);

			// Ensure the size does not exceed the maximum amount of available memory
			maxsize = std::min(size, g_maxmemory);
		}

		// nr_blocks=
		//
		// Sets the maximum allowable number of blocks rather than a specific size
		if(options.Arguments.Contains("nr_blocks")) {

			// Ensure the size does not exceed the maximum amount of available memory
			maxsize = std::min(ParseScaledInteger(options.Arguments["nr_blocks"]), g_maxmemory / SystemInformation::PageSize) * SystemInformation::PageSize;
		}

		// nr_inodes=
		//
		// Sets the maximum allowable number of inodes
		if(options.Arguments.Contains("nr_inodes")) maxnodes = ParseScaledInteger(options.Arguments["nr_inodes"]);

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
	m_fs = std::make_shared<tempfs_t>(); 
	m_fs->flags = options.Flags & ~UAPI_MS_PERMOUNT_MASK;

	// Set the initial maximum size and node count for the file system; maximum size
	// defaults to 50% of the available system memory
	m_fs->maxsize = (maxsize == 0) ? g_maxmemory >> 1 : maxsize;
	m_fs->maxnodes = (maxnodes == 0) ? std::numeric_limits<size_t>::max() : maxnodes;
	
	// Construct the root directory node instance
	m_rootnode = std::make_unique<Directory>(m_fs, mode, uid, gid);
}

//---------------------------------------------------------------------------
// TempFileSystem::Mount
//
// Mounts the file system
//
// Arguments:
//
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

std::unique_ptr<VirtualMachine::Mount> TempFileSystem::Mount(uint32_t flags, void const* data, size_t datalength)
{
	Capability::Demand(UAPI_CAP_SYS_ADMIN);

	// Convert the flags and data parameters into a MountOptions and check for invalid flags
	MountOptions options(flags, data, datalength);
	if(options.Flags & ~MOUNT_FLAGS) throw LinuxException(UAPI_EINVAL);

	// Construct the mount instance, passing only the mount specific flags
	std::unique_ptr<VirtualMachine::Mount> mount = std::make_unique<class Mount>(m_fs, options.Flags & UAPI_MS_PERMOUNT_MASK);

	// Reapply the file system level flags to the shared state instance after the mount operation
	m_fs->flags = options.Flags & ~UAPI_MS_PERMOUNT_MASK;

	return mount;
}

//---------------------------------------------------------------------------
// TempFileSystem::ParseScaledInteger (private, static)
//
// Parses a scaled integer value, which may include a K/M/G suffix
//
// Arguments:
//
//	str			- String value to be parsed (ANSI/UTF-8)

size_t TempFileSystem::ParseScaledInteger(std::string const& str)
{
	size_t		suffixindex;			// Position of the optional suffix
	uint64_t	multiplier = 1;			// Multiplier value to be applied

	// Convert the string into an usigned 64-bit integer and extract any suffix
	uint64_t interim = std::stoull(str, &suffixindex, 0);
	auto suffix = str.substr(suffixindex);

	// The suffix must not be more than one character in length
	if(suffix.length() > 1) throw std::invalid_argument(str);
	else if(suffix.length() == 1) {

		switch(suffix.at(0)) {

			case 'k': case 'K': multiplier = 1 KiB; break;
			case 'm': case 'M': multiplier = 1 MiB; break;
			case 'g': case 'G': multiplier = 1 GiB; break;
			default: throw std::invalid_argument(str);
		}
	}

	// Watch for overflow when applying the multiplier to the interim value
	if((std::numeric_limits<uint64_t>::max() / multiplier) < interim) throw std::overflow_error(str);
		
	// Apply the multiplier value
	interim *= multiplier;

	// Verify that the final result will not exceed size_t's numeric limits
	if((interim > std::numeric_limits<size_t>::max()) || (interim < std::numeric_limits<size_t>::min())) throw std::overflow_error(str);

	return static_cast<size_t>(interim);
}

//
// TEMPFILESYSTEM::TEMPFS_T IMPLEMENTATION
//

//-----------------------------------------------------------------------------
// TempFileSystem::tempfs_t Constructor
//
// Arguments:
//
//	flags		- File system specific flags
//	maxsize		- Maximum file system size in bytes
//	maxnodes	- Maximum number of allowed node objects
//	mode		- Initial permissions to assign to the root directory
//	uid			- Initial owner to assign to the root directory
//	gid			- Initial group to assign to the root directory

TempFileSystem::tempfs_t::tempfs_t() : m_heap(nullptr), m_heapsize(0)
{
	// Create a non-serialized private heap to contain the file system block data.  Do not 
	// specify a maximum size here, it limits what can be allocated and cannot be changed
	m_heap = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
	if(m_heap == nullptr) throw LinuxException(UAPI_ENOMEM, Win32Exception());
}

//-----------------------------------------------------------------------------
// TempFileSystem::tempfs_t Destructor

TempFileSystem::tempfs_t::~tempfs_t()
{
	if(m_heap) HeapDestroy(m_heap);
}

//-----------------------------------------------------------------------------
// TempFileSystem::tempfs_t::allocate
//
// Allocates blocks from the private heap instance
//
// Arguments:
//
//  count		- Number of contiguous blocks to allocate
//	zeroinit	- Flag to zero-initialize the allocated blocks

uintptr_t TempFileSystem::tempfs_t::allocate(size_t count, bool zeroinit)
{
	count *= SystemInformation::PageSize;		// Convert count into bytes

	sync::critical_section::scoped_lock cs(m_heaplock);

	if((m_heapsize + count) > maxsize) throw LinuxException(UAPI_ENOSPC);
	
	void* alloc = HeapAlloc(m_heap, (zeroinit) ? HEAP_ZERO_MEMORY : 0, count);
	if(alloc == nullptr) throw LinuxException(UAPI_ENOMEM);

	m_heapsize += count;			// Add to the allocated memory count
	return uintptr_t(alloc);		// Return the allocated heap pointer
}

//-----------------------------------------------------------------------------
// TempFileSystem::tempfs_t::release
//
// Releases blocks from the private heap instance
//
// Arguments:
//
//	base		- Pointer to base of the allocated blocks

void TempFileSystem::tempfs_t::release(uintptr_t base)
{
	if(base == 0) return;

	sync::critical_section::scoped_lock cs(m_heaplock);

	// Get the size of the block being released from the heap
	size_t count = HeapSize(m_heap, 0, reinterpret_cast<void*>(base));
	if(count == -1) throw LinuxException(UAPI_EFAULT);

	// Release the blocks of data from the heap and reduce the overall size counter
	if(!HeapFree(m_heap, 0, reinterpret_cast<void*>(base))) throw LinuxException(UAPI_EFAULT);
	m_heapsize -= count;
}

//
// TEMPFILESYSTEM::DIRECTORY IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::Directory Constructor
//
// Arguments:
//
//	fs			- Shared file system state instance
//	mode		- Permission flags to assign to the directory node
//	uid			- Owner UID of the directory node
//	gid			- Owner GID of the directory node

TempFileSystem::Directory::Directory(std::shared_ptr<tempfs_t> const& fs, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) 
	: m_fs(fs), m_mode(mode), m_uid(uid), m_gid(gid)
{
	_ASSERTE(fs);
}

//-----------------------------------------------------------------------------
// TempFileSystem::Directory::getOwnerGroupId
//
// Gets the currently set owner group identifier for the directory

uapi_gid_t TempFileSystem::Directory::getOwnerGroupId(void) const
{
	return m_gid;
}

//-----------------------------------------------------------------------------
// TempFileSystem::Directory::getOwnerUserId
//
// Gets the currently set owner user identifier for the directory

uapi_uid_t TempFileSystem::Directory::getOwnerUserId(void) const
{
	return m_uid;
}

//-----------------------------------------------------------------------------
// TempFileSystem::Directory::getPermissions
//
// Gets the currently set permissions mask for the directory

uapi_mode_t TempFileSystem::Directory::getPermissions(void) const
{
	return m_mode;
}

//
// TEMPFILESYSTEM::MOUNT IMPLEMENTATION
//

//-----------------------------------------------------------------------------
// TempFileSystem::Mount Constructor
//
// Arguments:
//
//	fs		- Reference to the shared file system implementation
//	flags	- Mount-specific flags to apply to this mount instance

TempFileSystem::Mount::Mount(std::shared_ptr<tempfs_t> const& fs, uint32_t flags) : m_fs(fs), m_flags(flags)
{
	_ASSERTE(fs);							// Should never be NULL on entry

	// The file system level flags should have been filtered out from the bitmask prior to creation
	_ASSERTE((flags & ~UAPI_MS_PERMOUNT_MASK) == 0);
	if((flags & ~UAPI_MS_PERMOUNT_MASK) != 0) throw LinuxException(UAPI_EINVAL);
}

//---------------------------------------------------------------------------

#pragma warning(pop)
