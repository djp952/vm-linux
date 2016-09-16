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
	struct Path;
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
	};

	// Mount
	//
	// Interface that must be implemented by a file system mount
	struct Mount
	{
		// Destructor
		//
		virtual ~Mount()=default;
	};

	// Node
	//
	// Interface that must be implemented by a file system node
	struct Node
	{
		// Destructor
		//
		virtual ~Node()=default;

		// OwnerGroupId
		//
		// Gets the node owner group identifier
		__declspec(property(get=getOwnerGroupId)) uapi_gid_t OwnerGroupId;
		virtual uapi_gid_t getOwnerGroupId(void) const = 0;

		// OwnerUserId
		//
		// Gets the node owner user identifier 
		__declspec(property(get=getOwnerUserId)) uapi_uid_t OwnerUserId;
		virtual uapi_uid_t getOwnerUserId(void) const = 0;

		// Permissions
		//
		// Gets the node permissions mask
		__declspec(property(get=getPermissions)) uapi_mode_t Permissions;
		virtual uapi_mode_t getPermissions(void) const = 0;
	};

	// Directory
	//
	// Interface that must be implemented by a directory object
	struct Directory : public Node
	{
		// Destructor
		//
		virtual ~Directory()=default;
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
	};

	// Path
	//
	// Interface that must be implemented by a path object
	struct Path
	{
		// Destructor
		//
		virtual ~Path()=default;

		// Duplicate
		//
		// Duplicates this Path instance
		virtual std::unique_ptr<Path> Duplicate(void) = 0;
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
