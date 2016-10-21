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
#include <convert.h>
#include <iomanip>
#include <messages.h>
#include <path.h>

#include <Exception.h>
#include <RpcObject.h>
#include <Win32Exception.h>

#include "CompressedFileReader.h"
#include "CpioArchive.h"
#include "Executable.h"
#include "HostFileSystem.h"
#include "LinuxException.h"
#include "Process.h"
#include "RootFileSystem.h"
#include "SystemInformation.h"
#include "SystemLog.h"
#include "TempFileSystem.h"

#pragma warning(push, 4)

//---------------------------------------------------------------------------
// MountProcFileSystem
//
// Creates an instance of InstanceService::ProcFileSystem file system
//
// Arguments:
//
//	source		- Source device string
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

std::unique_ptr<VirtualMachine::Mount> MountProcFileSystem(char_t const* source, uint32_t flags, void const* data, size_t datalength)
{
	UNREFERENCED_PARAMETER(source);
	UNREFERENCED_PARAMETER(flags);
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(datalength);

	return nullptr;
}

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
// InstanceService::LoadInitialRamFileSystem
//
// Loads the contents of an initramfs file into the root file system
//
// Arguments:
//
//	current			- Current working directory Path instance
//	initramfs		- Path to the initramfs file to be loaded

void InstanceService::LoadInitialRamFileSystem(Namespace const* ns, Namespace::Path const* current, std::tstring const& initramfs)
{
	if(ns == nullptr) throw LinuxException(UAPI_EFAULT);
	if(current == nullptr) throw LinuxException(UAPI_EFAULT);

	// sets a node's attributes after it's been created from the CPIO archive
	auto SetNodeAttributes = [](VirtualMachine::Mount const* mount, VirtualMachine::Node* node, CpioFile const& file) -> void 
	{ 
		node->SetMode(mount, file.Mode);
		node->SetUserId(mount, file.UserId);
		node->SetGroupId(mount, file.GroupId);
		node->SetModificationTime(mount, uapi_timespec{ static_cast<uapi___kernel_time_t>(file.ModificationTime), 0 });
	};

	LogMessage(VirtualMachine::LogLevel::Informational, TEXT("Extracting initramfs archive "), initramfs.c_str());

	//
	// TODO: hard links.  Check how CPIO stores them, I'm guessing multiple entries will have the same
	// inode number and report NumLinks > 1.  Perhaps need to use a map<> that holds the enumerated inode
	// numbers and the node that was created for it so it won't be created twice
	//

	// initramfs is stored in a CPIO archive that may be compressed via a variety of different
	// mechanisms. Wrap the file itself in a generic CompressedStreamReader to handle that
	CpioArchive::EnumerateFiles(CompressedFileReader(initramfs.c_str()), [=](CpioFile const& file) -> void {

		std::tstringstream message;						// Log message stream instance
		posix_path filepath(file.Path);					// Convert the path into a posix_path

		// Indicate the node being processed as an informational log message
		message << std::setw(6) << std::setfill(TEXT(' ')) << std::oct << file.Mode << TEXT(" ") << file.Path;
		LogMessage(VirtualMachine::LogLevel::Informational, message);
		
		// initramfs does not automatically create branch directory objects, they have to exist
		auto branchpath = ns->LookupPath(current, filepath.branch(), UAPI_O_DIRECTORY);

		// Get a pointer to the Directory instance pointed to by the branch path
		VirtualMachine::Directory* branchdir = dynamic_cast<VirtualMachine::Directory*>(branchpath->Node);
		if(branchdir == nullptr) throw LinuxException(UAPI_ENOTDIR);

		// S_IFDIR - Create and/or replace the target directory
		if((file.Mode & UAPI_S_IFMT) == UAPI_S_IFDIR) {

			// Attempt to create the target directory node object in the file system
			auto node = branchdir->CreateDirectory(branchpath->Mount, filepath.leaf(), UAPI_O_CREAT, file.Mode, file.UserId, file.GroupId);
			SetNodeAttributes(branchpath->Mount, node.get(), file);
		}

		// S_IFREG - Create and/or replace the target file
		else if((file.Mode & UAPI_S_IFMT) == UAPI_S_IFREG) {

			// Attempt to create the target file node object in the file system and open a handle against it
			auto node = branchdir->CreateFile(branchpath->Mount, filepath.leaf(), UAPI_O_CREAT, file.Mode, file.UserId, file.GroupId);

			// The file system can work more efficiently if the file size is set in advance
			node->SetLength(branchpath->Mount, file.Data.Length);

			// Extract the data from the CPIO archive into the target file instance
			size_t offset = 0;
			std::vector<uint8_t> buffer(SystemInformation::PageSize << 2);

			auto read = file.Data.Read(&buffer[0], SystemInformation::PageSize << 2);
			while(read) {

				offset += node->Write(branchpath->Mount, offset, &buffer[0], read);
				read = file.Data.Read(&buffer[0], SystemInformation::PageSize << 2);
			}

			// Set the node's attributes to match those speciifed in the CPIO archive
			SetNodeAttributes(branchpath->Mount, node.get(), file);
			node->Sync(branchpath->Mount);
		}

		// S_IFLNK - Create and/or replace the target symbolic link
		else if((file.Mode & UAPI_S_IFMT) == UAPI_S_IFLNK) {

			// Convert the file data into a null terminated C-style string
			auto target = std::make_unique<char_t[]>(file.Data.Length + 1);
			file.Data.Read(target.get(), file.Data.Length);
			target[file.Data.Length] = '\0';

			// Attempt to create the target symbolic link node object in the file system
			auto node = branchdir->CreateSymbolicLink(branchpath->Mount, filepath.leaf(), target.get(), file.UserId, file.GroupId);
			SetNodeAttributes(branchpath->Mount, node.get(), file);
			node->Sync(branchpath->Mount);
		}
	});
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
	std::vector<tchar_t const*>			initargs;		// Arguments passed to init
	std::vector<tchar_t const*>			initenv;		// Environment variables passed to init
	std::vector<tchar_t const*>			invalidargs;	// Invalid parameter arguments

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
		try { m_syslog = std::make_unique<SystemLog>(param_log_buf_len, param_loglevel); }
		catch(std::exception& ex) { throw CreateSystemLogException(ex.what()); }

		// Dump the arguments that couldn't be parsed as warnings into the system log
		for(auto p : invalidargs) LogMessage(VirtualMachine::LogLevel::Warning, TEXT("Failed to parse parameter: "), p);

		//
		// INITIALIZE JOB OBJECT
		//

		m_job = CreateJobObject(nullptr, nullptr);
		if(m_job == nullptr) throw CreateJobObjectException(GetLastError(), Win32Exception(GetLastError()));

		//
		// INITIALIZE FILE SYSTEM TYPES
		//

		m_fstypes.emplace(TEXT("hostfs"), MountHostFileSystem);
		m_fstypes.emplace(TEXT("procfs"), MountProcFileSystem);
		m_fstypes.emplace(TEXT("rootfs"), MountRootFileSystem);
		m_fstypes.emplace(TEXT("tmpfs"), MountTempFileSystem);

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

		// Attempt to create the root file system instance using the specified parameters
		std::unique_ptr<VirtualMachine::Mount> rootmount;
		try { rootmount = rootfsentry->second(std::to_string(param_root).c_str(), rootfsflags, rootflagsstr.data(), rootflagsstr.length()); }
		catch(std::exception& ex) { throw MountRootFileSystemException(ex.what()); }

		//
		// INITIALIZE ROOT NAMESPACE
		//

		try { m_rootns = std::make_unique<Namespace>(std::move(rootmount)); }
		catch(std::exception& ex) { throw CreateRootNamespaceException(ex.what()); }

		// Get a pointer to the namespace root path for lookups
		auto rootpath = m_rootns->GetRootPath();

		//
		// EXTRACT INITRAMFS INTO ROOT FILE SYSTEM
		//

		if(param_initrd) {

			std::tstring initrd = param_initrd;				// Pull out the param_initrd string

			// Attempt to extract the contents of the initramfs archive into the root file system
			try { LoadInitialRamFileSystem(m_rootns.get(), rootpath.get(), initrd); }
			catch(std::exception& ex) { throw InitialRamFileSystemException(initrd.c_str(), ex.what()); }
		}

		//
		// LAUNCH INIT PROCESS
		//

		std::string init = std::to_string(param_init);				// Pull out the param_init string

		try {
			
			// Look up the path to the init binary and attempt to open an execute handle against it
			auto initpath = m_rootns->LookupPath(rootpath.get(), init.c_str(), 0);
			//auto exechandle = initpath->Node->OpenExecute(...);

			//auto initexe = std::make_unique<Executable>(TEXT("D:\\Linux Stuff\\android-5.0.2_r1-x86\\root\\init"));
			//m_initprocess = std::make_unique<Process>();
			//m_initprocess->Load(initexe.get());
		}

		catch(std::exception& ex) { throw LaunchInitException(init.c_str(), ex.what()); }
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
