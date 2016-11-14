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

//---------------------------------------------------------------------------
// INITRAMFS CPIO ARCHIVE FORMAT
//
//  struct cpio_entry_t
//  {
//    // fixed-length header:
//    //
//    char_t c_magic[6];              // The string "070701" or "070702"
//    char_t c_ino[8];                // File inode number
//    char_t c_mode[8];               // File mode and permissions
//    char_t c_uid[8];                // File uid
//    char_t c_gid[8];                // File gid
//    char_t c_nlink[8];              // Number of links
//    char_t c_mtime[8];              // Modification time
//    char_t c_filesize[8];           // Size of data field
//    char_t c_maj[8];                // Major part of file device number
//    char_t c_min[8];                // Minor part of file device number
//    char_t c_rmaj[8];               // Major part of device node reference
//    char_t c_rmin[8];               // Minor part of device node reference
//    char_t c_namesize[8];           // Length of filename, including final \0
//    char_t c_chksum[8];             // Checksum of data field if c_magic is 070702
//    
//    // followed by:
//    //
//    char_t filename[];              // Null-terminated file name
//    uint8_t aligndata[];            // Padding to align data[] on a 4 byte boundary
//    uint8_t data[];                 // File data
//    uint8_t alignnext[];            // Padding to align next entry on a 4 byte boundary
//  };
//---------------------------------------------------------------------------

namespace zuki.vm.linux
{
	/// <summary>
	/// Describes a CPIO archive node
	/// </summary>
	abstract class CpioNode
	{
		/// <summary>
		/// Defines the length of the CPIO header
		/// </summary>
		protected const int HEADER_LENGTH = 110;        // c_magic[6] + (13 * c_xxxx[8])

		/// <summary>
		/// Instance Constructor
		/// </summary>
		/// <param name="path">Path to the destination node</param>
		protected CpioNode(string path)
		{
			Path = path;
		}

		//-------------------------------------------------------------------
		// Fields
		//-------------------------------------------------------------------

		/// <summary>
		/// Destination node owner group id
		/// </summary>
		public int GroupId = 0;			// root

		/// <summary>
		/// Destination node permissions mask
		/// </summary>
		public int Mode = 0x1FF;        // 0777

		/// <summary>
		/// Destination path within the generated archive
		/// </summary>
		public readonly string Path;

		/// <summary>
		/// Destination node owner user id
		/// </summary>
		public int UserId = 0;          // root

		//-------------------------------------------------------------------
		// Member Functions
		//-------------------------------------------------------------------

		/// <summary>
		/// Writes the node data into the specified stream
		/// </summary>
		/// <param name="stream">Stream to write the node data into</param>
		/// <param name="c_ino">Value to assign to the c_ino element</param>
		public abstract long Write(Stream stream, uint c_ino);

		//-------------------------------------------------------------------
		// Member Variables
		//-------------------------------------------------------------------

		/// <summary>
		/// Default modification time for non-file node types
		/// </summary>
		static protected readonly DateTime s_mtime = DateTime.UtcNow;
	}
}
