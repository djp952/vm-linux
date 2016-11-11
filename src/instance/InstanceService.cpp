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
// InstanceService::ExtractInitialRamFileSystem
//
// Extracts the contents of a CPIO archive file into a destination directory
//
// Arguments:
//
//	destination		- Destination directory Path instance
//	cpioarchive		- Path to the CPIO archive to be extracted
//	initramfs		- Path to the initramfs file to be loaded

void InstanceService::ExtractInitialRamFileSystem(Namespace const* ns, Namespace::Path const* destination, std::tstring const& cpioarchive)
{
	std::map<uint32_t, std::string>		links;				// Collection of established node links

	if(ns == nullptr) throw LinuxException(UAPI_EFAULT);
	if(destination == nullptr) throw LinuxException(UAPI_EFAULT);

	// WriteFileNode (local)
	//
	// Writes the contents of a CpioFile data stream into a file system File node instance
	auto WriteFileNode = [](VirtualMachine::Mount const* mount, VirtualMachine::File* node, CpioFile const& file) -> void 
	{ 
		std::vector<uint8_t> buffer(SystemInformation::PageSize << 2);

		// Write all of the data from the CpioFile data stream into the destination node
		auto handle = node->CreateFileHandle(mount, UAPI_O_WRONLY);
		handle->SetLength(file.Data.Length);
		while(auto read = file.Data.Read(&buffer[0], SystemInformation::PageSize << 2)) handle->Write(&buffer[0], read);

		// Update the modification time of the file to match what was specified in the CPIO archive
		node->SetModificationTime(mount, uapi_timespec{ static_cast<uapi___kernel_time_t>(file.ModificationTime), 0 });
	};

	LogMessage(VirtualMachine::LogLevel::Informational, TEXT("Extracting initramfs archive "), cpioarchive.c_str());

	// The CPIO archive may be compressed via a variety of different mechanisms; wrap in a CompressedStreamReader
	CpioArchive::EnumerateFiles(CompressedFileReader(cpioarchive.c_str()), [&](CpioFile const& file) -> void {

		// Convert the file path into a posix_path to access the branch and leaf separately
		posix_path filepath(file.Path);

		// SPECIAL CASE: "."
		//
		// If a . entry was specified in the CPIO archive, apply the metadata to the current directory
		if(strcmp(filepath.leaf(), ".") == 0) {

			// The node pointed to by path needs to be duplicated in order to get write access
			destination->Node->SetMode(destination->Mount, file.Mode);
			destination->Node->SetUserId(destination->Mount, file.UserId);
			destination->Node->SetGroupId(destination->Mount, file.GroupId);
			destination->Node->SetModificationTime(destination->Mount, uapi_timespec{ static_cast<uapi___kernel_time_t>(file.ModificationTime), 0 });

			return;
		}

		// Indicate the node being processed as an informational log message
		std::tstringstream message;
		message << std::setw(6) << std::setfill(TEXT(' ')) << std::oct << file.Mode << TEXT(" ") << file.Path;
		LogMessage(VirtualMachine::LogLevel::Informational, message);
		
		// Acquire a pointer to the branch path directory node, which must already exist
		auto branchpath = ns->LookupPath(destination, filepath.branch(), UAPI_O_DIRECTORY);

		// Get a pointer to the Directory instance pointed to by the branch path
		VirtualMachine::Directory* branchdir = dynamic_cast<VirtualMachine::Directory*>(branchpath->Node);
		if(branchdir == nullptr) throw LinuxException(UAPI_ENOTDIR);

		// Try to unlink any existing node with the same name in the destination directory
		try { branchdir->Unlink(branchpath->Mount, filepath.leaf()); }
		catch(LinuxException const& ex) { if(ex.Code != UAPI_ENOENT) throw; }

		// S_IFREG - Create a regular file node or hard link in the target directory
		//
		if((file.Mode & UAPI_S_IFMT) == UAPI_S_IFREG) {

			// If a file node with the same inode number has already been created, generate a hard
			// link to the existing node rather than creating a new standalone regular file node
			auto link = links.find(file.INode);
			if(link != links.end()) {

				// Create the hard link, overwrite any existing file data, and touch the modification time
				auto existing = ns->LookupPath(destination, link->second.c_str(), UAPI_O_NOFOLLOW);
				// todo: throw if existing is not a file
				branchdir->Link(existing->Mount, existing->Node, filepath.leaf());
				WriteFileNode(existing->Mount, dynamic_cast<VirtualMachine::File*>(existing->Node), file);
			}
				
			else {
				
				// Create a new regular file node as a child of the branch directory, write it, and then touch the modification time
				auto node = branchdir->CreateFile(branchpath->Mount, filepath.leaf(), file.Mode, file.UserId, file.GroupId);
				WriteFileNode(branchpath->Mount, dynamic_cast<VirtualMachine::File*>(node.get()), file);
			}
		}

		// S_IFDIR - Create a directory node in the target directory
		//
		else if((file.Mode & UAPI_S_IFMT) == UAPI_S_IFDIR) {

			// Create a new directory node as a child of the branch directory and touch the modification time
			auto node = branchdir->CreateDirectory(branchpath->Mount, filepath.leaf(), file.Mode, file.UserId, file.GroupId);
			node->SetModificationTime(branchpath->Mount, uapi_timespec{ static_cast<uapi___kernel_time_t>(file.ModificationTime), 0 });
		}

		// S_IFLNK - Create a symbolic link node in the target directory
		//
		else if((file.Mode & UAPI_S_IFMT) == UAPI_S_IFLNK) {

			// Convert the file data into a null terminated string to serve as the link target
			auto target = std::make_unique<char_t[]>(file.Data.Length + 1);
			file.Data.Read(target.get(), file.Data.Length);
			target[file.Data.Length] = '\0';

			// Create a new symbolic link node as a child of the branch directory and touch the modification time
			auto node = branchdir->CreateSymbolicLink(branchpath->Mount, filepath.leaf(), target.get(), file.UserId, file.GroupId);
			node->SetModificationTime(branchpath->Mount, uapi_timespec{ static_cast<uapi___kernel_time_t>(file.ModificationTime), 0 });
		}

		// Store at least one valid path for every inode number that was created for making hard links
		links.emplace(file.INode, file.Path);
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

		std::unique_ptr<VirtualMachine::Mount> rootmount;

		try {

			// If an initramfs archive has been specified, the root file system is always TempFileSystem
			if(param_initrd) rootmount = MountTempFileSystem("rootfs", UAPI_MS_KERNMOUNT | UAPI_MS_SILENT, nullptr, 0);

			else {

				// Find the file system mount function in the collection
				auto const& rootfsentry = m_fstypes.find(param_rootfstype);
				if(rootfsentry == m_fstypes.end()) throw FileSystemTypeNotFoundException(static_cast<std::tstring>(param_rootfstype).c_str());

				// Attempt to create/mount the specified file system using the specified source and flags
				uint32_t rootflags = UAPI_MS_KERNMOUNT | ((param_ro) ? UAPI_MS_RDONLY : 0);
				std::string rootflagsstr = std::to_string(param_rootflags);
				rootmount = rootfsentry->second(std::to_string(param_root).c_str(), rootflags , rootflagsstr.data(), rootflagsstr.length());
			}
		}

		catch(std::exception& ex) { throw MountRootFileSystemException(ex.what()); }

		//
		// INITIALIZE ROOT NAMESPACE
		//

		try { m_rootns = std::make_unique<Namespace>(std::move(rootmount)); }
		catch(std::exception& ex) { throw CreateRootNamespaceException(ex.what()); }

		// Get a pointer to the namespace root path for lookups
		auto rootpath = m_rootns->GetRootPath();

		//
		// EXTRACT INITRAMFS ARCHIVE INTO ROOT FILE SYSTEM
		//

		if(param_initrd) {

			std::tstring initrd = param_initrd;				// Pull out the param_initrd string

			// Attempt to extract the contents of the initramfs archive into the root file system
			try { ExtractInitialRamFileSystem(m_rootns.get(), rootpath.get(), initrd); }
			catch(std::exception& ex) { throw InitialRamFileSystemException(initrd.c_str(), ex.what()); }
		}

		//
		// LAUNCH INIT PROCESS
		//

		// The path to the init executable is always simply "init" when an initramfs archive has been used
		std::string initpath = (param_initrd) ? "init" : std::to_string(param_init);

		try {

			auto initexe = std::make_unique<Executable>(m_rootns->LookupPath(rootpath.get(), initpath.c_str(), 0));
			//auto initproc = std::make_unique<Process>(std::move(initexe));
		}

		catch(std::exception& ex) { throw LaunchInitException(initpath.c_str(), ex.what()); }
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
