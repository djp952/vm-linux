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
#include "Capability.h"

#include "LinuxException.h"

#pragma warning(push, 4)

// t_capabilities
//
// Thread-local bitmask that defines the set of active capabilities
__declspec(thread) uint64_t t_capabilities = 0;

//-----------------------------------------------------------------------------
// Capability::Check (static)
//
// Checks if the calling thread has the specified capability set
//
// Arguments:
//
//	capability		- Requested capabilitiy

bool Capability::Check(int capability)
{
	UNREFERENCED_PARAMETER(capability);
	return true;
}

//-----------------------------------------------------------------------------
// Capability::Demand (static)
//
// Demands the specified capabilities
//
// Arguments:
//
//	capability		- Requested capability flag

void Capability::Demand(int capability)
{
	// This is the same operation as Check(); it just throws an exception
	if(!Check(capability)) throw LinuxException(UAPI_EPERM);
}

//-----------------------------------------------------------------------------

#pragma warning(pop)
