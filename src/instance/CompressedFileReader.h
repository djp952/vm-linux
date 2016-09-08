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

#ifndef __COMPRESSEDFILEREADER_H_
#define __COMPRESSEDFILEREADER_H_
#pragma once

#include <memory>
#include <StreamReader.h>

#pragma warning(push, 4)				

//-----------------------------------------------------------------------------
// CompressedFileReader
//
// Generic compressed file stream reader, the underlying type of the compression
// is automatically detected by examining the data

class CompressedFileReader : public StreamReader
{
public:

	// Instance Constructors
	//
	CompressedFileReader(tchar_t const* path);
	CompressedFileReader(tchar_t const* path, size_t offset);
	CompressedFileReader(tchar_t const* path, size_t offset, size_t length);

	// Destructor
	//
	virtual ~CompressedFileReader();

	//-----------------------------------------------------------------------
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

	CompressedFileReader(CompressedFileReader const&)=delete;
	CompressedFileReader& operator=(CompressedFileReader const&)=delete;

	//-----------------------------------------------------------------------
	// Private Member Functions

	// CheckMagic
	//
	// Variadic function that recursively verifies a magic number
	static bool CheckMagic(void* pointer, size_t length);

	// CheckMagic
	//
	// Variadic function that recursively verifies a magic number
	template <typename _first, typename... _remaining>
	static bool CheckMagic(void* pointer, size_t length, _first first, _remaining... remaining)
	{
		// Ensure that the pointer is valid and there is at least one byte left
		if((pointer == nullptr) || (length == 0)) return false;

		// Cast the pointer into a uint8_t pointer and test the next byte
		_first* ptr = reinterpret_cast<_first*>(pointer);
		if(*ptr != first) return false;

		// Adjust the pointer and the length and recusrively continue to check magic numbers
		return CheckMagic(reinterpret_cast<uint8_t*>(ptr) + sizeof(_first), length - sizeof(_first), remaining...);
	}

	//-------------------------------------------------------------------------
	// Member Variables

	void*							m_view;		// Underlying mapped file view
	std::unique_ptr<StreamReader>	m_stream;	// Underlying stream implementation
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __COMPRESSEDFILEREADER_H_
