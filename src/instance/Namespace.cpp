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

#include <vector>
#include "LinuxException.h"

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

	m_mountns = std::make_shared<mountns_t>();
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
	// namespace isolation points or get new copies of them, depending on the clone flags
	//
	m_mountns = ((flags & UAPI_CLONE_NEWNS) == UAPI_CLONE_NEWNS) ? std::make_shared<mountns_t>(rhs->m_mountns) : rhs->m_mountns;
}

//---------------------------------------------------------------------------
// Namespace::LookupPath
//
// Performs a path name lookup operation
//
// Arguments:
//
//	NONE

std::unique_ptr<Namespace::Path> Namespace::LookupPath(void) const
{
	_ASSERTE(m_mountns);
	return nullptr;
}

//---------------------------------------------------------------------------
// Namespace::MountFileSystem
//
// Mounts a file system within this namespace at the specified location
//
// Arguments:
//
//	path		- Path within the namespace on which to mount the file system
//	fs			- File system to be mounted
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

//std::unique_ptr<VirtualMachine::Path> Namespace::MountFileSystem(char_t const* path, VirtualMachine::FileSystem* fs, uint32_t flags, void const* data, size_t datalength)
//{
//	_ASSERTE(m_mountns);
//	return m_mountns->MountFileSystem(path, fs, flags, data, datalength);
//}

//
// NAMESPACE::MOUNTNAMESPACE IMPLEMENTATION
//

//---------------------------------------------------------------------------
// Namespace::MountNamespace Constructor
//
// Arguments:
//
//	NONE

Namespace::MountNamespace::MountNamespace()
{
}

//---------------------------------------------------------------------------
// Namespace::MountNamespace::GetCanonicalPathString (private, static)
//
// Converts a path_t into a canonical string-based path
//
// Arguments:
//
//	path		- path_t to be converted into a string

std::string Namespace::MountNamespace::GetCanonicalPathString(std::shared_ptr<path_t> const& path)
{
	std::string delimiter("/");					// Standard path delimiter

	// If the path has no canonical parent, this is the root -- return just the delimiter
	if(!path->canonicalparent) return delimiter;

	std::shared_ptr<path_t>					current(path);
	std::vector<std::shared_ptr<path_t>>	pathvec;
	std::string								result;

	// Push all the path_t pointers in the canonical path chain into the vector<>
	while(current->canonicalparent) { pathvec.push_back(current); current = current->canonicalparent; }

	// Walk the vector<> in forward order to generate the path string
	for(auto const& iterator : pathvec) result += (delimiter + iterator->name);

	return result;
}

//---------------------------------------------------------------------------
// Namespace::MountNamespace::GetPath (private)
//
// Converts a string-based path into a path_t using the current namespace
//
// Arguments:
//
//	path		- Path string to be converted

std::shared_ptr<Namespace::path_t> Namespace::MountNamespace::GetPath(char_t const* path) const
{
	UNREFERENCED_PARAMETER(path);
	return nullptr;
}

//---------------------------------------------------------------------------
// Namespace::MountNamespace::GetPathString (private, static)
//
// Converts a path_t into a (non-canonical) string-based path
//
// Arguments:
//
//	path		- path_t to be converted into a string

std::string Namespace::MountNamespace::GetPathString(std::shared_ptr<path_t> const& path)
{
	std::string delimiter("/");					// Standard path delimiter

	// If the path has no parent, this is the root -- return just the delimiter
	if(!path->parent) return delimiter;

	std::shared_ptr<path_t>					current(path);
	std::vector<std::shared_ptr<path_t>>	pathvec;
	std::string								result;

	// Push all the path_t pointers in the path chain into the vector<>
	while(current->parent) { pathvec.push_back(current); current = current->parent; }

	// Walk the vector<> in forward order to generate the path string
	for(auto const& iterator : pathvec) result += (delimiter + iterator->name);

	return result;
}

//---------------------------------------------------------------------------
// Namespace::MountNamespace::MountFileSystem
//
// Mounts a file system within this namespace at the specified location
//
// Arguments:
//
//	path		- Path within the namespace on which to mount the file system
//	fs			- File system to be mounted
//	flags		- Standard mounting option flags
//	data		- Extended/custom mounting options
//	datalength	- Length of the extended mounting options data

//std::unique_ptr<VirtualMachine::Path> Namespace::MountNamespace::MountFileSystem(char_t const* path, VirtualMachine::FileSystem* fs, uint32_t flags, void const* data, size_t datalength)
//{
//	if(fs == nullptr) throw LinuxException(UAPI_EFAULT);
//
//	// A pure root mount requires the MS_KERNMOUNT flag be set (User-mode mounts would specify "/" as the path)
//	if((path == nullptr) && ((flags & UAPI_MS_KERNMOUNT) != UAPI_MS_KERNMOUNT)) throw LinuxException(UAPI_EPERM);
//
//	sync::reader_writer_lock::scoped_lock_write writer(m_mountslock);
//
//	// Create the new path_t that will own the actual mount instance
//	std::shared_ptr<path_t> mountpoint = std::make_shared<path_t>();
//
//	try {
//
//		// Attempt to look up the existing path that this mountpoint will hide
//		mountpoint->hides = (path == nullptr) ? m_mounts.at("/") : GetPath(path);
//		if(!mountpoint->hides) throw LinuxException(UAPI_ENOENT);
//
//		// Copy the name, parent and canonical parent of the existing path_t into the new path_t
//		mountpoint->name = mountpoint->hides->name;
//		mountpoint->parent = mountpoint->hides->parent;
//		mountpoint->canonicalparent = mountpoint->hides->canonicalparent;
//	}
//
//	catch(std::out_of_range&) { /* m_mounts.at("/") failed -- no existing root mount */ }
//
//	// Assign the mount point to the path_t by invoking the file system mount function
//	mountpoint->mount = fs->Mount(flags, data, datalength);
//
//	// Add or replace the path_t instance for this mount in the collection
//	m_mounts[GetCanonicalPathString(mountpoint)] = mountpoint;
//
//	// Generate a Path instance for the caller that references the mount path_t
//	return std::make_unique<Path>(mountpoint);
//}

//
// NAMESPACE::PATH IMPLEMENTATION
//

//---------------------------------------------------------------------------
// Namespace::Path Constructor
//
// Arguments:
//
//	path		- Shared path_t instance of the path object

Namespace::Path::Path(std::shared_ptr<path_t> const& path) : m_path(path)
{
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

Namespace::path_t::path_t(std::shared_ptr<path_t> const& rhs) : canonicalparent(rhs->canonicalparent), 
	hides(rhs->hides), name(rhs->name), parent(rhs->parent)
{
}

//
// NAMESPACE::MOUNTNS_T IMPLEMENTATION
//

//---------------------------------------------------------------------------
// Namespace::mountns_t Constructor
//
// Arguments:
//
//	rhs		- Right-hand shared_ptr<mountns_t> to clone into this mountns_t

Namespace::mountns_t::mountns_t(std::shared_ptr<mountns_t> const& rhs)
{
	sync::reader_writer_lock::scoped_lock_read reader(rhs->mountslock);
	for(auto const& iterator : rhs->mounts) {

		// clone the right-hand mount collection
		mounts.emplace(std::make_pair(std::make_shared<path_t>(iterator.first), std::move(iterator.second->Duplicate())));
	}
}

//---------------------------------------------------------------------------

#pragma warning(pop)
