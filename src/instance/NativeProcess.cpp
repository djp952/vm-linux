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
#include "NativeProcess.h"

#include <align.h>
#include <convert.h>
#include <NtApi.h>
#include <StructuredException.h>
#include <SystemInformation.h>
#include <Win32Exception.h>

#pragma warning(push, 4)

// SectionFlags
//
// Alias for ULONG, used with convert<> template
using SectionFlags = ULONG;

// SectionProtection
//
// Alias for ULONG, used with convert<> template
using SectionProtection = ULONG;

//-----------------------------------------------------------------------------
// Conversions
//-----------------------------------------------------------------------------

// VirtualMachine::AllocationFlags --> SectionFlags
//
// Converts a ProcessMemory::Protection value into section allocation type flags
template<> SectionFlags convert<SectionFlags>(VirtualMachine::AllocationFlags const& rhs)
{
	SectionFlags result = 0;

	// The only currently supported flag is MEM_TOP_DOWN
	if(rhs & (VirtualMachine::AllocationFlags::TopDown)) result |= MEM_TOP_DOWN;

	return result;
}

// VirtualMachine::ProtectionFlags --> SectionProtection
//
// Converts a ProcessMemory::Protection value into section protection flags
template<> SectionProtection convert<SectionProtection>(VirtualMachine::ProtectionFlags const& rhs)
{
	using prot = VirtualMachine::ProtectionFlags;	
	
	prot base(rhs & ~prot::Guard);
	SectionProtection result = PAGE_NOACCESS;

	if(base == prot::Execute)									result = PAGE_EXECUTE;
	else if(base == prot::Read)									result = PAGE_READONLY;
	else if(base == prot::Write)								result = PAGE_READWRITE;
	else if(base == (prot::Execute | prot::Read))				result = PAGE_EXECUTE_READ;
	else if(base == (prot::Execute | prot::Write))				result = PAGE_EXECUTE_READWRITE;
	else if(base == (prot::Read | prot::Write))					result = PAGE_READWRITE;
	else if(base == (prot::Execute | prot::Read | prot::Write))	result = PAGE_EXECUTE_READWRITE;

	return (rhs & prot::Guard) ? result |= PAGE_GUARD : result;
}

//---------------------------------------------------------------------------
// NativeProcess Constructor
//
// Arguments:
//
//	path		- Path to the native process binary file

NativeProcess::NativeProcess(tchar_t const* path) : NativeProcess(path, nullptr, nullptr, 0)
{
}

//---------------------------------------------------------------------------
// NativeProcess Constructor
//
// Arguments:
//
//	path		- Path to the native process binary file
//	arguments	- Command-line arguments to pass into the process

NativeProcess::NativeProcess(tchar_t const* path, tchar_t const* arguments) : NativeProcess(path, arguments, nullptr, 0)
{
}

//---------------------------------------------------------------------------
// NativeProcess Constructor
//
// Arguments:
//
//	path		- Path to the native process binary file
//	arguments	- Command-line arguments to pass into the process
//	handles		- Optional array of inheritable handle objects
//	numhandles	- Number of elements in the handles array

NativeProcess::NativeProcess(tchar_t const* path, tchar_t const* arguments, HANDLE handles[], size_t numhandles)
{
	memset(&m_procinfo, 0, sizeof(PROCESS_INFORMATION));

	// If a null argument string was provided, change it to an empty string
	if(arguments == nullptr) arguments = TEXT("");

	// Generate the command line for the child process, using the specifed path as argument zero
	tchar_t commandline[MAX_PATH];
	_sntprintf_s(commandline, MAX_PATH, MAX_PATH, _T("\"%s\"%s%s"), path, (arguments[0]) ? _T(" ") : _T(""), arguments);

	// Determine the size of the attributes buffer required to hold the inheritable handles property
	SIZE_T required = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &required);
	if(GetLastError() != ERROR_INSUFFICIENT_BUFFER) throw Win32Exception(GetLastError());

	// Allocate a buffer large enough to hold the attribute data and initialize it
	auto buffer = std::make_unique<uint8_t[]>(required);
	PPROC_THREAD_ATTRIBUTE_LIST attributes = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(&buffer[0]);
	if(!InitializeProcThreadAttributeList(attributes, 1, 0, &required)) throw Win32Exception(GetLastError());

	try {

		// UpdateProcThreadAttribute will fail if there are no handles in the specified array
		if((handles != nullptr) && (numhandles > 0)) {
			
			// Add the array of handles as inheritable handles for the client process
			if(!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles, numhandles * sizeof(HANDLE),
				nullptr, nullptr)) throw Win32Exception(GetLastError());
		}

		// Attempt to launch the process using the CREATE_SUSPENDED and EXTENDED_STARTUP_INFO_PRESENT flag
		STARTUPINFOEX startinfo;
		memset(&startinfo, 0, sizeof(STARTUPINFOEX));
		startinfo.StartupInfo.cb = sizeof(STARTUPINFOEX);
		startinfo.lpAttributeList = attributes;
		if(!CreateProcess(path, commandline, nullptr, nullptr, TRUE, CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr, 
			nullptr, &startinfo.StartupInfo, &m_procinfo)) throw Win32Exception(GetLastError());

		DeleteProcThreadAttributeList(attributes);			// Clean up the PROC_THREAD_ATTRIBUTE_LIST
	}

	catch(...) { DeleteProcThreadAttributeList(attributes); throw; }

	// Get the architecture of the created native process
	m_architecture = GetNativeArchitecture(m_procinfo.hProcess);
}

//---------------------------------------------------------------------------
// NativeProcess Destructor

NativeProcess::~NativeProcess()
{
	for(auto const& iterator : m_sections) ReleaseSection(m_procinfo.hProcess, iterator);

	if(m_procinfo.hThread) CloseHandle(m_procinfo.hThread);
	if(m_procinfo.hProcess) CloseHandle(m_procinfo.hProcess);
}

//---------------------------------------------------------------------------
// NativeProcess HANDLE conversion operator

NativeProcess::operator HANDLE() const
{
	return m_procinfo.hProcess;
}

//-----------------------------------------------------------------------------
// NativeProcess::AllocateMemory
//
// Allocates a region of virtual memory
//
// Arguments:
//
//	length			- Length of the region to allocate
//	protection		- Protection flags to assign to the allocated region

uintptr_t NativeProcess::AllocateMemory(size_t length, VirtualMachine::ProtectionFlags protection)
{
	return AllocateMemory(length, protection, VirtualMachine::AllocationFlags::None);
}

//-----------------------------------------------------------------------------
// NativeProcess::AllocateMemory
//
// Allocates a region of virtual memory
//
// Arguments:
//
//	length			- Length of the region to allocate
//	protection		- Protection flags to assign to the allocated region
//	flags			- Allocation flags to control the allocation region

uintptr_t NativeProcess::AllocateMemory(size_t length, VirtualMachine::ProtectionFlags protection, VirtualMachine::AllocationFlags flags)
{
	ULONG				previous;					// Previous memory protection flags

	sync::reader_writer_lock::scoped_lock_write writer(m_sectionslock);

	// Emplace a new section into the section collection, aligning the length up to the allocation granularity
	auto iterator = m_sections.emplace(CreateSection(m_procinfo.hProcess, uintptr_t(0), align::up(length, SystemInformation::AllocationGranularity), flags));
	if(!iterator.second) throw Win32Exception(ERROR_NOT_ENOUGH_MEMORY);

	// The pages for the section are implicitly committed when mapped, "allocation" merely applies the protection flags
	void* address = reinterpret_cast<void*>(iterator.first->m_baseaddress);
	NTSTATUS result = NtApi::NtProtectVirtualMemory(m_procinfo.hProcess, reinterpret_cast<void**>(&address), reinterpret_cast<PSIZE_T>(&length), convert<SectionProtection>(protection), &previous);
	if(result != NtApi::STATUS_SUCCESS) throw StructuredException(result);

	// Track the "allocated" pages in the section's allocation bitmap
	uint32_t pages = static_cast<uint32_t>(align::up(length, SystemInformation::PageSize) / SystemInformation::PageSize);
	iterator.first->m_allocationmap.Set(0, pages);

	return iterator.first->m_baseaddress;
}

//-----------------------------------------------------------------------------
// NativeProcess::AllocateMemory
//
// Allocates a region of virtual memory
//
// Arguments:
//
//	address			- Base address for the allocation
//	length			- Length of the region to allocate
//	protection		- Protection flags to assign to the allocated region

uintptr_t NativeProcess::AllocateMemory(uintptr_t address, size_t length, VirtualMachine::ProtectionFlags protection)
{
	// This operation is different when the caller doesn't care what the base address is
	if(address == 0) return AllocateMemory(length, protection, VirtualMachine::AllocationFlags::None);

	sync::reader_writer_lock::scoped_lock_write writer(m_sectionslock);

	ReserveRange(writer, address, length);				// Ensure address space is reserved

	// "Allocate" all of the pages in the specified range with the requested protection attributes
	IterateRange(writer, address, length, [=](section_t const& section, uintptr_t address, size_t length) -> void {

		ULONG			previous;						// Previous memory protection flags

		// Section pages are implicitly committed when mapped, just change the protection flags
		NTSTATUS result = NtApi::NtProtectVirtualMemory(m_procinfo.hProcess, reinterpret_cast<void**>(&address), reinterpret_cast<PSIZE_T>(&length), convert<SectionProtection>(protection), &previous);
		if(result != NtApi::STATUS_SUCCESS) throw StructuredException(result);

		// Track the allocated pages in the section's allocation bitmap
		uint32_t pages = static_cast<uint32_t>(align::up(length, SystemInformation::PageSize) / SystemInformation::PageSize);
		uint32_t start = static_cast<uint32_t>((address - section.m_baseaddress) / SystemInformation::PageSize);
		section.m_allocationmap.Set(start, pages);
	});

	return address;
}

//---------------------------------------------------------------------------
// NativeProcess::getArchitecture
//
// Gets the architecture of the nativeprocess process

NativeArchitecture NativeProcess::getArchitecture(void) const
{
	return m_architecture;
}

//-----------------------------------------------------------------------------
// NativeProcess::CreateSection (private, static)
//
// Creates a new memory section object and maps it into the process
//
// Arguments:
//
//	process		- Target process handle
//	address		- Base address of the section to be created and mapped
//	length		- Length of the section to be created and mapped
//	flags		- Allocation flags to use when creating the section

NativeProcess::section_t NativeProcess::CreateSection(HANDLE process, uintptr_t address, size_t length, VirtualMachine::AllocationFlags flags)
{
	HANDLE					section;				// The newly created section handle
	LARGE_INTEGER			sectionlength;			// Section length as a LARGE_INTEGER
	void*					mapping;				// Address of mapped section
	SIZE_T					mappinglength = 0;		// Length of the mapped section view
	ULONG					previous;				// Previously set page protection flags
	NTSTATUS				result;					// Result from function call

	// These values should have been aligned before attempting to create the section object
	_ASSERTE((address % SystemInformation::AllocationGranularity) == 0);
	_ASSERTE((length % SystemInformation::AllocationGranularity) == 0);

	// Create a section of the requested length with an ALL_ACCESS mask and PAGE_EXECUTE_READWRITE protection and commit all pages
	sectionlength.QuadPart = length;
	result = NtApi::NtCreateSection(&section, SECTION_ALL_ACCESS, nullptr, &sectionlength, PAGE_EXECUTE_READWRITE, SEC_COMMIT, nullptr);
	if(result != NtApi::STATUS_SUCCESS) throw StructuredException(result);

	// Convert the address into a void pointer for NtMapViewOfSection and section_t
	mapping = reinterpret_cast<void*>(address);

	try {

		// Attempt to map the section into the target process' address space with PAGE_EXECUTE_READWRITE as the allowable protection
		result = NtApi::NtMapViewOfSection(section, process, &mapping, 0, 0, nullptr, &mappinglength, NtApi::ViewUnmap, convert<SectionFlags>(flags), PAGE_EXECUTE_READWRITE);
		if(result != NtApi::STATUS_SUCCESS) throw StructuredException(result);

		try {

			// The allowable permissions of PAGE_EXECUTE_READWRITE are automatically applied by NtMapViewOfSection to the committed pages,
			// but should be brought back down to PAGE_NOACCESS since no pages in this section are soft-allocated at the time of creation
			result = NtApi::NtProtectVirtualMemory(process, &mapping, reinterpret_cast<PSIZE_T>(&length), PAGE_NOACCESS, &previous);
			if(result != NtApi::STATUS_SUCCESS) throw StructuredException(result);

		}

		catch(...) { NtApi::NtUnmapViewOfSection(process, &mapping); throw;  }
	}

	catch(...) { NtApi::NtClose(section); throw; }

	// Return a new section_t structure instance to the caller
	return section_t(section, uintptr_t(mapping), mappinglength);
}

//-----------------------------------------------------------------------------
// NativeProcess::EnsureSectionAllocation (private, static)
//
// Verifies that the specified address range is soft-allocated within a section
//
// Arguments:
//
//	section		- Section object to check the soft allocations
//	address		- Starting address of the range to check
//	length		- Length of the range to check

inline void NativeProcess::EnsureSectionAllocation(section_t const& section, uintptr_t address, size_t length)
{
	uint32_t pages = static_cast<uint32_t>(align::up(length, SystemInformation::PageSize) / SystemInformation::PageSize);
	uint32_t start = static_cast<uint32_t>((address - section.m_baseaddress) / SystemInformation::PageSize);
	if(!section.m_allocationmap.AreBitsSet(start, pages)) throw Win32Exception(ERROR_INVALID_ADDRESS);
}

//-----------------------------------------------------------------------------
// NativeProcess::GetNativeArchitecture (private, static)
//
// Determines the NativeArchitecture of a process
//
// Arguments:
//
//	process		- Native process handle

NativeArchitecture NativeProcess::GetNativeArchitecture(HANDLE process)
{
	BOOL				result;				// Result from IsWow64Process

	// If the operating system is 32-bit, the architecture must be x86
	if(SystemInformation::ProcessorArchitecture == SystemInformation::Architecture::Intel) 
		return NativeArchitecture::x86;

	// 64-bit operating system, check the WOW64 status of the process to determine architecture
	if(!IsWow64Process(process, &result)) throw Win32Exception{};
	return (result) ? NativeArchitecture::x86_64 : NativeArchitecture::x64;
}

//---------------------------------------------------------------------------
// NativeProcess::getHandle
//
// Exposes the native process handle

HANDLE NativeProcess::getHandle(void) const
{
	return m_procinfo.hProcess;
}

//-----------------------------------------------------------------------------
// NativeProcess::IterateRange (private)
//
// Iterates across an address range and invokes the specified operation for each section, this
// ensures that the range is managed by this implementation and allows for operations that do 
// not operate across sections (allocation, release, protection, etc)
//
// Arguments:
//
//	lock		- Reference to a scoped_lock (unused, ensures caller has locked m_sections)
//	start		- Starting address of the range to iterate over
//	length		- Length of the range to iterate over
//	operation	- Operation to execute against each section in the range individually

void NativeProcess::IterateRange(sync::reader_writer_lock::scoped_lock& lock, uintptr_t start, size_t length, sectioniterator_t operation) const
{
	UNREFERENCED_PARAMETER(lock);				// This is just to ensure the caller has locked m_sections

	uintptr_t end = start + length;				// Determine the range ending address
	auto iterator = m_sections.begin();			// Start at the beginning of the reserved sections

	while((start < end) && (iterator != m_sections.end())) {

		// If the starting address is lower than the current section, it has not been reserved
		if(start < iterator->m_baseaddress) throw Win32Exception(ERROR_INVALID_ADDRESS);

		// If the starting address is beyond the end of the current section, move to the next section
		else if(start >= (iterator->m_baseaddress + iterator->m_length)) ++iterator;

		// Starting address is within the current section, process up to the end of the section
		// or the specified address range end, whichever is the lower address, and advance start
		else {

			operation(*iterator, start, std::min(iterator->m_baseaddress + iterator->m_length, end) - start);
			start = iterator->m_baseaddress + iterator->m_length;
		}
	}

	// If any address space was left unprocessed, it has not been reserved
	if(start < end) throw Win32Exception(ERROR_INVALID_ADDRESS);
}

//-----------------------------------------------------------------------------
// NativeProcess::LockMemory
//
// Attempts to lock a region into physical memory
//
// Arguments:
//
//	address		- Starting address of the region to lock
//	length		- Length of the region to lock

void NativeProcess::LockMemory(uintptr_t address, size_t length) const
{
	sync::reader_writer_lock::scoped_lock_read reader(m_sectionslock);

	// Attempt to unlock all pages within the specified address range
	IterateRange(reader, address, length, [=](section_t const& section, uintptr_t address, size_t length) -> void {

		EnsureSectionAllocation(section, address, length);		// All pages must be marked as allocated

		// Attempt to lock the specified pages into physical memory
		NTSTATUS result = NtApi::NtLockVirtualMemory(m_procinfo.hProcess, reinterpret_cast<void**>(&address), reinterpret_cast<PSIZE_T>(&length), NtApi::MAP_PROCESS);
		if(result != NtApi::STATUS_SUCCESS) throw StructuredException(result);
	});
}

//---------------------------------------------------------------------------
// NativeProcess::getProcessId
//
// Exposes the native process identifier

DWORD NativeProcess::getProcessId(void) const
{
	return m_procinfo.dwProcessId;
}

//-----------------------------------------------------------------------------
// NativeProcess::ProtectMemory
//
// Sets the memory protection flags for a virtual memory region
//
// Arguments:
//
//	address		- Starting address of the region to protect
//	length		- Length of the region to protect
//	protection	- Protection flags to assign to the region

void NativeProcess::ProtectMemory(uintptr_t address, size_t length, VirtualMachine::ProtectionFlags protection) const
{
	sync::reader_writer_lock::scoped_lock_read reader(m_sectionslock);

	// Set the protection for all of the pages in the specified range
	IterateRange(reader, address, length, [=](section_t const& section, uintptr_t address, size_t length) -> void {

		ULONG previous = 0;										// Previously set protection flags
		EnsureSectionAllocation(section, address, length);		// All pages must be marked as allocated

		// Apply the specified protection flags to the region
		NTSTATUS result = NtApi::NtProtectVirtualMemory(m_procinfo.hProcess, reinterpret_cast<void**>(&address), reinterpret_cast<PSIZE_T>(&length), convert<SectionProtection>(protection), &previous);
		if(result != NtApi::STATUS_SUCCESS) throw StructuredException(result);
	});
}

//-----------------------------------------------------------------------------
// NativeProcess::ReadMemory
//
// Reads data from a virtual memory region into the calling process
//
// Arguments:
//
//	address		- Starting address from which to read
//	buffer		- Destination buffer
//	length		- Number of bytes to read from the process buffer

size_t NativeProcess::ReadMemory(uintptr_t address, void* buffer, size_t length) const
{
	size_t					total = 0;				// Number of bytes read from the process

	sync::reader_writer_lock::scoped_lock_read reader(m_sectionslock);

	// Execute the read operation in multiple steps as necessary to ensure all addresses are "allocated"
	IterateRange(reader, address, length, [&](section_t const& section, uintptr_t address, size_t length) -> void {

		SIZE_T read = 0;										// Number of bytes read from the process
		EnsureSectionAllocation(section, address, length);		// All pages must be marked as allocated

		// Attempt to read the next chunk of virtual memory from the target process' address space
		NTSTATUS result = NtApi::NtReadVirtualMemory(m_procinfo.hProcess, reinterpret_cast<void*>(address), buffer, length, &read);
		if(result != NtApi::STATUS_SUCCESS) throw StructuredException(result);

		// Increment the total number of bytes read as well as the buffer pointer
		total += read;
		buffer = reinterpret_cast<void*>(uintptr_t(buffer) + read);
	});

	return total;
}

//-----------------------------------------------------------------------------
// NativeProcess::ReleaseMemory
//
// Releases a virtual memory region
//
// Arguments:
//
//	address		- Base address of the region to be released
//	length		- Length of the region to be released

void NativeProcess::ReleaseMemory(uintptr_t address, size_t length)
{
	sync::reader_writer_lock::scoped_lock_write writer(m_sectionslock);

	// Release all of the pages in the specified range
	IterateRange(writer, address, length, [=](section_t const& section, uintptr_t address, size_t length) -> void {

		ULONG previous;				// Previously set protection flags for this range of pages

		// Attempt to change the protection of the pages involved to PAGE_NOACCESS since they can't be decommitted
		NTSTATUS result = NtApi::NtProtectVirtualMemory(m_procinfo.hProcess, reinterpret_cast<void**>(&address), reinterpret_cast<PSIZE_T>(&length), PAGE_NOACCESS, &previous);
		if(result != NtApi::STATUS_SUCCESS) throw StructuredException(result);

		// Unlock the pages from physical memory (this operation will typically fail, don't bother checking result)
		NtApi::NtUnlockVirtualMemory(m_procinfo.hProcess, reinterpret_cast<void**>(&address), reinterpret_cast<PSIZE_T>(&length), NtApi::MAP_PROCESS);

		// Clear the corresponding pages from the section allocation bitmap to indicate they are "released"
		uint32_t pages = static_cast<uint32_t>(align::up(length, SystemInformation::PageSize) / SystemInformation::PageSize);
		uint32_t start = static_cast<uint32_t>((address - section.m_baseaddress) / SystemInformation::PageSize);
		section.m_allocationmap.Clear(start, pages);
	});

	// Remove any sections that are now completely empty to actually release and unmap that memory
	auto iterator = m_sections.begin();
	while(iterator != m_sections.end()) {

		if(iterator->m_allocationmap.Empty) {

			ReleaseSection(m_procinfo.hProcess, *iterator);
			iterator = m_sections.erase(iterator);
		}

		else ++iterator;
	}
}

//-----------------------------------------------------------------------------
// NativeProcess::ReleaseSection (private, static)
//
// Releases a section represented by a section_t instance
//
// Arguments:
//
//	process		- Target process handle
//	section		- Reference to the section to be released

inline void NativeProcess::ReleaseSection(HANDLE process, section_t const& section)
{
	NtApi::NtUnmapViewOfSection(process, reinterpret_cast<void*>(section.m_baseaddress));
	NtApi::NtClose(section.m_section);
}

//-----------------------------------------------------------------------------
// NativeProcess::ReserveMemory
//
// Reserves a virtual memory region for later allocation
//
// Arguments:
//
//	length		- Length of the memory region to reserve

uintptr_t NativeProcess::ReserveMemory(size_t length)
{
	return ReserveMemory(length, VirtualMachine::AllocationFlags::None);
}

//-----------------------------------------------------------------------------
// NativeProcess::ReserveMemory
//
// Reserves a virtual memory region for later allocation
//
// Arguments:
//
//	length		- Length of the memory region to reserve
//	flags		- Allocation flags to control the allocation region

uintptr_t NativeProcess::ReserveMemory(size_t length, VirtualMachine::AllocationFlags flags)
{
	sync::reader_writer_lock::scoped_lock_write writer(m_sectionslock);

	// Emplace a new section into the section collection, aligning the length up to the allocation granularity
	auto iterator = m_sections.emplace(CreateSection(m_procinfo.hProcess, uintptr_t(0), align::up(length, SystemInformation::AllocationGranularity), flags));

	if(!iterator.second) throw Win32Exception(ERROR_NOT_ENOUGH_MEMORY);
	return iterator.first->m_baseaddress;
}

//-----------------------------------------------------------------------------
// NativeProcess::ReserveMemory
//
// Reserves a virtual memory region for later allocation
//
// Arguments:
//
//	address		- Base address of the region to be reserved
//	length		- Length of the region to be reserved

uintptr_t NativeProcess::ReserveMemory(uintptr_t address, size_t length)
{
	// This operation is different when the caller doesn't care what the base address is
	if(address == 0) return ReserveMemory(length, VirtualMachine::AllocationFlags::None);

	sync::reader_writer_lock::scoped_lock_write writer(m_sectionslock);

	ReserveRange(writer, address, length);			// Ensure range is reserved
	return address;									// Return original address
}

//-----------------------------------------------------------------------------
// NativeProcess::ReserveRange (private)
//
// Ensures that a range of address space is reserved
//
//	writer		- Reference to a scoped_lock_write (unused, ensures caller has locked m_sections)
//	address		- Starting address of the range to be reserved
//	length		- Length of the range to be reserved

void NativeProcess::ReserveRange(sync::reader_writer_lock::scoped_lock_write& writer, uintptr_t address, size_t length)
{
	UNREFERENCED_PARAMETER(writer);				// This is just to ensure the caller has locked m_sections

	// Align the address and length system allocation granularity boundaries
	uintptr_t start = align::down(address, SystemInformation::AllocationGranularity);
	length = align::up(address + length, SystemInformation::AllocationGranularity) - start;

	uintptr_t end = start + length;				// Determine the range ending address (aligned)
	auto iterator = m_sections.cbegin();		// Start at the beginning of the reserved sections

	// Iterate over the existing sections to look for gaps that need to be filled in with reservations
	while((iterator != m_sections.end()) && (start < end)) {

		// If the start address is lower than the current section, fill the region with a new reservation
		if(start < iterator->m_baseaddress) {

			m_sections.emplace(CreateSection(m_procinfo.hProcess, start, std::min(end, iterator->m_baseaddress) - start, VirtualMachine::AllocationFlags::None));
			start = (iterator->m_baseaddress + iterator->m_length);
		}

		// If the start address falls within this section, move to the end of this reservation
		else if(start < (iterator->m_baseaddress + iterator->m_length)) start = (iterator->m_baseaddress + iterator->m_length);

		++iterator;								// Move to the next section
	}

	// After all the sections have been examined, create a final section if necessary
	if(start < end) m_sections.emplace(CreateSection(m_procinfo.hProcess, start, end - start, VirtualMachine::AllocationFlags::None));
}

//-----------------------------------------------------------------------------
// NativeProcess::Resume
//
// Resumes the process
//
// Arguments:
//
//	NONE

void NativeProcess::Resume(void) const
{
	NTSTATUS result = NtApi::NtResumeProcess(m_procinfo.hProcess);
	if(result != NtApi::STATUS_SUCCESS) throw StructuredException(result);
}

//-----------------------------------------------------------------------------
// NativeProcess::Suspend
//
// Suspends the process
//
// Arguments:
//
//	NONE

void NativeProcess::Suspend(void) const
{
	NTSTATUS result = NtApi::NtSuspendProcess(m_procinfo.hProcess);
	if(result != NtApi::STATUS_SUCCESS) throw StructuredException(result);
}

//-----------------------------------------------------------------------------
// NativeProcess::Terminate
//
// Terminates the native process
//
// Arguments:
//
//	exitcode		- Exit code for the process

void NativeProcess::Terminate(uint32_t exitcode) const
{
	Terminate(exitcode, true);		// Wait for the process to terminate
}

//-----------------------------------------------------------------------------
// NativeProcess::Terminate
//
// Terminates the native process
//
// Arguments:
//
//	exitcode		- Exit code for the process
//	wait			- Flag to wait for the process to exit

void NativeProcess::Terminate(uint32_t exitcode, bool wait) const
{
	TerminateProcess(m_procinfo.hProcess, static_cast<UINT>(exitcode));
	if(wait) WaitForSingleObject(m_procinfo.hProcess, INFINITE);
}

//-----------------------------------------------------------------------------
// NativeProcess::UnlockMemory
//
// Attempts to unlock a region from physical memory
//
// Arguments:
//
//	address		- Starting address of the region to unlock
//	length		- Length of the region to unlock

void NativeProcess::UnlockMemory(uintptr_t address, size_t length) const
{
	sync::reader_writer_lock::scoped_lock_read reader(m_sectionslock);

	// Attempt to unlock all pages within the specified address range
	IterateRange(reader, address, length, [=](section_t const& section, uintptr_t address, size_t length) -> void {

		EnsureSectionAllocation(section, address, length);		// All pages must be marked as allocated

		// Attempt to unlock the specified pages from physical memory
		NTSTATUS result = NtApi::NtUnlockVirtualMemory(m_procinfo.hProcess, reinterpret_cast<void**>(&address), reinterpret_cast<PSIZE_T>(&length), NtApi::MAP_PROCESS);
		if(result != NtApi::STATUS_SUCCESS) throw StructuredException(result);
	});
}

//-----------------------------------------------------------------------------
// NativeProcess::WriteMemory
//
// Writes data into a virtual memory region from the calling process
//
// Arguments:
//
//	address		- Starting address from which to write
//	buffer		- Source buffer
//	length		- Number of bytes to write into the process

size_t NativeProcess::WriteMemory(uintptr_t address, void const* buffer, size_t length) const
{
	size_t					total = 0;				// Number of bytes read from the process

	sync::reader_writer_lock::scoped_lock_read reader(m_sectionslock);

	// Execute the write operation in multiple steps as necessary to ensure all addresses are "allocated"
	IterateRange(reader, address, length, [&](section_t const& section, uintptr_t address, size_t length) -> void {

		SIZE_T written = 0;										// Number of bytes written to the process
		EnsureSectionAllocation(section, address, length);		// All pages must be marked as allocated

		// Attempt to write the next chunk of data into the target process' virtual address space
		NTSTATUS result = NtApi::NtWriteVirtualMemory(m_procinfo.hProcess, reinterpret_cast<void const*>(address), buffer, length, &written);
		if(result != NtApi::STATUS_SUCCESS) throw StructuredException(result);

		// Increment the total number of bytes written as well as the buffer pointer
		total += written;
		buffer = reinterpret_cast<void const*>(uintptr_t(buffer) + written);
	});

	return total;
}

//
// NATIVEPROCESS::SECTION_T
//

//-----------------------------------------------------------------------------
// NativeProcess::section_t Constructor
//
// Arguments:
//
//	section			- Section object handle
//	baseaddress		- Mapping base address
//	length			- Section/mapping length

NativeProcess::section_t::section_t(HANDLE section, uintptr_t baseaddress, size_t length) : m_section(section), m_baseaddress(baseaddress), 
	m_length(length), m_allocationmap(static_cast<uint32_t>(length / SystemInformation::PageSize))
{
}

//-----------------------------------------------------------------------------
// NativeProcess::section_t::operator <

bool NativeProcess::section_t::operator <(section_t const& rhs) const
{
	return m_baseaddress < rhs.m_baseaddress;
}

//---------------------------------------------------------------------------

#pragma warning(pop)
