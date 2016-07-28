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

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// Class VirtualMachine
//
// Implements the top-level virtual machine instance

class VirtualMachine : public Service<VirtualMachine>
{
public:

	// Instance Constructor
	//
	VirtualMachine();

	// Destructor
	//
	~VirtualMachine()=default;

	//-------------------------------------------------------------------------
	// Member Functions

private:

	VirtualMachine(VirtualMachine const&)=delete;
	VirtualMachine& operator=(VirtualMachine const&)=delete;

	// Service<> Control Handler Map
	//
	BEGIN_CONTROL_HANDLER_MAP(VirtualMachine)
		CONTROL_HANDLER_ENTRY(SERVICE_CONTROL_STOP, OnStop)
	END_CONTROL_HANDLER_MAP()

	// OnStart (Service)
	//
	// Invoked when the service is started
	virtual void OnStart(int argc, LPTSTR* argv);

	// OnStop
	//
	// Invoked when the service is stopped
	void OnStop(void);

	//-------------------------------------------------------------------------
	// Member Variables
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __VIRTUALMACHINE_H_
