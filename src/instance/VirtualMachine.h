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

#ifndef __VIRTUALMACHINE_H_
#define __VIRTUALMACHINE_H_
#pragma once

#include <functional>
#include <bitmask.h>
#include <stdint.h>
#include <text.h>

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// Class VirtualMachine
//
// Defines the virtual machine interface that sits between the instance
// service and the system call implementation(s)

class VirtualMachine
{
public:

	// Destructor
	//
	virtual ~VirtualMachine()=default;

	//
	// FORWARD DECLARATIONS
	//

	struct Directory;
	struct DirectoryHandle;
	struct File;
	struct FileHandle;
	struct FileSystem;
	struct Handle;
	struct Mount;
	struct Node;
	struct SymbolicLink;
	struct SymbolicLinkHandle;

	//
	// CONSTANTS
	//

	// MaxSymbolicLinks
	//
	// Constant indicating the maximum recursion depth of a path lookup
	static const int MaxSymbolicLinks = 40;

	//
	// TYPES
	//

	// AllocationFlags (bitmask)
	//
	// Flags used with memory allocation and reservation operations
	struct AllocationFlags final : public bitmask<AllocationFlags, uint8_t, 0x01 /* TopDown */>
	{
		using bitmask::bitmask;

		//---------------------------------------------------------------------
		// Fields

		// None (static)
		//
		// Indicates no special allocation flags
		static AllocationFlags const None;

		// TopDown (static)
		//
		// Indicates to use the highest available address
		static AllocationFlags const TopDown;
	};

	// DirectoryEntry
	//
	// Information about a single directory entry
	struct DirectoryEntry
	{
		// Index
		//
		// The node index (inode number)
		intptr_t Index;

		// Mode
		//
		// The mode flags and permission bits for the directory entry
		uapi_mode_t Mode;

		// Name
		//
		// The name assigned to the directory entry
		char_t const* Name;
	};

	// LogLevel
	//
	// Strongly typed enumeration defining the level of a log entry
	enum class LogLevel : int8_t
	{
		Default			= -1,	// LOGLEVEL_DEFAULT: Default (or last) log level
		Emergency		= 0,	// LOGLEVEL_EMERG: System is unusable
		Alert			= 1,	// LOGLEVEL_ALERT: Action must be taken immediately
		Critical		= 2,	// LOGLEVEL_CRIT: Critical conditions
		Error			= 3,	// LOGLEVEL_ERR: Error conditions
		Warning			= 4,	// LOGLEVEL_WARN: Warning conditions
		Notice			= 5,	// LOGLEVEL_NOTICE: Normal but significant condition
		Informational	= 6,	// LOGLEVEL_INFO: Informational
		Debug			= 7,	// LOGLEVEL_DEBUG: Debug-level messages
	};

	// MountFileSystem
	//
	// Function signature for a file system's Mount() implementation
	using MountFileSystem = std::function<std::unique_ptr<Mount>(char_t const* source, uint32_t flags, void const* data, size_t datalength)>;

	// ProtectionFlags
	//
	// Generalized protection flags used with memory operations
	struct ProtectionFlags final : public bitmask<ProtectionFlags, uint8_t, 0x01 /* Execute */ | 0x02 /* Read */ | 0x04 /* Write */ | 0x80 /* Guard */>
	{
		using bitmask::bitmask;

		//-------------------------------------------------------------------------
		// Fields

		// Execute (static)
		//
		// Indicates that the memory region can be executed
		static ProtectionFlags const Execute;

		// Guard (static)
		//
		// Indicates that the memory region consists of guard pages
		static ProtectionFlags const Guard;

		// None (static)
		//
		// Indicates that the memory region cannot be accessed
		static ProtectionFlags const None;

		// Read (static)
		//
		// Indicates that the memory region can be read
		static ProtectionFlags const Read;

		// Write (static)
		//
		// Indicates that the memory region can be written to
		static ProtectionFlags const Write;
	};

	//
	// INTERFACES
	//

	// FileSystem
	//
	// Interface that must be implemented by a file system
	struct FileSystem
	{
		// Destructor
		//
		virtual ~FileSystem()=default;
	};

	// Mount
	//
	// Interface that must be implemented by a file system mount
	struct Mount
	{
		// Destructor
		//
		virtual ~Mount()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// Duplicate
		//
		// Duplicates the Mount instance
		virtual std::unique_ptr<Mount> Duplicate(void) const = 0;

		//-------------------------------------------------------------------
		// Properties

		// FileSystem
		//
		// Accesses the underlying file system instance
		__declspec(property(get=getFileSystem)) struct FileSystem* FileSystem;
		virtual struct FileSystem* getFileSystem(void) const = 0;

		// Flags
		//
		// Gets the mount point flags
		__declspec(property(get=getFlags)) uint32_t Flags;
		virtual uint32_t getFlags(void) const = 0;

		// RootNode
		//
		// Gets a pointer to the mount point root node instance
		__declspec(property(get=getRootNode)) struct Node* RootNode;
		virtual struct Node* getRootNode(void) const = 0;
	};

	// Node
	//
	// Interface that must be implemented by a file system node
	struct Node
	{
		// Destructor
		//
		virtual ~Node()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// CreateHandle
		//
		// Opens a Handle instance against this node
		virtual std::unique_ptr<Handle> CreateHandle(Mount const* mount, uint32_t flags) const = 0;

		// Duplicate
		//
		// Duplicates this node instance
		virtual std::unique_ptr<Node> Duplicate(void) const = 0;

		// SetAccessTime
		//
		// Changes the access time of this node
		virtual uapi_timespec SetAccessTime(Mount const* mount, uapi_timespec atime) = 0;

		// SetChangeTime
		//
		// Changes the change time of this node
		virtual uapi_timespec SetChangeTime(Mount const* mount, uapi_timespec ctime) = 0;

		// SetGroupId
		//
		// Changes the owner group id for this node
		virtual uapi_gid_t SetGroupId(Mount const* mount, uapi_gid_t gid) = 0;

		// SetMode
		//
		// Changes the mode flags for this node
		virtual uapi_mode_t SetMode(Mount const* mount, uapi_mode_t mode) = 0;

		// SetModificationTime
		//
		// Changes the modification time of this node
		virtual uapi_timespec SetModificationTime(Mount const* mount, uapi_timespec mtime) = 0;

		// SetUserId
		//
		// Changes the owner user id for this node
		virtual uapi_uid_t SetUserId(Mount const* mount, uapi_uid_t uid) = 0;

		// Stat
		//
		// Gets statistical information about this node
		virtual void Stat(Mount const* mount, uapi_stat3264* stat) = 0;

		// Sync
		//
		// Synchronizes all metadata and data associated with the node to storage
		virtual void Sync(Mount const* mount) const = 0;

		//-------------------------------------------------------------------
		// Properties

		// AccessTime
		//
		// Gets the access time of the node
		__declspec(property(get=getAccessTime)) uapi_timespec AccessTime;
		virtual uapi_timespec getAccessTime(void) const = 0;

		// ChangeTime
		//
		// Gets the change time of the node
		__declspec(property(get=getChangeTime)) uapi_timespec ChangeTime;
		virtual uapi_timespec getChangeTime(void) const = 0;

		// GroupId
		//
		// Gets the node owner group identifier
		__declspec(property(get=getGroupId)) uapi_gid_t GroupId;
		virtual uapi_gid_t getGroupId(void) const = 0;

		// Index
		//
		// Gets the node index within the file system (inode number)
		__declspec(property(get=getIndex)) intptr_t Index;
		virtual intptr_t getIndex(void) const = 0;

		// Mode
		//
		// Gets the type and permission masks from the node
		__declspec(property(get=getMode)) uapi_mode_t Mode;
		virtual uapi_mode_t getMode(void) const = 0;

		// ModificationTime
		//
		// Gets the modification time of the node
		__declspec(property(get=getModificationTime)) uapi_timespec ModificationTime;
		virtual uapi_timespec getModificationTime(void) const = 0;

		// UserId
		//
		// Gets the node owner user identifier 
		__declspec(property(get=getUserId)) uapi_uid_t UserId;
		virtual uapi_uid_t getUserId(void) const = 0;
	};

	// Handle
	//
	// Interface that must be implemented by a file system handle
	struct Handle
	{
		// Destructor
		//
		virtual ~Handle()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// Duplicate
		//
		// Duplicates this Handle instance
		virtual std::unique_ptr<Handle> Duplicate(uint32_t flags) const = 0;
	
		// Read
		//
		// Synchronously reads data from the underlying node into a buffer
		virtual size_t Read(void* buffer, size_t count) = 0;

		// Seek
		//
		// Changes the file position
		virtual size_t Seek(ssize_t offset, int whence) = 0;

		// Sync
		//
		// Synchronizes all data associated with the file to storage, not metadata
		virtual void Sync(void) const = 0;

		// Write
		//
		// Synchronously writes data from a buffer to the underlying node
		virtual size_t Write(const void* buffer, size_t count) = 0;

		//--------------------------------------------------------------------
		// Properties

		// Flags
		//
		// Gets the handle-level flags applied to this instance
		__declspec(property(get=getFlags)) uint32_t Flags;
		virtual uint32_t getFlags(void) const = 0;
	};

	// DirectoryHandle
	//
	// Interface that must be implemented by a directory object handle
	struct DirectoryHandle : public Handle
	{
		// Destructor
		//
		virtual ~DirectoryHandle()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// Enumerate
		//
		// Enumerates all of the entries in this directory
		virtual void Enumerate(std::function<bool(DirectoryEntry const&)> func) = 0;
	};

	// FileHandle
	//
	// Interface that must be implemented by a file object handle
	struct FileHandle : public Handle
	{
		// Destructor
		//
		virtual ~FileHandle()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// ReadAt
		//
		// Synchronously reads data from the underlying node into a buffer
		virtual size_t ReadAt(size_t offset, void* buffer, size_t count) = 0;

		// SetLength
		//
		// Sets the length of the node data
		virtual size_t SetLength(size_t length) = 0;

		// WriteAt
		//
		// Synchronously writes data from a buffer to the underlying node
		virtual size_t WriteAt(size_t offset, const void* buffer, size_t count) = 0;
	};

	// Directory
	//
	// Interface that must be implemented by a directory object
	struct Directory : public Node
	{
		// Destructor
		//
		virtual ~Directory()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// CreateDirectory
		//
		// Creates a directory node as a child of this directory
		virtual std::unique_ptr<Node> CreateDirectory(Mount const* mount, char_t const* name, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) = 0;

		// CreateFile
		//
		// Creates a regular file node as a child of this directory
		virtual std::unique_ptr<Node> CreateFile(Mount const* mount, char_t const* name, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) = 0;

		// CreateSymbolicLink
		//
		// Creates a symbolic link node as a child of this directory
		virtual std::unique_ptr<Node> CreateSymbolicLink(Mount const* mount, char_t const* name, char_t const* target, uapi_uid_t uid, uapi_uid_t gid) = 0;

		// Link
		//
		// Links an existing node as a child of this directory
		virtual void Link(Mount const* mount, Node const* node, char_t const* name) = 0;

		// Lookup
		//
		// Looks up a child node of this directory by name
		virtual std::unique_ptr<Node> Lookup(Mount const* mount, char_t const* name) = 0;

		// Open
		//
		// Opens or creates a child in this directory by name
		//virtual std::unique_ptr<Handle> Open(Mount const* mount, char_t const* name, .... blah blah

		// Unlink
		//
		// Unlinks a child node from this directory by name
		virtual void Unlink(Mount const* mount, char_t const* name) = 0;
	};

	// File
	//
	// Interface that must be implemented by a file object
	struct File : public Node
	{
		// Destructor
		//
		virtual ~File()=default;
	};

	// SymbolicLink
	//
	// Interface that must be implemented by a symbolic link object
	struct SymbolicLink : public Node
	{
		// Destructor
		//
		virtual ~SymbolicLink()=default;

		//-------------------------------------------------------------------
		// Member Functions

		// ReadTarget
		//
		// Reads the value of the symbolic link
		virtual size_t ReadTarget(Mount const* mount, char_t* buffer, size_t count) = 0;

		//-------------------------------------------------------------------
		// Properties

		// Length
		//
		// Gets the length of the target string, in characters
		__declspec(property(get=getLength)) size_t Length;
		virtual size_t getLength(void) const = 0;
	};

	//-----------------------------------------------------------------------
	// Member Functions

	// LogMessage
	//
	// Variadic template function to write a message to the system log
	template <typename... _remaining>
	void LogMessage(LogLevel level, _remaining const&... remaining)
	{
		LogMessage(0, level, remaining...);
	}

	// LogMessage
	//
	// Variadic template function to write a message to the system log
	template <typename... _remaining>
	void LogMessage(uint8_t facility, LogLevel level, _remaining const&... remaining)
	{
		std::string message;
		ConstructLogMessage(message, remaining...);
		WriteSystemLogEntry(facility, level, message.data(), message.size());
	}

protected:

	// Instance Constructor
	//
	VirtualMachine()=default;

	//--------------------------------------------------------------------------
	// Protected Member Functions

	// WriteSystemLogEntry
	//
	// Writes an entry into the system log
	virtual void WriteSystemLogEntry(uint8_t facility, LogLevel level, char_t const* message, size_t length) = 0;

private:

	VirtualMachine(VirtualMachine const&)=delete;
	VirtualMachine& operator=(VirtualMachine const&)=delete;

	//--------------------------------------------------------------------------
	// Private Member Functions

	// ConstructLogMessage
	//
	// Intermediate variadic overload; concatenates remaining message arguments
	template <typename _first, typename... _remaining>
	void ConstructLogMessage(std::string& message, _first const& first, _remaining const&... remaining)
	{
		ConstructLogMessage(message += std::to_string(first), remaining...);
	}

	// ConstructLogMessage
	//
	// Final variadic overload of ConstructLogMessage
	void ConstructLogMessage(std::string& message)
	{
		UNREFERENCED_PARAMETER(message);
	}
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __VIRTUALMACHINE_H_
