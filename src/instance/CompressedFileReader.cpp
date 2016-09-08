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
#include "CompressedFileReader.h"

#include <BZip2StreamReader.h>
#include <GZipStreamReader.h>
#include <Lz4StreamReader.h>
#include <LzmaStreamReader.h>
#include <LzopStreamReader.h>
#include <MemoryStreamReader.h>
#include <Win32Exception.h>
#include <XzStreamReader.h>

#pragma warning(push, 4)				

// UINT8_C redefinition
//
#pragma push_macro("UINT8_C")
#undef UINT8_C
#define UINT8_C(x) static_cast<uint8_t>(x)

//-----------------------------------------------------------------------------
// CompressedFileReader Constructor
//
// Arguments:
//
//	path		- Path to the input file

CompressedFileReader::CompressedFileReader(tchar_t const* path) : CompressedFileReader(path, 0, 0)
{
}

//-----------------------------------------------------------------------------
// CompressedFileReader Constructor
//
// Arguments:
//
//	path		- Path to the input file
//	offset		- Offset within the file to begin reading

CompressedFileReader::CompressedFileReader(tchar_t const* path, size_t offset) : CompressedFileReader(path, offset, 0)
{
}

//-----------------------------------------------------------------------------
// CompressedFileReader Constructor
//
// Arguments:
//
//	path		- Path to the input file
//	offset		- Offset within the file to begin reading
//	length		- Maximum number of bytes to read from the file

CompressedFileReader::CompressedFileReader(tchar_t const* path, size_t offset, size_t length) : m_view(nullptr)
{
	LARGE_INTEGER			filesize;				// Size of the input file
	ULARGE_INTEGER			uloffset;				// Offset as a ULARGE_INTEGER

	// Process the offset as a ULARGE_INTEGER to deal with varying width of size_t
	uloffset.QuadPart = offset;

	if(path == nullptr) throw Win32Exception(ERROR_INVALID_PARAMETER);

	// Open the specified file with GENERIC_READ access
	HANDLE file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) throw Win32Exception();

	try {

		// Get the size of the specified input file and ensure it does not exceed size_t
		if(!GetFileSizeEx(file, &filesize)) throw Win32Exception();
		if(filesize.QuadPart > std::numeric_limits<size_t>::max()) throw Win32Exception(ERROR_NOT_ENOUGH_MEMORY);

		// Adjust the length to the actual file size if zero was specified
		length = (length == 0) ? static_cast<size_t>(filesize.QuadPart) : length;

		// Map the specified file with PAGE_READONLY access
		HANDLE mapping = CreateFileMapping(file, nullptr, PAGE_READONLY, filesize.HighPart, filesize.LowPart, nullptr);
		if(mapping == nullptr) throw Win32Exception();

		try {

			// Create a FILE_MAP_READ view of the specified offset and length against the mapped file
			m_view = MapViewOfFile(mapping, FILE_MAP_READ, uloffset.HighPart, uloffset.LowPart, length);
			if(m_view == nullptr) throw Win32Exception();

			try {

				// GZIP
				if(CheckMagic(m_view, length, UINT8_C(0x1F), UINT8_C(0x8B), UINT8_C(0x08), UINT8_C(0x00))) 
					m_stream = std::make_unique<GZipStreamReader>(m_view, length);

				// XZ
				else if(CheckMagic(m_view, length, UINT8_C(0xFD), '7', 'z', 'X', 'Z', UINT8_C(0x00))) 
					m_stream = std::make_unique<XzStreamReader>(m_view, length);

				// BZIP2
				else if(CheckMagic(m_view, length, 'B', 'Z', 'h')) 
					m_stream = std::make_unique<BZip2StreamReader>(m_view, length);

				// LZMA
				else if(CheckMagic(m_view, length, UINT8_C(0x5D), UINT8_C(0x00), UINT8_C(0x00), UINT8_C(0x00))) 
					m_stream = std::make_unique<LzmaStreamReader>(m_view, length);
	
				// LZOP
				else if(CheckMagic(m_view, length, UINT8_C(0x89), 'L', 'Z', 'O', UINT8_C(0x00), UINT8_C(0x0D), UINT8_C(0x0A), UINT8_C(0x1A), UINT8_C(0x0A))) 
					m_stream = std::make_unique<LzopStreamReader>(m_view, length);

				// LZ4
				else if(CheckMagic(m_view, length, UINT8_C(0x02), UINT8_C(0x21), UINT8_C(0x4C), UINT8_C(0x18))) 
					m_stream = std::make_unique<Lz4StreamReader>(m_view, length);

				// UNKNOWN OR UNCOMPRESSED
				else m_stream = std::make_unique<MemoryStreamReader>(m_view, length);
			}

			catch(...) { UnmapViewOfFile(m_view); throw; }

			CloseHandle(mapping);				// <-- Mapping handle does not need to stay open
		}

		catch(...) { CloseHandle(mapping); throw; }

		CloseHandle(file);						// <-- File handle does not need to stay open
	}

	catch(...) { CloseHandle(file); throw; }
}

//-----------------------------------------------------------------------------
// CompressedFileReader Destructor

CompressedFileReader::~CompressedFileReader()
{
	if(m_view) UnmapViewOfFile(m_view);
}

//-----------------------------------------------------------------------------
// CompressedFileReader::CheckMagic (private, static)
//
// Variadic function that recursively verifies a magic number
//
// Arguments:
//
//	pointer		- Pointer to the current byte in the input buffer to check
//	size_t		- Length of the input buffer pointed to by pointer

bool CompressedFileReader::CheckMagic(void* pointer, size_t length) 
{ 
	UNREFERENCED_PARAMETER(pointer);
	UNREFERENCED_PARAMETER(length);

	return true; 
}
	
//-----------------------------------------------------------------------------
// CompressedFileReader::getPosition
//
// Gets the current position of the file stream

size_t CompressedFileReader::getPosition(void) const
{ 
	_ASSERTE(m_stream);
	return m_stream->Position; 
}

//-----------------------------------------------------------------------------
// CompressedFileReader::Read
//
// Reads the specified number of bytes from the input stream into the output buffer
//
// Arguments:
//
//	buffer			- Output buffer
//	length			- Length of the output buffer, in bytes

size_t CompressedFileReader::Read(void* buffer, size_t length)
{
	_ASSERTE(m_stream);
	return m_stream->Read(buffer, length);
}

//-----------------------------------------------------------------------------
// CompressedFileReader::Seek
//
// Advances the stream to the specified position
//
// Arguments:
//
//	position		- Position to advance the input stream to

void CompressedFileReader::Seek(size_t position)
{
	_ASSERTE(m_stream);
	m_stream->Seek(position);
}

//-----------------------------------------------------------------------------

#pragma pop_macro("UINT8_C")

#pragma warning(pop)
