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

#include "CommandLine.h"
#include "StructuredException.h"
#include "VirtualMachine.h"
#include "Win32Exception.h"

// g_harness
//
// Service harness used when instance is launched as a standalone process
ServiceHarness<VirtualMachine> g_harness;

//-----------------------------------------------------------------------------
// GenerateDefaultInstanceName
//
// Generate a default instance name if none was specified on the command line
//
// Arguments:
//
//	NONE

std::tstring GenerateDefaultInstanceName(void)
{
	uuid_t				uuid;					// Generated UUID
	rpc_tchar_t*		szuuid = nullptr;		// Converted UUID string

	// Use the RPC implementation to generate a unique identifier
	RPC_STATUS rpcresult = UuidCreate(&uuid);
	if(rpcresult != RPC_S_OK) throw Win32Exception(rpcresult);

	// Convert the uniqueidentifier into a string
	rpcresult = UuidToString(&uuid, &szuuid);
	if(rpcresult != RPC_S_OK) throw Win32Exception(rpcresult);

	std::tstring result(reinterpret_cast<tchar_t*>(szuuid));
	RpcStringFree(&szuuid);

	return result;
}

//-----------------------------------------------------------------------------
// WinMain
//
// Application entry point
//
// Arguments:
//
//	instance		- Application instance handle (base address)
//	previnstance	- Unused in Win32
//	cmdline			- Pointer to the application command line
//	show			- Initial window show command

int APIENTRY _tWinMain(HINSTANCE instance, HINSTANCE previnstance, LPTSTR cmdline, int show)
{
	BOOL				hasconsole = FALSE;				// Flag if console is active
	int					result = ERROR_SUCCESS;			// Result code from _tWinMain

	UNREFERENCED_PARAMETER(instance);
	UNREFERENCED_PARAMETER(previnstance);
	UNREFERENCED_PARAMETER(show);

#ifdef _DEBUG

	int nDbgFlags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);	// Get current flags
	nDbgFlags |= _CRTDBG_LEAK_CHECK_DF;						// Enable leak-check
	_CrtSetDbgFlag(nDbgFlags);								// Set the new flags

#endif	// _DEBUG

	// Initialize the SEH to C++ exception translator
	_set_se_translator(StructuredException::SeTranslator);

	try {

		CommandLine commandline(cmdline);					// Convert command line

		// Register the RPC protocol sequence(s) that will be used by the instance process
		RPC_STATUS rpcresult = RpcServerUseProtseq(reinterpret_cast<rpc_tchar_t*>(TEXT("ncalrpc")), RPC_C_PROTSEQ_MAX_REQS_DEFAULT, nullptr);
		if(rpcresult != RPC_S_OK) throw Win32Exception(rpcresult);

		// -service
		//
		// Start the instance as a service
		if(commandline.Switches.Contains(TEXT("service"))) {

			// Get the instance name from the command line, generate a unique one if not present
			auto instancename = commandline.Switches.GetValue(TEXT("service"));
			if(instancename.length() == 0) instancename = GenerateDefaultInstanceName();

			ServiceTable services = { ServiceTableEntry<VirtualMachine>(instancename) };
			services.Dispatch();
		}

		// -console
		//
		// Start the instance as a standalone process, forcing creation of a console if necessary
		else if(commandline.Switches.Contains(TEXT("console"))) {

			// Get the instance name from the command line, generate a unique one if not present
			auto instancename = commandline.Switches.GetValue(TEXT("console"));
			if(instancename.length() == 0) instancename = GenerateDefaultInstanceName();

			// Attempt to attach to a parent console if one exists, otherwise allocate a new one
			hasconsole = AttachConsole(ATTACH_PARENT_PROCESS);
			if(!hasconsole) hasconsole = AllocConsole();
			if(hasconsole) SetConsoleTitle(instancename.c_str());

			// Start the service harness using the specified or generated instance name
			g_harness.Start(instancename);

			// Register a handler to stop the service on any break event
			if(hasconsole) SetConsoleCtrlHandler([](DWORD) -> BOOL { g_harness.SendControl(ServiceControl::Stop); return TRUE; }, TRUE);

			// Wait for the service to stop naturally or was broken by an event
			g_harness.WaitForStatus(ServiceStatus::Stopped);
		}

		// not -service or -console
		//
		// Start the instance as a standalone process with a unique instance name. If there
		// is a parent console to attach to use it, otherwise run unattended
		else {

			// Generate a unique instance name
			auto instancename = GenerateDefaultInstanceName();

			// Attempt to attach to a parent console if one exists; if there is no parent
			// console this process will be non-interactive and needs to stop on its own
			hasconsole = AttachConsole(ATTACH_PARENT_PROCESS);
			if(hasconsole) SetConsoleTitle(instancename.c_str());

			// Start the service harness using a unique identifier as the instance name
			g_harness.Start(instancename);

			// Register a handler to stop the service on any break event
			if(hasconsole) SetConsoleCtrlHandler([](DWORD) -> BOOL { g_harness.SendControl(ServiceControl::Stop); return TRUE; }, TRUE);

			// Wait for the service to stop naturally or was broken by an event
			g_harness.WaitForStatus(ServiceStatus::Stopped);
		}
	}

	catch(Exception& exception) {

		//
		// todo: dump the exception to stdout
		//

		// Use the exception HRESULT as the return code from the process
		result = static_cast<int>(exception.HResult);
	}

	if(hasconsole) FreeConsole();		// Release attached/allocated console

	return result;
}

//-----------------------------------------------------------------------------
