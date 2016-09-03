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

#ifndef __CAPABILITY_H_
#define __CAPABILITY_H_
#pragma once

#include <stdint.h>

#pragma warning(push, 4)

// t_capabilities
//
// Thread-local capabilities bitmask
extern __declspec(thread) uint64_t t_capabilities;

//-----------------------------------------------------------------------------
// Capability
//
// Manages the capability flags for the current thread.  Provides a means to 
// check for a capability without throwing an exception or to demand it which
// will throw EPERM if the thread does not have the capability defined

class Capability final
{
public:

	//-------------------------------------------------------------------------
	// Member Functions

	// Check
	//
	// Checks if the specified capability is present on the calling thread
	static bool Check(int capability);

	// Demand
	//
	// Demands the specified capability be present on the calling thread
	static void Demand(int capability);

private:

	Capability()=delete;
	~Capability()=delete;
	Capability(Capability const&)=delete;
	Capability(Capability&&)=delete;
	Capability& operator=(Capability const&)=delete;
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __CAPABILITY_H_
