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

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// sys_x86_attach_thread
//
// Acquires a thread context for a 32-bit host
//
// Arguments:
//
//	rpchandle		- RPC binding handle
//	tid				- [in] native thread id within the host process
//	thread			- [out] set to the thread startup information
//	context			- [out] set to the newly allocated context handle

HRESULT sys_x86_attach_thread(handle_t rpchandle, /*sys_x86_uint_t tid, sys_x86_thread_t* thread,*/ sys_x86_context_exclusive_t* context)
{
	(rpchandle);
	(context);

	return E_FAIL;
}

//---------------------------------------------------------------------------

#pragma warning(pop)
