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

#ifndef __FILEDESCRIPTOR_H_
#define __FILEDESCRIPTOR_H_
#pragma once

#include <atomic>
#include <memory>

#include "Namespace.h"
#include "VirtualMachine.h"

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// Class FileDescriptor
//
// Implements a file descriptor, which is a handle opened against a file system
// node instance.  All normal I/O operations against the file system are executed
// via a file descriptor in order to add the necessary level of abstraction for
// permission checks, timestamp management (atime/ctime/mtime), etc.
//
// File descriptors are based on a Namespace::Path to a node rather than a direct
// pointer to the file system node since the associated mount point must also be
// tracked and accessed to call into file system node functions

class FileDescriptor
{
	// FORWARD DECLARATIONS
	//
	class handle_t;

public:

	// Instance Constructor
	//
	FileDescriptor(std::unique_ptr<Namespace::Path>&& pathptr, uint32_t flags);

	// Destructor
	//
	virtual ~FileDescriptor()=default;

	//-----------------------------------------------------------------------
	// Member Functions

	// Duplicate
	//
	// Creates a duplicate FileDescriptor instance
	std::unique_ptr<FileDescriptor> Duplicate(void) const;

	// Read
	//
	// Synchronously reads data from the underlying node into a buffer
	size_t Read(void* buffer, size_t count);

	// ReadAt
	//
	// Synchronously reads data from the underlying node into a buffer
	size_t ReadAt(ssize_t offset, int whence, void* buffer, size_t count);

	// Seek
	//
	// Changes the file position
	size_t Seek(ssize_t offset, int whence);

	// Sync
	//
	// Synchronizes all metadata and data associated with the file to storage
	void Sync(void) const;

	// SyncData
	//
	// Synchronizes all data associated with the file to storage, not metadata
	void SyncData(void) const;

	// Write
	//
	// Synchronously writes data from a buffer to the underlying node
	size_t Write(const void* buffer, size_t count);

	// WriteAt
	//
	// Synchronously writes data from a buffer to the underlying node
	size_t WriteAt(ssize_t offset, int whence, const void* buffer, size_t count);

	//-----------------------------------------------------------------------
	// Properties

	// Flags
	//
	// Gets the file descriptor flags
	__declspec(property(get=getFlags)) uint32_t Flags;
	uint32_t getFlags(void) const;

	// Position
	//
	// Gets the current file position for this file descriptor
	__declspec(property(get=getPosition)) size_t Position;
	size_t getPosition(void) const;

private:

	FileDescriptor(FileDescriptor const&)=delete;
	FileDescriptor& operator=(FileDescriptor const&)=delete;

	// Instance Constructor
	//
	FileDescriptor(std::shared_ptr<handle_t> const& handle, uint32_t flags);

	// handle_t
	//
	// Internal shared representation of a file system handle
	class handle_t
	{
	public:

		// Instance Constructor
		//
		handle_t(std::shared_ptr<Namespace::Path> const& pathptr);

		// Destructor
		//
		~handle_t()=default;

		//-------------------------------------------------------------------
		// Fields

		// path
		//
		// Shared pointer to the referenced path instance
		std::shared_ptr<Namespace::Path> const path;

		// position
		//
		// Current file pointer
		std::atomic<size_t> position;

	private:

		handle_t(handle_t const&)=delete;
		handle_t& operator=(handle_t const&)=delete;
	};

	//-------------------------------------------------------------------
	// Private Member Functions

	// AdjustPosition
	//
	// Generates an adjusted handle position based on a delta and starting location
	size_t AdjustPosition(ssize_t delta, int whence) const;

	//-----------------------------------------------------------------------
	// Member Variables

	std::shared_ptr<handle_t>		m_handle;	// The underlying handle_t
	std::atomic<uint32_t>			m_flags;	// Handle-level flags
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __FILEDESCRIPTOR_H_
