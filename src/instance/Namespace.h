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
//	CONTROLGROUP	- Isolates process resource usage
//	IPC				- Isolates System V IPC and posix message queues
//	MOUNT			- Isolates file system mount points
//	NETWORK			- Isolates network devices, ports, stacks, etc.
//	PID				- Isolates process identifiers
//	USER			- Isolates user and group identifiers
//	UTS				- Isolates host and domain name strings

class Namespace
{
	// FORWARD DECLARATIONS
	//
	struct mountns_t;
	struct path_t;

public:

	// Instance Constructors
	//
	Namespace();
	Namespace(Namespace const* rhs, uint32_t flags);

	// Destructor
	//
	~Namespace()=default;

	// Path
	//
	// todo words
	class Path
	{
	public:

		// Instance Constructor
		//
		// todo this can't be public
		Path(std::shared_ptr<path_t> const& path);

		// Destructor
		//
		~Path()=default;

	private:

		Path(Path const&)=delete;
		Path& operator=(Path const&)=delete;

		//---------------------------------------------------------------------
		// Member Variables

		std::shared_ptr<path_t>		m_path;			// Internal shared state
	};

	//-------------------------------------------------------------------------
	// Member Functions

	// LookupPath
	//
	// Performs a path name lookup operation
	std::unique_ptr<Path> LookupPath(void) const;

	//// MountFileSystem
	////
	//// Mounts a file system within this namespace at the specified location
	//std::unique_ptr<VirtualMachine::Path> MountFileSystem(char_t const* path, VirtualMachine::FileSystem* fs, uint32_t flags, void const* data, size_t datalength);

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
		// default constructor
		//
		path_t()=default;

		// converting constructor
		//
		path_t(std::shared_ptr<path_t> const& rhs);

		// canonicalparent
		//
		// Pointer to the canonical path_t, or nullptr if root
		std::shared_ptr<path_t> canonicalparent;

		// hides
		//
		// Pointer to the path_t hidden by a mount point
		std::shared_ptr<path_t> hides;

		// name
		//
		// Name of the node pointed to by this path
		std::string	name;

		// parent
		//
		// Pointer to the parent path_t, or nullptr if root
		std::shared_ptr<path_t> parent;
	};

	// mountns_t
	//
	// Provides an isolated view of file system mounts
	struct mountns_t
	{
		// default constructor
		//
		mountns_t()=default;

		// converting constructor
		//
		mountns_t(std::shared_ptr<mountns_t> const& rhs);

		// mounts
		//
		// Collection of mount points active in this namespace
		std::unordered_map<std::shared_ptr<path_t>, std::unique_ptr<VirtualMachine::Mount>> mounts;

		// mountslock
		//
		// Synchronization object
		sync::reader_writer_lock mountslock;

		mountns_t(mountns_t const&)=delete;
		mountns_t operator=(mountns_t const&)=delete;
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

		//// MountFileSystem
		////
		//// Mounts a file system at the specified location in the namespace
		//std::unique_ptr<VirtualMachine::Path> MountFileSystem(char_t const* path, VirtualMachine::FileSystem* fs, uint32_t flags, void const* data, size_t datalength);

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

	//-------------------------------------------------------------------------
	// Private Member Functions

	//-------------------------------------------------------------------------
	// Member Variables

	std::shared_ptr<mountns_t>			m_mountns;		// Shared mount namespace
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __NAMESPACE_H_
