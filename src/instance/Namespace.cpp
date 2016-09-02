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
#include "Namespace.h"

#pragma warning(push, 4)

//---------------------------------------------------------------------------
// Namespace Constructor
//
// Arguments:
//
//	NONE

Namespace::Namespace()
{
	// New namespace instances get unique isolation points; this is typically
	// only going to be for the initial root namespace

	m_cgroupns = std::make_shared<ControlGroupNamespace>();
	m_ipcns = std::make_shared<IpcNamespace>();
	m_mountns = std::make_shared<MountNamespace>();
	m_netns = std::make_shared<NetworkNamespace>();
	m_pidns = std::make_shared<PidNamespace>();
	m_userns = std::make_shared<UserNamespace>();
	m_utsns = std::make_shared<UtsNamespace>();
}

//---------------------------------------------------------------------------
// Namespace Constructor
//
// Arguments:
//
//	rhs			- Source namespace from which to clone internals
//	flags		- Flags defining which internals to clone

Namespace::Namespace(Namespace const* rhs, uint32_t flags)
{
	// Cloned namespace instances can either get a shared reference to the source
	// namespace isolation points or get new ones, depending on the clone flags
	//
	m_cgroupns = ((flags & UAPI_CLONE_NEWCGROUP) == UAPI_CLONE_NEWCGROUP) ? std::make_shared<ControlGroupNamespace>() : rhs->m_cgroupns;
	m_ipcns = ((flags & UAPI_CLONE_NEWIPC) == UAPI_CLONE_NEWIPC) ? std::make_shared<IpcNamespace>() : rhs->m_ipcns;
	m_mountns = ((flags & UAPI_CLONE_NEWNS) == UAPI_CLONE_NEWNS) ? std::make_shared<MountNamespace>() : rhs->m_mountns;
	m_netns = ((flags & UAPI_CLONE_NEWNET) == UAPI_CLONE_NEWNET) ? std::make_shared<NetworkNamespace>() : rhs->m_netns;
	m_pidns = ((flags & UAPI_CLONE_NEWPID) == UAPI_CLONE_NEWPID) ? std::make_shared<PidNamespace>() : rhs->m_pidns;
	m_userns = ((flags & UAPI_CLONE_NEWUSER) == UAPI_CLONE_NEWUSER) ? std::make_shared<UserNamespace>() : rhs->m_userns;
	m_utsns = ((flags & UAPI_CLONE_NEWUTS) == UAPI_CLONE_NEWUTS) ? std::make_shared<UtsNamespace>() : rhs->m_utsns;
}

//---------------------------------------------------------------------------
// Namespace::AddMount
//
// Adds a mount for a file system to this namespace
//
// Arguments:
//
//	fs		- VirtualMachine::FileSystem instance to add as a mount

void Namespace::AddMount(VirtualMachine::FileSystem const* fs)
{
	UNREFERENCED_PARAMETER(fs);
}


//---------------------------------------------------------------------------

#pragma warning(pop)
