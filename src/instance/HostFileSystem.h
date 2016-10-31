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
//	[no]sandbox		- Controls sandboxing of the virtual file system (see below)
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
	class Mount;

public:

	// Instance Constructor
	//
	HostFileSystem(uint32_t flags, bool sandbox);

	// Destructor
	//
	~HostFileSystem()=default;

	//-----------------------------------------------------------------------------
	// Fields

	// Flags
	//
	// File system specific flags
	std::atomic<uint32_t> Flags = 0;

	// Sandbox
	//
	// Flag indicating that file system access should be sandboxed
	std::atomic<bool> Sandbox = false;
	
private:

	HostFileSystem(HostFileSystem const&)=delete;
	HostFileSystem& operator=(HostFileSystem const&)=delete;

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

		// Flags (VirtualMachine::Node)
		//
		// Gets the handle-level flags applied to this instance
		__declspec(property(get=getFlags)) uint32_t Flags;
		virtual uint32_t getFlags(void) const override;

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
		Node(std::shared_ptr<HostFileSystem> const& fs, HANDLE handle, uint32_t flags);

		//-------------------------------------------------------------------
		// Protected Member Variables

		std::shared_ptr<HostFileSystem>	const	m_fs;		// File system instance
		HANDLE const							m_handle;	// Win32 object handle
		windows_path const						m_path;		// Win32 object path
		std::atomic<uint32_t>					m_flags;	// Handle flags
	};

	// Directory
	//
	// Implements a directory node for this file system
	class Directory : public Node<VirtualMachine::Directory>
	{
	public:

		// Instance Constructors
		//
		Directory(std::shared_ptr<HostFileSystem> const& fs, wchar_t const* path, uint32_t flags);
		Directory(Directory const& rhs, uint32_t flags);

		// Destructor
		//
		virtual ~Directory()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// CreateDirectory (VirtualMachine::Directory)
		//
		// Creates a directory node as a child of this directory
		virtual void CreateDirectory(VirtualMachine::Mount const* mount, char_t const* name, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) override;

		// CreateFile (VirtualMachine::Directory)
		//
		// Creates a regular file node as a child of this directory
		virtual void CreateFile(VirtualMachine::Mount const* mount, char_t const* name, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) override;

		// CreateSymbolicLink (VirtualMachine::Directory)
		//
		// Creates a symbolic link node as a child of this directory
		virtual void CreateSymbolicLink(VirtualMachine::Mount const* mount, char_t const* name, char_t const* target, uapi_uid_t uid, uapi_gid_t gid) override;

		// Duplicate (VirtualMachine::Node)
		//
		// Duplicates this node instance
		virtual std::unique_ptr<VirtualMachine::Node> Duplicate(uint32_t flags) const override;

		// Enumerate (VirtualMachine::Directory)
		//
		// Enumerates all of the entries in this directory
		virtual void Enumerate(VirtualMachine::Mount const* mount, std::function<bool(VirtualMachine::DirectoryEntry const&)> func) override;

		// LinkNode (VirtualMachine::Directory)
		//
		// Links an existing node as a child of this directory
		virtual void LinkNode(VirtualMachine::Mount const* mount, VirtualMachine::Node const* node, char_t const* name) override;

		// OpenNode (VirtualMachine::Directory)
		//
		// Opens a child node of this directory by name
		virtual std::unique_ptr<VirtualMachine::Node> OpenNode(VirtualMachine::Mount const* mount, char_t const* name, uint32_t flags, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) override;

		// Seek (VirtualMachine::Node)
		//
		// Changes the file position
		virtual size_t Seek(VirtualMachine::Mount const* mount, ssize_t offset, int whence) override;

		// Sync (VirtualMachine::Node)
		//
		// Synchronizes all metadata and data associated with the file to storage
		virtual void Sync(VirtualMachine::Mount const* mount) const override;

		// SyncData (VirtualMachine::Node)
		//
		// Synchronizes all data associated with the file to storage, not metadata
		virtual void SyncData(VirtualMachine::Mount const* mount) const override;

		// UnlinkNode (VirtualMachine::Directory)
		//
		// Unlinks a child node from this directory
		virtual void UnlinkNode(VirtualMachine::Mount const* mount, char_t const* name) override;

		//-------------------------------------------------------------------
		// Properties

		// Mode (VirtualMachine::Node)
		//
		// Gets the node type and permission mask for the node
		__declspec(property(get=getMode)) uapi_mode_t Mode;
		virtual uapi_mode_t getMode(void) const override;

	private:

		Directory(Directory const&)=delete;
		Directory& operator=(Directory const&)=delete;

		//-------------------------------------------------------------------
		// Private Member Functions

		// OpenHandle (static)
		//
		// Opens the native Win32 operating system handle
		static HANDLE OpenHandle(wchar_t const* path, uint32_t flags);
	};

	// File
	//
	// Implements VirtualMachine::File
	class File : public Node<VirtualMachine::File>
	{
	public:

		// Instance Constructors
		//
		File(std::shared_ptr<HostFileSystem> const& fs, wchar_t const* path, uint32_t flags);
		File(File const& rhs, uint32_t flags);

		// Destructor
		//
		~File()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// Duplicate (VirtualMachine::Node)
		//
		// Duplicates the Mount instance
		virtual std::unique_ptr<VirtualMachine::Node> Duplicate(uint32_t flags) const override;

		// Read (VirtualMachine::File)
		//
		// Synchronously reads data from the underlying node into a buffer
		virtual size_t Read(VirtualMachine::Mount const* mount, void* buffer, size_t count) override;

		// ReadAt (VirtualMachine::File)
		//
		// Synchronously reads data from the underlying node into a buffer
		virtual size_t ReadAt(VirtualMachine::Mount const* mount, ssize_t offset, int whence, void* buffer, size_t count) override;

		// Seek (VirtualMachine::Node)
		//
		// Changes the file position
		virtual size_t Seek(VirtualMachine::Mount const* mount, ssize_t offset, int whence) override;

		// SetLength (VirtualMachine::File)
		//
		// Sets the length of the node data
		virtual size_t SetLength(VirtualMachine::Mount const* mount, size_t length) override;

		// Sync (VirtualMachine::Node)
		//
		// Synchronizes all metadata and data associated with the file to storage
		virtual void Sync(VirtualMachine::Mount const* mount) const override;

		// SyncData (VirtualMachine::Node)
		//
		// Synchronizes all data associated with the file to storage, not metadata
		virtual void SyncData(VirtualMachine::Mount const* mount) const override;

		// Write (VirtualMachine::File)
		//
		// Synchronously writes data from a buffer to the underlying node
		virtual size_t Write(VirtualMachine::Mount const* mount, const void* buffer, size_t count) override;

		// WriteAt (VirtualMachine::File)
		//
		// Synchronously writes data from a buffer to the underlying node
		virtual size_t WriteAt(VirtualMachine::Mount const* mount, ssize_t offset, int whence, const void* buffer, size_t count) override;

		//-------------------------------------------------------------------
		// Properties

		// Length (VirtualMachine::File)
		//
		// Gets the length of the node data
		__declspec(property(get=getLength)) size_t Length;
		virtual size_t getLength(void) const override;

		// Mode (VirtualMachine::Node)
		//
		// Gets the node type and permission mask for the node
		__declspec(property(get=getMode)) uapi_mode_t Mode;
		virtual uapi_mode_t getMode(void) const override;

	private:

		File(File const&)=delete;
		File& operator=(File const&)=delete;

		//-------------------------------------------------------------------
		// Private Member Functions

		// OpenHandle (static)
		//
		// Opens the native Win32 operating system handle
		static HANDLE OpenHandle(wchar_t const* path, uint32_t flags);
	};

	// Mount
	//
	// Implements VirtualMachine::Mount
	class Mount : public VirtualMachine::Mount
	{
	public:

		// Instance Constructors
		//
		Mount(std::shared_ptr<HostFileSystem> const& fs, std::shared_ptr<Directory> const& rootdir, uint32_t flags);
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
		__declspec(property(get=getRootNode)) VirtualMachine::Node const* RootNode;
		virtual VirtualMachine::Node const* getRootNode(void) const override;

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
