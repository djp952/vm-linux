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
	struct File;
	struct FileSystem;
	struct Handle;
	struct Mount;
	struct Node;
	struct SymbolicLink;

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

	// NodeType
	//
	// Strogly typed enumeration for the S_IFxxx inode type constants
	enum class NodeType
	{
		BlockDevice			= UAPI_S_IFBLK,
		CharacterDevice		= UAPI_S_IFCHR,
		Directory			= UAPI_S_IFDIR,
		File				= UAPI_S_IFREG,
		Pipe				= UAPI_S_IFIFO,
		Socket				= UAPI_S_IFSOCK,
		SymbolicLink		= UAPI_S_IFLNK,
	};

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

	// Handle
	//
	// Interface that must be implemented by a file system handle
	struct Handle
	{
		// Destructor
		//
		virtual ~Handle()=default;

		// Duplicate
		//
		// Creates a duplicate Handle instance
		virtual std::unique_ptr<Handle> Duplicate(void) const = 0;

		// Read
		//
		// Synchronously reads data from the underlying node into a buffer
		virtual size_t Read(void* buffer, size_t count) = 0;

		// ReadAt
		//
		// Synchronously reads data from the underlying node into a buffer
		virtual size_t ReadAt(ssize_t offset, int whence, void* buffer, size_t count) = 0;

		// Seek
		//
		// Changes the file position
		virtual size_t Seek(ssize_t offset, int whence) = 0;

		// SetLength
		//
		// Sets the length of the file
		virtual size_t SetLength(size_t length) = 0;

		// Sync
		//
		// Synchronizes all metadata and data associated with the file to storage
		virtual void Sync(void) const = 0;

		// SyncData
		//
		// Synchronizes all data associated with the file to storage, not metadata
		virtual void SyncData(void) const = 0;

		// Write
		//
		// Synchronously writes data from a buffer to the underlying node
		virtual size_t Write(const void* buffer, size_t count) = 0;

		// WriteAt
		//
		// Synchronously writes data from a buffer to the underlying node
		virtual size_t WriteAt(ssize_t offset, int whence, const void* buffer, size_t count) = 0;

		// Flags
		//
		// Gets the handle flags
		__declspec(property(get=getFlags)) uint32_t Flags;
		virtual uint32_t getFlags(void) const = 0;

		// Position
		//
		// Gets the current file position for this handle
		__declspec(property(get=getPosition)) size_t Position;
		virtual size_t getPosition(void) const = 0;
	};

	// Mount
	//
	// Interface that must be implemented by a file system mount
	struct Mount
	{
		// Destructor
		//
		virtual ~Mount()=default;

		// Duplicate
		//
		// Duplicates the Mount instance
		virtual std::unique_ptr<Mount> Duplicate(void) const = 0;

		// GetRootNode
		//
		// Gets a pointer to the mount point root node instance
		virtual std::unique_ptr<Node> GetRootNode(void) const = 0;

		// FileSystem
		//
		// Accesses the underlying file system instance
		__declspec(property(get=getFileSystem)) struct FileSystem* FileSystem;
		virtual  struct FileSystem* getFileSystem(void) const = 0;

		// Flags
		//
		// Gets the mount point flags
		__declspec(property(get=getFlags)) uint32_t Flags;
		virtual uint32_t getFlags(void) const = 0;
	};

	// Node
	//
	// Interface that must be implemented by a file system node
	struct Node
	{
		// Destructor
		//
		virtual ~Node()=default;

		// OpenHandle
		//
		// Opens a handle against this node
		virtual std::unique_ptr<struct Handle> OpenHandle(Mount const* mount, uint32_t flags) = 0;

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

		// Permissions
		//
		// Gets the permissions mask assigned to this node
		__declspec(property(get=getPermissions)) uapi_mode_t Permissions;
		virtual uapi_mode_t getPermissions(void) const = 0;

		// Type
		//
		// Gets the type of file system node being represented
		__declspec(property(get=getType)) NodeType Type;
		virtual NodeType getType(void) const = 0;

		// UserId
		//
		// Gets the node owner user identifier 
		__declspec(property(get=getUserId)) uapi_uid_t UserId;
		virtual uapi_uid_t getUserId(void) const = 0;
	};

	// Directory
	//
	// Interface that must be implemented by a directory object
	struct Directory : public Node
	{
		// Destructor
		//
		virtual ~Directory()=default;

		// CreateDirectory
		//
		// Creates a new directory node as a child of this directory
		virtual std::unique_ptr<Directory> CreateDirectory(Mount const* mount, char_t const* name, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) = 0;

		// CreateFile
		//
		// Creates a new regular file node as a child of this directory
		virtual std::unique_ptr<File> CreateFile(Mount const* mount, char_t const* name, uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid) = 0;

		// Lookup
		//
		// Accesses a child node of this directory by name
		virtual std::unique_ptr<Node> Lookup(Mount const* mount, char_t const* name) const = 0;
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

		// Target
		//
		// Exposes the symbolic link target
		__declspec(property(get=getTarget)) char_t const* Target;
		virtual char_t const* getTarget(void) const = 0;
	};

	//-------------------------------------------------------------------------
	// Member Functions

	// LogMessage
	//
	// Variadic template function to write a message to the system log
	template<typename... _remaining>
	void LogMessage(LogLevel level, _remaining const&... remaining)
	{
		LogMessage(0, level, remaining...);
	}

	// LogMessage
	//
	// Variadic template function to write a message to the system log
	template<typename... _remaining>
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
	template<typename _first, typename... _remaining>
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
