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

#ifndef __INSTANCESERVICE_H_
#define __INSTANCESERVICE_H_
#pragma once

#include <map>
#include <memory>
#include <vector>
#include <VirtualMachine.h>

#include "Parameter.h"
#include "SystemLog.h"

#pragma warning(push, 4)

// PARAMETER_MAP
//
// Helper macros used to simplify the declaration of the parameters collection
#define BEGIN_PARAMETER_MAP(__mvar) std::map<std::tstring, ParameterBase*> __mvar = {
#define PARAMETER_ENTRY(__name, __paramvar) std::make_pair(std::tstring(__name), &__paramvar),
#define END_PARAMETER_MAP() };

//-----------------------------------------------------------------------------
// Class InstanceService
//
// Implements a virtual machine instance as a service

class InstanceService : public Service<InstanceService>, public VirtualMachine
{
public:

	// Instance Constructor
	//
	InstanceService();

	// Destructor
	//
	virtual ~InstanceService()=default;

	//-------------------------------------------------------------------------
	// Member Functions

protected:

	//-------------------------------------------------------------------------
	// Protected Member Functions

	// WriteSystemLogEntry (VirtualMachine)
	//
	// Writes an entry into the system log
	virtual void WriteSystemLogEntry(uint8_t facility, VirtualMachine::LogLevel level, char_t const* message, size_t length);

private:

	InstanceService(InstanceService const&)=delete;
	InstanceService& operator=(InstanceService const&)=delete;

	// Service<> Control Handler Map
	//
	BEGIN_CONTROL_HANDLER_MAP(InstanceService)
		CONTROL_HANDLER_ENTRY(SERVICE_CONTROL_STOP, OnStop)
	END_CONTROL_HANDLER_MAP()

	// Parameter Map
	//
	BEGIN_PARAMETER_MAP(m_params)
		PARAMETER_ENTRY(TEXT("log_buf_len"), param_log_buf_len)
		PARAMETER_ENTRY(TEXT("loglevel"), param_loglevel)
	END_PARAMETER_MAP()

	//-------------------------------------------------------------------------
	// Private Member Functions

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

	std::unique_ptr<SystemLog>		m_syslog;		// SystemLog instance
	HANDLE							m_job;			// Process job object

	// Parameters
	//
	Parameter<size_t>			param_log_buf_len	= 2 MiB;
	Parameter<LogLevel>			param_loglevel		= LogLevel::Warning;
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __INSTANCESERVICE_H_
