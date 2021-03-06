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

#ifndef __TEMPFILESYSTEM_H_
#define __TEMPFILESYSTEM_H_
#pragma once

#include <datetime.h>
#include <memory>
#include <sync.h>
#include <text.h>
#include <timespan.h>
#include <unordered_map>

#include "IndexPool.h"
#include "VirtualMachine.h"

#pragma warning(push, 4)

// MountTempFileSystem
//
// Creates an instance of TempFileSystem
std::unique_ptr<VirtualMachine::Mount> MountTempFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength);

//-----------------------------------------------------------------------------
// Class TempFileSystem
//
// TempFileSystem implements an in-memory file system.  Rather than using a virtual 
// block device constructed on raw virtual memory, this uses a private Windows heap 
// to store the file system data.  There are a number of challenges with the virtual 
// block device method that can be easily overcome by doing it this way; let the 
// operating system do the heavy lifting
//
// TODO: This list needs to be updated, OR the support for the options has to be added
//
// Supported mount options:
//
//	MS_DIRSYNC
//	MS_I_VERSION
//	MS_KERNMOUNT
//	MS_LAZYTIME
//	MS_MANDLOCK
//	MS_NOATIME
//	MS_NODEV
//	MS_NODIRATIME
//	MS_NOEXEC
//	MS_NOSUID
//	MS_RDONLY
//	MS_RELATIME
//	MS_SILENT
//	MS_STRICTATIME
//	MS_SYNCHRONOUS
//	
//	size=nnn[K|k|M|m|G|g|%]			- Defines the maximum file system size
//	nr_blocks=nnn[K|k|M|m|G|g]		- Defines the maximum number of blocks
//	nr_inodes=nnn[K|k|M|m|G|g]		- Defines the maximum number of inodes
//	mode=nnn						- Defines the permissions of the root directory
//	uid=nnn							- Defines the owner user id of the root directory
//	gid=nnn							- Defines the owner group id of the root directory
//	
// Supported remount options:
//
//	MS_I_VERSION
//	MS_LAZYTIME
//	MS_MANDLOCK
//	MS_RDONLY
//	MS_SYNCHRONOUS
//
//	size=nnn[K|k|M|m|G|g|%]			- See above
//	nr_blocks=nnn[K|k|M|m|G|g]		- See above
//	nr_inodes=nnn[K|k|M|m|G|g]		- See above

class TempFileSystem : public VirtualMachine::FileSystem
{
	// MOUNT_FLAGS
	//
	// Supported creation/mount operation flags
	static const uint32_t MOUNT_FLAGS = UAPI_MS_RDONLY | UAPI_MS_NOSUID | UAPI_MS_NODEV | UAPI_MS_NOEXEC | UAPI_MS_SYNCHRONOUS |
		UAPI_MS_MANDLOCK | UAPI_MS_DIRSYNC | UAPI_MS_NOATIME | UAPI_MS_NODIRATIME | UAPI_MS_RELATIME | UAPI_MS_SILENT | UAPI_MS_STRICTATIME | 
		UAPI_MS_LAZYTIME | UAPI_MS_I_VERSION | UAPI_MS_KERNMOUNT;

	// REMOUNT_FLAGS
	//
	// Supported remount operation flags
	static const uint32_t REMOUNT_FLAGS = UAPI_MS_REMOUNT | UAPI_MS_RDONLY | UAPI_MS_SYNCHRONOUS | UAPI_MS_MANDLOCK | UAPI_MS_I_VERSION | UAPI_MS_LAZYTIME;

	// MountTempFileSystem (friend)
	//
	// Creates an instance of TempFileSystem
	friend std::unique_ptr<VirtualMachine::Mount> MountTempFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength);

public:

	// Instance Constructor
	//
	TempFileSystem(uint32_t flags);

	// Destructor
	//
	virtual ~TempFileSystem();

	//-----------------------------------------------------------------------------
	// Fields

	// Flags
	//
	// File system specific flags
	std::atomic<uint32_t> Flags = 0;

	// NodeIndexPool
	//
	// Lock-free pool of node index numbers
	IndexPool<int64_t> NodeIndexPool;

	// MaximumNodes
	//
	// Maximum allowed number of file system nodes
	std::atomic<size_t> MaximumNodes = 0;

	// MaximumSize
	//
	// Maximum allowed size of the private heap
	std::atomic<size_t> MaximumSize = 0;

	//-----------------------------------------------------------------------
	// Member Functions
	
private:

	TempFileSystem(TempFileSystem const&)=delete;
	TempFileSystem& operator=(TempFileSystem const&)=delete;

	// allocator_t
	//
	// Custom stl allocator that operates against a TempFileSystem private heap
	template <typename _t>
	class allocator_t
	{
		// Necessary for private member access from different types of allocator_t<>
		template <typename _u> friend class allocator_t;

	public:

		using value_type		= _t;
		using pointer			= _t*;
		using const_pointer		= _t const*;
		using reference			= _t&;
		using const_reference	= _t const&;
		using size_type			= std::size_t;
		using difference_type	= std::ptrdiff_t;

		// struct rebind
		//
		// Convert this allocator_t<_t> to allocator_t<_u>
		template <class _u>
		struct rebind
		{
			typedef allocator_t<_u> other;
		};

		// Instance Constructors
		//
		allocator_t(std::shared_ptr<TempFileSystem> const& fs) : m_fs(fs) {}

		// Copy Constructors
		//
		allocator_t(allocator_t const& rhs) : m_fs(rhs.m_fs) {}
		template <typename _u> allocator_t(allocator_t<_u> const& rhs) : m_fs(rhs.m_fs) {}

		// Destructor
		//
		~allocator_t()=default;

		// Assignment Operator
		//
		template <typename _u>
		allocator_t<_t>& operator=(allocator_t<_u> const& rhs)
		{
			m_fs = rhs.m_fs;
			return *this;
		}

		//-------------------------------------------------------------------
		// Member Functions

		// address
		//
		// Returns the actual address of x even in presence of an overloaded operator&
		pointer address(reference value) const { return std::addressof(value); }
		const_pointer address(const_reference value) const { return std::addressof(value); }

		// allocate
		//
		// Allocates n * sizeof(_t) bytes of uninitialized storage
		pointer allocate(size_type n)
		{
			return reinterpret_cast<pointer>(m_fs->AllocateHeap((n * sizeof(_t)), false));
		}

		// allocate
		//
		// Allocates n * sizeof(_t) bytes of uninitialized storage
		pointer allocate(size_type n, void const* hint)
		{
			UNREFERENCED_PARAMETER(hint);
			return allocate(n);
		}

		// construct
		//
		// Constructs an object of type T in allocated uninitialized storage pointed to by p
		template <typename _t, typename... _args>
		void construct(pointer p, _args&&... args)
		{
			new(reinterpret_cast<void*>(p)) _t(std::forward<_args>(args)...);
		}

		// deallocate
		//
		// Deallocates the storage referenced by the pointer
		void deallocate(_t* p, size_type n)
		{
			UNREFERENCED_PARAMETER(n);
			m_fs->ReleaseHeap(reinterpret_cast<void*>(p));
		}

		// destroy
		//
		// Calls the destructor of the object pointed to by p 
		template <typename _u> void destroy(_u* p)
		{
			UNREFERENCED_PARAMETER(p);		// Not really, but shuts up the compiler
			p->~_u();
		}

		// max_size
		//
		// Returns the maximum theoretically possible allocation size
		size_type max_size() const
		{
			return static_cast<size_type>(m_fs->MaximumSize) / sizeof(_t);
		}

		// reallocate (non-standard)
		//
		// Reallocates the storage referenced by the pointer
		pointer reallocate(_t* p, size_type n)
		{
			return reinterpret_cast<pointer>(m_fs->ReallocateHeap((n * sizeof(_t)), false));
		}

	private:

		//-------------------------------------------------------------------
		// Member Variables

		std::shared_ptr<TempFileSystem>		m_fs;		// File system instance
	};

	// node_t
	//
	// Internal file system node representation
	class node_t
	{
	public:

		// Destructor
		//
		virtual ~node_t();

		//-------------------------------------------------------------------
		// Fields

		// atime
		//
		// Date/time that the node was last accessed
		std::atomic<uapi_timespec> atime;

		// ctime
		//
		// Date/time that the node metadata was last changed
		std::atomic<uapi_timespec> ctime;

		// fs
		//
		// Shared pointer to the parent file system
		std::shared_ptr<TempFileSystem> const fs;

		// gid
		//
		// Node owner group identifier
		std::atomic<uapi_gid_t> gid;

		// index
		//
		// The node index value
		int64_t const index;

		// mode
		//
		// The node type and permission flags
		std::atomic<uapi_mode_t> mode;

		// mtime
		//
		// Date/time that the node data was last changed
		std::atomic<uapi_timespec> mtime;

		// uid
		//
		// Node owner user identifier
		std::atomic<uapi_uid_t> uid;

		//-------------------------------------------------------------------
		// Member Functions

		// touch_atime
		//
		// Updates the access time for the node based on NOATIME/RELATIME/STRICTATIME
		uapi_timespec touch_atime(uapi_timespec accesstime, uint32_t mountflags);

	protected:

		// Instance Constructor
		//
		node_t(std::shared_ptr<TempFileSystem> const& filesystem, uapi_mode_t nodemode, uapi_uid_t userid, uapi_gid_t groupid);

	private:

		node_t(node_t const&)=delete;
		node_t& operator=(node_t const&)=delete;
	};

	// handle_t
	//
	// Internal representation of a file system handle
	template <typename _node_type>
	class handle_t
	{
	public:

		// Instance Constructor
		//
		handle_t(std::shared_ptr<_node_type> const& nodeptr);

		// Destructor
		//
		virtual ~handle_t()=default;

		//-------------------------------------------------------------------
		// Fields

		// node
		//
		// Shared pointer to the referenced node instance
		std::shared_ptr<_node_type> const node;

		// position
		//
		// Maintains the current file pointer
		std::atomic<size_t> position;

	private:

		handle_t(const handle_t& rhs)=delete;
		handle_t& operator=(const handle_t& rhs)=delete;
	};

	// directory_node_t
	//
	// Specialization of node_t for directory nodes
	class directory_node_t : public node_t
	{
	public:

		// nodemap_t
		//
		// Collection of named VirtualMachine::Node instances allocated on file system private heap
		using nodemap_t = std::unordered_map<std::string, std::shared_ptr<node_t>, std::hash<std::string>,
			std::equal_to<std::string>, allocator_t<std::pair<std::string const, std::shared_ptr<node_t>>>>;

		// Instance Constructor
		//
		directory_node_t(std::shared_ptr<TempFileSystem> const& filesystem, uapi_mode_t nodemode, uapi_uid_t userid, uapi_gid_t groupid);

		// Destructor
		//
		virtual ~directory_node_t()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// allocate_shared (static)
		//
		// Creates a new directory_node_t instance on the file system private heap
		static std::shared_ptr<directory_node_t> allocate_shared(std::shared_ptr<TempFileSystem> const& fs, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid);

		//-------------------------------------------------------------------
		// Fields

		// nodes
		//
		// Collection of child nodes
		nodemap_t nodes;

		// nodeslock
		//
		// Synchronization object
		sync::reader_writer_lock nodeslock;

	private:

		directory_node_t(directory_node_t const&)=delete;
		directory_node_t& operator=(directory_node_t const&)=delete;
	};

	// file_node_t
	//
	// Specialization of node_t for file nodes
	class file_node_t : public node_t
	{
	public:

		// data_t
		//
		// std::vector allocated on file system private heap
		using data_t = std::vector<uint8_t, allocator_t<uint8_t>>;

		// Instance Constructor
		//
		file_node_t(std::shared_ptr<TempFileSystem> const& filesystem, uapi_mode_t nodemode, uapi_uid_t userid, uapi_gid_t groupid);

		// Destructor
		//
		virtual ~file_node_t()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// allocate_shared (static)
		//
		// Creates a new file_node_t instance on the file system private heap
		static std::shared_ptr<file_node_t> allocate_shared(std::shared_ptr<TempFileSystem> const& fs, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid);

		//-------------------------------------------------------------------
		// Fields

		// data
		//
		// File data stored in a vector<> instance
		data_t data;

		// datalock
		//
		// Synchronization object
		sync::reader_writer_lock datalock;

	private:

		file_node_t(file_node_t const&)=delete;
		file_node_t& operator=(file_node_t const&)=delete;
	};

	// symlink_node_t
	//
	// Specialization of node_t for symbolic link nodes
	class symlink_node_t : public node_t
	{
	public:

		// target_t
		//
		// std::string allocated on file system private heap
		using target_t = std::basic_string<char_t, std::char_traits<char_t>, allocator_t<char_t>>;

		// Instance Constructor
		//
		symlink_node_t(std::shared_ptr<TempFileSystem> const& filesystem, char_t const* linktarget, uapi_uid_t userid, uapi_gid_t groupid);

		// Destructor
		//
		virtual ~symlink_node_t()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// allocate_shared (static)
		//
		// Creates a new symlink_node_t instance on the file system private heap
		static std::shared_ptr<symlink_node_t> allocate_shared(std::shared_ptr<TempFileSystem> const& fs, char_t const* target, uapi_uid_t uid, uapi_gid_t gid);

		//-------------------------------------------------------------------
		// Fields

		// target
		//
		// The symbolic link target
		target_t const target;

	private:

		symlink_node_t(symlink_node_t const&)=delete;
		symlink_node_t& operator=(symlink_node_t const&)=delete;
	};

	// Node
	//
	// Implements VirtualMachine::Node
	template <class _interface, typename _node_type>
	class Node : public _interface
	{
	public:

		// Destructor
		//
		virtual ~Node()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// SetAccessTime (VirtualMachine::Node)
		//
		// Changes the access time of this node
		virtual uapi_timespec SetAccessTime(VirtualMachine::Mount const* mount, uapi_timespec atime) override;

		// SetChangeTime (VirtualMachine::Node)
		//
		// Changes the change time of this node
		virtual uapi_timespec SetChangeTime(VirtualMachine::Mount const* mount, uapi_timespec ctime) override;

		// SetGroupId (VirtualMachine::Node)
		//
		// Changes the owner group id for this node
		virtual uapi_gid_t SetGroupId(VirtualMachine::Mount const* mount, uapi_gid_t gid) override;

		// SetMode (VirtualMachine::Node)
		//
		// Changes the mode flags for this node
		virtual uapi_mode_t SetMode(VirtualMachine::Mount const* mount, uapi_mode_t mode) override;

		// SetModificationTime (VirtualMachine::Node)
		//
		// Changes the modification time of this node
		virtual uapi_timespec SetModificationTime(VirtualMachine::Mount const* mount, uapi_timespec mtime) override;

		// SetUserId (VirtualMachine::Node)
		//
		// Changes the owner user id for this node
		virtual uapi_uid_t SetUserId(VirtualMachine::Mount const* mount, uapi_uid_t uid) override;

		// Stat (VirtualMachine::Node)
		//
		// Gets statistical information about this node
		virtual void Stat(VirtualMachine::Mount const* mount, uapi_stat3264* stat) override;

		// Sync (VirtualMachine::Node)
		//
		// Synchronizes all metadata and data associated with the file to storage
		virtual void Sync(VirtualMachine::Mount const* mount) const override;

		//---------------------------------------------------------------------
		// Properties

		// AccessTime (VirtualMachine::Node)
		//
		// Gets the access time of the node
		__declspec(property(get=getAccessTime)) uapi_timespec AccessTime;
		virtual uapi_timespec getAccessTime(void) const override;

		// ChangeTime (VirtualMachine::Node)
		//
		// Gets the change time of the node
		__declspec(property(get=getChangeTime)) uapi_timespec ChangeTime;
		virtual uapi_timespec getChangeTime(void) const override;

		// GroupId (VirtualMachine::Node)
		//
		// Gets the node owner group identifier
		__declspec(property(get=getGroupId)) uapi_gid_t GroupId;
		virtual uapi_gid_t getGroupId(void) const override;

		// Index (VirtualMachine::Node)
		//
		// Gets the node index within the file system (inode number)
		__declspec(property(get=getIndex)) int64_t Index;
		virtual int64_t getIndex(void) const override;

		// Mode (VirtualMachine::Node)
		//
		// Gets the node type and permission mask for the node
		__declspec(property(get=getMode)) uapi_mode_t Mode;
		virtual uapi_mode_t getMode(void) const override;

		// ModificationTime (VirtualMachine::Node)
		//
		// Gets the modification time of the node
		__declspec(property(get=getModificationTime)) uapi_timespec ModificationTime;
		virtual uapi_timespec getModificationTime(void) const override;

		// UserId (VirtualMachine::Node)
		//
		// Gets the node owner user identifier 
		__declspec(property(get=getUserId)) uapi_uid_t UserId;
		virtual uapi_uid_t getUserId(void) const override;

	protected:

		Node(Node const&)=delete;
		Node& operator=(Node const&)=delete;

		// Instance Constructor
		//
		Node(std::shared_ptr<_node_type> const& node);

		//-------------------------------------------------------------------
		// Protected Member Variables

		std::shared_ptr<_node_type> const	m_node;		// Shared node_t instance
	};

	// Handle
	//
	// Base implementation of a file system handle
	template<class _interface>
	class Handle : public _interface
	{
	public:

		// Destructor
		//
		virtual ~Handle()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// Read (VirtualMachine::Handle)
		//
		// Synchronously reads data from the underlying node into a buffer
		virtual size_t Read(void* buffer, size_t count) override;

		// Seek (VirtualMachine::Handle)
		//
		// Changes the file position
		virtual size_t Seek(ssize_t offset, int whence) override;

		// Sync (VirtualMachine::Handle)
		//
		// Synchronizes all data associated with the file to storage, not metadata
		virtual void Sync(void) const override;

		// Write (VirtualMachine::Handle)
		//
		// Synchronously writes data from a buffer to the underlying node
		virtual size_t Write(const void* buffer, size_t count) override;

		//--------------------------------------------------------------------
		// Properties

		// Flags (VirtualMachine::Handle)
		//
		// Gets the handle-level flags applied to this instance
		__declspec(property(get=getFlags)) uint32_t Flags;
		virtual uint32_t getFlags(void) const override;

	protected:

		Handle(Handle const&)=delete;
		Handle& operator=(Handle const&)=delete;

		// Instance Constructor
		//
		Handle(uint32_t flags, uint32_t mountflags);

		//-------------------------------------------------------------------
		// Protected Member Variables

		std::atomic<uint32_t>		m_flags;			// Handle flags
		uint32_t const				m_mountflags;		// Mount flags
	};

	// Directory
	//
	// Implements a directory node for this file system
	class Directory : public Node<VirtualMachine::Directory, directory_node_t>
	{
	friend class TempFileSystem;
	public:

		// Instance Constructors
		//
		Directory(std::shared_ptr<directory_node_t> const& node);

		// Destructor
		//
		virtual ~Directory()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// CreateDirectory (VirtualMachine::Directory)
		//
		// Creates a directory node as a child of this directory
		virtual std::unique_ptr<VirtualMachine::Node> CreateDirectory(VirtualMachine::Mount const* mount, char_t const* name, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) override;

		// CreateDirectoryHandle (VirtualMachine::Directory)
		//
		// Opens a DirectoryHandle instance against this node
		virtual std::unique_ptr<VirtualMachine::DirectoryHandle> CreateDirectoryHandle(VirtualMachine::Mount const* mount, uint32_t flags) const override;

		// CreateFile (VirtualMachine::Directory)
		//
		// Creates a regular file node as a child of this directory
		virtual std::unique_ptr<VirtualMachine::Node> CreateFile(VirtualMachine::Mount const* mount, char_t const* name, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) override;

		// CreateHandle (VirtualMachine::Node)
		//
		// Opens a Handle instance against this node
		virtual std::unique_ptr<VirtualMachine::Handle> CreateHandle(VirtualMachine::Mount const* mount, uint32_t flags) const override;

		// CreateSymbolicLink (VirtualMachine::Directory)
		//
		// Creates a symbolic link as a child of this directory
		virtual std::unique_ptr<VirtualMachine::Node> CreateSymbolicLink(VirtualMachine::Mount const* mount, char_t const* name, char_t const* target, uapi_uid_t uid, uapi_gid_t gid) override;

		// Duplicate (VirtualMachine::Node)
		//
		// Duplicates this node instance
		virtual std::unique_ptr<VirtualMachine::Node> Duplicate(void) const override;

		// Link (VirtualMachine::Directory)
		//
		// Links an existing node as a child of this directory
		virtual void Link(VirtualMachine::Mount const* mount, VirtualMachine::Node const* node, char_t const* name) override;

		// Lookup (VirtualMachine::Directory)
		//
		// Looks up a child node of this directory by name
		virtual std::unique_ptr<VirtualMachine::Node> Lookup(VirtualMachine::Mount const* mount, char_t const* name) override;

		// Unlink (VirtualMachine::Directory)
		//
		// Unlinks a child node from this directory
		virtual void Unlink(VirtualMachine::Mount const* mount, char_t const* name) override;

	private:

		Directory(Directory const&)=delete;
		Directory& operator=(Directory const&)=delete;
	};

	// DirectoryHandle
	//
	// Implements VirtualMachine::Handle
	class DirectoryHandle : public Handle<VirtualMachine::DirectoryHandle>
	{
	public:

		// Instance Constructor
		//
		DirectoryHandle(std::shared_ptr<handle_t<directory_node_t>> const& handle, uint32_t flags, uint32_t mountflags);

		// Destructor
		//
		virtual ~DirectoryHandle()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// Duplicate (VirtualMachine::Handle)
		//
		// Duplicates this Handle instance
		virtual std::unique_ptr<VirtualMachine::Handle> Duplicate(uint32_t flags) const override;
	
		// Enumerate (VirtualMachine::DirectoryHandle)
		//
		// Enumerates all of the children of this node
		virtual void Enumerate(std::function<bool(VirtualMachine::DirectoryEntry const&)> func) override;

		// Seek (VirtualMachine::Handle)
		//
		// Changes the file position
		virtual size_t Seek(ssize_t offset, int whence) override;

		// Sync (VirtualMachine::Handle)
		//
		// Synchronizes all data associated with the file to storage, not metadata
		virtual void Sync(void) const override;

	private:

		DirectoryHandle(DirectoryHandle const&)=delete;
		DirectoryHandle& operator=(DirectoryHandle const&)=delete;

		//-------------------------------------------------------------------
		// Protected Member Variables

		std::shared_ptr<handle_t<directory_node_t>>	m_handle;	// Shared handle_t
	};

	// File
	//
	// Implements VirtualMachine::File
	class File : public Node<VirtualMachine::File, file_node_t>
	{
	friend class TempFileSystem;
	public:

		// Instance Constructor
		//
		File(std::shared_ptr<file_node_t> const& node);

		// Destructor
		//
		~File()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// CreateFileHandle (VirtualMachine::File)
		//
		// Opens a DirectoryHandle instance against this node
		virtual std::unique_ptr<VirtualMachine::FileHandle> CreateFileHandle(VirtualMachine::Mount const* mount, uint32_t flags) const override;

		// CreateHandle (VirtualMachine::Node)
		//
		// Opens a Handle instance against this node
		virtual std::unique_ptr<VirtualMachine::Handle> CreateHandle(VirtualMachine::Mount const* mount, uint32_t flags) const override;

		// Duplicate (VirtualMachine::Node)
		//
		// Duplicates this node instance
		virtual std::unique_ptr<VirtualMachine::Node> Duplicate(void) const override;

	private:

		File(File const&)=delete;
		File& operator=(File const&)=delete;
	};

	// FileHandle
	//
	// Implements VirtualMachine::Handle
	class FileHandle : public Handle<VirtualMachine::FileHandle>
	{
	public:

		// Instance Constructor
		//
		FileHandle(std::shared_ptr<handle_t<file_node_t>> const& handle, uint32_t flags, uint32_t mountflags);

		// Destructor
		//
		virtual ~FileHandle()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// Duplicate (VirtualMachine::Handle)
		//
		// Duplicates this Handle instance
		virtual std::unique_ptr<VirtualMachine::Handle> Duplicate(uint32_t flags) const override;
	
		// Read (VirtualMachine::Handle)
		//
		// Synchronously reads data from the underlying node into a buffer
		virtual size_t Read(void* buffer, size_t count) override;

		// ReadAt (VirtualMachine::FileHandle)
		//
		// Synchronously reads data from the underlying node into a buffer
		virtual size_t ReadAt(size_t offset, void* buffer, size_t count) override;

		// Seek (VirtualMachine::Handle)
		//
		// Changes the file position
		virtual size_t Seek(ssize_t offset, int whence) override;

		// SetLength (VirtualMachine::FileHandle)
		//
		// Sets the length of the node data
		virtual size_t SetLength(size_t length) override;

		// Sync (VirtualMachine::Handle)
		//
		// Synchronizes all data associated with the file to storage, not metadata
		virtual void Sync(void) const override;

		// Write (VirtualMachine::FileHandle)
		//
		// Synchronously writes data from a buffer to the underlying node
		virtual size_t Write(const void* buffer, size_t count) override;

		// WriteAt (VirtualMachine::Handle)
		//
		// Synchronously writes data from a buffer to the underlying node
		virtual size_t WriteAt(size_t offset, const void* buffer, size_t count) override;

	private:

		FileHandle(FileHandle const&)=delete;
		FileHandle& operator=(FileHandle const&)=delete;

		//-------------------------------------------------------------------
		// Protected Member Variables

		std::shared_ptr<handle_t<file_node_t>>	m_handle;	// Shared handle_t
	};

	// Mount
	//
	// Implements VirtualMachine::Mount
	class Mount : public VirtualMachine::Mount
	{
	public:

		// Instance Constructor
		//
		Mount(std::shared_ptr<TempFileSystem> const& fs, std::unique_ptr<Directory>&& rootdir, uint32_t flags);

		// Copy Constructor
		//
		Mount(Mount const& rhs);

		// Destructor
		//
		~Mount()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// Duplicate (VirtualMachine::Mount)
		//
		// Duplicates this mount instance
		virtual std::unique_ptr<VirtualMachine::Mount> Duplicate(void) const override;

		//-------------------------------------------------------------------
		// Properties

		// FileSystem (VirtualMachine::Mount)
		//
		// Accesses the underlying file system instance
		__declspec(property(get=getFileSystem)) VirtualMachine::FileSystem* FileSystem;
		virtual VirtualMachine::FileSystem* getFileSystem(void) const override;

		// Flags (VirtualMachine::Mount)
		//
		// Gets the mount point flags
		__declspec(property(get=getFlags)) uint32_t Flags;
		virtual uint32_t getFlags(void) const override;

		// RootNode (VirtualMachine::Mount)
		//
		// Gets a pointer to the mount point root node instance
		__declspec(property(get=getRootNode)) VirtualMachine::Node* RootNode;
		virtual VirtualMachine::Node* getRootNode(void) const override;

	private:

		Mount& operator=(Mount const&)=delete;

		//---------------------------------------------------------------------
		// Member Variables

		std::shared_ptr<TempFileSystem>		m_fs;		// File system instance
		std::shared_ptr<Directory>			m_rootdir;	// Root node instance
		std::atomic<uint32_t>				m_flags;	// Mount-specific flags
	};

	// SymbolicLink
	//
	// Implements VirtualMachine::SymbolicLink
	class SymbolicLink : public Node<VirtualMachine::SymbolicLink, symlink_node_t>
	{
	friend class TempFileSystem;
	public:

		// Instance Constructors
		//
		SymbolicLink(std::shared_ptr<symlink_node_t> const& node);

		// Destructor
		//
		~SymbolicLink()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// CreateHandle (VirtualMachine::Node)
		//
		// Opens a Handle instance against this node
		virtual std::unique_ptr<VirtualMachine::Handle> CreateHandle(VirtualMachine::Mount const* mount, uint32_t flags) const override;

		// Duplicate (VirtualMachine::Node)
		//
		// Duplicates this node instance
		virtual std::unique_ptr<VirtualMachine::Node> Duplicate(void) const override;

		// ReadTarget (VirtualMachine::SymbolicLink)
		//
		// Reads the value of the symbolic link
		virtual size_t ReadTarget(VirtualMachine::Mount const* mount, char_t* buffer, size_t count) override;

		//-------------------------------------------------------------------
		// Properties

		// Length (VirtualMachine::SymbolicLink)
		//
		// Gets the length of the symbolic link target
		__declspec(property(get=getLength)) size_t Length;
		virtual size_t getLength(void) const override;

	private:

		SymbolicLink(SymbolicLink const&)=delete;
		SymbolicLink& operator=(SymbolicLink const&)=delete;
	};

	// SymbolicLinkHandle
	//
	// Implements VirtualMachine::Handle
	class SymbolicLinkHandle : public Handle<VirtualMachine::Handle>
	{
	public:

		// Instance Constructor
		//
		SymbolicLinkHandle(std::shared_ptr<handle_t<symlink_node_t>> const& handle, uint32_t flags, uint32_t mountflags);

		// Destructor
		//
		virtual ~SymbolicLinkHandle()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// Duplicate (VirtualMachine::Handle)
		//
		// Duplicates this Handle instance
		virtual std::unique_ptr<VirtualMachine::Handle> Duplicate(uint32_t flags) const override;
	
	private:

		SymbolicLinkHandle(SymbolicLinkHandle const&)=delete;
		SymbolicLinkHandle& operator=(SymbolicLinkHandle const&)=delete;

		//-------------------------------------------------------------------
		// Protected Member Variables

		std::shared_ptr<handle_t<symlink_node_t>>	m_handle;	// Shared handle_t
	};

	//-----------------------------------------------------------------------
	// Private Member Functions

	// AllocateHeap
	//
	// Allocates memory from the private heap
	void* AllocateHeap(size_t bytecount, bool zeroinit = false);

	// ReallocateHeap
	//
	// Reallocates memory in the private heap
	void* ReallocateHeap(void* ptr, size_t bytecount, bool zeroinit = false);

	// ReleaseHeap
	//
	// Releases memory from the private heap
	void ReleaseHeap(void* ptr);

	//-------------------------------------------------------------------------
	// Member Variables

	HANDLE							m_heap;			// Private heap handle
	size_t							m_heapsize;		// Currently allocated heap size
	sync::critical_section			m_heaplock;		// Heap synchronization object
};

//-----------------------------------------------------------------------------
// operator== (TempFileSystem::allocator_t<>)
//
// Compares two TempFileSystem::allocator_t<> instances for equivalence

template <typename _t, typename _u>
bool operator==(TempFileSystem::allocator_t<_t> const& lhs, TempFileSystem::allocator_t<_u> const& rhs)
{
	return lhs.m_fs == rhs.m_fs;
}

//-----------------------------------------------------------------------------
// operator== (TempFileSystem::allocator_t<>)
//
// Compares two TempFileSystem::allocator_t<> instances for equivalence

template <typename _t, typename _u>
bool operator!=(TempFileSystem::allocator_t<_t> const& lhs, TempFileSystem::allocator_t<_u> const& rhs)
{
	return lhs.m_fs != rhs.m_fs;
}

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __TEMPFILESYSTEM_H_
