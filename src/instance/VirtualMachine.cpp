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

//
// VIRTUALMACHINE::ALLOCATIONFLAGS
//

// VirtualMachine::AllocationFlags::None (static)
//
VirtualMachine::AllocationFlags const VirtualMachine::AllocationFlags::None { 0x00 };

// VirtualMachine::AllocationFlags::TopDown (static)
//
VirtualMachine::AllocationFlags const VirtualMachine::AllocationFlags::TopDown { 0x01 };

//
// VIRTUALMACHINE::CLONEFLAGS
//

// VirtualMachine::ProtectionFlags::None (static)
//
VirtualMachine::CloneFlags const VirtualMachine::CloneFlags::None { 0x00 };

// VirtualMachine::CloneFlags::NewMountNamespace (static)
//
VirtualMachine::CloneFlags const VirtualMachine::CloneFlags::NewMountNamespace { UAPI_CLONE_NEWNS };

// VirtualMachine::CloneFlags::NewControlGroupNamespace (static)
//
VirtualMachine::CloneFlags const VirtualMachine::CloneFlags::NewControlGroupNamespace { UAPI_CLONE_NEWCGROUP };

// VirtualMachine::CloneFlags::NewUtsNamespace (static)
//
VirtualMachine::CloneFlags const VirtualMachine::CloneFlags::NewUtsNamespace { UAPI_CLONE_NEWUTS };

// VirtualMachine::CloneFlags::NewIpcNamespace (static)
//
VirtualMachine::CloneFlags const VirtualMachine::CloneFlags::NewIpcNamespace { UAPI_CLONE_NEWIPC };	

// VirtualMachine::CloneFlags::NewUserNamespace (static)
//
VirtualMachine::CloneFlags const VirtualMachine::CloneFlags::NewUserNamespace { UAPI_CLONE_NEWUSER };

// VirtualMachine::CloneFlags::NewPidNamespace (static)
//
VirtualMachine::CloneFlags const VirtualMachine::CloneFlags::NewPidNamespace { UAPI_CLONE_NEWPID };	

// VirtualMachine::CloneFlags::NewNetworkNamespace (static)
//
VirtualMachine::CloneFlags const VirtualMachine::CloneFlags::NewNetworkNamespace { UAPI_CLONE_NEWNET };

//
// VIRTUALMACHINE::MOUNTFLAGS
//

// VirtualMachine::CloneFlags::ReadOnly (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::ReadOnly { UAPI_MS_RDONLY };

// VirtualMachine::CloneFlags::NoSetUserId (static)
//
// Do not honor set-user-ID and set-group-ID bits
VirtualMachine::MountFlags const VirtualMachine::MountFlags::NoSetUserId { UAPI_MS_NOSUID };

// VirtualMachine::CloneFlags::NoDevices (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::NoDevices { UAPI_MS_NODEV };

// VirtualMachine::CloneFlags::NoExecute (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::NoExecute { UAPI_MS_NOEXEC };

// VirtualMachine::CloneFlags::Synchronous (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Synchronous { UAPI_MS_SYNCHRONOUS };

// VirtualMachine::CloneFlags::Remount (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Remount { UAPI_MS_REMOUNT };

// VirtualMachine::CloneFlags::MandatoryLocks (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::MandatoryLocks { UAPI_MS_MANDLOCK };

// VirtualMachine::CloneFlags::SynchronousDirectories (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::SynchronousDirectories { UAPI_MS_DIRSYNC };

// VirtualMachine::CloneFlags::NoUpdateAccessTimeStamps (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::NoUpdateAccessTimeStamps { UAPI_MS_NOATIME };

// VirtualMachine::CloneFlags::NoUpdateDirectoryAccessTimeStampls(static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::NoUpdateDirectoryAccessTimeStamps { UAPI_MS_NODIRATIME };

// VirtualMachine::CloneFlags::Bind (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Bind { UAPI_MS_BIND };

// VirtualMachine::CloneFlags::Move (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Move { UAPI_MS_MOVE };

// VirtualMachine::CloneFlags::RecursiveBind (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::RecursiveBind { UAPI_MS_REC };

// VirtualMachine::CloneFlags::Silent (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Silent { UAPI_MS_SILENT };

// VirtualMachine::CloneFlags::PosixAccessControlLists (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::PosixAccessControlLists { UAPI_MS_POSIXACL };

// VirtualMachine::CloneFlags::Unbindable (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Unbindable { UAPI_MS_UNBINDABLE };

// VirtualMachine::CloneFlags::Private (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Private { UAPI_MS_PRIVATE };

// VirtualMachine::CloneFlags::Slave (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Slave { UAPI_MS_SLAVE };

// VirtualMachine::CloneFlags::Shared (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Shared { UAPI_MS_SHARED };

// VirtualMachine::CloneFlags::RelativeAccessTimeStamps (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::RelativeAccessTimeStamps { UAPI_MS_RELATIME };

// VirtualMachine::CloneFlags::KernelMount (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::KernelMount { UAPI_MS_KERNMOUNT };

// VirtualMachine::CloneFlags::IncrementNodeVersions (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::IncrementNodeVersions { UAPI_MS_I_VERSION };

// VirtualMachine::CloneFlags::StrictAccessTimeStamps (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::StrictAccessTimeStamps { UAPI_MS_STRICTATIME };

// VirtualMachine::CloneFlags::LasyTimeStamps (static)
//
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::LazyTimeStamps { UAPI_MS_LAZYTIME };

//
// VIRTUALMACHINE::PROTECTIONFLAGS
//

// VirtualMachine::ProtectionFlags::None (static)
//
VirtualMachine::ProtectionFlags const VirtualMachine::ProtectionFlags::None { 0x00 };

// VirtualMachine::ProtectionFlags::Execute (static)
//
VirtualMachine::ProtectionFlags const VirtualMachine::ProtectionFlags::Execute { 0x01 };

// VirtualMachine::ProtectionFlags::Read (static)
//
VirtualMachine::ProtectionFlags const VirtualMachine::ProtectionFlags::Read { 0x02 };

// VirtualMachine::ProtectionFlags::Write (static)
//
VirtualMachine::ProtectionFlags const VirtualMachine::ProtectionFlags::Write { 0x04 };

// VirtualMachine::ProtectionFlags::Guard (static)
//
VirtualMachine::ProtectionFlags const VirtualMachine::ProtectionFlags::Guard { 0x80 };

//-----------------------------------------------------------------------------

#pragma warning(pop)
