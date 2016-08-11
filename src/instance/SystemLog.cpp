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
#include "SystemLog.h"

#include <align.h>
#include <SystemInformation.h>
#include <Win32Exception.h>

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// SystemLog Constructor
//
// Arguments:
//
//	size		- Size of the system log ring buffer in bytes

SystemLog::SystemLog(size_t size) : m_tsfreq(GetTimestampFrequency()), m_tsbias(GetTimestampBias()), 
	m_defaultlevel(VirtualMachine::LogLevel::Warning), m_stdout(GetStdHandle(STD_OUTPUT_HANDLE))
{
	// Minimum log size is the page size, maximum is constant MAX_BUFFER
	size = std::min(std::max(size, SystemInformation::PageSize), MAX_BUFFER);

	// Attempt to allocate the log buffer from virtual memory, don't use the heap
	m_buffer = std::make_unique<MemoryRegion>(size, MEM_COMMIT);

	// Initialize the top, head and tail pointers to the top of the buffer
	m_top = m_head = m_tail = uintptr_t(m_buffer->Pointer);

	// Knowing the bottom of the buffer without calculating it is useful
	m_bottom = m_top + m_buffer->Length;
}

//-----------------------------------------------------------------------------
// SystemLog::getDefaultLevel
//
// Gets the default message logging level

VirtualMachine::LogLevel SystemLog::getDefaultLevel(void) const
{
	return m_defaultlevel;
}

//-----------------------------------------------------------------------------
// SystemLog::GetTimestampBias (private, static)
//
// Gets the current time to use as the timestamp bias
//
// Arguments:
//
//	NONE

int64_t SystemLog::GetTimestampBias(void)
{
	LARGE_INTEGER				qpcbias;		// QueryPerformanceCounter bias

	if(!QueryPerformanceCounter(&qpcbias)) throw Win32Exception{ GetLastError() };
	return qpcbias.QuadPart;
}

//-----------------------------------------------------------------------------
// SystemLog::GetTimestampFrequency (private, static)
//
// Gets the frequency of the high performance timer object
//
// Arguments:
//
//	NONE

double SystemLog::GetTimestampFrequency(void)
{
	LARGE_INTEGER				qpcfreq;		// QueryPerformanceCounter frequency

	if(!QueryPerformanceFrequency(&qpcfreq)) throw Win32Exception{ GetLastError() };
	return static_cast<double>(qpcfreq.QuadPart);
}

//-----------------------------------------------------------------------------
// SystemLog::IncrementTailPointer (private)
//
// Increments a tail pointer to point at the next system log entry
//
// Arguments:
//
//	lock		- Reference to a scoped_lock (unused, ensures caller has one)
//	tailptr		- Tail pointer to be incremented

inline bool SystemLog::IncrementTailPointer(sync::reader_writer_lock::scoped_lock& lock, uintptr_t& tailptr)
{
	UNREFERENCED_PARAMETER(lock);		// This is just to ensure the caller has locked

	// If the tail is in the same position as the head, the buffer is empty
	if(tailptr == m_head) return false;

	// Get the length of the entry currently being pointed to
	uint16_t entrylength = reinterpret_cast<entry_t*>(tailptr)->entrylength;

	// Increment the tail to point at the next entry in the buffer  
	tailptr += entrylength;

	// If there isn't enough room left to hold another entry header, or the length 
	// has been set to 0xFFFF, move the tail pointer back to the top of the buffer
	if((tailptr + sizeof(entry_t) > m_bottom) || (entrylength == UINT16_MAX)) tailptr = m_top;

	return true;
}

//-----------------------------------------------------------------------------
// SystemLog::SetDefaultLevel
//
// Changes the default message logging level
//
// Arguments:
//
//	level		- New default logging level to use

void SystemLog::SetDefaultLevel(VirtualMachine::LogLevel level)
{
	// defaultlevel is stored as an atomic<>, don't take a lock
	if(level != VirtualMachine::LogLevel::Default) m_defaultlevel = level;
}

//-----------------------------------------------------------------------------
// SystemLog::WriteEntry
//
// Writes a new log entry into the buffer
//
// Arguments:
//
//	facility	- Log entry facility code
//	level		- Log entry level code
//	message		- Message to be written
//	length		- Length, in characters, of the message

void SystemLog::WriteEntry(uint8_t facility, VirtualMachine::LogLevel level, char_t const* message, size_t length)
{
	static const char	crlf[] ={ '\r', '\n' };		// CRLF pair for STDOUT output
	DWORD				cch;						// Characters written to STDOUT

	length = std::min(length, MAX_MESSAGE);			// Truncate if > MAX_MESSAGE

	// Determine the overall aligned length of the log entry, must be <= 64KiB
	size_t entrylength = align::up(sizeof(entry_t) + length, __alignof(void*));
	_ASSERTE(entrylength <= UINT16_MAX);

	// The log write operation must be synchronized with any readers
	sync::reader_writer_lock::scoped_lock_write writer{ m_lock };

	// Check if writing this entry would wrap around to the top of the buffer
	if(m_head + entrylength > m_bottom) {

		// If the tail is currently at the top of the buffer, increment it
		if(m_tail == m_top) IncrementTailPointer(writer, m_tail);

		// Set all unused bytes at the end of the buffer to 0xFF and move head
		memset(reinterpret_cast<void*>(m_head), 0xFF, m_bottom - m_head);
		m_head = m_top;
	}

	// If the head pointer is behind the tail linearly, the tail may need to be
	// incremented until it's pushed out of the way of the new entry
	if(m_head < m_tail) {

		while((m_head != m_tail) && (m_head + entrylength > m_tail)) IncrementTailPointer(writer, m_tail);
	}

	// Write the entry into the buffer at the adjusted head position
	entry_t* entry = reinterpret_cast<entry_t*>(m_head);
	QueryPerformanceCounter(reinterpret_cast<PLARGE_INTEGER>(&entry->timestamp));
	
	entry->entrylength		= static_cast<uint16_t>(entrylength);
	entry->messagelength	= static_cast<uint16_t>(length);
	entry->facility			= facility;
	entry->level			= static_cast<uint8_t>(level);
	
	memcpy(entry->message, message, length);

	m_head += entrylength;			// Increment the head pointer

	// GetStdHandle can return INVALID_HANDLE_VALUE or NULL (-1 or 0, respectively) if
	// an error occurred or if there is no STDOUT handle for this process.  If there is
	// a valid STDOUT handle, write the message and a CRLF pair to it
	if(reinterpret_cast<intptr_t>(m_stdout) > 0) {
		
		writer.unlock();			// Release the write lock manually

		WriteConsoleA(m_stdout, message, static_cast<DWORD>(length), &cch, nullptr);
		WriteConsoleA(m_stdout, crlf, 2, &cch, nullptr);
	}
}

//---------------------------------------------------------------------------

#pragma warning(pop)
