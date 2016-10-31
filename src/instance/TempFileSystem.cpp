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
	
	// Create and return the mount point instance with an O_PATH handle against the root directory
	return std::make_unique<TempFileSystem::Mount>(fs, std::make_shared<TempFileSystem::Directory>(rootdir, UAPI_O_DIRECTORY | UAPI_O_PATH), options.Flags & UAPI_MS_PERMOUNT_MASK);
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
// TEMPFILESYSTEM::HANDLE_T IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::handle_t Constructor
//
// Arguments:
//
//	nodeptr			- Shared reference to the node instance

template <typename _node_type>
TempFileSystem::handle_t<_node_type>::handle_t(std::shared_ptr<_node_type> const& nodeptr) : node(nodeptr), position(0)
{
}

//
// TEMPFILESYSTEM::DIRECTORY IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::Directory Constructor
//
// Arguments:
//
//	node		- Shared node_t instance
//	flags		- Instance specific handle flags

TempFileSystem::Directory::Directory(std::shared_ptr<directory_node_t> const& node, uint32_t flags) : Directory(std::make_shared<handle_t<directory_node_t>>(node), flags)
{
	_ASSERTE(m_handle);
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory Constructor
//
// Arguments:
//
//	handle		- Shared handle_t instance
//	flags		- Instance specific handle flags

TempFileSystem::Directory::Directory(std::shared_ptr<handle_t<directory_node_t>> const& handle, uint32_t flags) : Node(handle, flags)
{
	_ASSERTE(m_handle);
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

void TempFileSystem::Directory::CreateDirectory(VirtualMachine::Mount const* mount, char_t const* name,	uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	
	// Check that the proper node type has been specified in the mode flags
	if((mode & UAPI_S_IFMT) != UAPI_S_IFDIR) throw LinuxException(UAPI_EINVAL);

	// Construct the new node on the file system heap using the specified attributes
	auto node = directory_node_t::allocate_shared(m_handle->node->fs, mode, uid, gid);

	// Lock the nodes collection for exclusive access
	sync::reader_writer_lock::scoped_lock_write writer(m_handle->node->nodeslock);

	// Attempt to insert the new node into the collection
	auto result = m_handle->node->nodes.emplace(name, node);
	if(result.second == false) throw LinuxException(UAPI_EEXIST);

	// Update mtime and ctime for this node
	m_handle->node->mtime = m_handle->node->ctime = convert<uapi_timespec>(datetime::now());
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

void TempFileSystem::Directory::CreateFile(VirtualMachine::Mount const* mount, char_t const* name, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	
	// Check that the proper node type has been specified in the mode flags
	if((mode & UAPI_S_IFMT) != UAPI_S_IFREG) throw LinuxException(UAPI_EINVAL);

	// Construct the new node on the file system heap using the specified attributes
	auto node = file_node_t::allocate_shared(m_handle->node->fs, mode, uid, gid);

	// Lock the nodes collection for exclusive access
	sync::reader_writer_lock::scoped_lock_write writer(m_handle->node->nodeslock);

	// Attempt to insert the new node into the collection
	auto result = m_handle->node->nodes.emplace(name, node);
	if(result.second == false) throw LinuxException(UAPI_EEXIST);

	// Update mtime and ctime for this node
	m_handle->node->mtime = m_handle->node->ctime = convert<uapi_timespec>(datetime::now());
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

void TempFileSystem::Directory::CreateSymbolicLink(VirtualMachine::Mount const* mount, char_t const* name, char_t const* target, uapi_uid_t uid, uapi_gid_t gid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);
	if(target == nullptr) throw LinuxException(UAPI_EFAULT);
	
	// Check that the mount is for this file system and it's not read-only
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	
	// Construct the new node on the file system heap using the specified attributes
	auto node = symlink_node_t::allocate_shared(m_handle->node->fs, target, uid, gid);

	// Lock the nodes collection for exclusive access
	sync::reader_writer_lock::scoped_lock_write writer(m_handle->node->nodeslock);

	// Attempt to insert the new node into the collection
	auto result = m_handle->node->nodes.emplace(name, node);
	if(result.second == false) throw LinuxException(UAPI_EEXIST);

	// Update mtime and ctime for this node
	m_handle->node->mtime = m_handle->node->ctime = convert<uapi_timespec>(datetime::now());
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::Duplicate
//
// Duplicates this node handle
//
// Arguments:
//
//	flags		- New flags to be applied to the duplicate handle

std::unique_ptr<VirtualMachine::Node> TempFileSystem::Directory::Duplicate(uint32_t flags) const
{
	// todo: flags need to be checked/filtered here

	// Duplicates reference the same handle but have their own flags
	return std::make_unique<Directory>(m_handle, flags);
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::Enumerate
//
// Enumerates all of the entries in this directory
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	func		- Callback function to invoke for each entry; return false to stop

void TempFileSystem::Directory::Enumerate(VirtualMachine::Mount const* mount, std::function<bool(VirtualMachine::DirectoryEntry const&)> func)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(func == nullptr) throw LinuxException(UAPI_EFAULT);

	// Check that the mount is for this file system
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Lock the nodes collection for shared access
	sync::reader_writer_lock::scoped_lock_read reader(m_handle->node->nodeslock);

	// There are many different formats used when reading directories from the system 
	// call interfaces, use a caller-provided function to do the actual processing
	for(auto const entry : m_handle->node->nodes) {

		// The callback function can return false to stop the enumeration
		if(!func({ entry.second->index, entry.second->mode, entry.first.c_str() })) break;
	}

	// Update atime for this node
	UpdateAccessTime(mount, { 0, UAPI_UTIME_NOW });
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
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// S_IFREG - File node instance
	if((node->Mode & UAPI_S_IFMT) == UAPI_S_IFREG) {
		if(File const* file = dynamic_cast<File const*>(node)) nodeptr = file->m_handle->node;
	}

	// S_IFDIR - Directory node instance
	else if((node->Mode & UAPI_S_IFMT) == UAPI_S_IFDIR) {
		if(Directory const* dir = dynamic_cast<Directory const*>(node)) nodeptr = dir->m_handle->node;
	}

	// S_IFLNK - SymbolicLink node instance
	else if((node->Mode & UAPI_S_IFMT) == UAPI_S_IFLNK) {
		if(SymbolicLink const* symlink = dynamic_cast<SymbolicLink const*>(node)) nodeptr = symlink->m_handle->node;
	}

	// Any other node type results in ENXIO for now
	if(!nodeptr) throw LinuxException(UAPI_ENXIO);

	// Lock the nodes collection for exclusive access
	sync::reader_writer_lock::scoped_lock_write writer(m_handle->node->nodeslock);

	// Attempt to insert the node into the collection with the new name
	auto result = m_handle->node->nodes.emplace(name, nodeptr);
	if(result.second == false) throw LinuxException(UAPI_EEXIST);

	// Update mtime and ctime for this node
	m_handle->node->mtime = m_handle->node->ctime = convert<uapi_timespec>(datetime::now());
}

//-----------------------------------------------------------------------------
// TempFileSystem::Directory::OpenNode
//
// Opens or creates a child node of this directory by name
//
// Arguments:
//
//	mount		- Mount point on which to perform the operation
//	name		- Name of the child node to be looked up
//	flags		- Handle flags to apply to the node instance
//	mode		- Permissions to assign if creating the node
//	uid			- Owner user id to assign if creating the node
//	gid			- Owner group id to assign if creating the node

std::unique_ptr<VirtualMachine::Node> TempFileSystem::Directory::OpenNode(VirtualMachine::Mount const* mount, char_t const* name, uint32_t flags, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid)
{
	std::unique_ptr<VirtualMachine::Node>		result;			// Resultant Node instance

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(name == nullptr) throw LinuxException(UAPI_EFAULT);

	// TODO: MUCH WORK IN HERE - NEEDS TO PROCESS FLAGS AND CREATE FILES
	UNREFERENCED_PARAMETER(mode);
	UNREFERENCED_PARAMETER(uid);
	UNREFERENCED_PARAMETER(gid);

	// Check that the provided mount is part of the same file system instance
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Lock the nodes collection for shared access
	sync::reader_writer_lock::scoped_lock_read reader(m_handle->node->nodeslock);

	// Attempt to find the node in the collection, ENOENT if it doesn't exist
	auto found = m_handle->node->nodes.find(name);
	if(found == m_handle->node->nodes.end()) throw LinuxException(UAPI_ENOENT);

	// Return the appropriate type of VirtualMachine::Node instance to the caller
	switch(found->second->mode & UAPI_S_IFMT) {

		case UAPI_S_IFDIR: 
			result = std::make_unique<Directory>(std::dynamic_pointer_cast<directory_node_t>(found->second), flags);
			break;

		case UAPI_S_IFREG: 
			result = std::make_unique<File>(std::dynamic_pointer_cast<file_node_t>(found->second), flags);
			break;

		case UAPI_S_IFLNK: 
			result = std::make_unique<SymbolicLink>(std::dynamic_pointer_cast<symlink_node_t>(found->second), flags);
			break;

		// todo: other valid node types (block device, fifo, char device, etc)
		default: throw LinuxException(UAPI_ENXIO);
	}

	// Update atime for this node
	UpdateAccessTime(mount, { 0, UAPI_UTIME_NOW });

	return result;										
}

//---------------------------------------------------------------------------
// TempFileSystem::Directory::Seek
//
// Changes the file position
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	offset		- Delta from the current handle position to be set
//	whence		- Location from which to apply the specified delta

size_t TempFileSystem::Directory::Seek(VirtualMachine::Mount const* mount, ssize_t offset, int whence)
{
	UNREFERENCED_PARAMETER(offset);
	UNREFERENCED_PARAMETER(whence);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// todo - this is actually valid for directories
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
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if(mount->Flags & UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);
	
	// Lock the nodes collection for exclusive access
	sync::reader_writer_lock::scoped_lock_write writer(m_handle->node->nodeslock);

	// Attempt to find the node in the collection, ENOENT if it doesn't exist
	auto found = m_handle->node->nodes.find(name);
	if(found == m_handle->node->nodes.end()) throw LinuxException(UAPI_ENOENT);

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
	m_handle->node->nodes.erase(found->first);

	// Update mtime and ctime for this node
	m_handle->node->mtime = m_handle->node->ctime = convert<uapi_timespec>(datetime::now());
}

//
// TEMPFILESYSTEM::FILE IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::File Constructor
//
// Arguments:
//
//	node			- Shared node_t instance
//	flags			- Instance-specific handle flags

TempFileSystem::File::File(std::shared_ptr<file_node_t> const& node, uint32_t flags) : File(std::make_shared<handle_t<file_node_t>>(node), flags)
{
	_ASSERTE(m_handle);
}

//---------------------------------------------------------------------------
// TempFileSystem::File Constructor
//
// Arguments:
//
//	handle			- Shared handle_t instance
//	flags			- Instance-specific handle flags

TempFileSystem::File::File(std::shared_ptr<handle_t<file_node_t>> const& handle, uint32_t flags) : Node(handle, flags)
{
	_ASSERTE(m_handle);
}

//---------------------------------------------------------------------------
// TempFileSystem::File::AdjustPosition (private)
//
// Generates an adjusted handle position based on a delta and starting location
//
// Arguments:
//
//	lock		- Reference to a scoped_lock against the node data
//	delta		- Delta to current handle position to be calculated
//	whence		- Location from which to apply the specified delta

size_t TempFileSystem::File::AdjustPosition(sync::reader_writer_lock::scoped_lock const& lock, ssize_t delta, int whence) const
{
	UNREFERENCED_PARAMETER(lock);			// Unused; ensure caller has a scoped_lock

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
// TempFileSystem::File::Duplicate
//
// Duplicates this node handle
//
// Arguments:
//
//	flags		- New flags to be applied to the duplicate handle

std::unique_ptr<VirtualMachine::Node> TempFileSystem::File::Duplicate(uint32_t flags) const
{
	// O_DIRECTORY is not applicable to file nodes, throw ENOTDIR
	if((flags & UAPI_O_DIRECTORY) == UAPI_O_DIRECTORY) throw LinuxException(UAPI_ENOTDIR);

	// todo: flags need to be checked/filtered here

	// Duplicates reference the same handle but have their own flags
	return std::make_unique<File>(m_handle, flags);
}

//---------------------------------------------------------------------------
// TempFileSystem::File::getLength
//
// Gets the length of the file node data

size_t TempFileSystem::File::getLength(void) const
{
	sync::reader_writer_lock::scoped_lock_read reader(m_handle->node->datalock);
	return m_handle->node->data.size();
}

//---------------------------------------------------------------------------
// TempFileSystem::File::Read
//
// Synchronously reads data from the underlying node into a buffer
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	buffer		- Destination data output buffer
//	count		- Maximum number of bytes to read into the buffer

size_t TempFileSystem::File::Read(VirtualMachine::Mount const* mount, void* buffer, size_t count)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	//
	// TODO: O_PATH here and elsewhere -- the semantics are easy for Files.  For directories
	// it may be a little different, but perhaps operations like mknodat() would duplicate the
	// O_PATH handle before accessing the directory node?  See open(2)
	//

	// Verify the provided mount point and ensure that the handle is not write-only
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) throw LinuxException(UAPI_EACCES);

	sync::reader_writer_lock::scoped_lock_read reader(m_handle->node->datalock);

	size_t pos = m_handle->position;		// Copy the current position

	// Determine the number of bytes to actually read from the file data
	if(pos >= m_handle->node->data.size()) return 0;
	count = std::min(count, m_handle->node->data.size() - pos);

	// Copy the requested data from the file into the provided buffer
	if(count > 0) memcpy(buffer, &m_handle->node->data[pos], count);

	m_handle->position = (pos + count);		// Set the new position

	// Update atime for this node
	UpdateAccessTime(mount, { 0, UAPI_UTIME_NOW });

	return count;
}

//---------------------------------------------------------------------------
// TempFileSystem::File::ReadAt
//
// Synchronously reads data from the underlying node into a buffer
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	offset		- Delta from the current handle position to read from
//	whence		- Location from which to apply the specified delta
//	buffer		- Destination data output buffer
//	count		- Maximum number of bytes to read into the buffer

size_t TempFileSystem::File::ReadAt(VirtualMachine::Mount const* mount, ssize_t offset, int whence, void* buffer, size_t count)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Verify the provided mount point and ensure that the handle is not write-only
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) throw LinuxException(UAPI_EACCES);

	sync::reader_writer_lock::scoped_lock_read reader(m_handle->node->datalock);

	// ReadAt() does not update the file position, just calculate the offset
	size_t pos = AdjustPosition(reader, offset, whence);

	// Determine the number of bytes to actually read from the file data
	if(pos >= m_handle->node->data.size()) return 0;
	count = std::min(count, m_handle->node->data.size() - pos);

	// Copy the requested data from the file into the provided buffer
	if(count > 0) memcpy(buffer, &m_handle->node->data[pos], count);

	// Update atime for this node
	UpdateAccessTime(mount, { 0, UAPI_UTIME_NOW });

	return count;
}

//---------------------------------------------------------------------------
// TempFileSystem::File::Seek
//
// Changes the file position
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	offset		- Delta from the current handle position to be set
//	whence		- Location from which to apply the specified delta

size_t TempFileSystem::File::Seek(VirtualMachine::Mount const* mount, ssize_t offset, int whence)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);

	sync::reader_writer_lock::scoped_lock_read reader(m_handle->node->datalock);
	
	auto newposition = AdjustPosition(reader, offset, whence);
	m_handle->position = newposition;

	return newposition;
}

//---------------------------------------------------------------------------
// TempFileSystem::FileHandle::SetLength
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

	// Verify the provided mount point and ensure that the handle is not read-only
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);

	sync::reader_writer_lock::scoped_lock_write writer(m_handle->node->datalock);

	// Determine if the operation will shrink the file data buffer
	bool shrink = (length < m_handle->node->data.size());

	// Resize the buffer initializing any new allocations to zero
	try { m_handle->node->data.resize(length, 0); }
	catch(...) { throw LinuxException(UAPI_ENOSPC); }

	// If the new length is less than the original length, trim the buffer
	if(shrink) m_handle->node->data.shrink_to_fit();

	// Update mtime and ctime for this node
	m_handle->node->mtime = m_handle->node->ctime = convert<uapi_timespec>(datetime::now());

	return m_handle->node->data.size();
}

//---------------------------------------------------------------------------
// TempFileSystem::File::Write
//
// Synchronously writes data from a buffer to the underlying node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	buffer		- Source data input buffer
//	count		- Maximum number of bytes to write into the node

size_t TempFileSystem::File::Write(VirtualMachine::Mount const* mount, const void* buffer, size_t count)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Verify the provided mount point and ensure that the handle is not read-only
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);

	sync::reader_writer_lock::scoped_lock_write writer(m_handle->node->datalock);

	// O_APPEND: Always move the position to the end of the file data
	size_t pos = ((m_flags & UAPI_O_APPEND) == UAPI_O_APPEND) ? m_handle->node->data.size() : m_handle->position;

	// Ensure that the node buffer is large enough to accept the data
	if((pos + count) > m_handle->node->data.size()) {
		
		try { m_handle->node->data.resize(pos + count); }
		catch(...) { throw LinuxException(UAPI_ENOSPC); }
	}

	// Copy the data from the input buffer into the node data buffer
	if(count > 0) memcpy(&m_handle->node->data[pos], buffer, count);
	
	m_handle->position = (pos + count);		// Set the new position

	// Update mtime and ctime for this node
	m_handle->node->mtime = m_handle->node->ctime = convert<uapi_timespec>(datetime::now());

	return count;
}

//---------------------------------------------------------------------------
// TempFileSystem::File::WriteAt
//
// Synchronously writes data from a buffer to the underlying node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	offset		- Delta from the current handle position to write to
//	whence		- Location from which to apply the specified delta
//	buffer		- Source data input buffer
//	count		- Maximum number of bytes to write into the node

size_t TempFileSystem::File::WriteAt(VirtualMachine::Mount const* mount, ssize_t offset, int whence, const void* buffer, size_t count)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	// Verify the provided mount point and ensure that the handle is not read-only
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);

	sync::reader_writer_lock::scoped_lock_write writer(m_handle->node->datalock);

	// WriteAt() does not update the file position, just calculate the offset
	size_t pos = AdjustPosition(writer, offset, whence);

	// Ensure that the node buffer is large enough to accept the data
	if((pos + count) > m_handle->node->data.size()) {
		
		try { m_handle->node->data.resize(pos + count); }
		catch(...) { throw LinuxException(UAPI_ENOSPC); }
	}

	// Copy the data from the input buffer into the node data buffer
	if(count > 0) memcpy(&m_handle->node->data[pos], buffer, count);

	// Update mtime and ctime for this node
	m_handle->node->mtime = m_handle->node->ctime = convert<uapi_timespec>(datetime::now());

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

TempFileSystem::Mount::Mount(std::shared_ptr<TempFileSystem> const& fs, std::shared_ptr<Directory> const& rootdir, uint32_t flags) : 
	m_fs(fs), m_rootdir(rootdir), m_flags(flags)
{
	_ASSERTE(m_fs);
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

VirtualMachine::Node const* TempFileSystem::Mount::getRootNode(void) const
{
	return m_rootdir.get();
}

//
// TEMPFILESYSTEM::NODE IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::Node Constructor (protected)
//
// Arguments:
//
//	handle		- Shared handle_t instance

template <class _interface, typename _handle_type>
TempFileSystem::Node<_interface, _handle_type>::Node(std::shared_ptr<_handle_type> const& handle, uint32_t flags) : m_handle(handle), m_flags(flags)
{
	_ASSERTE(m_handle);
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::getAccessTime
//
// Gets the access time of the node

template <class _interface, typename _handle_type>
uapi_timespec TempFileSystem::Node<_interface, _handle_type>::getAccessTime(void) const
{
	return m_handle->node->atime;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::getChangeTime
//
// Gets the change time of the node

template <class _interface, typename _handle_type>
uapi_timespec TempFileSystem::Node<_interface, _handle_type>::getChangeTime(void) const
{
	return m_handle->node->ctime;
}
		
//---------------------------------------------------------------------------
// TempFileSystem::Node::getFlags
//
// Gets the current set of handle flags

template <class _interface, typename _handle_type>
uint32_t TempFileSystem::Node<_interface, _handle_type>::getFlags(void) const
{
	return m_flags;
}
		
//---------------------------------------------------------------------------
// TempFileSystem::Node::getGroupId
//
// Gets the currently set owner group identifier for the file

template <class _interface, typename _handle_type>
uapi_gid_t TempFileSystem::Node<_interface, _handle_type>::getGroupId(void) const
{
	return m_handle->node->gid;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::getIndex
//
// Gets the node index within the file system (inode number)

template <class _interface, typename _handle_type>
intptr_t TempFileSystem::Node<_interface, _handle_type>::getIndex(void) const
{
	return m_handle->node->index;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::getMode
//
// Gets the type and permissions mask for the node

template <class _interface, typename _handle_type>
uapi_mode_t TempFileSystem::Node<_interface, _handle_type>::getMode(void) const
{
	return m_handle->node->mode;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::getModificationTime
//
// Gets the modification time of the node

template <class _interface, typename _handle_type>
uapi_timespec TempFileSystem::Node<_interface, _handle_type>::getModificationTime(void) const
{
	return m_handle->node->mtime;
}
		
//---------------------------------------------------------------------------
// TempFileSystem::Node::SetAccessTime
//
// Changes the access time of this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	atime		- New access time to be set

template <class _interface, typename _handle_type>
uapi_timespec TempFileSystem::Node<_interface, _handle_type>::SetAccessTime(VirtualMachine::Mount const* mount, uapi_timespec atime)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Check that the handle was not opened in read-only mode and the mount is not read-only
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// UTIME_OMIT - Don't actually change the access time
	if(atime.tv_nsec == UAPI_UTIME_OMIT) return m_handle->node->atime;

	// UTIME_NOW - Use the current datetime as the timestamp
	if(atime.tv_nsec == UAPI_UTIME_NOW) atime = convert<uapi_timespec>(datetime::now());

	m_handle->node->atime = atime;
	return atime;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::SetChangeTime
//
// Changes the change time of this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	ctime		- New change time to be set

template <class _interface, typename _handle_type>
uapi_timespec TempFileSystem::Node<_interface, _handle_type>::SetChangeTime(VirtualMachine::Mount const* mount, uapi_timespec ctime)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Check that the handle was not opened in read-only mode and the mount is not read-only
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// UTIME_OMIT - Don't actually change the access time
	if(ctime.tv_nsec == UAPI_UTIME_OMIT) return m_handle->node->atime;

	// UTIME_NOW - Use the current datetime as the timestamp
	if(ctime.tv_nsec == UAPI_UTIME_NOW) ctime = convert<uapi_timespec>(datetime::now());

	m_handle->node->ctime = ctime;
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

template <class _interface, typename _handle_type>
uapi_gid_t TempFileSystem::Node<_interface, _handle_type>::SetGroupId(VirtualMachine::Mount const* mount, uapi_gid_t gid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Check that the handle was not opened in read-only mode and the mount is not read-only
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_handle->node->gid = gid;

	// Update ctime for this node
	m_handle->node->ctime = convert<uapi_timespec>(datetime::now());

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

template <class _interface, typename _handle_type>
uapi_mode_t TempFileSystem::Node<_interface, _handle_type>::SetMode(VirtualMachine::Mount const* mount, uapi_mode_t mode)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Check that the handle was not opened in read-only mode and the mount is not read-only
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// Strip out all but the permissions from the provided mode; the type
	// cannot be changed after a node has been created
	mode = ((mode & UAPI_S_IALLUGO) | (m_handle->node->mode.load() & ~UAPI_S_IALLUGO));

	m_handle->node->mode = mode;

	// Update ctime for this node
	m_handle->node->ctime = convert<uapi_timespec>(datetime::now());

	return mode;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::SetModificationTime
//
// Changes the modification time of this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	mtime		- New modification time to be set

template <class _interface, typename _handle_type>
uapi_timespec TempFileSystem::Node<_interface, _handle_type>::SetModificationTime(VirtualMachine::Mount const* mount, uapi_timespec mtime)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Check that the handle was not opened in read-only mode and the mount is not read-only
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// UTIME_OMIT - Don't actually change the access time
	if(mtime.tv_nsec == UAPI_UTIME_OMIT) return m_handle->node->atime;

	// UTIME_NOW - Use the current datetime as the timestamp
	if(mtime.tv_nsec == UAPI_UTIME_NOW) mtime = convert<uapi_timespec>(datetime::now());

	// Setting the modification time also sets the change time
	m_handle->node->mtime = m_handle->node->ctime = mtime;
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

template <class _interface, typename _handle_type>
uapi_uid_t TempFileSystem::Node<_interface, _handle_type>::SetUserId(VirtualMachine::Mount const* mount, uapi_uid_t uid)
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Check that the handle was not opened in read-only mode and the mount is not read-only
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	m_handle->node->uid = uid;

	// Update the ctime for this node
	m_handle->node->ctime = convert<uapi_timespec>(datetime::now());

	return uid;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::Sync
//
// Synchronizes all metadata and data associated with the file to storage
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation

template <class _interface, typename _handle_type>
void TempFileSystem::Node<_interface, _handle_type>::Sync(VirtualMachine::Mount const* mount) const
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Check that the handle was not opened in read-only mode and the mount is not read-only
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// NO-OP
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::SyncData
//
// Synchronizes all data associated with the file to storage, not metadata
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation

template <class _interface, typename _handle_type>
void TempFileSystem::Node<_interface, _handle_type>::SyncData(VirtualMachine::Mount const* mount) const
{
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);

	// Check that the handle was not opened in read-only mode and the mount is not read-only
	if((m_flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);
	if((mount->Flags & UAPI_MS_RDONLY) == UAPI_MS_RDONLY) throw LinuxException(UAPI_EROFS);

	// NO-OP
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::UpdateAccessTime (private)
//
// Changes the access time of this node
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	atime		- New access time to be set

template <class _interface, typename _handle_type>
uapi_timespec TempFileSystem::Node<_interface, _handle_type>::UpdateAccessTime(VirtualMachine::Mount const* mount, uapi_timespec atime)
{
	bool update = false;								// Flag to actually change the atime

	_ASSERTE(mount);
	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);

	// O_NOATIME on the handle, MS_NOATIME on the mount or UTIME_OMIT on the timestamp -- do nothing
	if((m_flags & UAPI_O_NOATIME) == UAPI_O_NOATIME) return m_handle->node->atime;
	if((mount->Flags & UAPI_MS_NOATIME) == UAPI_MS_NOATIME) return m_handle->node->atime;
	if(atime.tv_nsec == UAPI_UTIME_OMIT) return m_handle->node->atime;

	// If this is a directory node, also do nothing if MS_NODIRATIME has been set
	if(((m_handle->node->mode & UAPI_S_IFMT) == UAPI_S_IFDIR) && ((mount->Flags & UAPI_MS_NODIRATIME) == UAPI_MS_NODIRATIME)) return m_handle->node->atime;

	// If UTIME_NOW has been specified, use the current date/time otherwise convert the provided timespec
	datetime newatime = (atime.tv_nsec == UAPI_UTIME_NOW) ? datetime::now() : convert<datetime>(atime);

	// Update atime if previous atime is more than 24 hours in the past (see mount(2))
	if(newatime > (convert<datetime>(m_handle->node->atime.load()) + timespan::days(1))) update = true;

	// MS_STRICTATIME - always update atime
	else if((mount->Flags & UAPI_MS_STRICTATIME) == UAPI_MS_STRICTATIME) update = true;

	// Default (MS_RELATIME) - update atime if it is more recent than ctime or mtime
	else if((newatime >= convert<datetime>(m_handle->node->ctime.load())) || (newatime >= convert<datetime>(m_handle->node->mtime.load()))) update = true;

	// Convert from a datetime back into a timespec and update the node timestamp
	atime = convert<uapi_timespec>(newatime);
	if(update) m_handle->node->atime = atime;

	return atime;
}

//---------------------------------------------------------------------------
// TempFileSystem::Node::getUserId
//
// Gets the currently set owner user identifier for the file

template <class _interface, typename _handle_type>
uapi_uid_t TempFileSystem::Node<_interface, _handle_type>::getUserId(void) const
{
	return m_handle->node->uid;
}

//
// TEMPFILESYSTEM::SYMBOLICLINK IMPLEMENTATION
//

//---------------------------------------------------------------------------
// TempFileSystem::SymbolicLink Constructor
//
// Arguments:
//
//	node			- Shared node_t instance
//	flags			- Instance-specific handle flags

TempFileSystem::SymbolicLink::SymbolicLink(std::shared_ptr<symlink_node_t> const& node, uint32_t flags) : SymbolicLink(std::make_shared<handle_t<symlink_node_t>>(node), flags)
{
	_ASSERTE(m_handle);
}

//---------------------------------------------------------------------------
// TempFileSystem::SymbolicLink Constructor
//
// Arguments:
//
//	handle_t		- Shared handle_t instance

TempFileSystem::SymbolicLink::SymbolicLink(std::shared_ptr<handle_t<symlink_node_t>> const& handle, uint32_t flags) : Node(handle, flags)
{
	_ASSERTE(m_handle);
}

//---------------------------------------------------------------------------
// TempFileSystem::SymbolicLink::Duplicate
//
// Duplicates this node handle
//
// Arguments:
//
//	flags		- New flags to be applied to the duplicate handle

std::unique_ptr<VirtualMachine::Node> TempFileSystem::SymbolicLink::Duplicate(uint32_t flags) const
{
	return std::make_unique<SymbolicLink>(m_handle, flags);
}

//---------------------------------------------------------------------------
// TempFileSystem::SymbolicLink::Seek
//
// Changes the file position
//
// Arguments:
//
//	mount		- Mount point on which to perform this operation
//	offset		- Delta from specified whence position
//	whence		- Position from which to apply the specified delta

size_t TempFileSystem::SymbolicLink::Seek(VirtualMachine::Mount const* mount, ssize_t offset, int whence)
{
	UNREFERENCED_PARAMETER(offset);
	UNREFERENCED_PARAMETER(whence);

	if(mount == nullptr) throw LinuxException(UAPI_EFAULT);
	if(mount->FileSystem != m_handle->node->fs.get()) throw LinuxException(UAPI_EXDEV);

	throw LinuxException(UAPI_EPERM);
}

//---------------------------------------------------------------------------
// TempFileSystem::SymbolicLink::getTarget
//
// Gets the symbolic link target as a null-terminated C style string

char_t const* TempFileSystem::SymbolicLink::getTarget(void) const
{
	// todo: need to update atime, therefore this must be a method instead that accepts a Mount*
	return m_handle->node->target.c_str();
}

//---------------------------------------------------------------------------

#pragma warning(pop)
