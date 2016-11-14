//---------------------------------------------------------------------------
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
//---------------------------------------------------------------------------

using System;

namespace zuki.vm.linux
{
	/// <summary>
	/// Defines the node types supported by this tool
	/// </summary>
	enum CpioNodeType
	{
		Socket				= 0xC000,		// S_IFSOCK / 0140000
		SymbolicLink		= 0xA000,		// S_IFLNK  / 0120000
		File				= 0x8000,		// S_IFREG  / 0100000
		BlockDevice			= 0x6000,		// S_IFBLK  / 0060000
		Directory			= 0x4000,		// S_IFDIR  / 0040000
		CharacterDevice		= 0x2000,		// S_IFCHR  / 0020000
		Pipe				= 0x1000,		// S_IFIFO  / 0010000
	}
}
