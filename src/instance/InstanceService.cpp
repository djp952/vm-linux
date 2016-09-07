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

#include "stdafx.h"
#include "InstanceService.h"

#include <algorithm>
#include <messages.h>

#include <Exception.h>
#include <RpcObject.h>
#include <Win32Exception.h>

#include "Executable.h"
#include "HostFile.h"
#include "HostFileSystem.h"
#include "Namespace.h"
#include "Process.h"
#include "RootFileSystem.h"
#include "SystemLog.h"
#include "TempFileSystem.h"

#pragma warning(push, 4)

//---------------------------------------------------------------------------
// InstanceService Constructor
//
// Arguments:
//
//	NONE

InstanceService::InstanceService()
{
}

//---------------------------------------------------------------------------
// InstanceService::CreateProcFileSystem (static)
//
// Creates an instance of the ProcFileSystem file system
//
// Arguments:
//
//	source		- Source device string
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

std::unique_ptr<VirtualMachine::FileSystem> InstanceService::CreateProcFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength)
{
	UNREFERENCED_PARAMETER(source);
	UNREFERENCED_PARAMETER(flags);
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(datalength);

	return nullptr;
}

//---------------------------------------------------------------------------
// InstanceService::OnStart (private)
//
// Invoked when the service is started
//
// Arguments:
//
//	argc		- Number of command line arguments
//	argv		- Array of command line argument strings

void InstanceService::OnStart(int argc, LPTSTR* argv)
{
	std::vector<tchar_t const*>				initargs;		// Arguments passed to init
	std::vector<tchar_t const*>				initenv;		// Environment variables passed to init
	std::vector<tchar_t const*>				invalidargs;	// Invalid parameter arguments
	std::unique_ptr<VirtualMachine::Path>	rootpath;		// Root Path instance

	try {

		//
		// PROCESS COMMAND LINE ARGUMENTS
		//

		// argv[0] is the service name; start at argv[1]
		int index = 1;

		// Attempt to process all command line arguments as parameters
		while(index < argc) {

			// Convert the entire argument into a tstring for processing
			auto arg = std::tstring(argv[index]);

			// An argument of "--" indicates that the remaining arguments are passed to init
			if(arg.compare(TEXT("--")) == 0) { index++; break; }

			// Split the argument into a key/value pair, use a blank string as the value if one
			// was not specified as part of the argument
			auto equalsign = arg.find(TEXT('='));
			auto key = arg.substr(0, equalsign);
			auto value = (equalsign == std::tstring::npos) ? std::tstring() : arg.substr(equalsign + 1);

			// Convert the key to lower case and replace all hyphens with underscores
			key = std::tolower(key);
			std::replace(key.begin(), key.end(), TEXT('-'), TEXT('_'));

			// Check if the parameter exists in the collection and attempt to parse it
			auto found = m_params.find(key);
			if(found != m_params.end()) {

				// If the parameter fails to parse, keep track of it for when the system log is running
				if(!found->second->TryParse(value)) invalidargs.push_back(argv[index]);
			}

			else {
			
				// The key does not represent a known parameter; if the argument is in the format key=value it
				// gets passed as an environment variable, otherwise it gets passed as a command line argument
				if(value.length() > 0) initenv.push_back(argv[index]);
				else initargs.push_back(argv[index]);
			}

			index++;
		}

		// Any remaining arguments not processed above are passed into init
		while(index < argc) { initargs.push_back(argv[index++]); }

		//
		// INITIALIZE SYSTEM LOG
		//

		// Create the system log instance, enforce a minimum size of 128KiB
		param_log_buf_len = std::max<size_t>(128 KiB, param_log_buf_len);
		try { m_syslog = std::make_unique<SystemLog>(param_log_buf_len); }
		catch(std::exception& ex) { throw CreateSystemLogException(ex.what()); }

		// Dump the arguments that couldn't be parsed as warnings into the system log
		for(auto p : invalidargs) LogMessage(VirtualMachine::LogLevel::Warning, TEXT("Failed to parse parameter: "), p);

		//
		// INITIALIZE JOB OBJECT
		//

		m_job = CreateJobObject(nullptr, nullptr);
		if(m_job == nullptr) throw CreateJobObjectException(GetLastError(), Win32Exception(GetLastError()));

		//
		// INITIALIZE ROOT NAMESPACE
		//

		try { m_rootns = std::make_unique<Namespace>(); }
		catch(std::exception& ex) { throw CreateRootNamespaceException(ex.what()); }

		//
		// INITIALIZE FILE SYSTEM TYPES
		//

		m_fstypes.emplace(TEXT("hostfs"), CreateHostFileSystem);
		m_fstypes.emplace(TEXT("procfs"), CreateProcFileSystem);
		m_fstypes.emplace(TEXT("rootfs"), CreateRootFileSystem);
		m_fstypes.emplace(TEXT("tmpfs"), CreateTempFileSystem);

		//
		// REGISTER SYSTEM CALL INTERFACES
		//

		m_syscalls_x86 = std::make_unique<RpcObject>(syscalls_x86_v1_0_s_ifspec, RPC_IF_AUTOLISTEN | RPC_IF_ALLOW_SECURE_ONLY);
#ifdef _M_X64
		m_syscalls_x64 = std::make_unique<RpcObject>(syscalls_x64_v1_0_s_ifspec, RPC_IF_AUTOLISTEN | RPC_IF_ALLOW_SECURE_ONLY);
#endif

		//
		// CREATE AND MOUNT ROOT FILE SYSTEM
		//

		// Find the file system mount function in the collection
		auto const& rootfsentry = m_fstypes.find(param_rootfstype);
		if(rootfsentry == m_fstypes.end()) throw FileSystemTypeNotFoundException(static_cast<std::tstring>(param_rootfstype).c_str());

		// Generate/convert the root file system flags
		uint32_t rootfsflags = UAPI_MS_KERNMOUNT | ((param_ro) ? UAPI_MS_RDONLY : 0);
		std::string rootflagsstr = std::to_string(param_rootflags);

		try {
			
			// Attempt to create the root file system instance using the specified parameters
			auto rootsource = std::to_string(param_root);
			auto rootfs = rootfsentry->second(rootsource.c_str(), rootfsflags, rootflagsstr.data(), rootflagsstr.length());

			// Mount the file system within the root namespace and set as an active file system if successful
			rootpath = m_rootns->MountFileSystem(nullptr, rootfs.get(), rootfsflags, rootflagsstr.data(), rootflagsstr.length());
			m_filesystems.emplace(rootsource, std::move(rootfs));
		}
		
		catch(std::exception& ex) { throw MountRootFileSystemException(ex.what()); }

		//
		// EXTRACT INITRAMFS INTO ROOT FILE SYSTEM
		//

		if(param_initrd) {

			// Verify that the specified file exists on the host system
			std::tstring initrd = param_initrd;
			if(!HostFile::Exists(initrd.c_str())) throw InitialRamFileSystemNotFoundException(initrd.c_str());

			// todo: process the CPIO archive here (pull in the code from the old project for reading CPIO)
		}

		//
		// LAUNCH INIT PROCESS
		//

		//auto initexe = std::make_unique<Executable>(TEXT("D:\\Linux Stuff\\android-5.0.2_r1-x86\\root\\init"));
		//m_initprocess = std::make_unique<Process>();
		//m_initprocess->Load(initexe.get());
	}

	catch(std::exception& ex) {

		PanicDuringInitializationException panic(ex.what());
		LogMessage(VirtualMachine::LogLevel::Emergency, panic.what());

		throw panic;
	}
}

//---------------------------------------------------------------------------
// InstanceService::OnStop (private)
//
// Invoked when the service is stopped
//
// Arguments:
//
//	NONE

void InstanceService::OnStop(void)
{
	// Forcibly terminate any remaining processes created by this instance
	TerminateJobObject(m_job, ERROR_PROCESS_ABORTED);
	CloseHandle(m_job);

	// Remove all active file systems
	m_filesystems.clear();

	// Revoke the system call interfaces
#ifdef _M_X64
	m_syscalls_x64.reset();
#endif
	m_syscalls_x86.reset();
}

//---------------------------------------------------------------------------
// InstanceService::WriteSystemLogEntry (protected)
//
// Writes an entry into the system log
//
// Arguments:
//
//	facility		- Facility code for the message
//	level			- LogLevel for the message
//	message			- ANSI/UTF-8 message to be written
//	length			- Length of the message, in bytes

void InstanceService::WriteSystemLogEntry(uint8_t facility, VirtualMachine::LogLevel level, char_t const* message, size_t length)
{
	if(m_syslog) m_syslog->WriteEntry(facility, level, message, length);
}

//---------------------------------------------------------------------------

#pragma warning(pop)
