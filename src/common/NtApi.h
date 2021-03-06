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

#ifndef __NTAPI_H_
#define __NTAPI_H_
#pragma once

#include <winternl.h>

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// NtApi
//
// Provides declarations and function pointers to use non-standard or 
// undocumented Windows APIs

class NtApi
{
public:

	//-------------------------------------------------------------------------
	// Type Declarations

	// DUPLICATE_SAME_ATTRIBUTES
	//
	// NTAPI constant not defined in the standard Win32 user-mode headers
	static const int DUPLICATE_SAME_ATTRIBUTES = 0x04;

	// FILE_ID_FULL_DIR_INFORMATION
	//
	// NTAPI structure not defined in the standard Win32 user-mode headers
	typedef struct _FILE_ID_FULL_DIR_INFORMATION {

		ULONG NextEntryOffset;
		ULONG FileIndex;
		LARGE_INTEGER CreationTime;
		LARGE_INTEGER LastAccessTime;
		LARGE_INTEGER LastWriteTime;
		LARGE_INTEGER ChangeTime;
		LARGE_INTEGER EndOfFile;
		LARGE_INTEGER AllocationSize;
		ULONG FileAttributes;
		ULONG FileNameLength;
		ULONG EaSize;
		LARGE_INTEGER FileId;
		WCHAR FileName[1];

	} FILE_ID_FULL_DIR_INFORMATION, *PFILE_ID_FULL_DIR_INFORMATION;

	// MAP_PROCESS / MAP_SYSTEM
	//
	// NTAPI constants not defined in the standard Win32 user-mode headers
	static const ULONG MAP_PROCESS = 0x0001;
	static const ULONG MAP_SYSTEM = 0x0002;

	// RTL_BITMAP
	//
	// NTAPI structure not defined in the standard Win32 user-mode headers.
	typedef struct _RTL_BITMAP {

		ULONG	SizeOfBitMap;			// Number of bits in bitmap
		PULONG	Buffer;					// Pointer to the bitmap itself
	
	} RTL_BITMAP, *PRTL_BITMAP;

	// PCTRL_BITMAP
	//
	// Const pointer to an RTL_BITMAP structure
	using PCRTL_BITMAP = const RTL_BITMAP*;

	// RTL_BITMAP_RUN
	//
	// NTAPI structure not defined in the standard Win32 user-mode headers
	typedef struct _RTL_BITMAP_RUN {

		ULONG	StartingIndex;
		ULONG	NumberOfBits;

	} RTL_BITMAP_RUN, *PRTL_BITMAP_RUN;

	// SECTION_INHERIT
	//
	// Section inheritance flags for NtMapViewOfSection
	using SECTION_INHERIT = int;
	static const SECTION_INHERIT ViewShare = 1;
	static const SECTION_INHERIT ViewUnmap = 2;

	// FileIdFullDirectoryInformation
	//
	// Flag passed to NtQueryDirectoryFile to retrieve directory entries
	static const FILE_INFORMATION_CLASS FileIdFullDirectoryInformation = (FILE_INFORMATION_CLASS)38;

	// STATUS_SUCCESS
	//
	// NTAPI constant not defined in the standard Win32 user-mode headers
	static const NTSTATUS STATUS_SUCCESS = 0;

	// STATUS_NO_MORE_FILES
	//
	// NTAPI constant not defined in the standard Win32 user-mode headers
	static const NTSTATUS STATUS_NO_MORE_FILES = 0x80000006;

	// NTAPI Functions
	//
	using NtAllocateVirtualMemoryFunc		= NTSTATUS(NTAPI*)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
	using NtCloseFunc						= NTSTATUS(NTAPI*)(HANDLE);
	using NtCreateSectionFunc				= NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PLARGE_INTEGER, ULONG, ULONG, HANDLE);
	using NtDuplicateObjectFunc				= NTSTATUS(NTAPI*)(HANDLE, HANDLE, HANDLE, PHANDLE, ACCESS_MASK, ULONG, ULONG);
	using NtFlushVirtualMemoryFunc			= NTSTATUS(NTAPI*)(HANDLE, PVOID*, PSIZE_T, PIO_STATUS_BLOCK);
	using NtFreeVirtualMemoryFunc			= NTSTATUS(NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG);
	using NtLockVirtualMemoryFunc			= NTSTATUS(NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG);
	using NtMapViewOfSectionFunc			= NTSTATUS(NTAPI*)(HANDLE, HANDLE, PVOID*, ULONG_PTR, SIZE_T, PLARGE_INTEGER, PSIZE_T, SECTION_INHERIT, ULONG, ULONG);
	using NtProtectVirtualMemoryFunc		= NTSTATUS(NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);
	using NtQueryDirectoryFileFunc			= NTSTATUS(NTAPI*)(HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS, BOOLEAN, PUNICODE_STRING, BOOLEAN);
	using NtReadVirtualMemoryFunc			= NTSTATUS(NTAPI*)(HANDLE, LPCVOID, PVOID, SIZE_T, PSIZE_T);
	using NtResumeProcessFunc				= NTSTATUS(NTAPI*)(HANDLE);
	using NtSuspendProcessFunc				= NTSTATUS(NTAPI*)(HANDLE);
	using NtUnlockVirtualMemoryFunc			= NTSTATUS(NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG);
	using NtUnmapViewOfSectionFunc			= NTSTATUS(NTAPI*)(HANDLE, LPCVOID);
	using NtWriteVirtualMemoryFunc			= NTSTATUS(NTAPI*)(HANDLE, LPCVOID, LPCVOID, SIZE_T, PSIZE_T);
	using RtlAreBitsClearFunc				= BOOLEAN(NTAPI*)(PCRTL_BITMAP, ULONG, ULONG);
	using RtlAreBitsSetFunc					= BOOLEAN(NTAPI*)(PCRTL_BITMAP, ULONG, ULONG);
	using RtlClearAllBitsFunc				= VOID(NTAPI*)(PRTL_BITMAP);
	using RtlClearBitFunc					= VOID(NTAPI*)(PRTL_BITMAP, ULONG);
	using RtlClearBitsFunc					= VOID(NTAPI*)(PRTL_BITMAP, ULONG, ULONG);
	using RtlFindClearBitsFunc				= ULONG(NTAPI*)(PCRTL_BITMAP, ULONG, ULONG);
	using RtlFindClearBitsAndSetFunc		= ULONG(NTAPI*)(PRTL_BITMAP, ULONG, ULONG);
	using RtlFindClearRunsFunc				= ULONG(NTAPI*)(PCRTL_BITMAP, PRTL_BITMAP_RUN, ULONG, BOOLEAN);
	using RtlFindLastBackwardRunClearFunc	= ULONG(NTAPI*)(PCRTL_BITMAP, ULONG, PULONG);
	using RtlFindLongestRunClearFunc		= ULONG(NTAPI*)(PCRTL_BITMAP, PULONG);
	using RtlFindNextForwardRunClearFunc	= ULONG(NTAPI*)(PCRTL_BITMAP, ULONG, PULONG);
	using RtlFindSetBitsFunc				= ULONG(NTAPI*)(PCRTL_BITMAP, ULONG, ULONG);
	using RtlFindSetBitsAndClearFunc		= ULONG(NTAPI*)(PRTL_BITMAP, ULONG, ULONG);
	using RtlInitializeBitMapFunc			= VOID(NTAPI*)(PRTL_BITMAP, PULONG, ULONG);
	using RtlNtStatusToDosErrorFunc			= ULONG(NTAPI*)(NTSTATUS);
	using RtlNumberOfClearBitsFunc			= ULONG(NTAPI*)(PCRTL_BITMAP);
	using RtlNumberOfClearBitsInRangeFunc	= ULONG(NTAPI*)(PCRTL_BITMAP, ULONG, ULONG);
	using RtlNumberOfSetBitsFunc			= ULONG(NTAPI*)(PCRTL_BITMAP);
	using RtlNumberOfSetBitsInRangeFunc		= ULONG(NTAPI*)(PCRTL_BITMAP, ULONG, ULONG);
	using RtlSetAllBitsFunc					= VOID(NTAPI*)(PRTL_BITMAP);
	using RtlSetBitFunc						= VOID(NTAPI*)(PRTL_BITMAP, ULONG);
	using RtlSetBitsFunc					= VOID(NTAPI*)(PRTL_BITMAP, ULONG, ULONG);
	using RtlTestBitFunc					= BOOLEAN(NTAPI*)(PCRTL_BITMAP, ULONG);

	//-------------------------------------------------------------------------
	// Fields

	static const NtAllocateVirtualMemoryFunc		NtAllocateVirtualMemory;
	static const NtCloseFunc						NtClose;
	static const NtCreateSectionFunc				NtCreateSection;
	static const HANDLE								NtCurrentProcess;
	static const NtDuplicateObjectFunc				NtDuplicateObject;
	static const NtFlushVirtualMemoryFunc			NtFlushVirtualMemory;
	static const NtFreeVirtualMemoryFunc			NtFreeVirtualMemory;
	static const NtLockVirtualMemoryFunc			NtLockVirtualMemory;
	static const NtMapViewOfSectionFunc				NtMapViewOfSection;
	static const NtProtectVirtualMemoryFunc			NtProtectVirtualMemory;
	static const NtQueryDirectoryFileFunc			NtQueryDirectoryFile;
	static const NtReadVirtualMemoryFunc			NtReadVirtualMemory;
	static const NtResumeProcessFunc				NtResumeProcess;
	static const NtSuspendProcessFunc				NtSuspendProcess;
	static const NtUnlockVirtualMemoryFunc			NtUnlockVirtualMemory;
	static const NtUnmapViewOfSectionFunc			NtUnmapViewOfSection;
	static const NtWriteVirtualMemoryFunc			NtWriteVirtualMemory;
	static const RtlAreBitsClearFunc				RtlAreBitsClear;
	static const RtlAreBitsSetFunc					RtlAreBitsSet;
	static const RtlClearAllBitsFunc				RtlClearAllBits;
	static const RtlClearBitFunc					RtlClearBit;
	static const RtlClearBitsFunc					RtlClearBits;
	static const RtlFindClearBitsFunc				RtlFindClearBits;
	static const RtlFindClearBitsAndSetFunc			RtlFindClearBitsAndSet;
	static const RtlFindClearRunsFunc				RtlFindClearRuns;
	static const RtlFindLastBackwardRunClearFunc	RtlFindLastBackwardRunClear;
	static const RtlFindLongestRunClearFunc			RtlFindLongestRunClear;
	static const RtlFindNextForwardRunClearFunc		RtlFindNextForwardRunClear;
	static const RtlFindSetBitsFunc					RtlFindSetBits;
	static const RtlFindSetBitsAndClearFunc			RtlFindSetBitsAndClear;
	static const RtlInitializeBitMapFunc			RtlInitializeBitMap;
	static const RtlNtStatusToDosErrorFunc			RtlNtStatusToDosError;
	static const RtlNumberOfClearBitsFunc			RtlNumberOfClearBits;
	static const RtlNumberOfClearBitsInRangeFunc	RtlNumberOfClearBitsInRange;
	static const RtlNumberOfSetBitsFunc				RtlNumberOfSetBits;
	static const RtlNumberOfSetBitsInRangeFunc		RtlNumberOfSetBitsInRange;
	static const RtlSetAllBitsFunc					RtlSetAllBits;
	static const RtlSetBitFunc						RtlSetBit;
	static const RtlSetBitsFunc						RtlSetBits;
	static const RtlTestBitFunc						RtlTestBit;

private:

	NtApi()=delete;
	~NtApi()=delete;

	NtApi(const NtApi&)=delete;
	NtApi& operator=(const NtApi&)=delete;
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __NTAPI_H_
