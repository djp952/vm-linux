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

#ifndef __RPCOBJECT_H_
#define __RPCOBJECT_H_
#pragma once

#include "text.h"
#include "Win32Exception.h"

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// RpcObject
//
// Specialized RPC interface registration wrapper used for implementing multiple
// instances of that interface within the same process

class RpcObject
{
public:

	// Instance Constructors
	//
	RpcObject(RPC_IF_HANDLE const& ifspec, unsigned int flags);
	RpcObject(RPC_IF_HANDLE const& ifspec, uuid_t const& objectid, unsigned int flags);
	RpcObject(RPC_IF_HANDLE const& ifspec, uuid_t const& objectid, uuid_t const& mgrtypeid, unsigned int flags);
	RpcObject(RPC_IF_HANDLE const& ifspec, uuid_t const& objectid, uuid_t const& mgrtypeid, RPC_MGR_EPV* const epv, unsigned int flags);

	// Destructor
	//
	~RpcObject();

	//-------------------------------------------------------------------------
	// Properties

	// BindingString
	//
	// Gets the binding string required for a client to connect to the object
	__declspec(property(get=getBindingString)) tchar_t const* BindingString;
	tchar_t const* getBindingString(void) const;

	// ObjectId
	//
	// Unique identifer for the constructed RPC object
	__declspec(property(get=getObjectId)) uuid_t ObjectId;
	uuid_t getObjectId(void) const;

private:

	RpcObject(RpcObject const &rhs)=delete;
	RpcObject& operator=(RpcObject const &rhs)=delete;

	//-------------------------------------------------------------------------
	// Private Member Functions

	// AddObjectMapping (static)
	//
	// Adds an object type mapping to a registered RPC interface
	static std::tstring AddObjectMapping(RPC_IF_HANDLE const& ifspec, uuid_t const& objectid, uuid_t const& mgrtypeid);

	// CreateUuid (static)
	//
	// Generates a new UUID
	static uuid_t CreateUuid(void);

	// RemoveObjectMapping (static)
	//
	// Removes an object type mapping from a registered RPC interface
	static void RemoveObjectMapping(RPC_IF_HANDLE const& ifspec, uuid_t const& objectid);

	//-------------------------------------------------------------------------
	// Member Variables

	const RPC_IF_HANDLE		m_ifspec;				// Interface specification
	const uuid_t			m_objectid;				// Object unique identifier
	const uuid_t			m_mgrtypeid;			// Entry point vector manager type
	std::tstring			m_bindingstr;			// Binding string for the object
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __RPCOBJECT_H_
