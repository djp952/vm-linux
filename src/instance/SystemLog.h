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

#ifndef __SYSTEMLOG_H_
#define __SYSTEMLOG_H_
#pragma once

#include <atomic>
#include <sync.h>
#include <text.h>
#include <MemoryRegion.h>
#include <VirtualMachine.h>

#pragma warning(push, 4)
#pragma warning(disable:4200)		// zero-sized array in struct/union

//-----------------------------------------------------------------------------
// SystemLog
//
// Provides the system log functionality for a virtual machine, similar to the
// linux kernel ring buffer

class SystemLog
{
public:

	// Instance Constructor
	//
	SystemLog(size_t size);

	// Destructor
	//
	~SystemLog()=default;

	//-------------------------------------------------------------------------
	// Member Functions

	// SetDefaultLevel
	//
	// Changes the default logging level
	void SetDefaultLevel(VirtualMachine::LogLevel level);

	// WriteEntry
	//
	// Writes an entry into the system log
	void WriteEntry(uint8_t facility, VirtualMachine::LogLevel, char_t const* message, size_t length);

	//-------------------------------------------------------------------------
	// Properties

	// DefaultLevel
	//
	// Gets the default message logging level (non-console)
	__declspec(property(get=getDefaultLevel)) VirtualMachine::LogLevel DefaultLevel;
	VirtualMachine::LogLevel getDefaultLevel(void) const;

private:

	SystemLog(SystemLog const&)=delete;
	SystemLog& operator=(SystemLog const&)=delete;

	// entry_t
	//
	// Represents the data contained in a single log entry
	struct entry_t
	{
		int64_t			timestamp;			// Entry timestamp
		uint16_t		entrylength;		// Overall entry length
		uint16_t		messagelength;		// Length of the message text
		uint8_t			facility : 5;		// Facility code
		uint8_t			level : 3;			// Entry level code
		uint8_t			reserved[3];		// Alignment padding (repurpose me)
		char_t			message[];			// Log message text

		// message[] is followed by padding to properly align the next entry
	};

	// MAX_BUFFER
	//
	// Controls the upper boundary on the system log ring buffer size
	static size_t const MAX_BUFFER	= (1 << 23);

	// MAX_MESSAGE
	//
	// Controls the upper boundary on the size of a single log message
	static size_t const MAX_MESSAGE = (UINT16_MAX - sizeof(entry_t));

	//-------------------------------------------------------------------------
	// Private Member Functions

	// GetTimestampBias (static)
	//
	// Gets the current system time to use as the timestamp bias
	static int64_t GetTimestampBias(void);

	// GetTimestampFrequency (static)
	//
	// Gets the performance counter timestamp frequency
	static double GetTimestampFrequency(void);

	// IncrementTailPointer
	//
	// Increments a tail pointer to reference the next log entry
	bool IncrementTailPointer(sync::reader_writer_lock::scoped_lock& lock, uintptr_t& tailptr);

	//-------------------------------------------------------------------------
	// Member Variables

	HANDLE const							m_stdout;		// STDOUT handle
	double const							m_tsfreq;		// Timestamp frequency
	int64_t	const							m_tsbias;		// Timestamp bias
	std::unique_ptr<MemoryRegion>			m_buffer;		// Underlying buffer
	uintptr_t								m_top;			// Top of the buffer
	uintptr_t								m_bottom;		// Bottom of the buffer
	uintptr_t								m_head;			// Write position
	uintptr_t								m_tail;			// Read position
	std::atomic<VirtualMachine::LogLevel>	m_defaultlevel;	// Default message level
	mutable sync::reader_writer_lock		m_lock;			// Synchronization object
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __SYSTEMLOG_H_
