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

#ifndef __NATIVEPROCESS_H_
#define __NATIVEPROCESS_H_
#pragma once

#include <text.h>

#include "NativeArchitecture.h"

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// Class NativeProcess
//
// Owns a native operating system process handle and abstracts the operations that
// can be performed against that process.
//
// Memory within the native process is managed by mapped section objects so that they
// can be shared among multiple processes as necessary.  Limitations of pagefile
// backed sections are similar to Win32 file mappings -- you can create them as 
// reservations and subsequently commit individual pages, but you cannot decommit
// them again, you can only release the entire section.
//
// Due to these limitations, when a section is created it is implicitly committed into
// the process' address space, but given PAGE_NOACCESS protection flags to prevent access
// until they are soft-allocated.  Soft allocation involves changing those protection
// flags to whatever the caller wants and marking which pages are now available in a
// bitmap created for each section.  Since pages cannot be decommitted, a soft release
// operation is also used, that merely resets the protection back to PAGE_NOACCESS (note
// that the contents are not cleared).  Only when an entire section has been soft-
// released will it be removed from the collection and formally deallocated.

class NativeProcess
{
public:

	// Instance Constructors
	//
	NativeProcess(tchar_t const* path);
	NativeProcess(tchar_t const* path, tchar_t const* arguments);
	NativeProcess(tchar_t const* path, tchar_t const* arguments, HANDLE handles[], size_t numhandles);

	// Destructor
	//
	~NativeProcess();

	// HANDLE conversion operator
	//
	operator HANDLE() const;

	//-------------------------------------------------------------------------
	// Member Functions

	// Resume
	//
	// Resumes the process
	void Resume(void) const;

	// Suspend
	//
	// Suspends the process
	void Suspend(void) const;
	
	// Terminate
	//
	// Terminates the native process
	void Terminate(uint32_t exitcode) const;
	void Terminate(uint32_t exitcode, bool wait) const;
	
	//-------------------------------------------------------------------------
	// Properties

	// Architecture
	//
	// Gets the architecture of the native process
	__declspec(property(get=getArchitecture)) NativeArchitecture Architecture;
	NativeArchitecture getArchitecture(void) const;

	// Handle
	// 
	// Exposes the native process handle
	__declspec(property(get=getHandle)) HANDLE Handle;
	HANDLE getHandle(void) const;

	// ProcessId
	//
	// Exposes the native process identifier
	__declspec(property(get=getProcessId)) DWORD ProcessId;
	DWORD getProcessId(void) const;

private:

	NativeProcess(NativeProcess const&)=delete;
	NativeProcess& operator=(NativeProcess const&)=delete;

	//-------------------------------------------------------------------------
	// Private Member Functions

	// GetNativeArchitecture (static)
	//
	// Determines the NativeArchitecture of a process
	static NativeArchitecture GetNativeArchitecture(HANDLE process);

	//-------------------------------------------------------------------------
	// Member Variables

	PROCESS_INFORMATION			m_procinfo;			// Created process information
	NativeArchitecture			m_architecture;		// Created process architecture
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __NATIVEPROCESS_H_
