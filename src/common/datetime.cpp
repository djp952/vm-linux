//-----------------------------------------------------------------------------
// Copyright (c) 2015 Michael G. Brehm
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
#include "datetime.h"

#include <Windows.h>

#pragma warning(push, 4)

// datetime::max (static)
//
const datetime datetime::max = std::numeric_limits<int64_t>::max();

// datetime::min (static)
//
const datetime datetime::min = 0ui64;

//-----------------------------------------------------------------------------
// datetime Constructor
//
// Arguments:
//
//	ticks			- Number of 100ns units from 1/1/1601 (Win32 FILETIME)

datetime::datetime(uint64_t ticks) : m_ticks(ticks)
{
	if(ticks > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) throw std::out_of_range("ticks");
}

//-----------------------------------------------------------------------------
// datetime assignment operator

datetime& datetime::operator=(datetime const& rhs)
{
	m_ticks = rhs.m_ticks;
	return *this;
}

//-----------------------------------------------------------------------------
// datetime uint64_t conversion operator

datetime::operator uint64_t() const
{
	return m_ticks;
}

//-----------------------------------------------------------------------------
// datetime addition operator

datetime datetime::operator+(timespan const& rhs) const
{
	return datetime(m_ticks + rhs);
}

//-----------------------------------------------------------------------------
// datetime subtraction operator

datetime datetime::operator-(timespan const& rhs) const
{
	return datetime(m_ticks - rhs);
}

//-----------------------------------------------------------------------------
// datetime addition assignment operator

datetime& datetime::operator+=(timespan const& rhs)
{
	uint64_t ticks = m_ticks + rhs;
	if(ticks > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) throw std::out_of_range("rhs");

	m_ticks = ticks;
	return *this;
}

//-----------------------------------------------------------------------------
// datetime subtraction assignment operator

datetime& datetime::operator-=(timespan const& rhs)
{
	uint64_t interval = rhs;
	m_ticks = (interval >= m_ticks) ? 0 : m_ticks - interval;
	return *this;
}

//-----------------------------------------------------------------------------
// datetime equality operator

bool datetime::operator==(datetime const& rhs) const
{
	return m_ticks == rhs.m_ticks;
}

//-----------------------------------------------------------------------------
// datetime inequality operator

bool datetime::operator!=(datetime const& rhs) const
{
	return m_ticks != rhs.m_ticks;
}

//-----------------------------------------------------------------------------
// datetime greater than operator

bool datetime::operator>(datetime const& rhs) const
{
	return m_ticks > rhs.m_ticks;
}

//-----------------------------------------------------------------------------
// datetime less than operator

bool datetime::operator<(datetime const& rhs) const
{
	return m_ticks < rhs.m_ticks;
}

//-----------------------------------------------------------------------------
// datetime greater than or equal to operator

bool datetime::operator>=(datetime const& rhs) const
{
	return m_ticks >= rhs.m_ticks;
}

//-----------------------------------------------------------------------------
// datetime less than or equal to operator

bool datetime::operator<=(datetime const& rhs) const
{
	return m_ticks <= rhs.m_ticks;
}

//-----------------------------------------------------------------------------
// datetime::difference
//
// Calculates the difference between two datetimes
//
// Arguments:
//
//	rhs		- Right-hand object reference

timespan datetime::difference(datetime const& rhs) const
{
	uint64_t ticks = rhs;
	return (ticks > m_ticks) ? (ticks - m_ticks) : (m_ticks - ticks);
}

//-----------------------------------------------------------------------------
// datetime::now (static)
//
// Generates a datetime representing the current date/time

datetime datetime::now(void)
{
	FILETIME filetime;
	GetSystemTimeAsFileTime(&filetime);

	// Microsoft specifically cautions against casting FILETIME as a uint64_t as it may
	// cause alignment faults on 64-bit platforms -- convert it via a temporary variable
	return datetime((static_cast<uint64_t>(filetime.dwHighDateTime) << 32) | filetime.dwLowDateTime);
}

//-----------------------------------------------------------------------------

#pragma warning(pop)
