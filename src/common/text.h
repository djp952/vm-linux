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

#ifndef __TEXT_H_
#define __TEXT_H_
#pragma once

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <tchar.h>
#include <vector>
#include <Windows.h>

#pragma warning(push, 4)

using rpc_char_t = unsigned char;
using rpc_wchar_t = unsigned short;
using rpc_tchar_t = std::conditional<sizeof(TCHAR) == sizeof(wchar_t), rpc_wchar_t, rpc_char_t>::type;

//-----------------------------------------------------------------------------
// Text Extensions

// char_t
//
// Typedef for an ANSI character
using char_t = char;

// tchar_t
//
// Typedef for a generic text character
using tchar_t = std::conditional<sizeof(TCHAR) == sizeof(wchar_t), wchar_t, char>::type;

namespace std { 
	
	// std::tstring
	//
	// Typedef for a generic text std::[w]string
	using tstring = std::conditional<sizeof(TCHAR) == sizeof(wchar_t), std::wstring, std::string>::type;

	// std::tstringstream
	//
	// Typedef for a generic std::[w]stringstream
	using tstringstream = std::conditional<sizeof(TCHAR) == sizeof(wchar_t), std::wstringstream, std::stringstream>::type;

	// std::to_string (conversion)
	//
	inline std::string to_string(const wchar_t* psz, int cch)
	{
		if(psz == nullptr) return std::string();

		// Create a buffer big enough to hold the converted string data and convert it
		int buffercch = WideCharToMultiByte(CP_UTF8, 0, psz, cch, nullptr, 0, nullptr, nullptr);
		auto buffer = std::make_unique<char_t[]>(buffercch);
		WideCharToMultiByte(CP_UTF8, 0, psz, cch, buffer.get(), buffercch, nullptr, nullptr);

		// Construct an std::string around the converted character data, watch for the API
		// returning the length including the NULL character when -1 was provided as length
		return std::string(buffer.get(), (cch == -1) ? buffercch - 1 : buffercch);
	}

	// std::to_string overloads
	//
	inline std::string to_string(const wchar_t* psz) { return to_string(psz, -1); }
	inline std::string to_string(const char_t* psz, int cch) { return std::string(psz, cch); }
	inline std::string to_string(const char_t* psz) { return std::string(psz); }
	inline std::string to_string(const std::wstring& str) { return to_string(str.data(), static_cast<int>(str.size())); }
	inline std::string to_string(const std::stringstream& stm) { return stm.str(); }
	inline std::string to_string(const std::wstringstream& stm) { return to_string(stm.str()); }

	// std::to_wstring (conversion)
	//
	inline std::wstring to_wstring(const char_t* psz, int cch)
	{
		if(psz == nullptr) return std::wstring();

		// Create a buffer big enough to hold the converted string data and convert it
		int buffercch = MultiByteToWideChar(CP_UTF8, 0, psz, cch, nullptr, 0);
		auto buffer = std::make_unique<wchar_t[]>(buffercch);
		MultiByteToWideChar(CP_UTF8, 0, psz, cch, buffer.get(), buffercch);

		// Construct an std::wstring around the converted character data, watch for the API
		// returning the length including the NULL character when -1 was provided as length
		return std::wstring(buffer.get(), (cch == -1) ? buffercch - 1 : buffercch);
	}

	// std::to_wstring overloads
	//
	inline std::wstring to_wstring(const char_t* psz) { return to_wstring(psz, -1); }
	inline std::wstring to_wstring(const wchar_t* psz, int cch) { return std::wstring(psz, cch); }
	inline std::wstring to_wstring(const wchar_t* psz) { return std::wstring(psz); }
	inline std::wstring to_wstring(const std::string& str) { return to_wstring(str.data(), static_cast<int>(str.size())); }
	inline std::wstring to_wstring(const std::wstringstream& stm) { return stm.str(); }
	inline std::wstring to_wstring(const std::stringstream& stm) { return to_wstring(stm.str()); }

	// std::to_tstring
	//
	// Generic text wrapper around std::to_[w]string
	template <typename _type> inline tstring to_tstring(_type value)
	{
#ifdef _UNICODE
		return to_wstring(value);
#else
		return to_string(value);
#endif
	}

	// std::to_tstring
	//
	// Generic text wrapper around std::to_[w]string
	template <typename _type> inline tstring to_tstring(_type value, int cch)
	{
#ifdef _UNICODE
		return to_wstring(value, cch);
#else
		return to_string(value, cch);
#endif
	}

	// std::tolower
	//
	// Performs an lowercase conversion of a string
	inline std::string tolower(const std::string& str)
	{
		std::string result;

		result.resize(str.size());
		std::transform(str.begin(), str.end(), result.begin(), ::tolower);

		return result;
	}

	// std::tolower
	//
	// Performs an lowercase conversion of a string
	inline std::wstring tolower(const std::wstring& str)
	{
		std::wstring result;

		result.resize(str.size());
		std::transform(str.begin(), str.end(), result.begin(), ::towlower);

		return result;
	}

	// std::toupper
	//
	// Performs an uppercase conversion of a string
	inline std::string toupper(const std::string& str)
	{
		std::string result;

		result.resize(str.size());
		std::transform(str.begin(), str.end(), result.begin(), ::toupper);

		return result;
	}

	// std::toupper
	//
	// Performs an uppercase conversion of a string
	inline std::wstring toupper(const std::wstring& str)
	{
		std::wstring result;

		result.resize(str.size());
		std::transform(str.begin(), str.end(), result.begin(), ::towupper);

		return result;
	}

	// std::ltrim
	//
	// Performs a left trim of a string
	inline std::string ltrim(const std::string& str)
	{
		auto left = std::find_if_not(str.begin(), str.end(), isspace);
		return std::string(left, str.end());
	}

	inline std::wstring ltrim(const std::wstring& str)
	{
		auto left = std::find_if_not(str.begin(), str.end(), iswspace);
		return std::wstring(left, str.end());
	}

	inline std::string ltrim(const std::string& str, const char_t& value)
	{
		auto left = std::find_if_not(str.begin(), str.end(), [&](char_t ch) { return ch == value; } );
		return std::string(left, str.end());
	}

	inline std::wstring ltrim(const std::wstring& str, const wchar_t& value)
	{
		auto left = std::find_if_not(str.begin(), str.end(), [&](wchar_t ch) { return ch == value; } );
		return std::wstring(left, str.end());
	}

	// std::rtrim
	//
	// Performs a right trim of a string
	inline std::string rtrim(const std::string& str)
	{
		auto right = std::find_if_not(str.rbegin(), str.rend(), isspace);
		return std::string(str.begin(), right.base());
	}

	inline std::wstring rtrim(const std::wstring& str)
	{
		auto right = std::find_if_not(str.rbegin(), str.rend(), iswspace);
		return std::wstring(str.begin(), right.base());
	}

	inline std::string rtrim(const std::string& str, const char_t& value)
	{
		auto right = std::find_if_not(str.rbegin(), str.rend(), [&](char_t ch) { return ch == value; } );
		return std::string(str.begin(), right.base());
	}

	inline std::wstring rtrim(const std::wstring& str, const wchar_t& value)
	{
		auto right = std::find_if_not(str.rbegin(), str.rend(), [&](wchar_t ch) { return ch == value; });
		return std::wstring(str.begin(), right.base());
	}

	// std::trim
	//
	// Performs a full trim of a string
	inline std::string trim(const std::string& str)
	{
		auto left = std::find_if_not(str.begin(), str.end(), isspace);
		auto right = std::find_if_not(str.rbegin(), str.rend(), isspace);
		return (right.base() <= left) ? std::string() : std::string(left, right.base());
	}

	inline std::wstring trim(const std::wstring& str)
	{
		auto left = std::find_if_not(str.begin(), str.end(), iswspace);
		auto right = std::find_if_not(str.rbegin(), str.rend(), iswspace);
		return (right.base() <= left) ? std::wstring() : std::wstring(left, right.base());
	}

	inline std::string trim(const std::string& str, const char_t& value)
	{
		auto lambda = [&](char_t ch) { return ch == value; };
		auto left = std::find_if_not(str.begin(), str.end(), lambda);
		auto right = std::find_if_not(str.rbegin(), str.rend(), lambda);
		return (right.base() <= left) ? std::string() : std::string(left, right.base());
	}

	inline std::wstring trim(const std::wstring& str, const wchar_t& value)
	{
		auto lambda = [&](wchar_t ch) { return ch == value; };
		auto left = std::find_if_not(str.begin(), str.end(), lambda);
		auto right = std::find_if_not(str.rbegin(), str.rend(), lambda);
		return (right.base() <= left) ? std::wstring() : std::wstring(left, right.base());
	}

	inline bool startswith(const std::string& str, const char& value)
	{
		return !str.empty() && str[0] == value;
	}

	inline bool startswith(const std::wstring& str, const wchar_t& value)
	{
		return !str.empty() && str[0] == value;
	}

	inline bool endswith(const std::string& str, const char_t& value)
	{
		return !str.empty() && str[str.size() - 1] == value;
	}

	inline bool endswith(const std::wstring& str, const wchar_t& value)
	{
		return !str.empty() && str[str.size() - 1] == value;
	}

	// std::split
	//
	// Splits a tstring into a vector<> of delimited parts
	template <typename _type = std::basic_string>
	std::vector<typename _type> split(const typename _type& str, const typename _type::value_type& delimiter)
	{
		std::vector<_type> parts;			// Resultant vector<>

		_type::size_type offset = 0;
		_type::size_type position = str.find(delimiter);
		auto substring = str.substr(offset, position - offset);
		if(substring.length() > 0) parts.emplace_back(std::move(substring));
		//parts.emplace_back(str.substr(offset, position));

		while(position != _type::npos) {

			offset = ++position;
			position = str.find(delimiter, position);
			substring = str.substr(offset, position - offset);
			if(substring.length() > 0) parts.emplace_back(std::move(substring));
			//parts.emplace_back(str.substr(offset, position));
		}

		return parts;			
	}

} // namespace text

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __TEXT_H_
