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

// VirtualMachine::MountFlags::ReadOnly (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::ReadOnly { UAPI_MS_RDONLY };

// VirtualMachine::MountFlags::NoSetUserId (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::NoSetUserId { UAPI_MS_NOSUID };

// VirtualMachine::MountFlags::NoDevices (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::NoDevices { UAPI_MS_NODEV };

// VirtualMachine::MountFlags::NoExecute (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::NoExecute { UAPI_MS_NOEXEC };

// VirtualMachine::MountFlags::Synchronous (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Synchronous { UAPI_MS_SYNCHRONOUS };

// VirtualMachine::MountFlags::Remount (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Remount { UAPI_MS_REMOUNT };

// VirtualMachine::MountFlags::MandatoryLocks (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::MandatoryLocks { UAPI_MS_MANDLOCK };

// VirtualMachine::MountFlags::SynchronousDirectories (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::SynchronousDirectories { UAPI_MS_DIRSYNC };

// VirtualMachine::MountFlags::NoUpdateAccessTimeStamps (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::NoUpdateAccessTimeStamps { UAPI_MS_NOATIME };

// VirtualMachine::MountFlags::NoUpdateDirectoryAccessTimeStampls(static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::NoUpdateDirectoryAccessTimeStamps { UAPI_MS_NODIRATIME };

// VirtualMachine::MountFlags::Bind (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Bind { UAPI_MS_BIND };

// VirtualMachine::MountFlags::Move (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Move { UAPI_MS_MOVE };

// VirtualMachine::MountFlags::RecursiveBind (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::RecursiveBind { UAPI_MS_REC };

// VirtualMachine::MountFlags::Silent (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Silent { UAPI_MS_SILENT };

// VirtualMachine::MountFlags::PosixAccessControlLists (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::PosixAccessControlLists { UAPI_MS_POSIXACL };

// VirtualMachine::MountFlags::Unbindable (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Unbindable { UAPI_MS_UNBINDABLE };

// VirtualMachine::ClonMountFlagseFlags::Private (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Private { UAPI_MS_PRIVATE };

// VirtualMachine::MountFlags::Slave (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Slave { UAPI_MS_SLAVE };

// VirtualMachine::MountFlags::Shared (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::Shared { UAPI_MS_SHARED };

// VirtualMachine::MountFlags::RelativeAccessTimeStamps (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::RelativeAccessTimeStamps { UAPI_MS_RELATIME };

// VirtualMachine::MountFlags::KernelMount (static)
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::KernelMount { UAPI_MS_KERNMOUNT };

// VirtualMachine::MountFlags::IncrementNodeVersions (static)
// 
VirtualMachine::MountFlags const VirtualMachine::MountFlags::IncrementNodeVersions { UAPI_MS_I_VERSION };

// VirtualMachine::MountFlags::StrictAccessTimeStamps (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::StrictAccessTimeStamps { UAPI_MS_STRICTATIME };

// VirtualMachine::MountFlags::LasyTimeStamps (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::LazyTimeStamps { UAPI_MS_LAZYTIME };

// VirtualMachine::MountFlags::RemountMask (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::RemountMask { UAPI_MS_RMT_MASK };

// VirtualMachine::MountFlags::PerMountMask (static)
//
VirtualMachine::MountFlags const VirtualMachine::MountFlags::PerMountMask { UAPI_MS_NODEV | UAPI_MS_NOEXEC | UAPI_MS_NOSUID | UAPI_MS_NOATIME | UAPI_MS_NODIRATIME | UAPI_MS_RELATIME };

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

//
// VIRTUALMACHINE::UNMOUNTFLAGS
//

// VirtualMachine::UnmountFlags::Force (static)
//
VirtualMachine::UnmountFlags const VirtualMachine::UnmountFlags::Force { 0x00000001 };		// MNT_FORCE

// VirtualMachine::UnmountFlags::Detach (static)
//
VirtualMachine::UnmountFlags const VirtualMachine::UnmountFlags::Detach { 0x00000002 };		// MNT_DETACH

// VirtualMachine::UnmountFlags::Expire (static)
//
VirtualMachine::UnmountFlags const VirtualMachine::UnmountFlags::Expire { 0x00000004 };		// MNT_EXPIRE

// VirtualMachine::UnmountFlags::NoFollow (static)
//
VirtualMachine::UnmountFlags const VirtualMachine::UnmountFlags::NoFollow { 0x00000008 };	// UMOUNT_NOFOLLOW

//-----------------------------------------------------------------------------

#pragma warning(pop)
