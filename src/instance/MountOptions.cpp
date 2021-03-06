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
#include "MountOptions.h"

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// MountOptions Constructor
//
// Arguments:
//
//	flags		- Standard mounting option flags

MountOptions::MountOptions(uint32_t flags) : m_flags(flags) {}

//-----------------------------------------------------------------------------
// MountOptions Constructor
//
// Arguments:
//
//	flags			- Initial set of mounting flags to apply before parsing
//	options			- String containing the mounting options to parse

MountOptions::MountOptions(uint32_t flags, char_t const* options) : m_flags(flags)
{
	// Process all of the string-based options provided, either modifying the
	// standard flags or collecting them as extra/custom option strings
	while((options) && (*options)) {

		char_t const* begin = options;
		char_t const* end;

		// skip leading whitespace and commas
		while((*begin) && (std::isspace(*begin) || (*begin == ','))) begin++;
		
		// double quote - read until the next double quote
		if(*begin == '\"') {

			end = ++begin;
			while((*end) && (*end != '\"')) end++;
			ParseToken(std::trim(std::string(begin, end)), m_flags, m_arguments);
			options = (*end) ? ++end : end;
		}

		// read until a comma or whitespace is detected
		else {

			end = begin;
			while((*end) && (!std::isspace(*end) && (*end != ','))) end++;
			ParseToken(std::trim(std::string(begin, end)), m_flags, m_arguments);
			options = (*end) ? ++end : end;
		}
	}
}

//-----------------------------------------------------------------------------
// MountOptions Constructor
//
// Arguments:
//
//	options			- String containing the mounting options to parse

MountOptions::MountOptions(char_t const* options) : MountOptions(0, options) 
{
}

//-----------------------------------------------------------------------------
// MountOptions Constructor
//
// Arguments:
//
//	flags		- Standard mounting option flags
//	data		- Optional pointer to extra parameter data
//	datalen		- Length, in bytes, of the extra parameter data

MountOptions::MountOptions(uint32_t flags, void const* data, size_t datalen) : MountOptions(flags, std::string(reinterpret_cast<char_t const*>(data), datalen).c_str()) 
{
}

//-----------------------------------------------------------------------------
// MountOptions::operator[]
//
// Arguments:
//
//	flag	- Standard mounting option flag to return set or clear

uint32_t MountOptions::operator[](uint32_t flag) const
{
	return (m_flags & flag);
}

//-----------------------------------------------------------------------------
// MountOptions::operator[]
//
// Arguments:
//
//	key		- Non-standard mounting option key to retrieve

std::string MountOptions::operator[](std::string const& key) const
{
	return m_arguments[key];
}

//-----------------------------------------------------------------------------
// MountOptions::getArguments
//
// Accesses the contained non-standard mounting options collection

MountOptions::MountArguments const& MountOptions::getArguments(void) const
{
	return m_arguments;
}

//-----------------------------------------------------------------------------
// MountOptions::getFlags
//
// Accesses the contained standard mounting options flags

uint32_t MountOptions::getFlags(void) const
{
	return m_flags;
}

//-----------------------------------------------------------------------------
// MountOptions::ParseToken (private, static)
//
// Parses a single mount options string token into flags or extra arguments
//
// Arguments:
//
//	token		- Token to be parsed
//	flags		- Reference to the working set of standard mount flags
//	arguments	- Reference to the working set of non-standard arguments

void MountOptions::ParseToken(std::string&& token, uint32_t& flags, MountArguments& arguments)
{
	if(token.length() == 0) return;

	//
	// STANDARD OPTIONS --> FLAGS
	//

	else if(token == "ro")			flags |= UAPI_MS_RDONLY;
	else if(token == "rw")			flags &= ~UAPI_MS_RDONLY;

	else if(token == "suid")		flags &= ~UAPI_MS_NOSUID;
	else if(token == "nosuid")		flags |= UAPI_MS_NOSUID;

	else if(token == "dev")			flags &= ~UAPI_MS_NODEV;
	else if(token == "nodev")		flags |= UAPI_MS_NODEV;

	else if(token == "exec")		flags &= ~UAPI_MS_NOEXEC;
	else if(token == "noexec")		flags |= UAPI_MS_NOEXEC;

	else if(token == "async")		flags &= ~UAPI_MS_SYNCHRONOUS;
	else if(token == "sync")		flags |= UAPI_MS_SYNCHRONOUS;

	else if(token == "remount")		flags |= UAPI_MS_REMOUNT;
	
	else if(token == "mand")		flags |= UAPI_MS_MANDLOCK;
	else if(token == "nomand")		flags &= ~UAPI_MS_MANDLOCK;

	else if(token == "dirsync")		flags |= UAPI_MS_DIRSYNC;

	else if(token == "atime")		flags &= ~UAPI_MS_NOATIME;
	else if(token == "noatime")		flags |= UAPI_MS_NOATIME;

	else if(token == "diratime")	flags &= ~UAPI_MS_NODIRATIME;
	else if(token == "nodiratime")	flags |= UAPI_MS_NODIRATIME;

	else if(token == "relatime")	flags |= UAPI_MS_RELATIME;
	else if(token == "norelatime")	flags &= ~UAPI_MS_RELATIME;

	else if(token == "silent")		flags |= UAPI_MS_SILENT;
	else if(token == "loud")		flags &= ~UAPI_MS_SILENT;

	else if(token == "strictatime")	flags |= UAPI_MS_STRICTATIME;
	
	else if(token == "lazytime")	flags |= UAPI_MS_LAZYTIME;
	else if(token == "nolazytime")	flags &= ~UAPI_MS_LAZYTIME;

	else if(token == "iversion")	flags |= UAPI_MS_I_VERSION;
	else if(token == "noiversion")	flags &= ~UAPI_MS_I_VERSION;

	//
	// NON-STANDARD OPTIONS --> ARGUMENTS
	//

	else {

		size_t equalsign = token.find(_T('='));
		if(equalsign == std::string::npos) { arguments.m_col.emplace(trim(token), std::string()); }
		else { arguments.m_col.emplace(trim(token.substr(0, equalsign)), trim(token.substr(equalsign + 1))); }
	}
}

//
// MOUNTOPTIONS::MOUNTARGUMENTS
//

//-----------------------------------------------------------------------------
// MountOptions::MountArguments::operator[]
//
// Arguments:
//
//	key		- Switch name/key to retrieve a single value for

std::string MountOptions::MountArguments::operator[](std::string const& key) const
{
	// Use find to locate the first element with the specified key
	const auto& iterator = m_col.find(key);
	return (iterator == m_col.cend()) ? std::string() : iterator->second;
}

//-----------------------------------------------------------------------------
// MountOptions::MountArguments::Contains
//
// Determines if the collection contains at least one of the specified keys
//
// Arguments:
//
//	key		- Switch name/key to check in the collection

bool MountOptions::MountArguments::Contains(std::string const& key) const
{
	// Use the count of the specified key in the collection to determine this
	return (m_col.count(key) > 0);
}

//-----------------------------------------------------------------------------
// MountOptions::MountArguments::GetValue
//
// Retrieves the first value associated with the specified key
//
// Arguments:
//
//	key		- Switch name/key to retrieve a single value for

std::string MountOptions::MountArguments::GetValue(std::string const& key) const
{
	// Use find to locate the first element with the specified key
	auto const& iterator = m_col.find(key);
	return (iterator == m_col.cend()) ? std::string() : iterator->second;
}

//-----------------------------------------------------------------------------
// MountOptions::MountArguments::GetValues
//
// Retrieves all the values associated with the specified key
//
// Arguments:
//
//	key		- Switch name/key to retrieve all values for

std::vector<std::string> MountOptions::MountArguments::GetValues(std::string const& key) const
{
	std::vector<std::string> values;			// vector<> of values to return

	// Use equal_range to retrieve all the elements with the specified key
	auto const& range = m_col.equal_range(key);
	for(auto iterator = range.first; iterator != range.second; iterator++) values.push_back(iterator->second);

	return values;
}

//-----------------------------------------------------------------------------

#pragma warning(pop)
