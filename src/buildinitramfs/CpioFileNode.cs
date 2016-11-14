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
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace zuki.vm.linux
{
	/// <summary>
	/// Implements a CPIO archive file node
	/// </summary>
	class CpioFileNode : CpioNode
	{
		/// <summary>
		/// Instance Constructor
		/// </summary>
		/// <param name="path">Path to the destination node</param>
		public CpioFileNode(string path) : base(path)
		{
		}

		//-------------------------------------------------------------------
		// Fields
		//-------------------------------------------------------------------

		/// <summary>
		/// List<> of additional hard link paths to the specified file
		/// </summary>
		public List<string> Links = new List<string>();

		/// <summary>
		/// Flag to normalize the line endings of the source file
		/// </summary>
		public bool NormalizeLineEndings = false;
		
		/// <summary>
		/// Source of the file data
		/// </summary>
		public string Source = String.Empty;

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

			long overall = 0;				// Overall length of the data written to the stream

			// Use only the permission flags from the Mode field, combine with S_IFREG
			int c_mode = ((Mode & 0x1FFF) | (int)CpioNodeType.File);

			// Get the modification time for the node(s) from the source file
			DateTime c_mtime = File.GetLastWriteTimeUtc(Source);

			// Generate a List<> containing all of the names (hard links) to the file node
			List<string> names = new List<string>(new string[] { Path });
			names.AddRange(Links);

			// Open or normalize the source file in order to copy the data into the output stream
			using (Stream data = (NormalizeLineEndings) ? NormalizeFile(Source) : File.OpenRead(Source))
			{
				// Process each link in a loop; the final link will be the one to get the file data
				for (int index = 0; index < names.Count; index++)
				{
					bool final = (index + 1 == names.Count);			// Final link?

					// Convert the Path field into a UTF-8 encoded null terminated byte array
					byte[] c_filename = Encoding.UTF8.GetBytes(names[index].TrimStart(new char[] { '/' }) + '\0');

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
					stream.Write(Encoding.ASCII.GetBytes(Convert.ToString(names.Count, 16).PadLeft(8, '0').ToUpper()), 0, 8);

					// c_mtime[8]
					stream.Write(Encoding.ASCII.GetBytes(Convert.ToString(c_mtime.ToModificationTime(), 16).PadLeft(8, '0').ToUpper()), 0, 8);

					// c_filesize[8]
					if(final) stream.Write(Encoding.ASCII.GetBytes(Convert.ToString(data.Length, 16).PadLeft(8, '0').ToUpper()), 0, 8);
					else stream.Write(Encoding.ASCII.GetBytes("00000000"), 0, 8);

					// c_maj[8]
					stream.Write(Encoding.ASCII.GetBytes("00000003"), 0, 8);

					// c_min[8]
					stream.Write(Encoding.ASCII.GetBytes("00000001"), 0, 8);

					// c_rmaj[8]
					stream.Write(Encoding.ASCII.GetBytes("00000000"), 0, 8);

					// c_rmin[8]
					stream.Write(Encoding.ASCII.GetBytes("00000000"), 0, 8);

					// c_namesize[8]
					stream.Write(Encoding.ASCII.GetBytes(Convert.ToString(c_filename.Length, 16).PadLeft(8, '0').ToUpper()), 0, 8);

					// c_chksum[8]
					stream.Write(Encoding.ASCII.GetBytes("00000000"), 0, 8);

					// c_filename[]
					stream.Write(c_filename, 0, c_filename.Length);

					// 4-byte alignment
					int padding = (HEADER_LENGTH + c_filename.Length).AlignUp(4) - (HEADER_LENGTH + c_filename.Length);
					stream.Write(new byte[padding], 0, padding);

					// Update the overall length of the data written
					overall += HEADER_LENGTH + c_filename.Length + padding;

					// Only the final link gets the data written to it
					if (final)
					{
						// c_filedata[]
						data.CopyTo(stream);

						// 4-byte alignment
						int padding2 = (int)((HEADER_LENGTH + c_filename.Length + padding + data.Length).AlignUp(4) - (HEADER_LENGTH + c_filename.Length + padding + data.Length));
						stream.Write(new byte[padding2], 0, padding2);

						// Update the overall length of the data written
						overall += data.Length + padding2;
					}
				}
			}

			return overall;
		}

		//-------------------------------------------------------------------
		// Private Member Functions
		//-------------------------------------------------------------------

		/// <summary>
		/// Normalizes the line endings of a text file for Unix
		/// </summary>
		/// <param name="filename">Input file name</param>
		private static Stream NormalizeFile(string filename)
		{
			if (!File.Exists(filename)) throw new FileNotFoundException("specified text file [" + filename + "] does not exist", filename);

			// Create the normalized text file in memory via a MemoryStream
			MemoryStream memstream = new MemoryStream();

			try
			{
				// Open up a reader against the source file ...
				using (StreamReader sr = new StreamReader(File.OpenRead(filename)))
				{
					// ... and a writer against the destination stream
					using (StreamWriter sw = new StreamWriter(memstream))
					{
						sw.NewLine = "\n";      // <-- Use Unix line endings

						// Just read in every line from the source file and write it back
						// out to the destination file using the defined line ending
						string line = sr.ReadLine();
						while (line != null)
						{
							sw.WriteLine(line);
							line = sr.ReadLine();
						}

						// Flush the output stream to disk
						sw.Flush();
					}
				}

				// Reset and return the generated MemoryStream back to the caller
				memstream.Position = 0;
				return memstream;
			}

			catch { memstream.Dispose(); throw; }
		}
	}
}
