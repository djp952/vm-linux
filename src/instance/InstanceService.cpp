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
#include "InstanceService.h"

#include <CommandLine.h>

#pragma warning(push, 4)

//---------------------------------------------------------------------------
// InstanceService Constructor
//
// Arguments:
//
//	NONE

InstanceService::InstanceService()
{
}

//---------------------------------------------------------------------------
// InstanceService::OnStart (private)
//
// Invoked when the service is started
//
// Arguments:
//
//	argc		- Number of command line arguments
//	argv		- Array of command line argument strings

void InstanceService::OnStart(int argc, LPTSTR* argv)
{
	std::string			logmessage;			// System log message instance

	UNREFERENCED_PARAMETER(argc);
	UNREFERENCED_PARAMETER(argv);

	// Create the system log instance
	m_syslog = std::make_unique<SystemLog>(8 MiB);

	logmessage = std::string("Instance Service started");
	m_syslog->WriteEntry(0, VirtualMachine::LogLevel::Notice, logmessage.data(), logmessage.size());
}

//---------------------------------------------------------------------------
// InstanceService::OnStop (private)
//
// Invoked when the service is stopped
//
// Arguments:
//
//	NONE

void InstanceService::OnStop(void)
{
	m_syslog.release();
}

//---------------------------------------------------------------------------

#pragma warning(pop)
