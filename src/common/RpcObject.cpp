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
#include "RpcObject.h"

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// RpcObject Constructor
//
// Creates a new RPC object on the specified interface.  A new unique identifier
// will be used for the object and the entry point vector manager type, and the 
// entry point vector will be the default MIDL-provided implementation
//
// Arguments:
//
//	ifspec		- RPC interface specification
//	flags		- Interface registration flags

RpcObject::RpcObject(RPC_IF_HANDLE const& ifspec, unsigned int flags) : RpcObject(ifspec, CreateUuid(), flags)
{
}

//-----------------------------------------------------------------------------
// RpcObject Constructor
//
// Creates a new RPC object on the specified interface.  The same unique identifier
// will be used for the object and the entry point vector manager type, and the 
// entry point vector will be the default MIDL-provided implementation
//
// Arguments:
//
//	ifspec		- RPC interface specification
//	objectid	- Unique identifier for the object instance
//	flags		- Interface registration flags

RpcObject::RpcObject(RPC_IF_HANDLE const& ifspec, uuid_t const& objectid, unsigned int flags) : RpcObject(ifspec, objectid, objectid, flags)
{
}

//-----------------------------------------------------------------------------
// RpcObject Constructor
//
// Creates a new RPC object on the specified interface.  The entry point vector 
// will be the default MIDL-provided implementation
//
// Arguments:
//
//	ifspec		- RPC interface specification
//	objectid	- Unique identifier for the object instance
//	mgrttypeid	- Unique identifier for the entry point vector manager type
//	flags		- Interface registration flags

RpcObject::RpcObject(RPC_IF_HANDLE const& ifspec, uuid_t const& objectid, uuid_t const& mgrtypeid, unsigned int flags) : RpcObject(ifspec, objectid, mgrtypeid, nullptr, flags)
{
}

//-----------------------------------------------------------------------------
// RpcObject Constructor
//
// Creates a new RPC object on the specified interface
//
// Arguments:
//
//	ifspec		- RPC interface specification
//	objectid	- Unique identifier for the object instance
//	mgrttypeid	- Unique identifier for the entry point vector manager type
//	epv			- Entry point vector
//	flags		- Interface registration flags

RpcObject::RpcObject(RPC_IF_HANDLE const& ifspec, uuid_t const& objectid, uuid_t const& mgrtypeid, RPC_MGR_EPV* const epv, unsigned int flags) :
	m_ifspec(ifspec), m_objectid(objectid), m_mgrtypeid(mgrtypeid)
{
	// Attempt to register the RPC object interface using the provided details
	RPC_STATUS rpcresult = RpcServerRegisterIfEx(ifspec, const_cast<uuid_t*>(&mgrtypeid), epv, flags, RPC_C_LISTEN_MAX_CALLS_DEFAULT, nullptr);
	if(rpcresult != RPC_S_OK) throw Win32Exception(rpcresult);
	
	// Attempt to add an object against the interface
	try { m_bindingstr = AddObjectMapping(ifspec, objectid, mgrtypeid); }
	catch(...) { RpcServerUnregisterIf(ifspec, const_cast<uuid_t*>(&mgrtypeid), false); throw; }
}

//-----------------------------------------------------------------------------
// RpcObject Destructor

RpcObject::~RpcObject()
{
	// Attempt to remove the object mapping from the interface first
	try { RemoveObjectMapping(m_ifspec, m_objectid); }
	catch(...) { _ASSERTE(false); }

	// Unregister the interface, waiting for calls to complete and context handle rundown to occur
	RPC_STATUS rpcresult = RpcServerUnregisterIfEx(m_ifspec, const_cast<uuid_t*>(&m_mgrtypeid), TRUE);
	_ASSERTE(rpcresult == RPC_S_OK);

	// Release builds warn that rpcresult is unused since it's only hit with _ASSERTE()
	UNREFERENCED_PARAMETER(rpcresult);
}

//-----------------------------------------------------------------------------
// RpcObject::AddObjectMapping (private, static)
//
// Associates an object unique identifier with an RPC interface
//
// Arguments:
//
//	ifspec		- RPC interface specification
//	objectid	- Object unique identifier to be associated
//	mgrtypeid	- Entry point vector manager type unique identifier

std::tstring RpcObject::AddObjectMapping(RPC_IF_HANDLE const& ifspec, uuid_t const& objectid, uuid_t const& mgrtypeid)
{
	RPC_BINDING_VECTOR*			bindings;				// RPC binding handles
	RPC_BINDING_HANDLE			copy = nullptr;			// Copy of first server binding
	rpc_tchar_t*				strbinding;				// String binding (raw)
	std::tstring				bindingstr;				// String binding (std::tstring)
	RPC_STATUS					rpcresult;				// Result from RPC function call

	// The server must have available protocol sequence bindings
	rpcresult = RpcServerInqBindings(&bindings);
	if(rpcresult != RPC_S_OK) throw Win32Exception(rpcresult);

	try {
		
		// If there are no bindings, the object endpoint cannot be registered
		if(bindings->Count == 0) throw Win32Exception(RPC_S_NO_BINDINGS);

		// Associate the object id with this interface's manager type uuid
		rpcresult = RpcObjectSetType(const_cast<uuid_t*>(&objectid), const_cast<uuid_t*>(&mgrtypeid));
		if(rpcresult != RPC_S_OK) throw Win32Exception(rpcresult);

		// Add an endpoint for the object
		UUID_VECTOR objects = { 1, const_cast<uuid_t*>(&objectid) };
		rpcresult = RpcEpRegister(ifspec, bindings, &objects, nullptr);
		if(rpcresult != RPC_S_OK) throw Win32Exception(rpcresult);

		// Create a copy of the first binding handle in the vector
		rpcresult = RpcBindingCopy(bindings->BindingH[0], &copy);
		if(rpcresult != RPC_S_OK) throw Win32Exception(rpcresult);

		// Associate the object id with the copied binding
		rpcresult = RpcBindingSetObject(copy, const_cast<uuid_t*>(&objectid));
		if(rpcresult != RPC_S_OK) throw Win32Exception(rpcresult);

		// Convert the binding into a string binding
		rpcresult = RpcBindingToStringBinding(copy, &strbinding);
		if(rpcresult != RPC_S_OK) throw Win32Exception(rpcresult);

		// Convert the string binding into an std::tstring for the caller
		bindingstr = std::tstring(reinterpret_cast<tchar_t*>(strbinding));
		RpcStringFree(&strbinding);

		RpcBindingFree(&copy);					// Release copied binding
		RpcBindingVectorFree(&bindings);		// Release bindings vector

		return bindingstr;						// Return binding string
	}

	catch(...) { 
		
		if(copy) RpcBindingFree(&copy);
		RpcObjectSetType(const_cast<uuid_t*>(&objectid), nullptr);
		RpcBindingVectorFree(&bindings);

		throw; 
	}
}

//-----------------------------------------------------------------------------
// RpcObject::CreateUuid (private, static)
//
// Generate a new UUID instance
//
// Arguments:
//
//	NONE

uuid_t RpcObject::CreateUuid(void)
{
	uuid_t			uuid;			// Generated object unique identifier

	// Attempt to generate a new uuid_t for the object/epv manager
	RPC_STATUS rpcresult = UuidCreate(&uuid);
	if(rpcresult != RPC_S_OK) throw Win32Exception(rpcresult);

	return uuid;					// Return generated UUID
}
		
//-----------------------------------------------------------------------------
// RpcObject::getBindingString
//
// Gets the binding string required for a client to connect to the object

tchar_t const* RpcObject::getBindingString(void) const
{
	return m_bindingstr.c_str();
}

//-----------------------------------------------------------------------------
// RpcObject::getObjectId
//
// Gets the object unique identifier

uuid_t RpcObject::getObjectId(void) const
{
	return m_objectid;
}

//-----------------------------------------------------------------------------
// RpcObject::RemoveObjectMapping (static, private)
//
// Disassociates an object unique identifier from an RPC interface
//
// Arguments:
//
//	ifspec		- RPC interface specification
//	objectid	- Object unique identifier to be disassociated

void RpcObject::RemoveObjectMapping(RPC_IF_HANDLE const& ifspec, uuid_t const& objectid)
{
	RPC_BINDING_VECTOR*			bindings;			// RPC binding handles
	RPC_STATUS					rpcresult;			// Result from RPC function call

	// The server must have available protocol sequence bindings
	rpcresult = RpcServerInqBindings(&bindings);
	if(rpcresult != RPC_S_OK) throw Win32Exception(rpcresult);

	try {
		
		// If there are no bindings, the object endpoint cannot be unregistered
		if(bindings->Count == 0) throw Win32Exception(RPC_S_NO_BINDINGS);

		UUID_VECTOR objects = { 1, const_cast<uuid_t*>(&objectid) };
		rpcresult = RpcEpUnregister(ifspec, bindings, &objects);
		if(rpcresult != RPC_S_OK) throw Win32Exception(rpcresult);

		// Disassociate the object id and release the binding vector
		RpcObjectSetType(const_cast<uuid_t*>(&objectid), nullptr);
		RpcBindingVectorFree(&bindings);
	}

	catch(...) { RpcBindingVectorFree(&bindings); throw; }
}

//-----------------------------------------------------------------------------

#pragma warning(pop)
