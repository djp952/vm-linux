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
#include "InstanceService.h"
#include "StructuredException.h"
#include "Win32Exception.h"

// g_harness
//
// Service harness used when instance is launched as a standalone process
ServiceHarness<InstanceService> g_harness;

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
	UNREFERENCED_PARAMETER(instance);
	UNREFERENCED_PARAMETER(previnstance);
	UNREFERENCED_PARAMETER(cmdline);
	UNREFERENCED_PARAMETER(show);

#ifdef _DEBUG

	// Initialize CRT memory leak checking in DEBUG builds
	int nDbgFlags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	nDbgFlags |= _CRTDBG_LEAK_CHECK_DF;
	_CrtSetDbgFlag(nDbgFlags);

#endif	// _DEBUG

	// Initialize the SEH to C++ exception translator
	_set_se_translator(StructuredException::SeTranslator);

	try {

		CommandLine commandline(__argc, __targv);		// Convert the command line

		// Register the RPC protocol sequence(s) that will be used by the instance process
		RPC_STATUS rpcresult = RpcServerUseProtseq(reinterpret_cast<rpc_tchar_t*>(TEXT("ncalrpc")), RPC_C_PROTSEQ_MAX_REQS_DEFAULT, nullptr);
		if(rpcresult != RPC_S_OK) throw Win32Exception(rpcresult);

		// -service[:name]
		//
		// Start the instance as a service
		if(commandline.Switches.Contains(TEXT("service"))) {

			// Get the instance name from the command line, generate a unique one if not present
			auto instancename = commandline.Switches.GetValue(TEXT("service"));
			if(instancename.length() == 0) instancename = GenerateDefaultInstanceName();

			ServiceTable services = { ServiceTableEntry<InstanceService>(instancename) };
			services.Dispatch();
		}

		// -console[:name]
		//
		// Start the instance as a standalone process with a console for output and
		// an ability to terminate the instance with a break event
		else if(commandline.Switches.Contains(TEXT("console"))) {

			// Get the instance name from the command line, generate a unique one if not present
			auto instancename = commandline.Switches.GetValue(TEXT("console"));
			if(instancename.length() == 0) instancename = GenerateDefaultInstanceName();

			// Attempt to create a new console for the instance
			BOOL hasconsole = AllocConsole();
			if(hasconsole) SetConsoleTitle(std::tstring(TEXT("VM:")).append(instancename).c_str());

			// Start the service harness using the specified or generated instance name
			g_harness.Start(instancename, __argc, __targv);

			// Register a handler to stop the service on any break event
			if(hasconsole) SetConsoleCtrlHandler([](DWORD) -> BOOL { g_harness.SendControl(ServiceControl::Stop); return TRUE; }, TRUE);

			// Wait for the service to stop naturally or be broken by a console event
			g_harness.WaitForStatus(ServiceStatus::Stopped);

			// Emit a "Press any key to continue ..." message and wait before releasing the console
			if(hasconsole) {

				// Get the console standard output handle, note that GetStdHandle can return either
				// INVALID_HANDLE_VALUE (-1) to indicate an error, or NULL (0) if there is no STDIN
				HANDLE standardout = GetStdHandle(STD_OUTPUT_HANDLE);
				if(reinterpret_cast<intptr_t>(standardout) > 0) {

					HANDLE standardin = GetStdHandle(STD_INPUT_HANDLE);
					if(reinterpret_cast<intptr_t>(standardin) > 0) {

						DWORD		dw;			// Generic DWORD for character counts and flags
						tchar_t		ch;			// Character read from the console

						std::tstring msg(TEXT("Press any key to continue . . ."));
						WriteConsole(standardout, msg.data(), static_cast<DWORD>(msg.size()), &dw, nullptr);

						// Before reading 'any key' from STDIN, set the console mode and flush it
						GetConsoleMode(standardin, &dw);
						SetConsoleMode(standardin, dw & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
						FlushConsoleInputBuffer(standardin);

						ReadConsole(standardin, &ch, 1, &dw, nullptr);
					}
				}

				// Release the console instance regardless of STDOUT/STDIN handles
				FreeConsole();
			}
		}

		// not -service or -console
		//
		// Start the instance as a standalone process with a unique instance name
		else {

			// Start the service harness using a unique identifier as the instance name
			g_harness.Start(GenerateDefaultInstanceName());

			// Wait for the service to stop on its own
			g_harness.WaitForStatus(ServiceStatus::Stopped);
		}
	}

	// On exception, use the HRESULT as the return code from the process
	catch(Exception& exception) { return static_cast<int>(exception.HResult); }

	return ERROR_SUCCESS;
}

//-----------------------------------------------------------------------------
