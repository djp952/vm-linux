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

namespace zuki.vm.linux
{
	/// <summary>
	/// CPIO generator stream class used in conjunction with the compression
	/// encoder since not all of them can be used incrementally; this provides
	/// a stream reader that builds the CPIO data as it's requested.  Not very
	/// efficient -- it will buffer an entire CPIO node in memory regardless of
	/// the underlying file length; could be improved to stream that as well
	/// </summary>
	class CpioGenerator : Stream
	{
		/// <summary>
		/// Instance constructor
		/// </summary>
		/// <param name="nodes">Collection of CpioNode instances to generate</param>
		public CpioGenerator(List<CpioNode> nodes)
		{
			if (nodes == null) throw new ArgumentNullException("nodes");
			m_nodes = nodes;
		}

		//-------------------------------------------------------------------
		// Member Functions
		//-------------------------------------------------------------------

		/// <summary>
		/// Flushes any data buffered by the stream
		/// </summary>
		public override void Flush()
		{
		}

		/// <summary>
		/// Reads data from the stream at the current position
		/// </summary>
		/// <param name="buffer">Buffer to fill with stream data</param>
		/// <param name="offset">Offset within the buffer to begin writing</param>
		/// <param name="count">Maximum number of bytes to write into the buffer</param>
		/// <returns></returns>
		public override int Read(byte[] buffer, int offset, int count)
		{
			if (buffer == null) throw new ArgumentNullException("buffer");
			if (offset < 0) throw new ArgumentOutOfRangeException("offset");
			if (count < 0) throw new ArgumentOutOfRangeException("count");
			if ((offset + count) > buffer.Length) throw new ArgumentException("The sum of offset and count is larger than the buffer length");

			// If the input buffer has been exhausted, refill it
			if (m_buffer.Position == m_buffer.Length)
			{
				// If all the nodes, trailer, and padding have been written there
				// is nothing more to do, return zero to the caller
				if (m_finished) return 0;

				m_buffer.SetLength(0);				// Reset stream length to zero

				// If there are more nodes to process, generate and buffer the next one
				if (m_index < m_nodes.Count) m_length += m_nodes[m_index++].Write(m_buffer, m_nodeindex++);

				// Otherwise generate the final block of archive file data
				else {

					// Generate the TRAILER!!! node ...
					m_length += (new CpioTrailerNode().Write(m_buffer, m_nodeindex++));

					// ... and the 512-byte alignment padding to finalize the archive
					int padding = (int)(m_length.AlignUp(512) - m_length);
					m_buffer.Write(new Byte[padding], 0, padding);

					m_length += padding;			// Add in the padding length
					m_finished = true;				// No more after this is read
				}

				m_buffer.Position = 0;              // Reset stream position to zero
			}

			// Read up to requested amount of data from the buffered node data
			return m_buffer.Read(buffer, offset, count);
		}

		/// <summary>
		/// Sets the position within the stream
		/// </summary>
		/// <param name="offset">Delta offset from specified origin</param>
		/// <param name="origin">Origin from which to apply the delta offset</param>
		/// <returns></returns>
		public override long Seek(long offset, SeekOrigin origin)
		{
			throw new NotSupportedException();
		}

		/// <summary>
		/// Sets the length of the stream
		/// </summary>
		/// <param name="value">New size of the stream</param>
		public override void SetLength(long value)
		{
			throw new NotSupportedException();
		}

		/// <summary>
		/// Writes data into the stream at the current position
		/// </summary>
		/// <param name="buffer">Input buffer</param>
		/// <param name="offset">Offset within the buffer to the input data</param>
		/// <param name="count">Length of the input data</param>
		public override void Write(byte[] buffer, int offset, int count)
		{
			throw new NotSupportedException();
		}

		//-------------------------------------------------------------------
		// Properties
		//-------------------------------------------------------------------

		/// <summary>
		/// Indicates if the stream can be read from
		/// </summary>
		public override bool CanRead
		{
			get { return true; }
		}

		/// <summary>
		/// Indicates if the position within the stream can be set
		/// </summary>
		public override bool CanSeek
		{
			get { return false; }
		}

		/// <summary>
		/// Indicates if the stream can be written to
		/// </summary>
		public override bool CanWrite
		{
			get { return false; }
		}

		/// <summary>
		/// Gets the length of the stream
		/// </summary>
		public override long Length
		{
			get { return m_length; }
		}

		/// <summary>
		/// Gets the current position within the stream
		/// </summary>
		public override long Position
		{
			get { throw new NotSupportedException(); }
			set { throw new NotSupportedException(); }
		}

		//-------------------------------------------------------------------
		// Member Variables
		//-------------------------------------------------------------------

		/// <summary>
		/// Collection of CpioNode instances from which to generate the data
		/// </summary>
		readonly List<CpioNode> m_nodes;

		/// <summary>
		/// Current index into the collection of nodes
		/// </summary>
		int	m_index = 0;

		/// <summary>
		/// The overall archive length
		/// </summary>
		long m_length = 0;

		/// <summary>
		/// The current inode index to use
		/// </summary>
		uint m_nodeindex = 721;			// see gen_init_cpio.c

		/// <summary>
		/// Local input/output buffer for the stream
		/// </summary>
		MemoryStream m_buffer = new MemoryStream();

		/// <summary>
		/// Flag if the generation operation has completed
		/// </summary>
		bool m_finished = false;
	}
}
