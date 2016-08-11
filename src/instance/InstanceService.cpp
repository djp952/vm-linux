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

#include <algorithm>
#include "Parameter.h"

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
	std::vector<text::tstring>	initargs;		// Arguments passed to init
	std::vector<text::tstring>	initenv;		// Environment variables passed to init
	std::vector<text::tstring>	invalidargs;	// Invalid parameter arguments

	//
	// PROCESS COMMAND LINE ARGUMENTS
	//

	// argv[0] --> Service name; start at argv[1]
	int index = 1;

	// Attempt to process all command line arguments as parameters
	while(index < argc) {

		// Convert the entire argument into a tstring for processing
		auto arg = text::tstring(argv[index++]);

		// Split the argument into a vector<> based upon equal signs
		auto argparts = text::split(arg, TEXT('='));
		if(argparts.size() == 0) continue;

		// An argument string of "--" indicates that all remaining arguments are passed
		// into init directly without being processed as a parameter
		if(argparts[0].compare(TEXT("--")) == 0) break;

		// Convert the parameter name to lower case and replace all hyphens with underscores
		auto key = text::tolower(argparts[0]);
		std::replace(key.begin(), key.end(), TEXT('-'), TEXT('_'));

		// Check if the parameter exists in the collection and attempt to parse it
		auto found = m_params.find(key);
		if(found != m_params.end()) {

			// If the parameter fails to parse, keep track of it for when the system log is running
			if(!found->second->TryParse((argparts.size() > 1) ? argparts[1] : text::tstring())) invalidargs.push_back(arg);
		}

		else {
			
			// The key does not represent a known parameter; if the argument is in the format key=value it
			// gets passed as an environment variable, otherwise it gets passed as a command line argument
			if(argparts.size() > 1) initenv.push_back(arg);
			else initargs.push_back(arg);
		}
	}

	// Any remaining arguments not processed above are passed into init
	while(index < argc) { initargs.emplace_back(argv[index++]); }

	//
	// INITIALIZE SYSTEM LOG
	//

	// Create the system log instance
	m_syslog = std::make_unique<SystemLog>(param_log_buf_len);

	//
	// todo: for each invalid argument, write a warning to the system log
	//

	auto logmessage = std::string("Instance Service started");
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
