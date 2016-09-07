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
#include "GZipStreamReader.h"

#include <exception>

#pragma warning(push, 4)				

//-----------------------------------------------------------------------------
// GZipStreamReader Constructor
//
// Arguments:
//
//	base		- Pointer to the start of the GZIP stream
//	length		- Length of the input stream, in bytes

GZipStreamReader::GZipStreamReader(void const* base, size_t length)
{
	if(!base) throw std::invalid_argument("base");
	if(length == 0) throw std::invalid_argument("length");

#ifdef _WIN64
	if(length > UINT32_MAX) throw std::invalid_argument("length");
#endif

	// Initialize the zlib stream structure
	memset(&m_stream, 0, sizeof(z_stream));
	m_stream.avail_in = static_cast<uInt>(length);
	m_stream.next_in  = reinterpret_cast<Bytef*>(const_cast<void*>(base));

	// inflateInit2() must be used when working with a GZIP stream
	int result = inflateInit2(&m_stream, 16 + MAX_WBITS);
	if(result != Z_OK) throw std::exception("gzip: decompression stream could not be initialized");
}

//-----------------------------------------------------------------------------
// GZipStreamReader Destructor

GZipStreamReader::~GZipStreamReader()
{
	inflateEnd(&m_stream);
}

//-----------------------------------------------------------------------------
// GZipStreamReader::getPosition
//
// Gets the current position of the file stream

size_t GZipStreamReader::getPosition(void) const
{ 
	return m_position; 
}

//-----------------------------------------------------------------------------
// GZipStreamReader::Read
//
// Reads the specified number of bytes from the input stream into the output buffer
//
// Arguments:
//
//	buffer			- Output buffer
//	length			- Length of the output buffer, in bytes

size_t GZipStreamReader::Read(void* buffer, size_t length)
{
	bool freemem = false;					// Flag to free buffer
	uint32_t out = m_stream.total_out;		// Save the current total

#ifdef _WIN64
	if(length > UINT32_MAX) throw std::invalid_argument("length");
#endif

	if((length == 0) || (m_finished)) return 0;		// Nothing to do

	// The caller can specify NULL if the output data is irrelevant, but zlib
	// expects to be able to write the decompressed data somewhere ...
	if(!buffer) {

		buffer = malloc(length);
		if(!buffer) throw std::bad_alloc();
		freemem = true;
	}

	// Set the output buffer pointer and length for zlib
	m_stream.next_out = reinterpret_cast<uint8_t*>(buffer);
	m_stream.avail_out = static_cast<uint32_t>(length);

	// Inflate up to the requested number of bytes from the compressed stream
	int result = inflate(&m_stream, Z_SYNC_FLUSH);
	if(freemem) free(buffer);

	if((result != Z_OK) && (result != Z_STREAM_END)) throw std::exception("gzip: decompression stream data is corrupt");

	// Prevent reading from beyond the end of the stream
	if(result == Z_STREAM_END) m_finished = true;

	out = (m_stream.total_out - out);			// Update output count
	m_position += out;							// Update stream position
	
	return out;
}

//-----------------------------------------------------------------------------
// GZipStreamReader::Seek
//
// Advances the stream to the specified position
//
// Arguments:
//
//	position		- Position to advance the input stream to

void GZipStreamReader::Seek(size_t position)
{
#ifdef _WIN64
	if(position > UINT32_MAX) throw std::invalid_argument("position");
#endif

	if(position < m_position) throw std::invalid_argument("position");
	
	// Use Read() to decompress and advance the stream
	Read(NULL, position - m_position);
	if(m_position != position) throw std::exception("gzip: decompression stream ended prematurely");
}

//-----------------------------------------------------------------------------

#pragma warning(pop)
