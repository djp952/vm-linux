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
using System.Text;

namespace zuki.vm.linux
{
	/// <summary>
	/// Implements a CPIO archive block device node
	/// </summary>
	class CpioBlockDeviceNode : CpioNode
	{
		/// <summary>
		/// Instance Constructor
		/// </summary>
		/// <param name="path">Path to the destination node</param>
		public CpioBlockDeviceNode(string path) : base(path)
		{
		}

		//-------------------------------------------------------------------
		// Fields
		//-------------------------------------------------------------------

		/// <summary>
		/// Defines the major device identifier code
		/// </summary>
		public int Major = 0;

		/// <summary>
		/// Defines the minor device identifier code
		/// </summary>
		public int Minor = 0;

		//-------------------------------------------------------------------
		// Member Functions
		//-------------------------------------------------------------------

		/// <summary>
		/// Writes the node into an output stream
		/// </summary>
		/// <param name="stream">Output stream instance</param>
		/// <param name="c_ino">Node number to apply to the output record(s)</param>
		/// <returns>Number of bytes written into the output stream</returns>
		public override long Write(Stream stream, uint c_ino)
		{
			if ((stream == null) || (!stream.CanWrite)) throw new ArgumentException("stream");

			// Use only the permission flags from the Mode field, combine with S_IFBLK
			int c_mode = ((Mode & 0x1FFF) | (int)CpioNodeType.BlockDevice);

			// Convert the Path field into a UTF-8 encoded null terminated byte array
			byte[] c_filename = Encoding.UTF8.GetBytes(Path.TrimStart(new char[] { '/' }) + '\0');

			// c_magic[6]
			stream.Write(Encoding.ASCII.GetBytes("070701"), 0, 6);

			// c_ino[8]
			stream.Write(Encoding.ASCII.GetBytes(Convert.ToString(c_ino, 16).PadLeft(8, '0').ToUpper()), 0, 8);

			// c_mode[8]
			stream.Write(Encoding.ASCII.GetBytes(Convert.ToString(c_mode, 16).PadLeft(8, '0').ToUpper()), 0, 8);

			// c_uid[8]
			stream.Write(Encoding.ASCII.GetBytes(Convert.ToString(UserId, 16).PadLeft(8, '0').ToUpper()), 0, 8);

			// c_gid[8]
			stream.Write(Encoding.ASCII.GetBytes(Convert.ToString(GroupId, 16).PadLeft(8, '0').ToUpper()), 0, 8);

			// c_nlink[8]
			stream.Write(Encoding.ASCII.GetBytes("00000001"), 0, 8);

			// c_mtime[8]
			stream.Write(Encoding.ASCII.GetBytes(Convert.ToString(s_mtime.ToModificationTime(), 16).PadLeft(8, '0').ToUpper()), 0, 8);

			// c_filesize[8]
			stream.Write(Encoding.ASCII.GetBytes("00000000"), 0, 8);

			// c_maj[8]
			stream.Write(Encoding.ASCII.GetBytes("00000003"), 0, 8);

			// c_min[8]
			stream.Write(Encoding.ASCII.GetBytes("00000001"), 0, 8);

			// c_rmaj[8]
			stream.Write(Encoding.ASCII.GetBytes(Convert.ToString(Major, 16).PadLeft(8, '0').ToUpper()), 0, 8);

			// c_rmin[8]
			stream.Write(Encoding.ASCII.GetBytes(Convert.ToString(Minor, 16).PadLeft(8, '0').ToUpper()), 0, 8);

			// c_namesize[8]
			stream.Write(Encoding.ASCII.GetBytes(Convert.ToString(c_filename.Length, 16).PadLeft(8, '0').ToUpper()), 0, 8);

			// c_chksum[8]
			stream.Write(Encoding.ASCII.GetBytes("00000000"), 0, 8);

			// c_filename[]
			stream.Write(c_filename, 0, c_filename.Length);

			// 4-byte alignment
			int padding = (HEADER_LENGTH + c_filename.Length).AlignUp(4) - (HEADER_LENGTH + c_filename.Length);
			stream.Write(new byte[padding], 0, padding);

			return HEADER_LENGTH + c_filename.Length + padding;
		}
	}
}
