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

#ifndef __VIRTUALMACHINE_H_
#define __VIRTUALMACHINE_H_
#pragma once

#include <stdint.h>

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// Class VirtualMachine
//
// Defines the virtual machine interface that sits between the instance
// service and the system call implementation(s)

class VirtualMachine
{
public:

	// LogLevel
	//
	// Strongly typed enumeration defining the level of a log entry
	enum class LogLevel : int8_t
	{
		Default			= -1,	// LOGLEVEL_DEFAULT: Default (or last) log level
		Emergency		= 0,	// LOGLEVEL_EMERG: System is unusable
		Alert			= 1,	// LOGLEVEL_ALERT: Action must be taken immediately
		Critical		= 2,	// LOGLEVEL_CRIT: Critical conditions
		Error			= 3,	// LOGLEVEL_ERR: Error conditions
		Warning			= 4,	// LOGLEVEL_WARN: Warning conditions
		Notice			= 5,	// LOGLEVEL_NOTICE: Normal but significant condition
		Informational	= 6,	// LOGLEVEL_INFO: Informational
		Debug			= 7,	// LOGLEVEL_DEBUG: Debug-level messages
	};

	//-------------------------------------------------------------------------
	// Member Functions

protected:

	// Instance Constructor
	//
	VirtualMachine()=default;

	// Destructor
	//
	~VirtualMachine()=default;

private:

	VirtualMachine(VirtualMachine const&)=delete;
	VirtualMachine& operator=(VirtualMachine const&)=delete;
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __VIRTUALMACHINE_H_
