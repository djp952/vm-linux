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
//	nr_blocks=nnn[K|k|M|m|G|g]		- Defines the maximum number of inodes
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
//	nr_blocks=nnn[K|k|M|m|G|g]		- See above

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

	// FORWARD DECLARATIONS
	//
	class node_t;
	class Directory;
	class File;
	class Mount;

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
	IndexPool<intptr_t>	NodeIndexPool;

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
	template<typename _t>
	class allocator_t
	{
		// Necessary for private member access from different types of allocator_t<>
		template<typename _u> friend class allocator_t;

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
		template<class _u>
		struct rebind
		{
			typedef allocator_t<_u> other;
		};

		// Instance Constructor
		//
		allocator_t(std::shared_ptr<TempFileSystem> const& fs) : m_fs(fs) {}

		// Copy Constructors
		//
		allocator_t(allocator_t const& rhs) : m_fs(rhs.m_fs) {}
		template<typename _u> allocator_t(allocator_t<_u> const& rhs) : m_fs(rhs.m_fs) {}

		// Destructor
		//
		~allocator_t()=default;

		// Assignment Operator
		//
		template<typename _u>
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
		template<typename _t, typename... _args>
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
		template<typename _u> void destroy(_u* p)
		{
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

		// Instance Constructors
		//
		node_t(std::shared_ptr<TempFileSystem> const& fs, VirtualMachine::NodeType type);
		node_t(std::shared_ptr<TempFileSystem> const& fs, VirtualMachine::NodeType type, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid);

		// Destructor
		//
		~node_t();

		//-------------------------------------------------------------------
		// Fields

		// AccessTime
		//
		// Date/time that the node was last accessed
		datetime AccessTime;

		// ChangeTime
		//
		// Date/time that the node metadata was last changed
		datetime ChangeTime;

		// Index
		//
		// The node index value
		intptr_t const Index;

		// ModifyTime
		//
		// Date/time that the node data was last changed
		datetime ModifyTime;

		// OwnerGroupId
		//
		// Node owner group identifier
		std::atomic<uapi_gid_t> OwnerGroupId;

		// OwnerUserId
		//
		// Node owner user identifier
		std::atomic<uapi_uid_t> OwnerUserId;

		// Permissions
		//
		// The node permission flags
		std::atomic<uapi_mode_t> Permissions;

		// Type
		//
		// The type of node being represented
		VirtualMachine::NodeType const Type;

		//-------------------------------------------------------------------
		// Member Functions

		// FromFileSystem (static)
		//
		// Helper function to allocate a new node_t on a TempFileSystem heap
		static std::shared_ptr<node_t> FromFileSystem(std::shared_ptr<TempFileSystem> const& fs, VirtualMachine::NodeType type);
		static std::shared_ptr<node_t> FromFileSystem(std::shared_ptr<TempFileSystem> const& fs, VirtualMachine::NodeType type, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid);

	private:

		node_t(node_t const&)=delete;
		node_t& operator=(node_t const&)=delete;

		//-------------------------------------------------------------------
		// Member Variables

		std::shared_ptr<TempFileSystem> m_fs;		// File system instance
		uint8_t*						m_data;		// Node data
		size_t							m_datalen;	// Length of the node data
		sync::reader_writer_lock		m_datalock;	// Synchronization object
	};

	// Directory
	//
	// Implements a directory node for this file system
	class Directory : public VirtualMachine::Directory
	{
	public:

		// Instance Constructors
		//
		Directory(std::shared_ptr<node_t> const& node);

		// Destructor
		//
		virtual ~Directory()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// CreateDirectory (VirtualMachine::Directory)
		//
		// Creates a new Directory node within this directory
		virtual VirtualMachine::Directory* CreateDirectory(VirtualMachine::Mount const* mount, char_t const* name) /* override - todo */;
		virtual VirtualMachine::Directory* CreateDirectory(VirtualMachine::Mount const* mount, char_t const* name, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) /* override - todo */;

		//-------------------------------------------------------------------
		// Properties

		// OwnerGroupId (VirtualMachine::Node)
		//
		// Gets the node owner group identifier
		__declspec(property(get=getOwnerGroupId)) uapi_gid_t OwnerGroupId;
		virtual uapi_gid_t getOwnerGroupId(void) const override;

		// OwnerUserId (VirtualMachine::Node)
		//
		// Gets the node owner user identifier 
		__declspec(property(get=getOwnerUserId)) uapi_uid_t OwnerUserId;
		virtual uapi_uid_t getOwnerUserId(void) const override;

		// Permissions (VirtualMachine::Node)
		//
		// Gets the node permissions mask
		__declspec(property(get=getPermissions)) uapi_mode_t Permissions;
		virtual uapi_mode_t getPermissions(void) const override;

		// Type (VirtualMachine::Node)
		//
		// Gets the node type
		__declspec(property(get=getType)) VirtualMachine::NodeType Type;
		virtual VirtualMachine::NodeType getType(void) const override;

	private:

		Directory(Directory const&)=delete;
		Directory& operator=(Directory const&)=delete;

		// nodemap_t
		//
		// Collection of named VirtualMachine::Node instances
		using nodemap_t = std::unordered_map<std::string, std::unique_ptr<VirtualMachine::Node>>;

		//---------------------------------------------------------------------
		// Member Variables

		std::shared_ptr<TempFileSystem>	m_fs;				// Owning file system
		std::shared_ptr<node_t>			m_node;				// Shared node instance
		nodemap_t						m_children;			// Collection of children
		sync::reader_writer_lock		m_childrenlock;		// Synchronization object
	};

	// File
	//
	// Implements VirtualMachine::File
	class File : public VirtualMachine::File
	{
	public:

		// Instance Constructor
		//
		File(std::shared_ptr<node_t> const& node);

		// Destructor
		//
		~File()=default;

		//---------------------------------------------------------------------
		// Properties

		// OwnerGroupId (VirtualMachine::Node)
		//
		// Gets the node owner group identifier
		__declspec(property(get=getOwnerGroupId)) uapi_gid_t OwnerGroupId;
		virtual uapi_gid_t getOwnerGroupId(void) const override;

		// OwnerUserId (VirtualMachine::Node)
		//
		// Gets the node owner user identifier 
		__declspec(property(get=getOwnerUserId)) uapi_uid_t OwnerUserId;
		virtual uapi_uid_t getOwnerUserId(void) const override;

		// Permissions (VirtualMachine::Node)
		//
		// Gets the node permissions mask
		__declspec(property(get=getPermissions)) uapi_mode_t Permissions;
		virtual uapi_mode_t getPermissions(void) const override;

		// Type (VirtualMachine::Node)
		//
		// Gets the node type
		__declspec(property(get=getType)) VirtualMachine::NodeType Type;
		virtual VirtualMachine::NodeType getType(void) const override;

	private:

		File(File const&)=delete;
		File& operator=(File const&)=delete;

		//-------------------------------------------------------------------
		// Member Variables

		std::shared_ptr<node_t>			m_node;		// Shared node instance
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

		// Destructor
		//
		~Mount()=default;

		//-------------------------------------------------------------------
		// Properties

		// Flags (VirtualMachine::Mount)
		//
		// Gets the mount point flags
		__declspec(property(get=getFlags)) uint32_t Flags;
		virtual uint32_t getFlags(void) const override;

	private:

		//---------------------------------------------------------------------
		// Member Variables

		std::shared_ptr<TempFileSystem>		m_fs;		// File system instance
		std::shared_ptr<Directory>			m_rootdir;	// Root node instance
		std::atomic<uint32_t>				m_flags;	// Mount-specific flags
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
