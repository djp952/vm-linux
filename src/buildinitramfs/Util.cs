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
using System.IO;
using System.Linq;

namespace zuki.vm.linux
{
	static class Util
	{
		/// <summary>
		/// [Extension] Aligns a numeric value up to a specific alignment
		/// </summary>
		/// <param name="value">Value to be aligned</param>
		/// <param name="alignment">Requested alignment value</param>
		public static int AlignUp(this int value, int alignment)
		{
			return (value == 0) ? 0 : value + ((alignment - (value % alignment)) % alignment);
		}

		/// <summary>
		/// [Extension] Aligns a numeric value up to a specific alignment
		/// </summary>
		/// <param name="value">Value to be aligned</param>
		/// <param name="alignment">Requested alignment value</param>
		public static long AlignUp(this long value, long alignment)
		{
			return (value == 0) ? 0 : value + ((alignment - (value % alignment)) % alignment);
		}

		/// <summary>
		/// [Extension] Converts a DateTime into a CPIO archive modification time instance
		/// </summary>
		/// <param name="dt">DateTime value to be converted</param>
		public static long ToModificationTime(this DateTime dt)
		{
			long ticks = (dt.ToUniversalTime() - s_epoch).Ticks;
			return ticks / TimeSpan.TicksPerSecond;
		}

		/// <summary>
		/// [Extension] Converts CPIO archive modification time to a DateTime instance
		/// </summary>
		/// <param name="mtime">CPIO archive modification time to convert</param>
		private static DateTime ToDateTime(this long mtime)
		{
			long ticks = (mtime * TimeSpan.TicksPerSecond);
			return new DateTime(s_epoch.Ticks + ticks, System.DateTimeKind.Utc);
		}

		/// <summary>
		/// Attempts to create a directory; does not throw any exceptions
		/// </summary>
		/// <param name="path">Path to the file to be deleted</param>
		public static bool TryCreateDirectory(string path)
		{
			try { Directory.CreateDirectory(path); return true; }
			catch { return false; }
		}

		/// <summary>
		/// Attempts to delete a file; does not throw any exceptions
		/// </summary>
		/// <param name="path"></param>
		/// <returns></returns>
		public static bool TryDeleteFile(string path)
		{
			try { File.Delete(path); return true; }
			catch { return false; }
		}

		/// <summary>
		/// Attempts to parse a string as an unsigned 32-bit integer.  Surprisingly, .NET doesn't
		/// implicitly support standard octal or hexadecimal prefixes.  This function follows that
		/// convention: a prefix of '0x' indicates hexadecimal, a prefix of '0' indicates octal
		/// </summary>
		/// <param name="value">Input string</param>
		/// <param name="result">On success, contains the parsed numeric value</param>
		public static bool TryParseInt32(string value, out int result)
		{
			result = 0;             // Initialize [out] value to zero

			if (String.IsNullOrEmpty(value)) return false;

			// 0x prefix: base-16 hexadecimal number
			if (value.ToLower().StartsWith("0x"))
			{
				string hex = value.Substring(2).ToLower();
				if (!hex.All(c => (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
				result = Convert.ToInt32(hex, 16);
				return true;
			}

			// 0 prefix: base-8 octal number
			else if (value.StartsWith("0"))
			{
				if (!value.All(c => (c >= '0' && c <= '7'))) return false;
				result = Convert.ToInt32(value, 8);
				return true;
			}

			// Unrecognized or non-existent prefix: base-10 decimal number
			else return Int32.TryParse(value, out result);
		}

		//-------------------------------------------------------------------
		// Member Variables
		//-------------------------------------------------------------------

		/// <summary>
		/// The Unix time epoch as a UTC DateTime instance
		/// </summary>
		static private readonly DateTime s_epoch = new DateTime(1970, 1, 1, 0, 0, 0, 0, System.DateTimeKind.Utc);
	}
}
