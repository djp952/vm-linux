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

#include <NtApi.h>
#include <StructuredException.h>
#include <SystemInformation.h>
#include <Win32Exception.h>

#pragma warning(push, 4)

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
	if(m_procinfo.hThread) CloseHandle(m_procinfo.hThread);
	if(m_procinfo.hProcess) CloseHandle(m_procinfo.hProcess);
}

//---------------------------------------------------------------------------
// NativeProcess HANDLE conversion operator

NativeProcess::operator HANDLE() const
{
	return m_procinfo.hProcess;
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

//---------------------------------------------------------------------------
// NativeProcess::getProcessId
//
// Exposes the native process identifier

DWORD NativeProcess::getProcessId(void) const
{
	return m_procinfo.dwProcessId;
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

//---------------------------------------------------------------------------

#pragma warning(pop)
