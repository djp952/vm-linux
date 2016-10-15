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
	auto rootdir = TempFileSystem::directory_node_t::allocate_shared(fs, (mode & ~UAPI_S_IFMT) | UAPI_S_IFDIR, uid, gid);

	// Create and return the mount point instance to return to the caller
	return std::make_unique<TempFileSystem::Mount>(fs, rootdir, options.Flags & UAPI_MS_PERMOUNT_MASK);
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
// TempFileSystem::node_t Constructor (protected)
//
// Arguments:
//
//	filesystem		- Shared file system instance
//	nodemode		- Initial type and permissions to assign to the node
//	userid			- Initial owner UID to assign to the node
//	groupid			- Initial owner GID to assign to the node

TempFileSystem::node_t::node_t(std::shared_ptr<TempFileSystem> const& filesystem, uapi_mode_t nodemode, uapi_uid_t userid, uapi_gid_t groupid) : 
	fs(filesystem), index(filesystem->NodeIndexPool.Allocate()), atime(datetime::now()), ctime(atime), mtime(atime), mode(nodemode), uid(userid), gid(groupid)
{
	_ASSERTE(fs);
}

//---------------------------------------------------------------------------
// TempFileSystem::node_t Destructor

TempFileSystem::node_t::~node_t()
{
	// Release the node index from the file system index pool
	fs->NodeIndexPool.Release(index);
}

//
// TEMPFILESYSTEM::DIRECTORY_NODE_T IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::directory_node_t Constructor
//
// Arguments:
//
//	filesystem		- Shared file system instance
//	nodemode		- Initial type and permissions to assign to the node
//	userid			- Initial owner UID to assign to the node
//	groupid			- Initial owner GID to assign to the node

TempFileSystem::directory_node_t::directory_node_t(std::shared_ptr<TempFileSystem> const& filesystem, uapi_mode_t nodemode, uapi_uid_t userid, uapi_gid_t groupid) :
	node_t(filesystem, nodemode, userid, groupid), nodes(allocator_t<nodemap_t>(filesystem))
{
}

//---------------------------------------------------------------------------
// TempFileSystem::directory_node_t::allocate_shared (static)
//
// Creates a new directory_node_t instance on the file system private heap
//
// Arguments:
//
//	fs				- Shared file system instance
//	mode			- Initial type and permissions to assign to the node
//	uid				- Initial owner UID to assign to the node
//	gid				- Initial owner GID to assign to the node

std::shared_ptr<TempFileSystem::directory_node_t> TempFileSystem::directory_node_t::allocate_shared(std::shared_ptr<TempFileSystem> const& fs, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	_ASSERTE((mode & UAPI_S_IFMT) == UAPI_S_IFDIR);
	if((mode & UAPI_S_IFMT) != UAPI_S_IFDIR) throw LinuxException(UAPI_EINVAL);

	return std::allocate_shared<directory_node_t, allocator_t<directory_node_t>>(allocator_t<directory_node_t>(fs), fs, mode, uid, gid);
}

//
// TEMPFILESYSTEM::FILE_NODE_T IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::file_node_t Constructor
//
// Arguments:
//
//	filesystem		- Shared file system instance
//	nodemode		- Initial type and permissions to assign to the node
//	userid			- Initial owner UID to assign to the node
//	groupid			- Initial owner GID to assign to the node

TempFileSystem::file_node_t::file_node_t(std::shared_ptr<TempFileSystem> const& filesystem, uapi_mode_t nodemode, uapi_uid_t userid, uapi_gid_t groupid) :
	node_t(filesystem, nodemode, userid, groupid), data(allocator_t<uint8_t>(filesystem))
{
}

//---------------------------------------------------------------------------
// TempFileSystem::file_node_t::allocate_shared (static)
//
// Creates a new file_node_t instance on the file system private heap
//
// Arguments:
//
//	fs				- Shared file system instance
//	mode			- Initial type and permissions to assign to the node
//	uid				- Initial owner UID to assign to the node
//	gid				- Initial owner GID to assign to the node

std::shared_ptr<TempFileSystem::file_node_t> TempFileSystem::file_node_t::allocate_shared(std::shared_ptr<TempFileSystem> const& fs, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	_ASSERTE((mode & UAPI_S_IFMT) == UAPI_S_IFREG);
	if((mode & UAPI_S_IFMT) != UAPI_S_IFREG) throw LinuxException(UAPI_EINVAL);

	return std::allocate_shared<file_node_t, allocator_t<file_node_t>>(allocator_t<file_node_t>(fs), fs, mode, uid, gid);
}

//
// TEMPFILESYSTEM::FILE_HANDLE_T IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::file_handle_t Constructor
//
// Arguments:
//
//	filenode	- Shared file_node_t instance

TempFileSystem::file_handle_t::file_handle_t(std::shared_ptr<file_node_t> const& filenode) : node(filenode), position(0)
{
}

//
// TEMPFILESYSTEM::SYMLINK_NODE_T IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::symlink_node_t Constructor
//
// Arguments:
//
//	filesystem		- Shared file system instance
//	linktarget		- The symbolic link target string
//	userid			- Initial owner UID to assign to the node
//	groupid			- Initial owner GID to assign to the node

TempFileSystem::symlink_node_t::symlink_node_t(std::shared_ptr<TempFileSystem> const& filesystem, char_t const* linktarget, uapi_uid_t userid, uapi_gid_t groupid) :
	node_t(filesystem, (UAPI_S_IFLNK | 0777), userid, groupid), target(linktarget, allocator_t<char_t>(filesystem))
{
	_ASSERTE(linktarget);
	if(linktarget == nullptr) throw new LinuxException(UAPI_EFAULT);
}

//---------------------------------------------------------------------------
// TempFileSystem::symlink_node_t::allocate_shared (static)
//
// Creates a new symlink_node_t instance on the file system private heap
//
// Arguments:
//
//	fs				- Shared file system instance
//	mode			- Initial type and permissions to assign to the node
//	uid				- Initial owner UID to assign to the node
//	gid				- Initial owner GID to assign to the node

std::shared_ptr<TempFileSystem::symlink_node_t> TempFileSystem::symlink_node_t::allocate_shared(std::shared_ptr<TempFileSystem> const& fs, char_t const* target, uapi_uid_t uid, uapi_gid_t gid)
{
	_ASSERTE(target);
	if(target == nullptr) throw new LinuxException(UAPI_EFAULT);

	return std::allocate_shared<symlink_node_t, allocator_t<symlink_node_t>>(allocator_t<symlink_node_t>(fs), fs, target, uid, gid);
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

TempFileSystem::Directory::Directory(std::shared_ptr<node_t> const& node) : m_node(std::dynamic_pointer_cast<directory_node_t>(node))
{
	_ASSERTE(m_node);
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::CreateDirectory
//
// Creates or opens a directory node as a child of this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name to assign to the new directory
//	flags		- Flags to use when opening/creating the directory
//	mode		- Initial permissions to assign to the node
//	uid			- Initial owner user id to assign to the node
//	gid			- Initial owner group id to assign to the node

std::unique_ptr<VirtualMachine::Directory> TempFileSystem::Directory::CreateDirectory(VirtualMachine::Mount const* mount, char_t const* name,
	uint32_t flags, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the provided mount is part of the same file system instance
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Check that the proper node type has been specified in the mode flags
	if((mode & UAPI_S_IFMT) != UAPI_S_IFDIR) throw LinuxException(UAPI_EINVAL);

	// Verify that the file system was not mounted read-only
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// Construct the new node on the file system heap using the specified attributes
	auto node = directory_node_t::allocate_shared(m_node->fs, mode, uid, gid);

	// Lock the nodes collection for exclusive access
	sync::reader_writer_lock::scoped_lock_write writer(m_node->nodeslock);

	// Attempt to insert the new node into the collection
	auto result = m_node->nodes.emplace(name, node);
	if(result.second == false) throw LinuxException(UAPI_EEXIST);

	return std::make_unique<Directory>(node);
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::CreateFile
//
// Creates a new file node as a child of this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name to assign to the new node
//	flags		- Flags to use when opening/creating the file
//	mode		- Initial permissions to assign to the node
//	uid			- Initial owner user id to assign to the node
//	gid			- Initial owner group id to assign to the node

std::unique_ptr<VirtualMachine::File> TempFileSystem::Directory::CreateFile(VirtualMachine::Mount const* mount, char_t const* name,
	uint32_t flags, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the provided mount is part of the same file system instance
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Check that the proper node type has been specified in the mode flags
	if((mode & UAPI_S_IFMT) != UAPI_S_IFREG) throw LinuxException(UAPI_EINVAL);

	// Verify that the file system was not mounted read-only
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// Construct the new node on the file system heap using the specified attributes
	auto node = file_node_t::allocate_shared(m_node->fs, mode, uid, gid);

	// Lock the nodes collection for exclusive access
	sync::reader_writer_lock::scoped_lock_write writer(m_node->nodeslock);

	// Attempt to insert the new node into the collection
	auto result = m_node->nodes.emplace(name, node);
	if(result.second == false) throw LinuxException(UAPI_EEXIST);

	return std::make_unique<File>(node);
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::getGroupId
//
// Gets the currently set owner group identifier for the directory

uapi_gid_t TempFileSystem::Directory::getGroupId(void) const
{
	return m_node->gid;
}

//-----------------------------------------------------------------------------
// TempFileSystem::Directory::Lookup
//
// Accesses a child node of this directory by name
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name of the child node to be looked up

std::unique_ptr<VirtualMachine::Node> TempFileSystem::Directory::Lookup(VirtualMachine::Mount const* mount, char_t const* name) const 
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the provided mount is part of the same file system instance
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Lock the nodes collection for shared access
	sync::reader_writer_lock::scoped_lock_read reader(m_node->nodeslock);

	// Attempt to find the node in the collection, ENOENT if it doesn't exist
	auto found = m_node->nodes.find(name);
	if(found == m_node->nodes.end()) throw LinuxException(UAPI_ENOENT);

	// Return the appropriate type of VirtualMachine::Node instance to the caller
	switch(found->second->mode & UAPI_S_IFMT) {

		case UAPI_S_IFDIR: return std::make_unique<Directory>(found->second);
		case UAPI_S_IFREG: return std::make_unique<File>(found->second);
	}

	throw LinuxException(UAPI_ENOENT);	// <--- todo; need all the node types above
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::OpenHandle
//
// Opens a handle against this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	flags		- Handle flags

std::unique_ptr<VirtualMachine::Handle> TempFileSystem::Directory::OpenHandle(VirtualMachine::Mount const* mount, uint32_t flags)
{
	UNREFERENCED_PARAMETER(mount);
	UNREFERENCED_PARAMETER(flags);
	return nullptr;
}
		
//---------------------------------------------------------------------------
// TempFileSystem::Directory::getIndex
//
// Gets the node index within the file system (inode number)

intptr_t TempFileSystem::Directory::getIndex(void) const
{
	return m_node->index;
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::getMode
//
// Gets the type and permissions mask for the node

uapi_mode_t TempFileSystem::Directory::getMode(void) const
{
	return m_node->mode;
}

//---------------------------------------------------------------------------
// TempFileSystem::File:SetGroupId
//
// Changes the owner group id for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	gid			- New owner group id to be set

uapi_gid_t TempFileSystem::Directory::SetGroupId(VirtualMachine::Mount const* mount, uapi_gid_t gid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_node->gid = gid;							// Update the node gid
	m_node->ctime = datetime::now();			// Update change time

	return gid;
}

//---------------------------------------------------------------------------
// TempFileSystem::File:SetMode
//
// Changes the mode flags for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	mode		- New mode flags to be set

uapi_mode_t TempFileSystem::Directory::SetMode(VirtualMachine::Mount const* mount, uapi_mode_t mode)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// Strip out all but the permissions from the provided mode; the type
	// cannot be changed after a node has been created
	mode = ((mode & UAPI_S_IALLUGO) | (m_node->mode & ~UAPI_S_IALLUGO));

	m_node->mode = mode;						// Update the node mode
	m_node->ctime = datetime::now();			// Update change time

	return mode;
}

//---------------------------------------------------------------------------
// TempFileSystem::File:SetUserId
//
// Changes the owner user id for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	uid			- New owner user id to be set

uapi_uid_t TempFileSystem::Directory::SetUserId(VirtualMachine::Mount const* mount, uapi_uid_t uid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_node->uid = uid;							// Update the node uid
	m_node->ctime = datetime::now();			// Update change time

	return uid;
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::getUserId
//
// Gets the currently set owner user identifier for the directory

uapi_uid_t TempFileSystem::Directory::getUserId(void) const
{
	return m_node->uid;
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

TempFileSystem::File::File(std::shared_ptr<node_t> const& node) : m_node(std::dynamic_pointer_cast<file_node_t>(node))
{
	_ASSERTE(m_node);
	_ASSERTE((m_node->mode & UAPI_S_IFMT) == UAPI_S_IFREG);
}

//---------------------------------------------------------------------------
// TempFileSystem::File::getGroupId
//
// Gets the currently set owner group identifier for the file

uapi_gid_t TempFileSystem::File::getGroupId(void) const
{
	return m_node->gid;
}

//---------------------------------------------------------------------------
// TempFileSystem::File::OpenHandle
//
// Opens a handle against this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	flags		- Handle flags

std::unique_ptr<VirtualMachine::Handle> TempFileSystem::File::OpenHandle(VirtualMachine::Mount const* mount, uint32_t flags)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the provided mount is part of the same file system instance
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// todo: validate flags (both mount and handle)

	// Handles are allocated normally within the process heap
	return std::make_unique<FileHandle>(std::make_shared<file_handle_t>(m_node), flags);
}

//---------------------------------------------------------------------------
// TempFileSystem::File::getIndex
//
// Gets the node index within the file system (inode number)

intptr_t TempFileSystem::File::getIndex(void) const
{
	return m_node->index;
}

//---------------------------------------------------------------------------
// TempFileSystem::File::getMode
//
// Gets the type and permissions mask for the node

uapi_mode_t TempFileSystem::File::getMode(void) const
{
	return m_node->mode;
}

//---------------------------------------------------------------------------
// TempFileSystem::File:SetGroupId
//
// Changes the owner group id for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	gid			- New owner group id to be set

uapi_gid_t TempFileSystem::File::SetGroupId(VirtualMachine::Mount const* mount, uapi_gid_t gid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_node->gid = gid;							// Update the node gid
	m_node->ctime = datetime::now();			// Update change time

	return gid;
}

//---------------------------------------------------------------------------
// TempFileSystem::File:SetMode
//
// Changes the mode flags for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	mode		- New mode flags to be set

uapi_mode_t TempFileSystem::File::SetMode(VirtualMachine::Mount const* mount, uapi_mode_t mode)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// Strip out all but the permissions from the provided mode; the type
	// cannot be changed after a node has been created
	mode = ((mode & UAPI_S_IALLUGO) | (m_node->mode & ~UAPI_S_IALLUGO));

	m_node->mode = mode;						// Update the node mode
	m_node->ctime = datetime::now();			// Update change time

	return mode;
}

//---------------------------------------------------------------------------
// TempFileSystem::File:SetUserId
//
// Changes the owner user id for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	uid			- New owner user id to be set

uapi_uid_t TempFileSystem::File::SetUserId(VirtualMachine::Mount const* mount, uapi_uid_t uid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_node->uid = uid;							// Update the node uid
	m_node->ctime = datetime::now();			// Update change time

	return uid;
}

//---------------------------------------------------------------------------
// TempFileSystem::File::getUserId
//
// Gets the currently set owner user identifier for the file

uapi_uid_t TempFileSystem::File::getUserId(void) const
{
	return m_node->uid;
}

//
// TEMPFILESYSTEM::FILEHANDLE IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::FileHandle Constructor
//
// Arguments:
//
//	handle		- Shared file_handle_t instance
//	flags		- Instance-specific handle flags

TempFileSystem::FileHandle::FileHandle(std::shared_ptr<file_handle_t> const& handle, uint32_t flags) : m_handle(handle), m_flags(flags)
{
}

//---------------------------------------------------------------------------
// TempFileSystem::FileHandle::AdjustPosition (private)
//
// Generates an adjusted handle position based on a delta and starting location
//
// Arguments:
//
//	lock		- Reference to a scoped_lock against the node data
//	delta		- Delta to current handle position to be calculated
//	whence		- Location from which to apply the specified delta

size_t TempFileSystem::FileHandle::AdjustPosition(sync::reader_writer_lock::scoped_lock const& lock, ssize_t delta, int whence) const
{
	UNREFERENCED_PARAMETER(lock);			// Unused; ensures caller has a scoped_lock

	size_t pos = m_handle->position;		// Copy the current position

	switch(whence) {

		// UAPI_SEEK_SET - Seeks to an offset relative to the beginning of the file;
		case UAPI_SEEK_SET:

			if(delta < 0) throw LinuxException(UAPI_EINVAL);
			pos = static_cast<size_t>(delta);
			break;

		// UAPI_SEEK_CUR - Seeks to an offset relative to the current position
		case UAPI_SEEK_CUR:

			if((delta < 0) && ((pos - delta) < 0)) throw LinuxException(UAPI_EINVAL);
			pos += delta;
			break;

		// UAPI_SEEK_END - Seeks to an offset relative to the end of the file
		case UAPI_SEEK_END:

			pos = m_handle->node->data.size() + delta;
			break;

		default: throw LinuxException(UAPI_EINVAL);
	}

	return pos;
}

//---------------------------------------------------------------------------
// TempFileSystem::FileHandle::Duplicate
//
// Duplicates this handle instance
//
// Arguments:
//
//	NONE

std::unique_ptr<VirtualMachine::Handle> TempFileSystem::FileHandle::Duplicate(void) const
{
	// Duplication of a file handle removes O_CLOEXEC from the flags -- see dup(2)
	return std::make_unique<TempFileSystem::FileHandle>(m_handle, (m_flags & ~(UAPI_O_CLOEXEC)));
}

//---------------------------------------------------------------------------
// TempFileSystem::FileHandle::getFlags
//
// Gets the current flags for this handle

uint32_t TempFileSystem::FileHandle::getFlags(void) const
{
	return m_flags;
}

//---------------------------------------------------------------------------
// TempFileSystem::FileHandle::getPosition
//
// Gets the current file position for this handle

size_t TempFileSystem::FileHandle::getPosition(void) const
{
	return m_handle->position;
}

//---------------------------------------------------------------------------
// TempFileSystem::FileHandle::Read
//
// Synchronously reads data from the underlying node into a buffer
//
// Arguments:
//
//	buffer		- Pointer to the output buffer
//	count		- Maximum number of bytes to read into the buffer

size_t TempFileSystem::FileHandle::Read(void* buffer, size_t count)
{
	if(count == 0) return 0;
	if(buffer == nullptr) throw LinuxException(UAPI_EFAULT);

	uint32_t flags = m_flags;				// Copy the currently set flags

	// The file cannot be open for write-only mode
	if((flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) throw LinuxException(UAPI_EACCES);

	sync::reader_writer_lock::scoped_lock_read reader(m_handle->node->datalock);

	size_t pos = m_handle->position;		// Copy the current position

	// Determine the number of bytes to actually read from the file data
	if(pos >= m_handle->node->data.size()) return 0;
	count = std::min(count, m_handle->node->data.size() - pos);

	// Copy the requested data from the file into the provided buffer
	memcpy(buffer, &m_handle->node->data[pos], count);

	m_handle->position = (pos + count);		// Set the new position	
	
	// todo: atime

	return count;
}

//---------------------------------------------------------------------------
// TempFileSystem::FileHandle::ReadAt
//
// Synchronously reads data from the underlying node into a buffer, does not
// change the current file position
//
// Arguments:
//
//	offset		- Offset within the file from which to read
//	whence		- Location from which to apply the specified offset
//	buffer		- Pointer to the output buffer
//	count		- Maximum number of bytes to read into the buffer

size_t TempFileSystem::FileHandle::ReadAt(ssize_t offset, int whence, void* buffer, size_t count)
{
	if(count == 0) return 0;
	if(buffer == nullptr) throw LinuxException(UAPI_EFAULT);

	uint32_t flags = m_flags;				// Copy the currently set flags

	// The file cannot be open for write-only mode
	if((flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) throw LinuxException(UAPI_EACCES);

	sync::reader_writer_lock::scoped_lock_read reader(m_handle->node->datalock);

	// Calculate the starting position for the read operation
	size_t pos = AdjustPosition(reader, offset, whence);

	// Determine the number of bytes to actually read from the file data
	if(pos >= m_handle->node->data.size()) return 0;
	count = std::min(count, m_handle->node->data.size() - pos);

	// Copy the requested data from the file into the provided buffer
	memcpy(buffer, &m_handle->node->data[pos], count);

	// todo: atime: requires access to or a copy of the mount flags

	return count;
}

//---------------------------------------------------------------------------
// TempFileSystem::FileHandle::Seek
//
// Changes the file position
//
// Arguments:
//
//	offset		- Offset within the file to set the new position
//	whence		- Location from which to apply the specified offset

size_t TempFileSystem::FileHandle::Seek(ssize_t offset, int whence)
{
	sync::reader_writer_lock::scoped_lock_read reader(m_handle->node->datalock);

	m_handle->position = AdjustPosition(reader, offset, whence);
	return m_handle->position;
}

//---------------------------------------------------------------------------
// TempFileSystem::FileHandle::SetLength
//
// Sets the length of the file
//
// Arguments:
//
//	length		- New length to assign to the file

size_t TempFileSystem::FileHandle::SetLength(size_t length)
{
	uint32_t flags = m_flags;				// Copy the currently set flags

	// The file cannot be open for read-only mode
	if((flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);

	sync::reader_writer_lock::scoped_lock_write writer(m_handle->node->datalock);

	// Determine if the operation will shrink the file data buffer
	bool shrink = (length < m_handle->node->data.size());

	// Resize the buffer initializing any new allocations to zero
	try { m_handle->node->data.resize(length, 0); }
	catch(...) { throw LinuxException(UAPI_ENOSPC); }

	// If the new length is less than the original length, trim the buffer
	if(shrink) m_handle->node->data.shrink_to_fit();

	// Update the modification and change times for the node
	m_handle->node->mtime = m_handle->node->ctime = datetime::now();

	return m_handle->node->data.size();
}

//---------------------------------------------------------------------------
// TempFileSystem::FileHandle::Sync
//
// Synchronizes all metadata and data associated with the file to storage
//
// Arguments:
//
//	NONE

void TempFileSystem::FileHandle::Sync(void) const
{
	// no operation
}

//---------------------------------------------------------------------------
// TempFileSystem::FileHandle::SyncData
//
// Synchronizes all data associated with the file to storage, not metadata
//
// Arguments:
//
//	NONE

void TempFileSystem::FileHandle::SyncData(void) const
{
	// no operation
}

//---------------------------------------------------------------------------
// TempFileSystem::FileHandle::Write
//
// Synchronously writes data from a buffer to the underlying node
//
// Arguments:
//
//	buffer		- Pointer to the input buffer
//	count		- Maximum number of bytes to write from the buffer

size_t TempFileSystem::FileHandle::Write(const void* buffer, size_t count)
{
	if(count == 0) return 0;
	if(buffer == nullptr) throw LinuxException(UAPI_EFAULT);

	uint32_t flags = m_flags;				// Copy the currently set flags

	// The file cannot be open for read-only mode
	if((flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);

	sync::reader_writer_lock::scoped_lock_write writer(m_handle->node->datalock);

	size_t pos = m_handle->position;		// Copy the current position

	// Ensure that the node buffer is large enough to accept the data
	if((pos + count) > m_handle->node->data.size()) {
		
		try { m_handle->node->data.resize(pos + count); }
		catch(...) { throw LinuxException(UAPI_ENOSPC); }
	}

	// Copy the data from the input buffer into the node data buffer
	memcpy(&m_handle->node->data[pos], buffer, count);
	
	// Adjust the current file position to immediately beyond the write
	m_handle->position += count;

	// Update the modification and change times for the node
	m_handle->node->mtime = m_handle->node->ctime = datetime::now();

	return count;
}

//---------------------------------------------------------------------------
// TempFileSystem::FileHandle::WriteAt
//
// Synchronously writes data from a buffer to the underlying node, does not
// change the current file position
//
// Arguments:
//
//	offset		- Offset within the file to begin writing
//	whence		- Location from which to apply the specified offset
//	buffer		- Pointer to the input buffer
//	count		- Maximum number of bytes to write from the buffer

size_t TempFileSystem::FileHandle::WriteAt(ssize_t offset, int whence, const void* buffer, size_t count)
{
	if(count == 0) return 0;
	if(buffer == nullptr) throw LinuxException(UAPI_EFAULT);

	uint32_t flags = m_flags;				// Copy the currently set flags

	// The file cannot be open for read-only mode
	if((flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);

	sync::reader_writer_lock::scoped_lock_write writer(m_handle->node->datalock);

	// Calculate the starting position for the write operation
	size_t pos = AdjustPosition(writer, offset, whence);

	// Ensure that the node buffer is large enough to accept the data
	if((pos + count) > m_handle->node->data.size()) {
		
		try { m_handle->node->data.resize(pos + count); }
		catch(...) { throw LinuxException(UAPI_ENOSPC); }
	}

	// Copy the data from the input buffer into the node data buffer
	memcpy(&m_handle->node->data[pos], buffer, count);

	// Update the modification and change times for the node
	m_handle->node->mtime = m_handle->node->ctime = datetime::now();

	return count;
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
//	rootdir		- Root directory node instance
//	flags		- Mount-specific flags

TempFileSystem::Mount::Mount(std::shared_ptr<TempFileSystem> const& fs, std::shared_ptr<directory_node_t> const& rootdir, uint32_t flags) : 
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
// TempFileSystem::Mount Copy Constructor
//
// Arguments:
//
//	rhs		- Existing Mount instance to create a copy of

TempFileSystem::Mount::Mount(Mount const& rhs) : m_fs(rhs.m_fs), m_rootdir(rhs.m_rootdir), m_flags(static_cast<uint32_t>(rhs.m_flags))
{
	// A copy of a mount references the same shared file system and root
	// directory instance as well as a copy of the mount flags
}

//---------------------------------------------------------------------------
// TempFileSystem::Mount::Duplicate
//
// Duplicates this mount instance
//
// Arguments:
//
//	NONE

std::unique_ptr<VirtualMachine::Mount> TempFileSystem::Mount::Duplicate(void) const
{
	return std::make_unique<TempFileSystem::Mount>(*this);
}

//---------------------------------------------------------------------------
// TempFileSystem::Mount::getFileSystem
//
// Accesses the underlying file system instance

VirtualMachine::FileSystem* TempFileSystem::Mount::getFileSystem(void) const
{
	return m_fs.get();
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
// TempFileSystem::Mount::GetRootNode
//
// Gets the root node of the mount point
//
// Arguments:
//
//	NONE

std::unique_ptr<VirtualMachine::Node> TempFileSystem::Mount::GetRootNode(void) const
{
	return std::make_unique<TempFileSystem::Directory>(m_rootdir);
}

//---------------------------------------------------------------------------

#pragma warning(pop)
