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

	struct __declspec(novtable) FileSystem;
	struct __declspec(novtable) Handle;
	struct __declspec(novtable) Mount;
	struct __declspec(novtable) Node;

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

	// CloneFlags (bitmask)
	//
	// Clone operation flags 
	class CloneFlags final : public bitmask<CloneFlags, uint32_t>
	{
	public:

		using bitmask::bitmask;

		//-------------------------------------------------------------------------
		// Fields

		// None (static)
		//
		// Indicates no special clone flags
		static CloneFlags const None;

		// NewControlGroupNamespace (static)
		//
		// Creates a new control group namespace during clone operation
		static CloneFlags const NewControlGroupNamespace;

		// NewIpcNamespace (static)
		//
		// Creates a new IPC namespace during clone operation
		static CloneFlags const NewIpcNamespace;

		// NewMountNamespace (static)
		//
		// Creates a new mount namespace during clone operation
		static CloneFlags const NewMountNamespace;

		// NewNetworkNamespace (static)
		//
		// Creates a new network namespace during clone operation
		static CloneFlags const NewNetworkNamespace;

		// NewPidNamespace (static)
		//
		// Creates a new PID namespace during clone operation
		static CloneFlags const NewPidNamespace;

		// NewUserNamespace (static)
		//
		// Creates a new user namespace during clone operation
		static CloneFlags const NewUserNamespace;

		// NewUtsNamespace (static)
		//
		// Creates a new UTS namespace during clone operation
		static CloneFlags const NewUtsNamespace;
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

	// MountFlags
	//
	// Mount operation flags
	struct MountFlags final : public bitmask<MountFlags, uint32_t>
	{
		using bitmask::bitmask;

		//-------------------------------------------------------------------------
		// Fields

		// Bind (static)
		//
		// Perform a bind mount
		static MountFlags const Bind;

		// IncrementNodeVersions (static)
		//
		// Update the node version anytime it is modified
		static MountFlags const IncrementNodeVersions;

		// KernelMount (static)
		//
		// Indicates that this mount was performed by the kernel
		static MountFlags const KernelMount;

		// LazyTimeStamps (static)
		//
		// Reduce on-disk updates of inode timestamps 
		static MountFlags const LazyTimeStamps;

		// MandatoryLocks (static)
		//
		// Permit mandatory locking on files in this filesystem
		static MountFlags const MandatoryLocks;

		// Move (static)
		//
		// Move an existing mount to a new location
		static MountFlags const Move;

		// NoDevices (static)
		//
		// Do not interpret character or block special devices 
		static MountFlags const NoDevices;

		// NoSetUserId (static)
		//
		// Do not honor set-user-ID and set-group-ID bits
		static MountFlags const NoSetUserId;

		// NoExecute (static)
		//
		// Do not allow programs to be executed from this filesystem
		static MountFlags const NoExecute;

		// NoUpdateAccessTimeStamps (static)
		//
		// Do not update access times for (all types of) files  
		static MountFlags const NoUpdateAccessTimeStamps;

		// NoUpdateDirectoryAccessTimeStamps (static)
		//
		// Do not update access times for directories on this filesystem
		static MountFlags const NoUpdateDirectoryAccessTimeStamps;

		// PosixAccessControlLists (static)
		//
		// Support POSIX Access Control Lists 
		static MountFlags const PosixAccessControlLists;

		// Private (static)
		//
		// Make this mount point private
		static MountFlags const Private;

		// ReadOnly (static)
		//
		// Mount the file system as read-only
		static MountFlags const ReadOnly;

		// RecursiveBind (static)
		//
		// Perform a recusrive bind mount
		static MountFlags const RecursiveBind;

		// RelativeAccessTimeStamps (static)
		//
		// Use relative access times
		static MountFlags const RelativeAccessTimeStamps;

		// Remount (static)
		//
		// Remount an existing mount
		static MountFlags const Remount;

		// Slave (static)
		//
		// Convert into a slave mount
		static MountFlags const Slave;

		// Shared (static)
		//
		// Make this mount point shared
		static MountFlags const Shared;

		// Silent (static)
		//
		// Suppress the display of certain warning messages 
		static MountFlags const Silent;

		// StrictAccessTimeStamps (static)
		//
		// Always update the last access time (atime)
		static MountFlags const StrictAccessTimeStamps;

		// Synchronous (static)
		//
		// Make writes on this filesystem synchronous
		static MountFlags const Synchronous;

		// SynchronousDirectories (static)
		//
		// Make directory changes on this filesystem synchronous
		static MountFlags const SynchronousDirectories;

		// Unbindable (static)
		//
		// Make this mount unbindable
		static MountFlags const Unbindable;
	};

	// MountFunction
	//
	// Function signature for a file system's Mount() implementation, which must be a public static method
	using MountFunction = std::function<struct FileSystem*(const char_t* source, uint32_t flags, const void* data, size_t datalength)>;

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

	// UnmountFlags
	//
	// Unmount operation flags
	struct UnmountFlags final : public bitmask<UnmountFlags, uint32_t>
	{
		using bitmask::bitmask;

		//-------------------------------------------------------------------------
		// Fields

		// Detach
		//
		// Perform a lazy unmount -- see umount2(2)
		static UnmountFlags const Detach;

		// Expire
		//
		// Mark the mount point as expired -- see umount2(2)
		static UnmountFlags const Expire;

		// Force
		//
		// Forces an unmount even if the file system is busy
		static UnmountFlags const Force;
		
		// NoFollow
		//
		// Indicates that if the target is symbolic link it is not to be followed
		static UnmountFlags const NoFollow;
	};

	//
	// INTERFACES
	//

	// FileSystem
	//
	// Interface that must be implemented by a file system
	struct __declspec(novtable) FileSystem
	{
	};

	// Mount
	//
	// Interface that must be implemented by a file system mount
	struct __declspec(novtable) Mount
	{
	};

	// Handle
	//
	// Interface that must be implemented by a file system handle
	struct __declspec(novtable) Handle
	{
	};

	// Node
	//
	// Interface that must be implemented by a file system node
	struct __declspec(novtable) Node
	{
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
