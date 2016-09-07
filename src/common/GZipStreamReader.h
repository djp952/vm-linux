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

//-----------------------------------------------------------------------------
// EXTERNAL DEPENDENCY: ZLIB
//
// - Add the following files from external-zlib to the parent project:
//
// adler32.c
// crc32.c
// inffast.c
// inflate.c
// inftrees.c
// zconf.h
// zlib.h
// zutil.c
//
// - Disable precompiled headers for all the above .c files
// - Define NO_GZ_COMPRESS as a preprocessor definition for all the above .c files
// - Add external-zlib to the project Additional Include Directories
//-----------------------------------------------------------------------------

#ifndef __GZIPSTREAMREADER_H_
#define __GZIPSTREAMREADER_H_
#pragma once

#include <zlib.h>
#include "StreamReader.h"

#pragma warning(push, 4)				

//-----------------------------------------------------------------------------
// GZipStreamReader
//
// GZIP-based decompression stream reader implementation

class GZipStreamReader : public StreamReader
{
public:

	// Instance Constructor
	//
	GZipStreamReader(void const* base, size_t length);

	// Destructor
	//
	virtual ~GZipStreamReader();

	//---------------------------------------------------------------------
	// Member Functions

	// Read (StreamReader)
	//
	// Reads data from the current position within the stream
	virtual size_t Read(void* buffer, size_t length) override;
		
	// Seek (StreamReader)
	//
	// Sets the position within the stream
	virtual void Seek(size_t position) override;
		
	//---------------------------------------------------------------------
	// Properties

	// Position (StreamReader)
	//
	// Gets the current position within the stream
	__declspec(property(get=getPosition)) size_t Position;
	virtual size_t getPosition(void) const override;

private:

	GZipStreamReader(GZipStreamReader const&)=delete;
	GZipStreamReader& operator=(GZipStreamReader const&)=delete;

	//-------------------------------------------------------------------------
	// Member Variables

	z_stream			m_stream;				// GZIP decompression stream
	size_t				m_position = 0;			// Current stream position
	bool				m_finished = false;		// End of stream has been reached
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __GZIPSTREAMREADER_H_
