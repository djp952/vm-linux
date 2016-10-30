//---------------------------------------------------------------------------
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
//---------------------------------------------------------------------------

#include "stdafx.h"
#include "FileDescriptor.h"

#include <convert.h>
#include <datetime.h>

#include "LinuxException.h"

#pragma warning(push, 4)

//---------------------------------------------------------------------------
// FileDescriptor Constructor
//
// Arguments:
//
//	pathptr		- Path instance to take ownership of
//	flags		- File descriptor / handle flags

FileDescriptor::FileDescriptor(std::unique_ptr<Namespace::Path>&& pathptr, uint32_t flags) :
	m_handle(std::make_shared<handle_t>(std::move(pathptr))), m_flags(flags)
{
	_ASSERTE(m_handle);
	// todo: this may be better off as a static method?  It needs to throw
}

//---------------------------------------------------------------------------
// FileDescriptor Constructor
//
// Arguments:
//
//	handle		- Existing shared_ptr<> to a handle_t instance
//	flags		- File descriptor / handle flags

FileDescriptor::FileDescriptor(std::shared_ptr<handle_t> const& handle, uint32_t flags) :
	m_handle(handle), m_flags(flags)
{
	_ASSERTE(m_handle);
}

//---------------------------------------------------------------------------
// FileDescriptor::AdjustPosition (private)
//
// Generates an adjusted handle position based on a delta and starting location
//
// Arguments:
//
//	lock		- Reference to a scoped_lock against the node data
//	delta		- Delta to current handle position to be calculated
//	whence		- Location from which to apply the specified delta

size_t FileDescriptor::AdjustPosition(ssize_t delta, int whence) const
{
	size_t pos = m_handle->position;		// Copy the current position

	switch(whence) {

		// UAPI_SEEK_SET - Seeks to an offset relative to the beginning of the file;
		case UAPI_SEEK_SET:

			if(delta < 0) throw LinuxException(UAPI_EINVAL);
			pos = static_cast<size_t>(delta);
			break;

		// UAPI_SEEK_CUR - Seeks to an offset relative to the current position
		case UAPI_SEEK_CUR:

			if((delta < 0) && ((pos - delta) < 0)) throw LinuxException(UAPI_EINVAL);
			pos += delta;
			break;

		// UAPI_SEEK_END - Seeks to an offset relative to the end of the file
		case UAPI_SEEK_END:

			///pos = m_handle->path->Node->Length + delta;
			throw LinuxException(UAPI_EINVAL); /// all this stuff will need to be moved back into Handle, Length only applies to Files now
			break;

		default: throw LinuxException(UAPI_EINVAL);
	}

	return pos;
}

//---------------------------------------------------------------------------
// FileDescriptor::getAllowsExecute
//
// Determines if the caller has EXECUTE access to the underlying node

bool FileDescriptor::getAllowsExecute(void) const
{
	uint32_t flags = m_flags;			// Copy the current set of flags
	
	// Write-only access to the node prevents execution regardless of permissions
	if((flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) return false;

	// todo - permission check
	return true;
}

//---------------------------------------------------------------------------
// FileDescriptor::getAllowsRead
//
// Determines if the caller has READ access to the underlying node

bool FileDescriptor::getAllowsRead(void) const
{
	uint32_t flags = m_flags;			// Copy the current set of flags
	
	// Write-only access to the node prevents read regardless of permissions
	if((flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) return false;

	// todo - permission check
	return true;
}

//---------------------------------------------------------------------------
// FileDescriptor::getAllowsWrite
//
// Determines if the caller has WRITE access to the underlying node

bool FileDescriptor::getAllowsWrite(void) const
{
	uint32_t flags = m_flags;			// Copy the current set of flags
	
	// Read-only access to the node prevents write regardless of permissions
	if((flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) return false;

	// todo - permission check
	return true;
}

//---------------------------------------------------------------------------
// FileDescriptor::Duplicate
//
// Duplicates this file descriptor
//
// Arguments:
//
//	NONE

std::unique_ptr<FileDescriptor> FileDescriptor::Duplicate(void) const
{
	// Duplicating a file descriptor automatically removes the O_CLOEXEC flag -- see dup(2)
	return std::unique_ptr<FileDescriptor>(new FileDescriptor(m_handle, m_flags & ~UAPI_O_CLOEXEC));
}

//---------------------------------------------------------------------------
// FileDescriptor::getFlags
//
// Gets the file descriptor flags

uint32_t FileDescriptor::getFlags(void) const
{
	return m_flags;
}

//---------------------------------------------------------------------------
// FileDescriptor::getPosition
//
// Gets the current file position for this file descriptor

size_t FileDescriptor::getPosition(void) const
{
	return m_handle->position;
}

//---------------------------------------------------------------------------
// FileDescriptor::Read
//
// Synchronously reads data from the underlying node into a buffer
//
// Arguments:
//
//	buffer		- Pointer to the output buffer
//	count		- Maximum number of bytes to read into the buffer

size_t FileDescriptor::Read(void* buffer, size_t count)
{
	return 0;
	//if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	//uint32_t flags = m_flags;				// Copy the currently set flags

	//// The file descriptor cannot be open for write-only mode
	//if((flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) throw LinuxException(UAPI_EACCES);

	//auto mount = m_handle->path->Mount;		// Pull out a pointer to the mount
	//auto node = m_handle->path->Node;		// Pull out a pointer to the node
	//size_t pos = m_handle->position;		// Get the current position value

	//// Read the data from the underlying node and increment the position pointer
	//count = node->Read(mount, pos, buffer, count);
	//m_handle->position = (pos + count);

	//// O_NOATIME - Do not update the last access time of the node after a read
	//if((flags & UAPI_O_NOATIME) == 0) node->SetAccessTime(mount, convert<uapi_timespec>(datetime::now()));

	//return count;							// Return number of bytes read
}

//---------------------------------------------------------------------------
// FileDescriptor::ReadAt
//
// Synchronously reads data from the underlying node into a buffer, does not
// change the current file position
//
// Arguments:
//
//	offset		- Offset within the file from which to read
//	whence		- Location from which to apply the specified offset
//	buffer		- Pointer to the output buffer
//	count		- Maximum number of bytes to read into the buffer

size_t FileDescriptor::ReadAt(ssize_t offset, int whence, void* buffer, size_t count)
{
	return 0;
	//if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	//uint32_t flags = m_flags;				// Copy the currently set flags

	//// The file descriptor cannot be open for write-only mode
	//if((flags & UAPI_O_ACCMODE) == UAPI_O_WRONLY) throw LinuxException(UAPI_EACCES);

	//auto mount = m_handle->path->Mount;		// Pull out a pointer to the mount
	//auto node = m_handle->path->Node;		// Pull out a pointer to the node

	//// Read the data from the underlying node -- do not change the position pointer
	//count = node->Read(mount, AdjustPosition(offset, whence), buffer, count);

	//// O_NOATIME - Do not update the last access time of the node after a read
	//if((flags & UAPI_O_NOATIME) == 0) node->SetAccessTime(mount, convert<uapi_timespec>(datetime::now()));

	//return count;							// Return the number of bytes read
}

//---------------------------------------------------------------------------
// FileDescriptor::Seek
//
// Changes the file position pointer
//
// Arguments:

//	offset		- Offset within the node to set the new position
//	whence		- Location from which to apply the specified offset

size_t FileDescriptor::Seek(ssize_t offset, int whence)
{
	size_t pos = AdjustPosition(offset, whence);

	m_handle->position = pos;
	return pos;
}
	
//---------------------------------------------------------------------------
// FileDescriptor::Sync
//
// Synchronizes all metadata and data associated with the file to storage
//
// Arguments:
//
//	NONE

void FileDescriptor::Sync(void) const
{
	//m_handle->path->Node->Sync(m_handle->path->Mount);
}

//---------------------------------------------------------------------------
// FileDescriptor::SyncData
//
// Synchronizes all data associated with the file to storage, not metadata
//
// Arguments:
//
//	NONE

void FileDescriptor::SyncData(void) const
{
	//m_handle->path->Node->SyncData(m_handle->path->Mount);
}

//---------------------------------------------------------------------------
// FileDescriptor::Write
//
// Synchronously writes data to the underlying node from a buffer
//
// Arguments:
//
//	buffer		- Pointer to the input buffer
//	count		- Maximum number of bytes to write from the buffer

size_t FileDescriptor::Write(void const* buffer, size_t count)
{
	return 0;
	//if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	//uint32_t flags = m_flags;				// Copy the currently set flags

	//// The file descriptor cannot be open for read-only mode
	//if((flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);

	//auto mount = m_handle->path->Mount;		// Pull out a pointer to the mount
	//auto node = m_handle->path->Node;		// Pull out a pointer to the node
	//size_t pos = m_handle->position;		// Get the current position value

	//// Write the data into the underlying node and increment the position pointer
	//count = node->Write(mount, pos, buffer, count);
	//m_handle->position = (pos + count);

	//// Writing to the node always updates the modification time stamp
	//node->SetModificationTime(mount, convert<uapi_timespec>(datetime::now()));

	//return count;							// Return number of bytes read
}

//---------------------------------------------------------------------------
// FileDescriptor::WriteAt
//
// Synchronously writes data to the underlying node from a buffer, does not
// change the current file position
//
// Arguments:
//
//	offset		- Offset within the file from which to write
//	whence		- Location from which to apply the specified offset
//	buffer		- Pointer to the input buffer
//	count		- Maximum number of bytes to write from the buffer

size_t FileDescriptor::WriteAt(ssize_t offset, int whence, void const* buffer, size_t count)
{
	return 0;
	//if((count > 0) && (buffer == nullptr)) throw LinuxException(UAPI_EFAULT);

	//uint32_t flags = m_flags;				// Copy the currently set flags

	//// The file descriptor cannot be open for read-only mode
	//if((flags & UAPI_O_ACCMODE) == UAPI_O_RDONLY) throw LinuxException(UAPI_EACCES);

	//auto mount = m_handle->path->Mount;		// Pull out a pointer to the mount
	//auto node = m_handle->path->Node;		// Pull out a pointer to the node

	//// Write the data into the underlying node -- do not change the position pointer
	//count = node->Write(mount, AdjustPosition(offset, whence), buffer, count);

	//// Writing to the node always updates the modification time stamp
	//node->SetModificationTime(mount, convert<uapi_timespec>(datetime::now()));

	//return count;							// Return the number of bytes read
}

//
// FILEDESCRIPTOR::HANDLE_T IMPLEMENTATION
//

//---------------------------------------------------------------------------
// FileDescriptor::handle_t Constructor
//
// Arguments:
//
//	pathptr		- Path instance to take ownership of

FileDescriptor::handle_t::handle_t(std::shared_ptr<Namespace::Path> const& pathptr) : path(pathptr), position(0)
{
	_ASSERTE(path);
}

//---------------------------------------------------------------------------

#pragma warning(pop)
