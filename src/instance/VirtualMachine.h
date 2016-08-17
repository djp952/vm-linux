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
