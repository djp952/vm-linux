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

#ifndef __ROOTFILESYSTEM_H_
#define __ROOTFILESYSTEM_H_
#pragma once

#include <atomic>
#include <memory>
#include <text.h>

#include "VirtualMachine.h"

#pragma warning(push, 4)

// FORWARD DECLARATIONS
//
class MountOptions;

// MountRootFileSystem
//
// VirtualMachine::MountFunction for RootFileSystem
std::unique_ptr<VirtualMachine::FileSystem> MountRootFileSystem(char_t const* source, VirtualMachine::MountFlags flags, void const* data, size_t datalength);

//-----------------------------------------------------------------------------
// Class RootFileSystem
//
// RootFileSystem implements a virtual single directory node file system in which
// no child nodes can be created
//
// Supported mount options:
//
//	MS_KERNMOUNT
//	MS_NOATIME
//	MS_NODIRATIME
//	MS_RDONLY
//	MS_RELATIME
//	MS_STRICTATIME
//
//	mode=nnn	- Sets the permissions of the directory node
//	uid=nnn		- Sets the owner user id of the directory node
//	gid=nnn		- Sets the owner group id of the directory node
//	
//	(MS_NODEV, MS_NOEXEC and MS_NOSUID are always set)
//
// Supported remount options:
//
//	MS_RDONLY

class RootFileSystem : public VirtualMachine::FileSystem
{
public:

	// Instance Constructor
	//
	RootFileSystem(MountOptions const& options);

	// Destructor
	//
	virtual ~RootFileSystem()=default;

	//-----------------------------------------------------------------------------
	// Member Functions

private:

	RootFileSystem(RootFileSystem const&)=delete;
	RootFileSystem& operator=(RootFileSystem const&)=delete;

	// DirectoryNode
	//
	// Implements a directory node for this file system
	class DirectoryNode : public VirtualMachine::Directory
	{
	public:

		// Instance Constructor
		//
		DirectoryNode(uapi_mode_t mode, uapi_uid_t uid, uapi_gid_t gid);

		// Destructor
		//
		virtual ~DirectoryNode()=default;

		// OwnerGroupId (VirtualMachine::Node)
		//
		// Gets the node owner group identifier
		__declspec(property(get=getOwnerGroupId)) uapi_gid_t OwnerGroupId;
		virtual uapi_gid_t getOwnerGroupId(void) const override;

		// OwnerUserId (VirtualMachine::Node)
		//
		// Gets the node owner user identifier 
		__declspec(property(get=getOwnerUserId)) uapi_uid_t OwnerUserId;
		virtual uapi_uid_t getOwnerUserId(void) const override;

		// Permissions (VirtualMachine::Node)
		//
		// Gets the node permissions mask
		__declspec(property(get=getPermissions)) uapi_mode_t Permissions;
		virtual uapi_mode_t getPermissions(void) const override;

	private:

		DirectoryNode(DirectoryNode const&)=delete;
		DirectoryNode& operator=(DirectoryNode const&)=delete;

		//---------------------------------------------------------------------
		// Member Variables

		std::atomic<uapi_mode_t>		m_mode;		// Permission mask
		std::atomic<uapi_uid_t>			m_uid;		// Owner id
		std::atomic<uapi_gid_t>			m_gid;		// Owner group id
	};

	//-------------------------------------------------------------------------
	// Member Variables

	std::unique_ptr<DirectoryNode>		m_node;		// The root directory node
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __ROOTFILESYSTEM_H_
