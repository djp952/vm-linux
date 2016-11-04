//-----------------------------------------------------------------------------
// Copyright (c) 2016 Michael G. Brehm
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"}; to deal
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

#include <convert.h>
#include <datetime.h>

#pragma warning(push, 4)

// datetime --> uapi_timespec
//
template<> uapi_timespec convert<uapi_timespec>(datetime const& rhs)
{
	int64_t unixtime = (static_cast<uint64_t>(rhs) - 116444736000000000i64);
	
	// Convert the timespec components individually so they can be range checked
	int64_t tv_sec = unixtime / 10000000i64;
	int64_t tv_nsec = (unixtime * 100ui64) % 1000000000i64;

	if(tv_sec > std::numeric_limits<uapi___kernel_time_t>::max()) throw std::out_of_range("tv_sec");
	if(tv_nsec > std::numeric_limits<long>::max()) throw std::out_of_range("tv_nsec");

	return{ static_cast<uapi___kernel_time_t>(tv_sec), static_cast<long>(tv_nsec) };
}

// LARGE_INTEGER --> uapi_timespec
//
template<> uapi_timespec convert<uapi_timespec>(LARGE_INTEGER const& rhs)
{
	int64_t unixtime = (static_cast<uint64_t>(rhs.QuadPart) - 116444736000000000i64);
	
	// Convert the timespec components individually so they can be range checked
	int64_t tv_sec = unixtime / 10000000i64;
	int64_t tv_nsec = (unixtime * 100ui64) % 1000000000i64;

	if(tv_sec > std::numeric_limits<uapi___kernel_time_t>::max()) throw std::out_of_range("tv_sec");
	if(tv_nsec > std::numeric_limits<long>::max()) throw std::out_of_range("tv_nsec");

	return{ static_cast<uapi___kernel_time_t>(tv_sec), static_cast<long>(tv_nsec) };
}

// FILETIME --> uapi_timespec
//
template<> uapi_timespec convert<uapi_timespec>(FILETIME const& rhs)
{
	LARGE_INTEGER largeint{ rhs.dwLowDateTime, static_cast<LONG>(rhs.dwHighDateTime) };
	return convert<uapi_timespec>(largeint);
}

// uapi_timespec --> datetime
//
template<> datetime convert<datetime>(uapi_timespec const& rhs)
{
	return datetime((rhs.tv_sec * 10000000i64) + (rhs.tv_nsec / 100i64) + 116444736000000000i64);
}

// uapi_timespec --> LARGE_INTEGER
//
template<> LARGE_INTEGER convert<LARGE_INTEGER>(uapi_timespec const& rhs)
{
	LARGE_INTEGER result;
	result.QuadPart = ((rhs.tv_sec * 10000000i64) + (rhs.tv_nsec / 100i64) + 116444736000000000i64);

	return result;
}

// uapi_timespec --> FILETIME
//
template<> FILETIME convert<FILETIME>(uapi_timespec const& rhs)
{
	LARGE_INTEGER largeint = convert<LARGE_INTEGER>(rhs);
	return{ largeint.LowPart, static_cast<DWORD>(largeint.HighPart) };
}

//-----------------------------------------------------------------------------

#pragma warning(pop)
