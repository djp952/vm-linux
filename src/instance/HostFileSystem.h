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

#ifndef __HOSTFILESYSTEM_H_
#define __HOSTFILESYSTEM_H_
#pragma once

#include <atomic>
#include <memory>
#include <path.h>
#include <text.h>
#include <sync.h>

#include "VirtualMachine.h"

#pragma warning(push, 4)

// MountHostFileSystem
//
// Creates an instance of HostFileSystem
std::unique_ptr<VirtualMachine::Mount> MountHostFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength);

//-----------------------------------------------------------------------------
// Class HostFileSystem
//
// HostFileSystem implements a pass-through file system that operates against
// a directory accessible to the host operating system
//
// Supported mount options:
//
//	MS_DIRSYNC
//	MS_KERNMOUNT
//	MS_NODEV		(Always set)
//	MS_NOEXEC
//	MS_NOSUID		(Always set)
//	MS_RDONLY
//	MS_SILENT
//	MS_SYNCHRONOUS
//	
// Supported remount options:
//
//	MS_RDONLY
//	MS_SYNCHRONOUS

class HostFileSystem : public VirtualMachine::FileSystem
{
	// MOUNT_FLAGS
	//
	// Supported creation/mount operation flags
	static const uint32_t MOUNT_FLAGS = UAPI_MS_RDONLY | UAPI_MS_NOSUID | UAPI_MS_NODEV | UAPI_MS_NOEXEC | UAPI_MS_SYNCHRONOUS |
		UAPI_MS_DIRSYNC | UAPI_MS_SILENT | UAPI_MS_KERNMOUNT;

	// REMOUNT_FLAGS
	//
	// Supported remount operation flags
	static const uint32_t REMOUNT_FLAGS = UAPI_MS_REMOUNT | UAPI_MS_RDONLY | UAPI_MS_SYNCHRONOUS;

	// MountHostFileSystem (friend)
	//
	// Creates an instance of HostFileSystem
	friend std::unique_ptr<VirtualMachine::Mount> MountHostFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength);

	// FORWARD DECLARATIONS
	//
	class Directory;
	class DirectoryHandle;
	class File;
	class FileHandle;
	class Mount;

public:

	// Instance Constructor
	//
	HostFileSystem(uint32_t flags);

	// Destructor
	//
	~HostFileSystem()=default;

	//-----------------------------------------------------------------------------
	// Fields

	// Flags
	//
	// File system specific flags
	std::atomic<uint32_t> Flags = 0;
	
private:

	HostFileSystem(HostFileSystem const&)=delete;
	HostFileSystem& operator=(HostFileSystem const&)=delete;

	// node_t
	//
	// Internal representation of a file system node
	class node_t
	{
	public:

		// Instance Constructors
		//
		node_t(std::shared_ptr<HostFileSystem> const& filesystem, windows_path&& hostpath);
		node_t(std::shared_ptr<HostFileSystem> const& filesystem, windows_path&& hostpath, DWORD attributes);

		// Destructor
		//
		virtual ~node_t()=default;

		//-------------------------------------------------------------------
		// Fields

		// attributes
		//
		// Host file system object attributes
		DWORD const attributes;

		// fs
		//
		// Shared pointer to the parent file system
		std::shared_ptr<HostFileSystem> const fs;

		// path
		//
		// Path to the underlying host file system node
		windows_path const path;

	private:

		node_t(node_t const&)=delete;
		node_t& operator=(node_t const&)=delete;
	};

	// handle_t
	//
	// Internal representation of a handle instance
	class handle_t
	{
	public:

		// Instance Constructor
		//
		handle_t(std::shared_ptr<node_t> const& nodeptr);

		// Destructor
		//
		virtual ~handle_t()=default;

		//-------------------------------------------------------------------
		// Fields

		// node
		//
		// Shared pointer to the referenced node instance
		std::shared_ptr<node_t> const node;

	private:

		handle_t(handle_t const&)=delete;
		handle_t& operator=(handle_t const&)=delete;
	};

	// directory_handle_t
	//
	// Internal representation of a directory handle instance
	class directory_handle_t : public handle_t
	{
	public:

		// Instance Constructor
		//
		directory_handle_t(std::shared_ptr<node_t> const& nodeptr);

		// Destructor
		//
		virtual ~directory_handle_t()=default;

		//-------------------------------------------------------------------
		// Fields

		// position
		//
		// Maintains the current file pointer
		std::atomic<size_t> position;

	private:

		directory_handle_t(directory_handle_t const&)=delete;
		directory_handle_t& operator=(directory_handle_t const&)=delete;
	};

	// file_handle_t
	//
	// Internal representation of a file handle instance
	class file_handle_t : public handle_t
	{
	public:

		// Instance Constructor
		//
		using handle_t::handle_t;

		// Destructor
		//
		virtual ~file_handle_t()=default;

	private:

		file_handle_t(file_handle_t const&)=delete;
		file_handle_t& operator=(file_handle_t const&)=delete;
	};

	// Node
	//
	// Implements VirtualMachine::Node
	template <class _interface>
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

		// SetModificationTime (VirtualMachine::Node)
		//
		// Changes the modification time of this node
		virtual uapi_timespec SetModificationTime(VirtualMachine::Mount const* mount, uapi_timespec mtime) override;

		// SetUserId (VirtualMachine::Node)
		//
		// Changes the owner user id for this node
		virtual uapi_uid_t SetUserId(VirtualMachine::Mount const* mount, uapi_uid_t uid) override;

		// Sync (VirtualMachine::Node)
		//
		// Synchronizes all metadata and data associated with the node to storage
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
		__declspec(property(get=getIndex)) intptr_t Index;
		virtual intptr_t getIndex(void) const override;

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
		Node(std::shared_ptr<node_t> const& node);

		//-------------------------------------------------------------------
		// Protected Member Functions

		// OpenHandle
		//
		// Opens the host operating system handle againt the node
		virtual HANDLE OpenHandle(uint32_t flags) const = 0;

		//-------------------------------------------------------------------
		// Protected Member Variables

		std::shared_ptr<node_t>		m_node;		// Shared node_t instance
	};

	// Directory
	//
	// Implements a directory node for this file system
	class Directory : public Node<VirtualMachine::Directory>
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
		// Creates a directory node as a child of this directory
		virtual std::unique_ptr<VirtualMachine::Node> CreateDirectory(VirtualMachine::Mount const* mount, char_t const* name, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) override;

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
		// Creates a symbolic link node as a child of this directory
		virtual std::unique_ptr<VirtualMachine::Node> CreateSymbolicLink(VirtualMachine::Mount const* mount, char_t const* name, char_t const* target, uapi_uid_t uid, uapi_gid_t gid) override;

		// Duplicate (VirtualMachine::Node)
		//
		// Duplicates this node instance
		virtual std::unique_ptr<VirtualMachine::Node> Duplicate(void) const override;

		// Enumerate (VirtualMachine::Directory)
		//
		// Enumerates all of the entries in this directory
		virtual void Enumerate(VirtualMachine::Mount const* mount, std::function<bool(VirtualMachine::DirectoryEntry const&)> func) override;

		// Link (VirtualMachine::Directory)
		//
		// Links an existing node as a child of this directory
		virtual void Link(VirtualMachine::Mount const* mount, VirtualMachine::Node const* node, char_t const* name) override;

		// Lookup (VirtualMachine::Directory)
		//
		// Looks up a child node of this directory by name
		virtual std::unique_ptr<VirtualMachine::Node> Lookup(VirtualMachine::Mount const* mount, char_t const* name) override;

		// SetMode (VirtualMachine::Node)
		//
		// Changes the mode flags for this node
		virtual uapi_mode_t SetMode(VirtualMachine::Mount const* mount, uapi_mode_t mode) override;

		// Stat (VirtualMachine::Node)
		//
		// Gets statistical information about this node
		virtual void Stat(VirtualMachine::Mount const* mount, uapi_stat3264* stat) override;

		// UnlinkNode (VirtualMachine::Directory)
		//
		// Unlinks a child node from this directory
		virtual void Unlink(VirtualMachine::Mount const* mount, char_t const* name) override;

		//-------------------------------------------------------------------
		// Properties

		// Mode (VirtualMachine::Node)
		//
		// Gets the type and permission masks from the node
		__declspec(property(get=getMode)) uapi_mode_t Mode;
		virtual uapi_mode_t getMode(void) const override;

	protected:

		//-------------------------------------------------------------------
		// Protected Member Functions

		// OpenHandle (Node)
		//
		// Opens a host operating system handle against the node
		virtual HANDLE OpenHandle(uint32_t flags) const override;

	private:

		Directory(Directory const&)=delete;
		Directory& operator=(Directory const&)=delete;
	};

	// DirectoryHandle
	//
	// Implements VirtualMachine::Handle
	class DirectoryHandle : public VirtualMachine::Handle
	{
	public:

		// Instance Constructor
		//
		DirectoryHandle(std::shared_ptr<directory_handle_t> const& handle, HANDLE oshandle, uint32_t flags);

		// Destructor
		//
		virtual ~DirectoryHandle()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// Duplicate
		//
		// Duplicates this Handle instance
		virtual std::unique_ptr<Handle> Duplicate(uint32_t flags) const override;
	
		// Read
		//
		// Synchronously reads data from the underlying node into a buffer
		virtual size_t Read(void* buffer, size_t count) override;

		// ReadAt
		//
		// Synchronously reads data from the underlying node into a buffer
		virtual size_t ReadAt(size_t offset, void* buffer, size_t count) override;

		// Seek
		//
		// Changes the file position
		virtual size_t Seek(ssize_t offset, int whence) override;

		// SetLength
		//
		// Sets the length of the node data
		virtual size_t SetLength(size_t length) override;

		// Sync
		//
		// Synchronizes all data associated with the file to storage, not metadata
		virtual void Sync(void) const override;

		// Write
		//
		// Synchronously writes data from a buffer to the underlying node
		virtual size_t Write(const void* buffer, size_t count) override;

		// WriteAt
		//
		// Synchronously writes data from a buffer to the underlying node
		virtual size_t WriteAt(size_t offset, const void* buffer, size_t count) override;

		//--------------------------------------------------------------------
		// Properties

		// Flags
		//
		// Gets the handle-level flags applied to this instance
		__declspec(property(get=getFlags)) uint32_t Flags;
		virtual uint32_t getFlags(void) const override;

	private:

		DirectoryHandle(DirectoryHandle const&)=delete;
		DirectoryHandle& operator=(DirectoryHandle const&)=delete;

		//-------------------------------------------------------------------
		// Protected Member Variables

		std::shared_ptr<directory_handle_t>	m_handle;		// Shared handle_t instance
		HANDLE const						m_oshandle;		// Native object handle
		std::atomic<uint32_t>				m_flags;		// Handle instance flags
	};

	// File
	//
	// Implements VirtualMachine::File
	class File : public Node<VirtualMachine::File>
	{
	public:

		// Instance Constructors
		//
		File(std::shared_ptr<node_t> const& node);

		// Destructor
		//
		~File()=default;

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

		// SetMode (VirtualMachine::Node)
		//
		// Changes the mode flags for this node
		virtual uapi_mode_t SetMode(VirtualMachine::Mount const* mount, uapi_mode_t mode) override;

		// Stat (VirtualMachine::Node)
		//
		// Gets statistical information about this node
		virtual void Stat(VirtualMachine::Mount const* mount, uapi_stat3264* stat) override;

		//-------------------------------------------------------------------
		// Properties

		// Mode (VirtualMachine::Node)
		//
		// Gets the type and permission masks from the node
		__declspec(property(get=getMode)) uapi_mode_t Mode;
		virtual uapi_mode_t getMode(void) const override;

	protected:

		//-------------------------------------------------------------------
		// Protected Member Functions

		// OpenHandle (Node)
		//
		// Opens a host operating system handle against the node
		virtual HANDLE OpenHandle(uint32_t flags) const override;

	private:

		File(File const&)=delete;
		File& operator=(File const&)=delete;
	};

	// FileHandle
	//
	// Implements VirtualMachine::Handle
	class FileHandle : public VirtualMachine::Handle
	{
	public:

		// Instance Constructor
		//
		FileHandle(std::shared_ptr<file_handle_t> const& handle, HANDLE oshandle, uint32_t flags);

		// Destructor
		//
		virtual ~FileHandle()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// Duplicate
		//
		// Duplicates this Handle instance
		virtual std::unique_ptr<Handle> Duplicate(uint32_t flags) const override;
	
		// Read
		//
		// Synchronously reads data from the underlying node into a buffer
		virtual size_t Read(void* buffer, size_t count) override;

		// ReadAt
		//
		// Synchronously reads data from the underlying node into a buffer
		virtual size_t ReadAt(size_t offset, void* buffer, size_t count) override;

		// Seek
		//
		// Changes the file position
		virtual size_t Seek(ssize_t offset, int whence) override;

		// SetLength
		//
		// Sets the length of the node data
		virtual size_t SetLength(size_t length) override;

		// Sync
		//
		// Synchronizes all data associated with the file to storage, not metadata
		virtual void Sync(void) const override;

		// Write
		//
		// Synchronously writes data from a buffer to the underlying node
		virtual size_t Write(const void* buffer, size_t count) override;

		// WriteAt
		//
		// Synchronously writes data from a buffer to the underlying node
		virtual size_t WriteAt(size_t offset, const void* buffer, size_t count) override;

		//--------------------------------------------------------------------
		// Properties

		// Flags
		//
		// Gets the handle-level flags applied to this instance
		__declspec(property(get=getFlags)) uint32_t Flags;
		virtual uint32_t getFlags(void) const override;

	private:

		FileHandle(FileHandle const&)=delete;
		FileHandle& operator=(FileHandle const&)=delete;

		//-------------------------------------------------------------------
		// Protected Member Variables

		std::shared_ptr<file_handle_t>	m_handle;	// Shared handle_t instance
		HANDLE const					m_oshandle;	// Native object handle
		std::atomic<uint32_t>			m_flags;	// Handle instance flags
	};

	// Mount
	//
	// Implements VirtualMachine::Mount
	class Mount : public VirtualMachine::Mount
	{
	public:

		// Instance Constructors
		//
		Mount(std::shared_ptr<HostFileSystem> const& fs, std::unique_ptr<Directory>&& rootdir, uint32_t flags);
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

		std::shared_ptr<HostFileSystem>		m_fs;			// File system instance
		std::shared_ptr<Directory>			m_rootdir;		// Root node instance
		std::atomic<uint32_t>				m_flags;		// Mount-specific flags
	};
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __HOSTFILESYSTEM_H_
