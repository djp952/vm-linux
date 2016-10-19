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

#ifndef __PARAMETER_H_
#define __PARAMETER_H_
#pragma once

#include <string>
#include <text.h>

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// Class ParameterBase
//
// Abstract base class for generalizing access to a Parameter<> instance

class ParameterBase
{
public:

	// Destructor
	//
	virtual ~ParameterBase()=default;

	//-------------------------------------------------------------------------
	// Member Functions

	// Parse
	//
	// Parses the specified string into the contained value
	virtual void Parse(const tchar_t* value) = 0;

	// Parse
	//
	// Parses the specified string into the contained value
	virtual void Parse(const std::tstring& value) = 0;

	// TryParse
	//
	// Tries to parse the specified string into the contained value
	virtual bool TryParse(const tchar_t* value) = 0;

	// TryParse
	//
	// Tries to parse the specified string into the contained value
	virtual bool TryParse(const std::tstring& value) = 0;

protected:

	// Instance Constructor
	//
	ParameterBase()=default;
};

//-----------------------------------------------------------------------------
// Template Class Parameter<>
//
// Implements a strongly typed parsable parameter.  Use Parameter<void> or
// simply Parameter to indicate a 'switch', which is a named parameter that
// has no parsable value and implicitly behaves like a boolean

template <typename _type>
class Parameter : public ParameterBase
{
	// value_t -> underlying parameter value type
	using value_t = typename std::conditional<std::is_void<_type>::value, bool, _type>::type;

public:

	// Instance Constructor
	//
	template<bool _zeroinit = std::is_trivial<_type>::value>
	Parameter()
	{
		// Trivial types can be zero-initialized
		if(_zeroinit) memset(&m_value, 0, sizeof(m_value));
	}

	// Instance Constructor
	//
	Parameter(const value_t& value) : m_value(value)
	{
	}

	// Destructor
	//
	virtual ~Parameter()=default;

	// Assignment Operator (C-style string)
	//
	Parameter& operator=(const tchar_t* value)
	{
		Parse<_type>(value, m_value);
		return *this;
	}

	// Assignment Operator (std::tstring)
	//
	Parameter& operator=(const std::tstring& value)
	{
		Parse<_type>(value, m_value);
		return *this;
	}

	// Assignment Operator (non-std::tstring)
	//
	template <typename _t = value_t>
	typename std::enable_if<!std::is_same<_t, std::tstring>::value, Parameter&>::type operator=(const value_t& value)
	{
		m_value = value;
		return *this;
	}

	// _type Conversion Operator
	//
	operator const typename std::conditional<std::is_void<_type>::value, bool, _type>::type& () const
	{
		return m_value;
	}

	// bool Conversion Operator (integral, non-bool, enum)
	//
	template <typename _t = value_t>
	operator typename std::enable_if<(!std::is_same<_t, bool>::value && std::is_integral<_t>::value) || std::is_enum<_t>::value, bool>::type () const
	{
		return m_value != 0;
	}

	// bool Conversion Operator (std::tstring)
	//
	template <typename _t = value_t>
	operator typename std::enable_if<std::is_same<_t, std::tstring>::value, bool>::type () const
	{
		return m_value.length() > 0;
	}

	// Logical Not Operator (bool)
	//
	template <typename _t = value_t>
	typename std::enable_if<std::is_same<_t, bool>::value, bool>::type operator !() const
	{
		return m_value == false;
	}

	// Logical Not Operator (integral, non-bool, enum)
	//
	template <typename _t = value_t>
	typename std::enable_if<(!std::is_same<_t, bool>::value && std::is_integral<_t>::value) || std::is_enum<_t>::value, bool>::type operator !() const
	{
		return m_value == 0;
	}

	// Logical Not Operator (std::tstring)
	//
	template <typename _t = value_t>
	typename std::enable_if<std::is_same<_t, std::tstring>::value, bool>::type operator !() const
	{
		return m_value.length() == 0;
	}

	//-------------------------------------------------------------------------
	// Member Functions

	// Parse (ParameterBase)
	//
	// Parses the specified string into the contained value
	virtual void Parse(const tchar_t* value) override
	{
		Parse(std::tstring(value));
	}

	// Parse (ParameterBase)
	//
	// Parses the specified string into the contained value
	virtual void Parse(const std::tstring& value) override
	{
		Parse(value, m_value);
	}

	// TryParse (ParameterBase)
	//
	// Tries to parse the specified string into the contained value
	virtual bool TryParse(const tchar_t* value) override
	{
		return TryParse(std::tstring(value));
	}

	// TryParse (ParameterBase)
	//
	// Tries to parse the specified string into the contained value
	virtual bool TryParse(const std::tstring& value) override
	{
		try { Parse<_type>(value, m_value); }
		catch(...) { return false; }

		return true;
	}

private:

	Parameter(const Parameter&)=delete;
	Parameter(Parameter&&)=delete;
	Parameter& operator=(const Parameter&)=delete;

	//-------------------------------------------------------------------------
	// Private Member Functions

	// ParseInteger64 (int64_t)
	//
	// Parses a string into a signed 64-bit integer
	template <typename _type>
	static inline typename std::enable_if<std::is_same<_type, int64_t>::value, int64_t>::type ParseInteger64(const std::tstring& str, size_t* idx, int base)
	{
		return std::stoll(str, idx, base);
	}

	// ParseInteger64 (uint64_t)
	//
	// Parses a string into an unsigned 64-bit integer
	template <typename _type>
	static inline typename std::enable_if<std::is_same<_type, uint64_t>::value, uint64_t>::type ParseInteger64(const std::tstring& str, size_t* idx, int base)
	{
		return std::stoull(str, idx, base);
	}

	// Parse (Integer)
	//
	// Parses a string value into an integer value, optionally applying the K/M/G multiplier suffix
	template <typename _t = _type>
	static typename std::enable_if<std::is_integral<_t>::value && !std::is_same<_t, bool>::value, void>::type Parse(const std::tstring& str, value_t& val)
	{
		// interim_t -> interim 64-bit value type used during conversion
		using interim_t = std::conditional<std::is_signed<_t>::value, int64_t, uint64_t>::type;

		size_t		suffixindex;			// Position of the optional suffix
		interim_t	multiplier = 1;			// Multiplier value to be applied

		// Convert the string into a signed 64-bit integer and extract any suffix
		interim_t interim = ParseInteger64<interim_t>(str, &suffixindex, 0);
		auto suffix = str.substr(suffixindex);

		// The suffix must not be more than one character in length
		if(suffix.length() > 1) throw std::invalid_argument(std::to_string(str));
		else if(suffix.length() == 1) {

			switch(suffix.at(0)) {

				case 'k': case 'K': multiplier = 1 KiB; break;
				case 'm': case 'M': multiplier = 1 MiB; break;
				case 'g': case 'G': multiplier = 1 GiB; break;
				default: throw std::invalid_argument(std::to_string(str));
			}
		}

		// Watch for overflow when applying the multiplier to the interim value
		if((std::numeric_limits<interim_t>::max() / multiplier) < interim) 
			throw std::overflow_error(std::to_string(str));
		
		// Apply the multiplier value
		interim *= multiplier;

		// Verify that the final result will not exceed the target type's numeric limits
		if((interim > std::numeric_limits<_t>::max()) || (interim < std::numeric_limits<_t>::min())) 
			throw std::overflow_error(std::to_string(str));

		// Value has been successfully converted, set the provided reference
		val = static_cast<_t>(interim);
	}

	// Parse (enum)
	//
	// Parses a string value into an enumeration ordinal value
	template <typename _t = _type>
	static typename std::enable_if<std::is_enum<_t>::value, void>::type Parse(const std::tstring& str, value_t& val)
	{
		// interim_t -> interim 64-bit value type used during conversion
		using interim_t = std::conditional<std::is_signed<std::underlying_type<_t>::type>::value, int64_t, uint64_t>::type;

		// Convert the string into a 64-bit integer
		interim_t interim = ParseInteger64<interim_t>(str, nullptr, 0);

		// Verify that the final result will not exceed the underlying type's numeric limits
		if((interim > std::numeric_limits<std::underlying_type<_t>::type>::max()) || (interim < std::numeric_limits<std::underlying_type<_t>::type>::min())) 
			throw std::overflow_error(std::to_string(str));

		// Value has been successfully converted, set the provided reference
		val = static_cast<_t>(interim);
	}

	// Parse (std::tstring)
	//
	// Parses a string value
	template <typename _t = _type> 
	static typename std::enable_if<std::is_same<_t, std::tstring>::value, void>::type Parse(const std::tstring& str, value_t& val)
	{
		val = str;
	}

	// Parse (bool)
	//
	// Parses a boolean value
	template <typename _t = _type>
	static typename std::enable_if<std::is_same<_t, bool>::value, void>::type Parse(const std::tstring& str, value_t& val)
	{
		auto strlower = std::tolower(str);

		// The acceptable strings here are "true", "false", "0" and "1"
		if((strlower.compare(TEXT("true")) == 0) || (strlower.compare(TEXT("1")) == 0)) val = true;
		else if((strlower.compare(TEXT("false")) == 0) || (strlower.compare(TEXT("0")) == 0)) val = false;
		else throw std::invalid_argument(str);
	}

	// Parse (void)
	//
	// Parses a void value which represents a switch that always evaluates to true
	template <typename _t = _type>
	static typename std::enable_if<std::is_void<_t>::value, void>::type Parse(const std::tstring&, value_t& val)
	{
		val = true;
	}

	//-------------------------------------------------------------------------
	// Member Variables

	value_t					m_value;			// Contained value instance
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __PARAMETER_H_
