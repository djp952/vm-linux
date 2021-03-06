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

#ifndef __BITMASK_H_
#define __BITMASK_H_
#pragma once

#include <limits>

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// bitmask
//
// Assists with generation of bitmask-based classes.  Use by declaring static const
// objects within the derived class and initializing them with the desired value.
//
// Most folks would use enum classes and overload a bunch of operators, but to me that
// goes against the contract of an enum class -- if you combine values together and that
// new value is not in the enum class then it's no longer strongly typed.
//
// bitmask<> takes two template arguments, the base data type and an optional mask that
// indicates what the allowable values are.  Bits not in the allowed mask will be silently
// stripped out, no exception will be thrown.
//
// class MyBitmask : public bitmask<MyBitmask, uint8_t>
// {
//     using bitmask::bitmask;
//
// public:
//
//     static const MyBitmask ValueOne;
//     static const MyBitmask ValueTwo;
// };
//
// MyBitmask const MyBitmask::ValueOne { 0x01 };
// MyBitmask const MyBitmask::ValueTwo { 0x02 };

template <typename _derived, typename _basetype = decltype(uint32_t), _basetype _allowed = std::numeric_limits<_basetype>::max()>
class bitmask
{
public:

	// Instance Constructor
	//
	bitmask() : m_value(0) {}

	// Instance Constructor
	//
	bitmask(_basetype value) : m_value(value & _allowed) {}

	// Copy Constructor
	//
	bitmask(_derived const& rhs) : m_value(rhs.m_value & _allowed) {}

	// Destructor
	//
	~bitmask()=default;

	// bitwise and operator
	//
	_derived operator&(const _derived& rhs) const
	{
		return _derived((m_value & rhs.m_value) & _allowed);
	}

	// bitwise or operator
	//
	_derived operator|(_derived const& rhs) const
	{
		return _derived((m_value | rhs.m_value) & _allowed);
	}

	// bitwise xor operator
	//
	_derived operator^(_derived const& rhs) const
	{
		return _derived((m_value ^ rhs.m_value) & _allowed);
	}

	// bitwise not operator
	//
	_derived operator~(void) const
	{
		return _derived((~m_value) & _allowed);
	}

	// equality operator
	//
	bool operator==(_derived const& rhs) const
	{
		return m_value == rhs.m_value;
	}

	// inequality operator
	//
	bool operator!=(_derived const& rhs) const
	{
		return m_value != rhs.m_value;
	}

	// bitwise or-equals operator
	//
	bitmask& operator|=(_derived const& rhs)
	{
		m_value |= rhs.m_value;
		return *this;
	}

	// bitwise and-equals operator
	//
	bitmask& operator&=(_derived const& rhs)
	{
		m_value &= rhs.m_value;
		return *this;
	}

	// bitwise xor-equals operator
	//
	bitmask& operator^=(_derived const& rhs)
	{
		m_value ^= rhs.m_value;
		return *this;
	}

	// bool conversion operator
	//
	operator bool() const
	{
		return m_value != 0;
	}

	// assignment operator
	//
	bitmask& operator=(bitmask const& rhs)
	{
		m_value = (rhs.m_value & _allowed);
		return *this;
	}

	// assignment operator
	//
	_derived& operator=(_derived const& rhs)
	{
		m_value = (rhs.m_value & _allowed);
		return *this;
	}

private:

	_basetype			m_value;			// Contained value
};

//---------------------------------------------------------------------------

#pragma warning(pop)

#endif	// __BITMASK_H_
