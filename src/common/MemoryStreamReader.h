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

#ifndef __MEMORYSTREAMREADER_H_
#define __MEMORYSTREAMREADER_H_
#pragma once

#include "StreamReader.h"

#pragma warning(push, 4)				

//-----------------------------------------------------------------------------
// MemoryStreamReader
//
// Memory buffer stream reader implementation

class MemoryStreamReader : public StreamReader
{
public:

	// Instance Constructor
	//
	MemoryStreamReader(void const* base, size_t length);

	// Destructor
	//
	virtual ~MemoryStreamReader()=default;

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

	// Length (StreamReader)
	//
	// Gets the length of the stream in bytes
	__declspec(property(get=getLength)) size_t Length;
	virtual size_t getLength(void) const override;

	// Position (StreamReader)
	//
	// Gets the current position within the stream
	__declspec(property(get=getPosition)) size_t Position;
	virtual size_t getPosition(void) const override;

private:

	MemoryStreamReader(MemoryStreamReader const&)=delete;
	MemoryStreamReader& operator=(MemoryStreamReader const&)=delete;

	//-------------------------------------------------------------------------
	// Member Variables

	void const*				m_base;				// Base memory address
	size_t					m_length;			// Length of memory buffer
	size_t					m_offset;			// Offset into the memory buffer
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __MEMORYSTREAMREADER_H_
