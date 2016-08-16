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
#include "VirtualMachine.h"

#pragma warning(push, 4)

// VirtualMachine::CloneFlags::NewMountNamespace (static)
//
VirtualMachine::CloneFlags const VirtualMachine::CloneFlags::NewMountNamespace { 0x00020000 };			// CLONE_NEWNS

// VirtualMachine::CloneFlags::NewControlGroupNamespace (static)
//
VirtualMachine::CloneFlags const VirtualMachine::CloneFlags::NewControlGroupNamespace { 0x02000000 };	// CLONE_NEWCGROUP

// VirtualMachine::CloneFlags::NewUtsNamespace (static)
//
VirtualMachine::CloneFlags const VirtualMachine::CloneFlags::NewUtsNamespace { 0x04000000 };			// CLONE_NEWUTS

// VirtualMachine::CloneFlags::NewIpcNamespace (static)
//
VirtualMachine::CloneFlags const VirtualMachine::CloneFlags::NewIpcNamespace { 0x08000000 };			// CLONE_NEWIPC

// VirtualMachine::CloneFlags::NewUserNamespace (static)
//
VirtualMachine::CloneFlags const VirtualMachine::CloneFlags::NewUserNamespace { 0x10000000 };			// CLONE_NEWUSER

// VirtualMachine::CloneFlags::NewPidNamespace (static)
//
VirtualMachine::CloneFlags const VirtualMachine::CloneFlags::NewPidNamespace { 0x20000000 };			// CLONE_NEWPID

// VirtualMachine::CloneFlags::NewNetworkNamespace (static)
//
VirtualMachine::CloneFlags const VirtualMachine::CloneFlags::NewNetworkNamespace { 0x40000000 };		// CLONE_NEWNET

//-----------------------------------------------------------------------------

#pragma warning(pop)