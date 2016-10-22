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

#include <convert.h>
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
	fs(filesystem), index(filesystem->NodeIndexPool.Allocate()), mode(nodemode), uid(userid), gid(groupid)
{
	_ASSERTE(fs);

	// Set the access time, change time and modification time to now
	uapi_timespec now = convert<uapi_timespec>(datetime::now());
	atime = ctime = mtime = now;
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

TempFileSystem::Directory::Directory(std::shared_ptr<node_t> const& node) : Node(std::dynamic_pointer_cast<directory_node_t>(node))
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
//	mode		- Initial permissions to assign to the node
//	uid			- Initial owner user id to assign to the node
//	gid			- Initial owner group id to assign to the node

std::unique_ptr<VirtualMachine::Directory> TempFileSystem::Directory::CreateDirectory(VirtualMachine::Mount const* mount, char_t const* name,
	uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	
	// Check that the proper node type has been specified in the mode flags
	if((mode & UAPI_S_IFMT) != UAPI_S_IFDIR) throw LinuxException(UAPI_EINVAL);

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
//	mode		- Initial permissions to assign to the node
//	uid			- Initial owner user id to assign to the node
//	gid			- Initial owner group id to assign to the node

std::unique_ptr<VirtualMachine::File> TempFileSystem::Directory::CreateFile(VirtualMachine::Mount const* mount, char_t const* name,
	uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	
	// Check that the proper node type has been specified in the mode flags
	if((mode & UAPI_S_IFMT) != UAPI_S_IFREG) throw LinuxException(UAPI_EINVAL);

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
// TempFileSystem::Directory::CreateSymbolicLink
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

std::unique_ptr<VirtualMachine::SymbolicLink> TempFileSystem::Directory::CreateSymbolicLink(VirtualMachine::Mount const* mount, 
	char_t const* name, char_t const* target, uapi_uid_t uid, uapi_gid_t gid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);
	if(target == nullptr) throw LinuxException(UAPI_EFAULT);
	
	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	
	// Construct the new node on the file system heap using the specified attributes
	auto node = symlink_node_t::allocate_shared(m_node->fs, target, uid, gid);

	// Lock the nodes collection for exclusive access
	sync::reader_writer_lock::scoped_lock_write writer(m_node->nodeslock);

	// Attempt to insert the new node into the collection
	auto result = m_node->nodes.emplace(name, node);
	if(result.second == false) throw LinuxException(UAPI_EEXIST);

	return std::make_unique<SymbolicLink>(node);
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::LinkNode
//
// Links an existing node as a child of this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	node		- Node to be linked into this directory
//	name		- Name to assign to the new link

void TempFileSystem::Directory::LinkNode(VirtualMachine::Mount const* mount, VirtualMachine::Node const* node, char_t const* name)
{
	std::shared_ptr<node_t>			nodeptr;			// The node_t shared pointer to be linked

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(node == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);
	
	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// S_IFREG - File node instance
	if((node->Mode & UAPI_S_IFMT) == UAPI_S_IFREG) {
		if(File const* file = dynamic_cast<File const*>(node)) nodeptr = std::dynamic_pointer_cast<node_t>(file->m_node);
	}

	// S_IFDIR - Directory node instance
	else if((node->Mode & UAPI_S_IFMT) == UAPI_S_IFDIR) {
		if(Directory const* dir = dynamic_cast<Directory const*>(node)) nodeptr = std::dynamic_pointer_cast<node_t>(dir->m_node);
	}

	// S_IFLNK - SymbolicLink node instance
	else if((node->Mode & UAPI_S_IFMT) == UAPI_S_IFLNK) {
		if(SymbolicLink const* symlink = dynamic_cast<SymbolicLink const*>(node)) nodeptr = std::dynamic_pointer_cast<node_t>(symlink->m_node);
	}

	// Any other node type results in ENXIO for now
	if(!nodeptr) throw LinuxException(UAPI_ENXIO);

	// Lock the nodes collection for exclusive access
	sync::reader_writer_lock::scoped_lock_write writer(m_node->nodeslock);

	// Attempt to insert the node into the collection with the new name
	auto result = m_node->nodes.emplace(name, nodeptr);
	if(result.second == false) throw LinuxException(UAPI_EEXIST);
}

//-----------------------------------------------------------------------------
// TempFileSystem::Directory::getLength
//
// Gets the length of the node data

size_t TempFileSystem::Directory::getLength(void) const
{
	// The size of the data is 2 (. and ..) plus the number of children
	return (2 + m_node->nodes.size()) * sizeof(uapi_linux_dirent64);
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
		case UAPI_S_IFLNK: return std::make_unique<SymbolicLink>(found->second);
	}

	throw LinuxException(UAPI_ENXIO);
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::Read
//
// Reads data from the underlying node into a buffer
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	offset		- Offset to being writing the data
//	buffer		- Destination data output buffer
//	count		- Maximum number of bytes to write from the buffer

size_t TempFileSystem::Directory::Read(VirtualMachine::Mount const* mount, size_t offset, void* buffer, size_t count)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Lock the nodes collection for exclusive access
	sync::reader_writer_lock::scoped_lock_write writer(m_node->nodeslock);



	// TODO - directory read format
	UNREFERENCED_PARAMETER(offset);
	return 0;
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::SetLength
//
// Sets the length of the node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	length		- New length to assign to the file

size_t TempFileSystem::Directory::SetLength(VirtualMachine::Mount const* mount, size_t length)
{
	UNREFERENCED_PARAMETER(length);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	throw LinuxException(UAPI_EISDIR);
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::UnlinkNode
//
// Unlinks a child node from this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name of the node to be unlinked

void TempFileSystem::Directory::UnlinkNode(VirtualMachine::Mount const* mount, char_t const* name)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);
	
	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	
	// Lock the nodes collection for exclusive access
	sync::reader_writer_lock::scoped_lock_write writer(m_node->nodeslock);

	// Attempt to find the node in the collection, ENOENT if it doesn't exist
	auto found = m_node->nodes.find(name);
	if(found == m_node->nodes.end()) throw LinuxException(UAPI_ENOENT);

	// Directory nodes are processed using different semantics than other nodes
	if((found->second->mode & UAPI_S_IFMT) == UAPI_S_IFDIR) {

		// Cast out a pointer to the child's directory_node_t and lock it
		auto dir = std::dynamic_pointer_cast<directory_node_t>(found->second);
		sync::reader_writer_lock::scoped_lock_read reader(dir->nodeslock);

		// If the directory is not empty, it cannot be unlinked
		if(dir->nodes.size() > 0) throw LinuxException(UAPI_ENOTEMPTY);
	}

	// Unlink the node by removing it from this directory; the node itself will
	// die off when it's no longer in use but this prevents it from being looked up
	m_node->nodes.erase(found->first);
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::Write
//
// Writes data into the node at the specified position
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	offset		- Offset to being writing the data
//	buffer		- Source data input buffer
//	count		- Maximum number of bytes to write from the buffer

size_t TempFileSystem::Directory::Write(VirtualMachine::Mount const* mount, size_t offset, void const* buffer, size_t count)
{
	UNREFERENCED_PARAMETER(offset);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	
	throw LinuxException(UAPI_EISDIR);
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

TempFileSystem::File::File(std::shared_ptr<node_t> const& node) : Node(std::dynamic_pointer_cast<file_node_t>(node))
{
	_ASSERTE(m_node);
	_ASSERTE((m_node->mode & UAPI_S_IFMT) == UAPI_S_IFREG);
}

//-----------------------------------------------------------------------------
// TempFileSystem::File::getLength
//
// Gets the length of the node data

size_t TempFileSystem::File::getLength(void) const
{
	return m_node->data.size();
}

//---------------------------------------------------------------------------
// TempFileSystem::File::SetLength
//
// Sets the length of the file
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	length		- New length to assign to the file

size_t TempFileSystem::File::SetLength(VirtualMachine::Mount const* mount, size_t length)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	sync::reader_writer_lock::scoped_lock_write writer(m_node->datalock);

	// Determine if the operation will shrink the file data buffer
	bool shrink = (length < m_node->data.size());

	// Resize the buffer initializing any new allocations to zero
	try { m_node->data.resize(length, 0); }
	catch(...) { throw LinuxException(UAPI_ENOSPC); }

	// If the new length is less than the original length, trim the buffer
	if(shrink) m_node->data.shrink_to_fit();

	return m_node->data.size();
}

//---------------------------------------------------------------------------
// TempFileSystem::File::Read
//
// Reads data from the underlying node into a buffer
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	offset		- Offset to being writing the data
//	buffer		- Destination data output buffer
//	count		- Maximum number of bytes to write from the buffer

size_t TempFileSystem::File::Read(VirtualMachine::Mount const* mount, size_t offset, void* buffer, size_t count)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	sync::reader_writer_lock::scoped_lock_read reader(m_node->datalock);

	// Determine the number of bytes to actually read from the file data
	if(offset >= m_node->data.size()) return 0;
	count = std::min(count, m_node->data.size() - offset);

	// Copy the requested data from the file into the provided buffer
	memcpy(buffer, &m_node->data[offset], count);

	return count;
}

//---------------------------------------------------------------------------
// TempFileSystem::File::Write
//
// Writes data into the node at the specified position
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	offset		- Offset to being writing the data
//	buffer		- Source data input buffer
//	count		- Maximum number of bytes to write from the buffer

size_t TempFileSystem::File::Write(VirtualMachine::Mount const* mount, size_t offset, void const* buffer, size_t count)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	sync::reader_writer_lock::scoped_lock_write writer(m_node->datalock);

	// Ensure that the node buffer is large enough to accept the data
	if((offset + count) > m_node->data.size()) {
		
		try { m_node->data.resize(offset + count); }
		catch(...) { throw LinuxException(UAPI_ENOSPC); }
	}

	// Copy the data from the input buffer into the node data buffer
	memcpy(&m_node->data[offset], buffer, count);

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

//
// TEMPFILESYSTEM::NODE IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::Node Constructor
//
// Arguments:
//
//	node		- Shared _node_type instance

template <class _interface, typename _node_type>
TempFileSystem::Node<_interface, _node_type>::Node(std::shared_ptr<_node_type> const& node) : m_node(node)
{
	_ASSERTE(m_node);
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::getAccessTime
//
// Gets the access time of the node

template <class _interface, typename _node_type>
uapi_timespec TempFileSystem::Node<_interface, _node_type>::getAccessTime(void) const
{
	return m_node->atime;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::getChangeTime
//
// Gets the change time of the node

template <class _interface, typename _node_type>
uapi_timespec TempFileSystem::Node<_interface, _node_type>::getChangeTime(void) const
{
	return m_node->ctime;
}
		
//---------------------------------------------------------------------------
// TempFileSystem::Node::getGroupId
//
// Gets the currently set owner group identifier for the file

template <class _interface, typename _node_type>
uapi_gid_t TempFileSystem::Node<_interface, _node_type>::getGroupId(void) const
{
	return m_node->gid;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::getIndex
//
// Gets the node index within the file system (inode number)

template <class _interface, typename _node_type>
intptr_t TempFileSystem::Node<_interface, _node_type>::getIndex(void) const
{
	return m_node->index;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::getMode
//
// Gets the type and permissions mask for the node

template <class _interface, typename _node_type>
uapi_mode_t TempFileSystem::Node<_interface, _node_type>::getMode(void) const
{
	return m_node->mode;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::getModificationTime
//
// Gets the modification time of the node

template <class _interface, typename _node_type>
uapi_timespec TempFileSystem::Node<_interface, _node_type>::getModificationTime(void) const
{
	return m_node->mtime;
}
		
//---------------------------------------------------------------------------
// TempFileSystem::Node::SetAccessTime
//
// Changes the access time of this node
//
// Arugments:
//
//	mount		- Mount point on which to perform this operation
//	atime		- New access time to be set

template <class _interface, typename _node_type>
uapi_timespec TempFileSystem::Node<_interface, _node_type>::SetAccessTime(VirtualMachine::Mount const* mount, uapi_timespec atime)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// If the mount point prevents changing the access time there is nothing to do
	if((mount->Flags & UAPI_MS_NOATIME) == UAPI_MS_NOATIME) return m_node->atime;

	bool		update = false;							// Flag to actually update atime
	datetime	newatime = convert<datetime>(atime);	// New access time as a datetime

	// Update atime if previous atime is more than 24 hours in the past (see mount(2))
	if(newatime > (convert<datetime>(m_node->atime.load()) + timespan::days(1))) update = true;

	// MS_STRICTATIME - update atime
	else if((mount->Flags & UAPI_MS_STRICTATIME) == UAPI_MS_STRICTATIME) update = true;

	// Default (MS_RELATIME) - update atime if it is more recent than ctime or mtime
	else if((newatime >= convert<datetime>(m_node->ctime.load())) || (newatime >= convert<datetime>(m_node->mtime.load()))) update = true;

	if(update) m_node->atime = atime;				// Update access time
	return atime;									// Return new access time
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::SetChangeTime
//
// Changes the change time of this node
//
// Arugments:
//
//	mount		- Mount point on which to perform this operation
//	ctime		- New change time to be set

template <class _interface, typename _node_type>
uapi_timespec TempFileSystem::Node<_interface, _node_type>::SetChangeTime(VirtualMachine::Mount const* mount, uapi_timespec ctime)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_node->ctime = ctime;
	return ctime;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::SetGroupId
//
// Changes the owner group id for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	gid			- New owner group id to be set

template <class _interface, typename _node_type>
uapi_gid_t TempFileSystem::Node<_interface, _node_type>::SetGroupId(VirtualMachine::Mount const* mount, uapi_gid_t gid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_node->gid = gid;
	return gid;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::SetMode
//
// Changes the mode flags for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	mode		- New mode flags to be set

template <class _interface, typename _node_type>
uapi_mode_t TempFileSystem::Node<_interface, _node_type>::SetMode(VirtualMachine::Mount const* mount, uapi_mode_t mode)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// Strip out all but the permissions from the provided mode; the type
	// cannot be changed after a node has been created
	mode = ((mode & UAPI_S_IALLUGO) | (m_node->mode & ~UAPI_S_IALLUGO));

	m_node->mode = mode;
	return mode;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::SetModificationTime
//
// Changes the modification time of this node
//
// Arugments:
//
//	mount		- Mount point on which to perform this operation
//	mtime		- New modification time to be set

template <class _interface, typename _node_type>
uapi_timespec TempFileSystem::Node<_interface, _node_type>::SetModificationTime(VirtualMachine::Mount const* mount, uapi_timespec mtime)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_node->mtime = m_node->ctime = mtime;
	return mtime;
}
		
//---------------------------------------------------------------------------
// TempFileSystem::Node::SetUserId
//
// Changes the owner user id for this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	uid			- New owner user id to be set

template <class _interface, typename _node_type>
uapi_uid_t TempFileSystem::Node<_interface, _node_type>::SetUserId(VirtualMachine::Mount const* mount, uapi_uid_t uid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_node->uid = uid;
	return uid;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::Sync
//
// Synchronizes all metadata and data associated with the file to storage
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation

template <class _interface, typename _node_type>
void TempFileSystem::Node<_interface, _node_type>::Sync(VirtualMachine::Mount const* mount) const
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// This is a no-operation in TempFileSystem
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::SyncData
//
// Synchronizes all data associated with the file to storage, not metadata
//
// Arguments:
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation

template <class _interface, typename _node_type>
void TempFileSystem::Node<_interface, _node_type>::SyncData(VirtualMachine::Mount const* mount) const
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// This is a no-operation in TempFileSystem
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::getUserId
//
// Gets the currently set owner user identifier for the file

template <class _interface, typename _node_type>
uapi_uid_t TempFileSystem::Node<_interface, _node_type>::getUserId(void) const
{
	return m_node->uid;
}

//
// TEMPFILESYSTEM::SYMBOLICLINK IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::SymbolicLink Constructor
//
// Arguments:
//
//	node		- Shared node_t instance

TempFileSystem::SymbolicLink::SymbolicLink(std::shared_ptr<node_t> const& node) : Node(std::dynamic_pointer_cast<symlink_node_t>(node))
{
	_ASSERTE(m_node);
	_ASSERTE((m_node->mode & UAPI_S_IFMT) == UAPI_S_IFLNK);
}

//-----------------------------------------------------------------------------
// TempFileSystem::SymbolicLink::getLength
//
// Gets the length of the node data

size_t TempFileSystem::SymbolicLink::getLength(void) const
{
	return m_node->target.size();
}

//---------------------------------------------------------------------------
// TempFileSystem::SymbolicLink::Read
//
// Reads data from the underlying node into a buffer
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	offset		- Offset to being writing the data
//	buffer		- Destination data output buffer
//	count		- Maximum number of bytes to write from the buffer

size_t TempFileSystem::SymbolicLink::Read(VirtualMachine::Mount const* mount, size_t offset, void* buffer, size_t count)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Determine the number of bytes to actually read from the target string
	if(offset >= m_node->target.size()) return 0;
	count = std::min(count, m_node->target.size() - offset);

	// Copy the requested data from the target string into the provided buffer
	memcpy(buffer, &m_node->target.data()[offset], count);

	return count;
}

//---------------------------------------------------------------------------
// TempFileSystem::SymbolicLink::SetLength
//
// Sets the length of the node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	length		- New length to assign to the file

size_t TempFileSystem::SymbolicLink::SetLength(VirtualMachine::Mount const* mount, size_t length)
{
	UNREFERENCED_PARAMETER(length);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	throw LinuxException(UAPI_EPERM);
}

//---------------------------------------------------------------------------
// TempFileSystem::SymbolicLink::getTarget
//
// Gets the symbolic link target as a null-terminated C style string

char_t const* TempFileSystem::SymbolicLink::getTarget(void) const
{
	return m_node->target.c_str();
}

//---------------------------------------------------------------------------
// TempFileSystem::SymbolicLink::Write
//
// Writes data into the node at the specified position
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	offset		- Offset to being writing the data
//	buffer		- Source data input buffer
//	count		- Maximum number of bytes to write from the buffer

size_t TempFileSystem::SymbolicLink::Write(VirtualMachine::Mount const* mount, size_t offset, void const* buffer, size_t count)
{
	UNREFERENCED_PARAMETER(offset);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	
	throw LinuxException(UAPI_EPERM);
}

//---------------------------------------------------------------------------

#pragma warning(pop)
