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
#include "CpioArchive.h"

#include <align.h>
#include <Win32Exception.h>

#pragma warning(push, 4)				

//-----------------------------------------------------------------------------
// ConvertHexString
//
// Converts an ANSI hexadecimal string into a numeric value
//
// Arguments:
//
//	str		- Pointer to the hex string to convert
//	len		- Length of the hex string, not including any NULL terminator

static uint32_t ConvertHexString(const char_t* str, size_t len)
{
	uint32_t accumulator = 0;			// Value accumulator

	// Process until the end of the string or length reaches zero
	while((str != nullptr) && (*str != '\0') && (len-- > 0)) {

		char_t ch = *str++;			// Cast out for clarity and increment
		int delta;					// ASCII delta to get value

		if(!isxdigit(static_cast<int>(ch))) break;

		// Determine what the delta is for this character code
		if((ch >= '0') && (ch <= '9')) delta = 48;
		else if((ch >= 'A') && (ch <= 'F')) delta = 55;
		else if((ch >= 'a') && (ch <= 'f')) delta = 87;
		else return 0;

		accumulator = (accumulator << 4) + (ch - delta);
	}

	return accumulator;
}

//-----------------------------------------------------------------------------
// CpioArchive::EnumerateFiles (static)
//
// Enumerates over all of the files/objects contained in a CPIO archive
//
// Arguments:
//
//	reader		- StreamReader instance set to the beginning of the archive
//	func		- Function to process each entry in the archive

void CpioArchive::EnumerateFiles(StreamReader* reader, std::function<void(CpioFile const&)> func)
{
	cpio_header_t			header;				// Current file header

	if(reader == nullptr) throw Win32Exception(ERROR_INVALID_PARAMETER);

	// Process each file embedded in the CPIO archive input stream
	while(reader->Read(&header, sizeof(cpio_header_t)) == sizeof(cpio_header_t)) {

		// CPIO header magic number is "070701" or "070702" if a checksum is present.
		// (I'm not bothering to test the checksum; can't be used to verify the file data)
		if(strncmp(header.c_magic, "07070", 5) != 0) return;
		if((header.c_magic[5] != '1') && (header.c_magic[5] != '2')) return;

		// Read the entry path string
		char_t path[_MAX_PATH];
		if(reader->Read(path, static_cast<size_t>(ConvertHexString(header.c_namesize, 8))) == 0) path[0] = '\0';
		
		// A path of "TRAILER!!!" indicates there are no more entries to process
		if(strcmp(path, "TRAILER!!!") == 0) return;

		// 32-bit alignment for the file data in the archive
		reader->Seek(align::up(reader->Position, 4));

		// Create a FileStream around the current base stream position
		uint32_t datalength = ConvertHexString(header.c_filesize, 8);
		FileStream filestream(reader, datalength);

		// Invoke the caller-supplied function with a new CpioFile object
		func(std::move(CpioFile(header, path, filestream)));

		// In the event the entire file stream was not read, seek beyond it and
		// apply the 32-bit alignment to get to the next entry header
		reader->Seek(align::up(reader->Position + (datalength - filestream.Position), 4));
	}
}

//-----------------------------------------------------------------------------
// CpioArchive::EnumerateFiles (static)
//
// Enumerates over all of the files/objects contained in a CPIO archive
//
// Arguments:
//
//	reader		- StreamReader instance set to the beginning of the archive
//	func		- Function to process each entry in the archive

void CpioArchive::EnumerateFiles(StreamReader& reader, std::function<void(CpioFile const&)> func)
{
	return EnumerateFiles(&reader, func);
}

//-----------------------------------------------------------------------------
// CpioArchive::EnumerateFiles (static)
//
// Enumerates over all of the files/objects contained in a CPIO archive
//
// Arguments:
//
//	reader		- StreamReader instance set to the beginning of the archive
//	func		- Function to process each entry in the archive

void CpioArchive::EnumerateFiles(StreamReader&& reader, std::function<void(CpioFile const&)> func)
{
	return EnumerateFiles(&reader, func);
}

//
// CPIOARCHIVE::FILESTREAM IMPLEMENTATION
//

//-----------------------------------------------------------------------------
// CpioArchive::FileStream Constructor
//
//
// Arguments:
//
//	basestream		- Pointer to the input StreamReader
//	length			- Length of the input stream

CpioArchive::FileStream::FileStream(StreamReader* basestream, uint32_t length) : m_basestream(basestream), m_length(length) 
{
	_ASSERTE(basestream);
}

//-----------------------------------------------------------------------------
// CpioArchive::FileStream::getLength
//
// Gets the length of the file stream

size_t CpioArchive::FileStream::getLength(void) const 
{ 
	return m_length; 
}

//-----------------------------------------------------------------------------
// CpioArchive::FileStream::getPosition
//
// Gets the current position of the file stream

size_t CpioArchive::FileStream::getPosition(void) const
{ 
	return m_position; 
}
		
//-----------------------------------------------------------------------------
// CpioArchive::FileStream::Read
//
// Reads data from the CPIO file stream
//
// Arguments:
//
//	buffer		- Destination buffer (can be NULL)
//	length		- Number of bytes to be read from the file stream

size_t CpioArchive::FileStream::Read(void* buffer, size_t length)
{
	// Check for null read and end-of-stream
	if((length == 0) || (m_position >= m_length)) return 0;

	// Do not read beyond the end of the length specified in the constructor
	if(m_position + length > m_length) length = (m_length - m_position);

	// Read the data from the base stream
	size_t out = m_basestream->Read(buffer, length);
	m_position += out;

	return out;
}

//-----------------------------------------------------------------------------
// CpioArchive::FileStream::Seek
//
// Seeks the stream pointer to the specified position
//
// Arguments:
//
//	position		- Position within the stream to seek

void CpioArchive::FileStream::Seek(size_t position) 
{ 
	UNREFERENCED_PARAMETER(position); 
	throw Win32Exception(ERROR_CALL_NOT_IMPLEMENTED);
}

//
// CPIOFILE IMPLEMENTATION
//

//-----------------------------------------------------------------------------
// CpioFile Constructor
//
// Arguments:
//
//	basestream	- Reference to the base stream object, positioned at file data
//	header		- Reference to the CPIO file header
//	path		- File path extracted from the data stream

CpioFile::CpioFile(cpio_header_t const& header, char_t const* path, StreamReader& data) : m_path(path), m_data(data)
{
	m_inode		= ConvertHexString(header.c_ino, 8);
	m_mode		= ConvertHexString(header.c_mode, 8);
	m_uid		= ConvertHexString(header.c_uid, 8);
	m_gid		= ConvertHexString(header.c_gid, 8);
	m_numlinks	= ConvertHexString(header.c_nlink, 8);
	m_mtime		= ConvertHexString(header.c_mtime, 8);
	m_devmajor	= ConvertHexString(header.c_maj, 8);
	m_rdevmajor = ConvertHexString(header.c_rmaj, 8);
	m_rdevminor = ConvertHexString(header.c_rmin, 8);
	m_devminor	= ConvertHexString(header.c_min, 8);
}

//-----------------------------------------------------------------------------
// CpioFile::getData
//
// Accesses the embedded file stream reader

StreamReader& CpioFile::getData(void) const 
{
	return m_data; 
}

//-----------------------------------------------------------------------------
// CpioFile::getDeviceMajor
//
// Gets the file device major version

uint32_t CpioFile::getDeviceMajor(void) const 
{
	return m_devmajor; 
}

//-----------------------------------------------------------------------------
// CpioFile::getDeviceMinor
//
// Gets the file device minor version

uint32_t CpioFile::getDeviceMinor(void) const 
{ 
	return m_devminor; 
}

//-----------------------------------------------------------------------------
// CpioFile::getGroupId
//
// Gets the file owner GID

uint32_t CpioFile::getGID(void) const 
{ 
	return m_gid; 
}

//-----------------------------------------------------------------------------
// CpioFile::getINode
//
// Gets the file inode number

uint32_t CpioFile::getINode(void) const 
{ 
	return m_inode; 
}

//-----------------------------------------------------------------------------
// CpioFile::getMode
//
// Gets the file mode and permission flags

uint32_t CpioFile::getMode(void) const 
{ 
	return m_mode; 
}

//-----------------------------------------------------------------------------
// CpioFile::getModificationTime
//
// Gets the file modification time

uint32_t CpioFile::getModificationTime(void) const 
{ 
	return m_mtime; 
}

//-----------------------------------------------------------------------------
// CpioFile::getNumLinks
//
// Gets the number of links to this file

uint32_t CpioFile::getNumLinks(void) const 
{ 
	return m_numlinks; 
}

//-----------------------------------------------------------------------------
// CpioFile::getPath
//
// Gets the path of the file (ANSI)

char_t const* CpioFile::getPath(void) const 
{ 
	return m_path.c_str(); 
}

//-----------------------------------------------------------------------------
// CpioFile::getReferencedDeviceMajor
//
// Gets the major version of the device node referenced by a special file

uint32_t CpioFile::getReferencedDeviceMajor(void) const 
{ 
	return m_rdevmajor; 
}

//-----------------------------------------------------------------------------
// CpioFile::getReferencedDeviceMinor
//
// Gets the minor version of the device node referenced by a special file

uint32_t CpioFile::getReferencedDeviceMinor(void) const 
{ 
	return m_rdevminor; 
}

//-----------------------------------------------------------------------------
// CpioFile::getUserId
//
// Gets the file owner UID

uint32_t CpioFile::getUID(void) const 
{ 
	return m_uid; 
}
	
//-----------------------------------------------------------------------------

#pragma warning(pop)
