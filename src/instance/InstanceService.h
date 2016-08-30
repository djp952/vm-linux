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

#ifndef __INSTANCESERVICE_H_
#define __INSTANCESERVICE_H_
#pragma once

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>
#include <Parameter.h>

#include "VirtualMachine.h"

#pragma warning(push, 4)

// FORWARD DECLARATIONS
//
class Namespace;
class Process;
class RpcObject;
class SystemLog;

// PARAMETER_MAP
//
// Helper macros used to simplify the declaration of the parameters collection
#define BEGIN_PARAMETER_MAP(__mvar) std::map<std::tstring, ParameterBase*> __mvar = {
#define PARAMETER_ENTRY(__name, __paramvar) std::make_pair(std::tstring(__name), &__paramvar),
#define END_PARAMETER_MAP() };

//-----------------------------------------------------------------------------
// Class InstanceService
//
// Implements a virtual machine instance as a service

class InstanceService : public Service<InstanceService>, public VirtualMachine
{
public:

	// Instance Constructor
	//
	InstanceService();

	// Destructor
	//
	virtual ~InstanceService()=default;

	//-------------------------------------------------------------------------
	// Member Functions

protected:

	//-------------------------------------------------------------------------
	// Protected Member Functions

	// WriteSystemLogEntry (VirtualMachine)
	//
	// Writes an entry into the system log
	virtual void WriteSystemLogEntry(uint8_t facility, VirtualMachine::LogLevel level, char_t const* message, size_t length);

private:

	InstanceService(InstanceService const&)=delete;
	InstanceService& operator=(InstanceService const&)=delete;

	// Service<> Control Handler Map
	//
	BEGIN_CONTROL_HANDLER_MAP(InstanceService)
		CONTROL_HANDLER_ENTRY(SERVICE_CONTROL_STOP, OnStop)
	END_CONTROL_HANDLER_MAP()

	// Parameter Map
	//
	BEGIN_PARAMETER_MAP(m_params)
		PARAMETER_ENTRY(TEXT("log_buf_len"), param_log_buf_len)
		PARAMETER_ENTRY(TEXT("loglevel"), param_loglevel)
		PARAMETER_ENTRY(TEXT("ro"), param_ro)
		PARAMETER_ENTRY(TEXT("root"), param_root)
		PARAMETER_ENTRY(TEXT("rootflags"), param_rootflags)
		PARAMETER_ENTRY(TEXT("rootfstype"), param_rootfstype)
		PARAMETER_ENTRY(TEXT("rw"), param_rw)
	END_PARAMETER_MAP()

	// filesystem_map_t
	//
	// Collection of available file systems (name, mount function)
	using filesystem_map_t = std::unordered_map<std::tstring, VirtualMachine::CreateFileSystem>;

	// ProcFileSystem
	//
	// Implements the procfs file system
	class ProcFileSystem : public VirtualMachine::FileSystem
	{
	public:

		// Instance Constructor
		//
		ProcFileSystem()=default;

		// Destructor
		//
		~ProcFileSystem()=default;

	private:

		ProcFileSystem(ProcFileSystem const& rhs)=delete;
		ProcFileSystem& operator=(ProcFileSystem const&)=delete;
	};

	//-------------------------------------------------------------------------
	// Private Member Functions

	// CreateProcFileSystem (static)
	//
	// Creates an instance of the procfs file system for this instance
	static std::unique_ptr<VirtualMachine::FileSystem> CreateProcFileSystem(char_t const* source, VirtualMachine::MountFlags flags, void const* data, size_t datalength);

	// OnStart (Service)
	//
	// Invoked when the service is started
	virtual void OnStart(int argc, LPTSTR* argv);

	// OnStop
	//
	// Invoked when the service is stopped
	void OnStop(void);

	//-------------------------------------------------------------------------
	// Member Variables

	std::unique_ptr<SystemLog>		m_syslog;			// SystemLog instance
	std::unique_ptr<Namespace>		m_rootns;			// Root Namespace instance
	HANDLE							m_job;				// Process job object
	std::unique_ptr<Process>		m_initprocess;		// Init process instance
	
	// File System
	//
	filesystem_map_t							m_filesystems;	// Available file systems
	std::unique_ptr<VirtualMachine::FileSystem>	m_rootfs;		// Root file system

	// RPC System Call Objects
	//
	std::unique_ptr<RpcObject>		m_syscalls_x86;		// 32-bit system call interface
#ifdef _M_X64
	std::unique_ptr<RpcObject>		m_syscalls_x64;		// 64-bit system call interface
#endif

	// Parameters
	//
	Parameter<size_t>					param_log_buf_len	= 2 MiB;
	Parameter<VirtualMachine::LogLevel>	param_loglevel		= VirtualMachine::LogLevel::Warning;
	Parameter<void>						param_ro;
	Parameter<std::tstring>				param_root;
	Parameter<std::tstring>				param_rootflags;
	Parameter<std::tstring>				param_rootfstype	= TEXT("rootfs");
	Parameter<void>						param_rw;
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __INSTANCESERVICE_H_
