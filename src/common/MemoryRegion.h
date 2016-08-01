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

#ifndef __MEMORYREGION_H_
#define __MEMORYREGION_H_
#pragma once

#include <memory>
#include <stdint.h>
#include "Win32Exception.h"

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// MemoryRegion
//
// Manages a region of virtual memory that is released during destruction

class MemoryRegion
{
public:

	// Instance Constructors
	//
	MemoryRegion::MemoryRegion(size_t length);
	MemoryRegion::MemoryRegion(size_t length, uint32_t flags);
	MemoryRegion::MemoryRegion(size_t length, void* address);
	MemoryRegion::MemoryRegion(size_t length, void* address, uint32_t flags);
	MemoryRegion::MemoryRegion(HANDLE process, size_t length);
	MemoryRegion::MemoryRegion(HANDLE process, size_t length, uint32_t flags);
	MemoryRegion::MemoryRegion(HANDLE process, size_t length, void* address);
	MemoryRegion::MemoryRegion(HANDLE process, size_t length, void* address, uint32_t flags);

	// Destructor
	//
	~MemoryRegion();

	//-------------------------------------------------------------------------
	// Member Functions

	// Commit
	//
	// Commits page(s) within the region
	void Commit(void* address, size_t length, uint32_t protect);

	// Decommit
	//
	// Decommits page(s) within the region
	void Decommit(void* address, size_t length);

	// Detach
	//
	// Detaches the memory region from the class instance
	void* Detach(void);
	void* Detach(PMEMORY_BASIC_INFORMATION meminfo);

	// Protect
	//
	// Applies protection flags to page(s) within the region
	uint32_t Protect(void* address, size_t length, uint32_t protect);

	//-------------------------------------------------------------------------
	// Properties

	// Length
	//
	// Gets the length of the memory region
	__declspec(property(get=getLength)) size_t Length;
	size_t getLength(void) const;

	// Pointer
	//
	// Gets the base pointer for the memory region
	__declspec(property(get=getPointer)) void* Pointer;
	void* getPointer(void) const;

private:

	MemoryRegion(const MemoryRegion&)=delete;
	MemoryRegion& operator=(const MemoryRegion&)=delete;

	// Instance Constructor
	//
	MemoryRegion(HANDLE process, size_t length, void* address, uint32_t flags, uint32_t protect);

	//-------------------------------------------------------------------------
	// Member Variables

	void*						m_base;			// Base pointer for the memory region
	size_t						m_length;		// Length of the memory region
	HANDLE						m_process;		// Process to operate against
	MEMORY_BASIC_INFORMATION	m_meminfo;		// Actual allocation information
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __MEMORYREGION_H_
