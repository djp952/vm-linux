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

#include <path.h>
#include <vector>
#include "LinuxException.h"

#pragma warning(push, 4)

//---------------------------------------------------------------------------
// Namespace Constructor
//
// Arguments:
//
//	rootmount		- The namespace root mount

Namespace::Namespace(std::unique_ptr<VirtualMachine::Mount>&& rootmount)
{
	// Convert the provided root mount instance into a shared_ptr<>
	std::shared_ptr<VirtualMachine::Mount> mountpoint(std::move(rootmount));

	// Insert the mount into the mounts collection with a null path to indicate that
	// it is the absolute root of the namespace file system.  (Using nullptr prevents
	// the node from ever being matched during overmount lookups -- see equals_path_t())
	auto result = m_mounts.emplace(nullptr, mountpoint);
	if(!result.second) throw LinuxException(UAPI_ENOMEM);

	// Create a root "/" path_t instance that can be accessed for path lookups that
	// initially refers to the absolute root of the namespace file system
	m_rootpath = std::make_shared<path_t>();
	m_rootpath->mount = mountpoint;
	m_rootpath->node = mountpoint->RootNode->Duplicate();
	m_rootpath->name = "/";
	m_rootpath->parent = nullptr;
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
	UNREFERENCED_PARAMETER(rhs);
	UNREFERENCED_PARAMETER(flags);

	// todo: clone the internal namespace(s)
}

//---------------------------------------------------------------------------
// Namespace::AddMount
//
// Adds a mount point to this namespace
//
// Arguments:
//
//	mount		- Mount instance to be added (takes ownership)
//	path		- Path on which to apply the mount point

std::unique_ptr<Namespace::Path> Namespace::AddMount(std::unique_ptr<VirtualMachine::Mount>&& mount, Path const* path)
{
	// Convert the provided mount point into a shared_ptr<>
	std::shared_ptr<VirtualMachine::Mount> mountpoint(std::move(mount));

	// Copy the provided path into a new path_t that refers to the mount point
	auto mountpath = std::make_shared<path_t>();
	mountpath->mount = mountpoint;
	mountpath->name = path->m_path->name;
	mountpath->node = mountpoint->RootNode->Duplicate();
	mountpath->parent = path->m_path->parent;

	// Acquire an exclusive lock against the mount collection
	sync::reader_writer_lock::scoped_lock_write writer(m_mountslock);

	// Insert the mount point into the collection using the ORIGINAL path instance, this way
	// way whenever that ORIGINAL path instance is discovered it can be replaced with the mount
	auto result = m_mounts.emplace(path->m_path, mountpoint);
	if(!result.second) throw LinuxException(UAPI_ENOMEM);

	// Return the newly constructed path_t to the caller as a Path instance
	//return std::make_unique<Path>(mountpath);
	return std::unique_ptr<Path>(new Path(mountpath));
}

//---------------------------------------------------------------------------
// Namespace::GetRootPath
//
// Gets the namespace root path
//
// Arguments:
//
//	NONE

std::unique_ptr<Namespace::Path> Namespace::GetRootPath(void) const
{
	sync::reader_writer_lock::scoped_lock_read reader(m_mountslock);
	return std::unique_ptr<Path>(new Path(m_rootpath));
}

//---------------------------------------------------------------------------
// Namespace::LookupPath
//
// Performs a path name lookup operation
//
// Arguments:
//
//	working			- Current working directory path
//	path			- Path to be looked up
//	flags			- Lookup flags (O_DIRECTORY, O_NOFOLLOW, etc)

std::unique_ptr<Namespace::Path> Namespace::LookupPath(Path const* working, char_t const* path, uint32_t flags) const
{
	int numlinks = 0;							// Number of encountered symbolic links

	if(working == nullptr) throw LinuxException(UAPI_EFAULT);
	if(path == nullptr) throw LinuxException(UAPI_EFAULT);

	sync::reader_writer_lock::scoped_lock_read reader(m_mountslock);

	// Hit the internal version of LookupPath that accepts shared_ptr<path_t>
	return std::unique_ptr<Path>(new Path(LookupPath(reader, working->m_path, path, flags, &numlinks)));
}

//---------------------------------------------------------------------------
// Namespace::LookupPath (private)
//
// Performs a path name lookup operation.  This is the internal version that
// requires a lock be held against the mount namespace elements
//
// Arguments:
//
//	lock		- Ensures that the caller holds a lock against the mounts
//	current		- Reference to the current path_t
//	path		- Remaining path to be looked up
//	flags		- Lookup operation flags (O_DIRECTORY, O_NOFOLLOW, etc)
//	numlinks	- Running count of symbolic links encountered

std::shared_ptr<Namespace::path_t> Namespace::LookupPath(sync::reader_writer_lock::scoped_lock& lock, std::shared_ptr<path_t> const& working, 
	char_t const* path, uint32_t flags, int* numlinks) const
{
	mountmap_t::const_iterator		mountpoint;			// Mount collection iterator
	posix_path						lookuppath(path);	// Convert into a posix_path

	UNREFERENCED_PARAMETER(lock);		// Unused; ensures the caller holds a scoped_lock

	if(path == nullptr) throw LinuxException(UAPI_EFAULT);
	if(numlinks == nullptr) throw LinuxException(UAPI_EFAULT);

	// Clone either the working path_t or the namespace root path_t as the starting point
	auto current = std::make_shared<path_t>((lookuppath.absolute()) ? m_rootpath : working);

	// Handle any mount points stacked on top of the starting node by switching the mount and
	// node pointers appropriately; this does not change the name or the parent pointer
	mountpoint = m_mounts.find(current);
	while(mountpoint != m_mounts.end()) { 

		current->mount = mountpoint->second;
		current->node = mountpoint->second->RootNode->Duplicate();
		mountpoint = m_mounts.find(current);
	}

	// Iterate over each component of the lookup path and build out the resultant path_t
	for(auto const& iterator : lookuppath) {

		// SELF [.]: skip the path component
		if(strcmp(iterator, ".") == 0) continue;

		// PARENT [..] move current to its parent if there is one
		else if(strcmp(iterator, "..") == 0) { if(current->parent) current = current->parent; }

		// ROOT [/]: move current to the namespace root
		else if(strcmp(iterator, "/") == 0) current = m_rootpath;

		// DIRECTORY LOOKUP
		else if((current->node->Mode & UAPI_S_IFMT) == UAPI_S_IFDIR) {

			auto directory = std::dynamic_pointer_cast<VirtualMachine::Directory>(current->node);
			if(directory == nullptr) throw LinuxException(UAPI_ENOTDIR);

			// Create a new path_t for the child that uses the directory as its parent
			auto child = std::make_shared<path_t>();
			child->mount = current->mount;
			child->parent = current;
			child->name = iterator;
			child->node = directory->Lookup(current->mount.get(), iterator);

			current = child;			// move to the child node
		}

		// FOLLOW SYMBOLIC LINK
		else if((current->node->Mode & UAPI_S_IFMT) == UAPI_S_IFLNK) {
		
			auto symlink = std::dynamic_pointer_cast<VirtualMachine::SymbolicLink>(current->node);
			if(symlink == nullptr) throw LinuxException(UAPI_ENOTDIR);

			// Ensure that the maximum number of symbolic links has not been reached
			if(++(*numlinks) > VirtualMachine::MaxSymbolicLinks) throw LinuxException(UAPI_ELOOP);

			// Read the symbolic link target (changed to a method to allow for access time updates)
			size_t length = symlink->Length;
			auto target = std::make_unique<char_t[]>(length + 1);
			symlink->ReadTarget(current->mount.get(), &target[0], length);
			target[length] = TEXT('\0');
			
			// Move current to the target of the symbolic link; note that the lookup is
			// relative to the symbolic link's parent, not the symbolic link itself
			_ASSERTE(current->parent);
			current = LookupPath(lock, current->parent, &target[0], flags, numlinks);
		}

		// LOOKUP ERROR
		else throw LinuxException(UAPI_ENOTDIR);

		// Bubble up any mount points stacked on top of the current node
		mountpoint = m_mounts.find(current);
		while(mountpoint != m_mounts.end()) { 

			current->mount = mountpoint->second;
			current->node = mountpoint->second->RootNode->Duplicate();
			mountpoint = m_mounts.find(current);
		}
	}

	// If the final node is a symbolic link, follow it unless O_NOFOLLOW was specified
	if(((current->node->Mode & UAPI_S_IFMT) == UAPI_S_IFLNK) && ((flags & UAPI_O_NOFOLLOW) == 0)) {

		auto symlink = std::dynamic_pointer_cast<VirtualMachine::SymbolicLink>(current->node);
		if(symlink == nullptr) throw LinuxException(UAPI_ENOTDIR);

		// Ensure that the maximum number of symbolic links has not been reached
		if(++(*numlinks) > VirtualMachine::MaxSymbolicLinks) throw LinuxException(UAPI_ELOOP);

		// Read the symbolic link target (changed to a method to allow for access time updates)
		size_t length = symlink->Length;
		auto target = std::make_unique<char_t[]>(length + 1);
		symlink->ReadTarget(current->mount.get(), &target[0], length);
		target[length] = TEXT('\0');

		// Move current to the target of the symbolic link; note that the lookup is
		// relative to the symbolic link's parent, not the symbolic link itself
		_ASSERTE(current->parent);
		current = LookupPath(lock, current->parent, &target[0], flags, numlinks);
	}

	// If O_DIRECTORY has been specified the final path component must be a directory node
	if((flags & UAPI_O_DIRECTORY) == UAPI_O_DIRECTORY) {

		if((current->node->Mode & UAPI_S_IFMT) != UAPI_S_IFDIR) throw LinuxException(UAPI_ENOTDIR);
	}

	return current;
}

//
// NAMESPACE::PATH IMPLEMENTATION
//

//---------------------------------------------------------------------------
// Namespace::Path Constructor (private)
//
// Arguments:
//
//	path		- Shared path_t instance of the path object

Namespace::Path::Path(std::shared_ptr<path_t> const& path) : m_path(path)
{
}

//---------------------------------------------------------------------------
// Namespace::Path::getMount
//
// Gets a pointer to the referenced mount instance

VirtualMachine::Mount* Namespace::Path::getMount(void) const
{
	return m_path->mount.get();
}
		
//---------------------------------------------------------------------------
// Namespace::Path::getName
//
// Gets the name of the node pointed by by this path

char_t const* Namespace::Path::getName(void) const
{
	return m_path->name.c_str();
}
		
//---------------------------------------------------------------------------
// Namespace::Path::getNode
//
// Gets a pointer to the referenced node instance

VirtualMachine::Node* Namespace::Path::getNode(void) const
{
	return m_path->node.get();
}
		
//
// NAMESPACE::PATH_T IMPLEMENTATION
//

//---------------------------------------------------------------------------
// Namespace::path_t Constructor
//
// Arguments:
//
//	rhs		- Right-hand shared_ptr<path_t> to clone into this path_t

Namespace::path_t::path_t(std::shared_ptr<path_t> const& rhs) : mount(rhs->mount), name(rhs->name), node(rhs->node), parent(rhs->parent)
{
}

//
// NAMESPACE::EQUALSPATH_T IMPLEMENTATION
//

//---------------------------------------------------------------------------
// Namespace::equals_path_t::operator()

bool Namespace::equals_path_t::operator()(std::shared_ptr<path_t> const& lhs, std::shared_ptr<path_t> const& rhs) const
{
	if((!lhs) || (!rhs)) return false;				// Neither shared_ptr<> can be null for this to work
	if(lhs.get() == rhs.get()) return true;			// Equal if the same underlying pointer

	return ((lhs->mount->FileSystem == rhs->mount->FileSystem) && (lhs->node->Index == rhs->node->Index));
}

//
// NAMESPACE::HASH_PATH_T IMPLEMENTATION
//

//---------------------------------------------------------------------------
// Namespace::hash_path_t::operator()

size_t Namespace::hash_path_t::operator()(std::shared_ptr<path_t> const& key) const
{
	if(!key) return 0;					// Null shared_ptr has no hash value

	// http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-source

#ifndef _M_X64
	// 32-bit FNV-1a hash
	const size_t fnv_offset_basis{ 2166136261U };
	const size_t fnv_prime{ 16777619U };
#else
	// 64-bit FNV-1a hash
	const size_t fnv_offset_basis{ 14695981039346656037ULL };
	const size_t fnv_prime{ 1099511628211ULL };
#endif

	// Calcuate the FNV-1a hash for this path_t instance; base it on the file system
	// instance pointer and the node unique identifier value on that file system
	size_t hash = fnv_offset_basis & (reinterpret_cast<uintptr_t>(key->mount->FileSystem) ^ key->node->Index);
	return hash * fnv_prime;
}

//---------------------------------------------------------------------------

#pragma warning(pop)
