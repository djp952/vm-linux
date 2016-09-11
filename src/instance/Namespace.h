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

#ifndef __NAMESPACE_H_
#define __NAMESPACE_H_
#pragma once

#include <memory>
#include <unordered_map>
#include <sync.h>

#include "VirtualMachine.h"

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// Namespace
//
// Wraps certain global system resources into an abstraction that makes it appear 
// to any processes within the namespace that they have their own isolated instances 
// of these resources:
//
//	ControlGroup	- Isolates process resource usage
//	Ipc				- Isolates System V IPC and posix message queues
//	Mount			- Isolates file system mount points
//	Network			- Isolates network devices, ports, stacks, etc.
//	Pid				- Isolates process identifiers
//	User			- Isolates user and group identifiers
//	Uts				- Isolates host and domain name strings

class Namespace
{
public:

	// Instance Constructors
	//
	Namespace();
	Namespace(Namespace const* rhs, uint32_t flags);

	// Destructor
	//
	~Namespace()=default;

	//-------------------------------------------------------------------------
	// Member Functions

	// LookupPath
	//
	// Performs a path name lookup operation
	std::unique_ptr<VirtualMachine::Path> LookupPath(void) const;

	// MountFileSystem
	//
	// Mounts a file system within this namespace at the specified location
	std::unique_ptr<VirtualMachine::Path> MountFileSystem(char_t const* path, VirtualMachine::FileSystem* fs, uint32_t flags, void const* data, size_t datalength);

	//-------------------------------------------------------------------------
	// Properties

private:

	Namespace(Namespace const&)=delete;
	Namespace& operator=(Namespace const&)=delete;

	// path_t
	//
	// Internal shared representation of a Path instance
	struct path_t
	{
		// canonicalparent
		//
		// Pointer to the canonical path_t, or nullptr if root
		std::shared_ptr<path_t> canonicalparent;

		// hides
		//
		// Pointer to the path_t hidden by a mount point
		std::shared_ptr<path_t> hides;

		// mount
		//
		// Pointer to the VirtualMachine::Mount if this path is a mount point
		std::unique_ptr<VirtualMachine::Mount> mount;

		// name
		//
		// Name of the node pointed to by this path
		std::string	name;

		// parent
		//
		// Pointer to the parent path_t, or nullptr if root
		std::shared_ptr<path_t> parent;
	};

	// ControlGroupNamespace
	//
	// Provides an isolated view of process resource usage
	class ControlGroupNamespace
	{
	public:

		ControlGroupNamespace()=default;
		~ControlGroupNamespace()=default;

	private:

		ControlGroupNamespace(ControlGroupNamespace const&)=delete;
		ControlGroupNamespace& operator=(ControlGroupNamespace const&)=delete;
	};

	// IpcNamespace
	//
	// Provides an isolated view of System V IPC / Posix message queues
	class IpcNamespace
	{
	public:

		IpcNamespace()=default;
		~IpcNamespace()=default;

	private:

		IpcNamespace(IpcNamespace const&)=delete;
		IpcNamespace& operator=(IpcNamespace const&)=delete;
	};

	// MountNamespace
	//
	// Provides an isolated view of file system mounts
	class MountNamespace
	{
	public:

		// Instance Constructor
		//
		MountNamespace();

		// Destructor
		//
		~MountNamespace()=default;

		//---------------------------------------------------------------------
		// Member Functions

		// MountFileSystem
		//
		// Mounts a file system at the specified location in the namespace
		std::unique_ptr<VirtualMachine::Path> MountFileSystem(char_t const* path, VirtualMachine::FileSystem* fs, uint32_t flags, void const* data, size_t datalength);

	private:

		MountNamespace(MountNamespace const&)=delete;
		MountNamespace& operator=(MountNamespace const&)=delete;

		// mount_map_t
		//
		// Collection of mounted path_t instances, which keeps them as well as any
		// parent path_t instances alive until it's unmounted/removed from the namespace
		using mount_map_t = std::unordered_map<std::string, std::shared_ptr<path_t>>;

		//---------------------------------------------------------------------
		// Private Member Functions

		// GetCanonicalPathString (static)
		//
		// Gets the canonical path string of a path_t instance
		static std::string GetCanonicalPathString(std::shared_ptr<path_t> const& path);
		
		// GetPath
		//
		// Converts a string-based path into a path_t instance
		std::shared_ptr<path_t> GetPath(char_t const* path) const;

		// GetPathString (static)
		//
		// Gets the path string of a path_t instance
		static std::string GetPathString(std::shared_ptr<path_t> const& path);

		//---------------------------------------------------------------------
		// Member Variables

		mount_map_t					m_mounts;			// Collection of mount points
		sync::reader_writer_lock	m_mountslock;		// Synchronization object
	};

	// NetworkNamespace
	//
	// Provides an isolated view of network devices, stacks, ports, etc
	class NetworkNamespace
	{
	public:

		NetworkNamespace()=default;
		~NetworkNamespace()=default;

	private:

		NetworkNamespace(NetworkNamespace const&)=delete;
		NetworkNamespace& operator=(NetworkNamespace const&)=delete;
	};

	// Path
	//
	// Implements VirtualMachine::Path
	class Path : public VirtualMachine::Path
	{
	public:

		// Instance Constructor
		//
		Path(std::shared_ptr<path_t> const& path);

		// Destructor
		//
		~Path()=default;

		// Duplicate (VirtualMachine::Path)
		//
		// Duplicates this Path instance
		virtual std::unique_ptr<VirtualMachine::Path> Duplicate(void) override;

	private:

		Path(Path const&)=delete;
		Path& operator=(Path const&)=delete;

		//---------------------------------------------------------------------
		// Member Variables

		std::shared_ptr<path_t>		m_path;			// Internal shared state
	};

	// PidNamespace
	//
	// Provides an isolated process id number space. See pid_namespace(7).
	class PidNamespace
	{
	public:

		PidNamespace()=default;
		~PidNamespace()=default;

	private:

		PidNamespace(PidNamespace const&)=delete;
		PidNamespace& operator=(PidNamespace const&)=delete;
	};

	// UserNamespace
	//
	// Provides isolation of user and group identifiers 
	class UserNamespace
	{
	public:

		UserNamespace()=default;
		~UserNamespace()=default;

	private:

		UserNamespace(UserNamespace const&)=delete;
		UserNamespace& operator=(UserNamespace const&)=delete;
	};

	// UtsNamespace
	//
	// Provides isolation of host and domain name identifiers 
	class UtsNamespace
	{
	public:

		UtsNamespace()=default;
		~UtsNamespace()=default;

	private:

		UtsNamespace(UtsNamespace const&)=delete;
		UtsNamespace& operator=(UtsNamespace const&)=delete;
	};

	//-------------------------------------------------------------------------
	// Private Member Functions

	//-------------------------------------------------------------------------
	// Member Variables

	std::shared_ptr<ControlGroupNamespace>	m_cgroupns;		// Shared ControlGroupNamespace instance
	std::shared_ptr<IpcNamespace>			m_ipcns;		// Shared IpcNamespace instance
	std::shared_ptr<MountNamespace>			m_mountns;		// Shared MountNamespace instance
	std::shared_ptr<NetworkNamespace>		m_netns;		// Shared NetworkNamespace instance
	std::shared_ptr<PidNamespace>			m_pidns;		// Shared PidNamespace instance
	std::shared_ptr<UserNamespace>			m_userns;		// Shared UserNamespace instance
	std::shared_ptr<UtsNamespace>			m_utsns;		// Shared UtsNamespace instance
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __NAMESPACE_H_
