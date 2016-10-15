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
	Namespace(std::unique_ptr<VirtualMachine::Mount>&& rootmount);
	Namespace(Namespace const* rhs, uint32_t flags);

	// Destructor
	//
	~Namespace()=default;

	// Path
	//
	// Represents a file system path within the namespace
	class Path
	{
	friend class Namespace;
	public:

		// Destructor
		//
		~Path()=default;

		//-------------------------------------------------------------------
		// Properties

		// Mount
		//
		// Accesses the underlying mount instance
		__declspec(property(get=getMount)) VirtualMachine::Mount* Mount;
		VirtualMachine::Mount* getMount(void) const;

		// Node
		//
		// Accesses the underlying node instance
		__declspec(property(get=getNode)) VirtualMachine::Node* Node;
		VirtualMachine::Node* getNode(void) const;

	private:

		Path(Path const&)=delete;
		Path& operator=(Path const&)=delete;

		// Instance Constructor
		//
		Path(std::shared_ptr<path_t> const& path);

		//---------------------------------------------------------------------
		// Member Variables

		std::shared_ptr<path_t>					m_path;		// Internal shared state
		std::unique_ptr<VirtualMachine::Node>	m_node;		// Node instance
	};

	//-------------------------------------------------------------------------
	// Member Functions

	// AddMount
	//
	// Adds a new mount point to the namespace
	std::unique_ptr<Path> AddMount(std::unique_ptr<VirtualMachine::Mount>&& mount, Path const* path);

	// GetRootPath
	//
	// Gets the namespace root path
	std::unique_ptr<Path> GetRootPath(void) const;

	// LookupPath
	//
	// Performs a path name lookup operation
	std::unique_ptr<Path> LookupPath(Path const* working, char_t const* path, uint32_t flags) const;

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

		// mount
		//
		// Pointer to the mount point for this path
		std::shared_ptr<VirtualMachine::Mount> mount;

		// name
		//
		// Name of the node pointed to by this path
		std::string	name;

		// node
		//
		// Pointer to the node that the path references
		std::shared_ptr<VirtualMachine::Node> node;

		// parent
		//
		// Pointer to the parent path_t, or nullptr if root
		std::shared_ptr<path_t> parent;
	};

	// equals_path_t
	//
	// Equality algorthm used for hash-based collections of path_t instances
	struct equals_path_t
	{
		bool operator()(std::shared_ptr<path_t> const& lhs, std::shared_ptr<path_t> const& rhs) const;
	};

	// hash_path_t
	//
	// Hash algorithm used for hash-based collections of path_t instances
	struct hash_path_t
	{
		size_t operator()(std::shared_ptr<path_t> const& key) const;
	};

	// mountmap_t
	//
	// Type defintion for an unordered_map<> collection of mount points
	using mountmap_t = std::unordered_map<std::shared_ptr<path_t>, std::shared_ptr<VirtualMachine::Mount>, hash_path_t, equals_path_t>;

	//-------------------------------------------------------------------------
	// Private Member Functions

	// LookupPath
	//
	// Performs a path name lookup operation
	std::shared_ptr<path_t> LookupPath(sync::reader_writer_lock::scoped_lock& lock, std::shared_ptr<path_t> const& working, 
		char_t const* path, uint32_t flags, int* numlinks) const;

	//-------------------------------------------------------------------------
	// Member Variables

	std::shared_ptr<path_t>				m_rootpath;		// Namespace root path
	mountmap_t							m_mounts;		// Collection of mount points
	mutable sync::reader_writer_lock	m_mountslock;	// Synchronization object
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __NAMESPACE_H_
