//---------------------------------------------------------------------------
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
//---------------------------------------------------------------------------

#include "stdafx.h"
#include "TempFileSystem.h"

#include <SystemInformation.h>
#include <Win32Exception.h>

#include "Capability.h"
#include "LinuxException.h"
#include "MountOptions.h"

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
// ParseScaledInteger (local)
//
// Parses a scaled integer value, which may include a K/M/G suffix
//
// Arguments:
//
//	str			- String value to be parsed (ANSI/UTF-8)

size_t ParseScaledInteger(std::string const& str)
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

//---------------------------------------------------------------------------
// MountTempFileSystem
//
// Creates an instance of TempFileSystem
//
// Arguments:
//
//	source		- Source device string
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

std::unique_ptr<VirtualMachine::Mount> MountTempFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength)
{
	size_t				maxsize = 0;			// Maximum file system size in bytes
	size_t				maxnodes = 0;			// Maximum number of file system nodes

	Capability::Demand(UAPI_CAP_SYS_ADMIN);

	// Source is ignored, but has to be specified by contract
	if(source == nullptr) throw LinuxException(UAPI_EFAULT);

	// Convert the specified options into MountOptions to process the custom parameters
	MountOptions options(flags, data, datalength);

	// Verify that the specified flags are supported for a creation operation
	if(options.Flags & ~TempFileSystem::MOUNT_FLAGS) throw LinuxException(UAPI_EINVAL);

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

	// Construct the shared file system instance
	auto fs = std::make_shared<TempFileSystem>(options.Flags & ~UAPI_MS_PERMOUNT_MASK);
	
	// Set the initial maximum size and node count for the file system; maximum size
	// defaults to 50% of the available system memory
	fs->MaximumSize = (maxsize == 0) ? g_maxmemory >> 1 : maxsize;
	fs->MaximumNodes = (maxnodes == 0) ? std::numeric_limits<size_t>::max() : maxnodes;

	// Construct the root directory node on the file system heap using the specified attributes
	auto rootnode = TempFileSystem::node_t::FromFileSystem(fs, VirtualMachine::NodeType::Directory, mode, uid, gid);
	auto rootdir = std::make_unique<TempFileSystem::Directory>(rootnode);

	// Create and return the mount point instance, transferring ownership of the root node
	return std::make_unique<TempFileSystem::Mount>(fs, std::move(rootdir), options.Flags & UAPI_MS_PERMOUNT_MASK);
}

//---------------------------------------------------------------------------
// TempFileSystem Constructor
//
// Arguments:
//
//	flags		- Initial file system level flags

TempFileSystem::TempFileSystem(uint32_t flags) : Flags(flags), m_heap(nullptr), m_heapsize(0)
{
	// The specified flags should not include any that apply to the mount point
	_ASSERTE((flags & UAPI_MS_PERMOUNT_MASK) == 0);
	if((flags & UAPI_MS_PERMOUNT_MASK) != 0) throw LinuxException(UAPI_EINVAL);

	// Create a non-serialized private heap to contain the file system block data.  Do not 
	// specify a maximum size here, it limits what can be allocated and cannot be changed
	m_heap = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
	if(m_heap == nullptr) throw LinuxException(UAPI_ENOMEM, Win32Exception());
}

//---------------------------------------------------------------------------
// TempFileSystem Destructor

TempFileSystem::~TempFileSystem()
{
	// Check for leaks in _DEBUG builds only; there should be no allocations
	// on the private heap remaining when it's destroyed if everything is 
	// cleaning up after itself properly
	_ASSERTE(m_heapsize == 0);

	// Destroy the private heap
	if(m_heap) HeapDestroy(m_heap);
}

//---------------------------------------------------------------------------
// TempFileSystem::AllocateHeap (private)
//
// Allocates memory from the private heap
//
// Arguments:
//
//  bytecount	- Number of bytes to allocate from the heap
//	zeroinit	- Flag to zero-initialize the allocated memory

void* TempFileSystem::AllocateHeap(size_t bytecount, bool zeroinit)
{
	sync::critical_section::scoped_lock cs(m_heaplock);

	if((m_heapsize + bytecount) > MaximumSize) throw LinuxException(UAPI_ENOSPC);
	
	void* ptr = HeapAlloc(m_heap, (zeroinit) ? HEAP_ZERO_MEMORY : 0, bytecount);
	if(ptr == nullptr) throw LinuxException(UAPI_ENOMEM);

	m_heapsize += bytecount;		// Add to the allocated memory count
	return ptr;						// Return the allocated heap pointer
}

//---------------------------------------------------------------------------
// TempFileSystem::ReallocateHeap (private)
//
// Reallocates memory previously allocated from the private heap
//
// Arguments:
//
//	ptr			- Pointer to allocation to be resized
//	bytecount	- New length of the allocation in bytes
//	zeroinit	- Flag to zero-initialize newly allocated memory

void* TempFileSystem::ReallocateHeap(void* ptr, size_t bytecount, bool zeroinit)
{
	if(ptr == nullptr) throw LinuxException(UAPI_EFAULT);

	sync::critical_section::scoped_lock cs(m_heaplock);

	// Get the current size of the allocation
	size_t oldcount = HeapSize(m_heap, 0, ptr);
	if(oldcount == -1) throw LinuxException(UAPI_EFAULT);

	// If the old and new byte counts are the same, there is nothing to do
	if(bytecount == oldcount) return ptr;

	// Attempt to reallocate the original heap block
	void* newptr = HeapReAlloc(m_heap, (zeroinit) ? HEAP_ZERO_MEMORY : 0, ptr, bytecount);
	if(newptr == nullptr) throw LinuxException(UAPI_ENOMEM);

	// Adjust the allocated memory counter accordingly
	if(bytecount > oldcount) m_heapsize += (bytecount - oldcount);
	else if(bytecount < oldcount) m_heapsize -= (oldcount - bytecount);

	return newptr;
}

//---------------------------------------------------------------------------
// TempFileSystem::ReleaseHeap (private)
//
// Releases memory from the private heap
//
// Arguments:
//
//	ptr			- Pointer to allocation to be released

void TempFileSystem::ReleaseHeap(void* ptr)
{
	// Don't throw an exception if the pointer was never allocated
	if(ptr == nullptr) return;

	sync::critical_section::scoped_lock cs(m_heaplock);

	// Get the size of the allocation being released from the heap
	size_t bytecount = HeapSize(m_heap, 0, ptr);
	if(bytecount == -1) throw LinuxException(UAPI_EFAULT);

	// Release the allocation from the heap and reduce the overall size counter
	if(!HeapFree(m_heap, 0, ptr)) throw LinuxException(UAPI_EFAULT);
	m_heapsize -= bytecount;
}

//
// TEMPFILESYSTEM::NODE_T IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::node_t Constructor
//
// Arguments:
//
//	fs		- Shared file system instance
//	type	- Type of node being created

TempFileSystem::node_t::node_t(std::shared_ptr<TempFileSystem> const& fs, VirtualMachine::NodeType type) : node_t(fs, type, 0, 0, 0)
{
}

//---------------------------------------------------------------------------
// TempFileSystem::node_t Constructor
//
// Arguments:
//
//	fs		- Shared file system instance
//	type	- Type of node being created
//	mode	- Initial permissions to assign to the node
//	uid		- Initial owner UID to assign to the node
//	gid		- Initial owner GID to assign to the node

TempFileSystem::node_t::node_t(std::shared_ptr<TempFileSystem> const& fs, VirtualMachine::NodeType type, uapi_mode_t mode, uapi_uid_t uid, 
	uapi_gid_t gid) : m_fs(fs), m_data(nullptr), m_datalen(0), Index(fs->NodeIndexPool.Allocate()), Type(type), AccessTime(datetime::now()), 
	ChangeTime(AccessTime), ModifyTime(AccessTime), Permissions(mode), OwnerUserId(uid), OwnerGroupId(gid)
{
	_ASSERTE(m_fs);
}

//---------------------------------------------------------------------------
// TempFileSystem::node_t Destructor

TempFileSystem::node_t::~node_t()
{
	// Release the data if any has been allocated for this node
	if(m_data != nullptr) allocator_t<uint8_t>(m_fs).deallocate(m_data, m_datalen);
	
	// Release the node index from the file system index pool
	m_fs->NodeIndexPool.Release(Index);
}

//---------------------------------------------------------------------------
// TempFileSystem::node_t::FromFileSystem (static)
//
// Creates a new shared_ptr<node_t> on a TempFileSystem private heap
//
// Arguments:
//
//	fs		- TempFileSystem instance on which to create the node
//	type	- Type of node being created

std::shared_ptr<TempFileSystem::node_t> TempFileSystem::node_t::FromFileSystem(std::shared_ptr<TempFileSystem> const& fs, VirtualMachine::NodeType type)
{
	return FromFileSystem(fs, type, 0, 0, 0);
}

//---------------------------------------------------------------------------
// TempFileSystem::node_t::FromFileSystem (static)
//
// Creates a new shared_ptr<node_t> on a TempFileSystem private heap
//
// Arguments:
//
//	fs		- TempFileSystem instance on which to create the node
//	type	- Type of node being created
//	mode	- Initial permission mask to assign to the node
//	uid		- Initial node owner UID
//	gid		- Initial node owner GID

std::shared_ptr<TempFileSystem::node_t> TempFileSystem::node_t::FromFileSystem(std::shared_ptr<TempFileSystem> const& fs, VirtualMachine::NodeType type, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	_ASSERTE(fs);
	return std::allocate_shared<node_t, allocator_t<node_t>>(allocator_t<node_t>(fs), fs, type, mode, uid, gid);
}

//
// TEMPFILESYSTEM::DIRECTORY IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::Directory Constructor
//
// Arguments:
//
//	node		- Shared Node instance

TempFileSystem::Directory::Directory(std::shared_ptr<node_t> const& node) : m_node(node)
{
	_ASSERTE(m_node);
	_ASSERTE(m_node->Type == VirtualMachine::NodeType::Directory);
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::CreateDirectory
//
// Creates a new Directory node within this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name to assign to the new directory

VirtualMachine::Directory* TempFileSystem::Directory::CreateDirectory(VirtualMachine::Mount const* mount, char_t const* name)
{
	// Create the child directory instance inheriting this node's mode, uid and gid values
	// TODO: This is not correct, see mkdir(2) -- needs umask support among other things
	return CreateDirectory(mount, name, m_node->Permissions, m_node->OwnerUserId, m_node->OwnerGroupId);
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::CreateDirectory
//
// Creates a new Directory node within this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name to assign to the new directory
//	mode		- Initial permission flags to assign to the new node
//	uid			- Initial owner UID to assign to the new node
//	gid			- Initial owner GID to assign to the new node

VirtualMachine::Directory* TempFileSystem::Directory::CreateDirectory(VirtualMachine::Mount const* mount, char_t const* name,
	uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	_ASSERTE(mount);
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	_ASSERTE(name);
	// todo - EFAULT?

	// todo: capability check
	// todo: permission check

	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	(mode);
	(uid);
	(gid);
	//auto node = std::make_unique<Directory>(m_fs, mode, uid, gid);
	// todo - create the node

	return nullptr;
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::getOwnerGroupId
//
// Gets the currently set owner group identifier for the directory

uapi_gid_t TempFileSystem::Directory::getOwnerGroupId(void) const
{
	return m_node->OwnerGroupId;
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::getOwnerUserId
//
// Gets the currently set owner user identifier for the directory

uapi_uid_t TempFileSystem::Directory::getOwnerUserId(void) const
{
	return m_node->OwnerUserId;
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::getPermissions
//
// Gets the currently set permissions mask for the directory

uapi_mode_t TempFileSystem::Directory::getPermissions(void) const
{
	return m_node->Permissions;
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::getType
//
// Gets the type of the node instance

VirtualMachine::NodeType TempFileSystem::Directory::getType(void) const
{
	_ASSERTE(m_node->Type == VirtualMachine::NodeType::Directory);
	return VirtualMachine::NodeType::Directory;
}

//
// TEMPFILESYSTEM::FILE IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::File Constructor
//
// Arguments:
//
//	node		- Shared Node instance

TempFileSystem::File::File(std::shared_ptr<node_t> const& node) : m_node(node)
{
	_ASSERTE(m_node);
	_ASSERTE(m_node->Type == VirtualMachine::NodeType::File);
}

//---------------------------------------------------------------------------
// TempFileSystem::File::getOwnerGroupId
//
// Gets the currently set owner group identifier for the file

uapi_gid_t TempFileSystem::File::getOwnerGroupId(void) const
{
	return m_node->OwnerGroupId;
}

//---------------------------------------------------------------------------
// TempFileSystem::File::getOwnerUserId
//
// Gets the currently set owner user identifier for the file

uapi_uid_t TempFileSystem::File::getOwnerUserId(void) const
{
	return m_node->OwnerUserId;
}

//---------------------------------------------------------------------------
// TempFileSystem::File::getPermissions
//
// Gets the currently set permissions mask for the file

uapi_mode_t TempFileSystem::File::getPermissions(void) const
{
	return m_node->Permissions;
}

//---------------------------------------------------------------------------
// TempFileSystem::File::getType
//
// Gets the type of the node instance

VirtualMachine::NodeType TempFileSystem::File::getType(void) const
{
	_ASSERTE(m_node->Type == VirtualMachine::NodeType::File);
	return VirtualMachine::NodeType::File;
}

//
// TEMPFILESYSTEM::MOUNT IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::Mount Constructor
//
// Arguments:
//
//	fs			- Shared file system instance
//	flags		- Mount-specific flags

TempFileSystem::Mount::Mount(std::shared_ptr<TempFileSystem> const& fs, std::unique_ptr<Directory>&& rootdir, uint32_t flags) : 
	m_fs(fs), m_rootdir(std::move(rootdir)), m_flags(flags)
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
// TempFileSystem::Mount::getFlags
//
// Gets the mount point flags

uint32_t TempFileSystem::Mount::getFlags(void) const
{
	// Combine the mount flags with those of the underlying file system
	return m_fs->Flags | m_flags;
}

//---------------------------------------------------------------------------

#pragma warning(pop)
