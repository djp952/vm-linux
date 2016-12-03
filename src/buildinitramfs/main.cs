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

//---------------------------------------------------------------------------
// Usage: 
//
// buildinitramfs {archive} {manifest} [-compress:{bzip2|gzip|lz4|lzma|xz}]
//
// Creates an initramfs CPIO archive based on an XML manifest
//
//  archive				- archive file to be created
//  manifest			- manifest file describing the archive contents
//	-compress:[method]	- compress the created archive
//
// Valid compression [method] values:
//
//	bzip2		- compress using BZIP2 compression encoder
//	gzip		- compress using GZIP compression encoder
//	lz4			- compress using LZ4 compression encoder (legacy mode)
//	lzma		- compress using LZMA compression encoder
//	xz			- compress using XZ compression encoder
//---------------------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Xml;
using zuki.io.compression;

namespace zuki.vm.linux
{
	class main
	{
		/// <summary>
		/// Application entry point
		/// </summary>
		/// <param name="arguments">Command-line arguments</param>
		static int Main(string[] arguments)
		{
			// Parse the command line arguments and switches
			CommandLine commandline = new CommandLine(arguments);

			// Simple banner for the output panel
			Console.WriteLine();
			Console.WriteLine("buildinitramfs " + String.Join(" ", arguments));
			Console.WriteLine();

			try
			{
				// There needs to be exactly 2 command line arguments present - {archive} and {manifest}
				if (commandline.Arguments.Count != 2) throw new ArgumentException("Invalid command line arguments");

				// Create an appropriate compression encoder if specified
				Encoder compressor = null;
				if (commandline.Switches.ContainsKey("compress"))
				{
					switch (commandline.Switches["compress"].ToLower())
					{
						case "bzip2": compressor = CreateBzip2Encoder(); break;
						case "gzip": compressor = CreateGzipEncoder(); break;
						case "lz4": compressor = CreateLz4Encoder(); break;
						case "lzma": compressor = CreateLzmaEncoder(); break;
						case "xz": compressor = CreateXzEncoder(); break;
						default: throw new ArgumentException("Invalid compression method '" + commandline.Switches["compress"] + "'.");
					}
				}

				// Append the archive file name to the current directory in case it's relative
				string archivefile = Path.Combine(Environment.CurrentDirectory, commandline.Arguments[0]);

				// Ensure that the specified output directory exists
				if (!Directory.Exists(Path.GetDirectoryName(archivefile)) && !Util.TryCreateDirectory(Path.GetDirectoryName(archivefile)))
					throw new DirectoryNotFoundException("specified output directory [" + Path.GetDirectoryName(archivefile) + "] does not exist");

				try
				{
					// Use a CpioGenerator to build the output data incrementally in a stream
					using (CpioGenerator generator = new CpioGenerator(LoadManifest(commandline.Arguments[1])))
					{
						// Create the output archive file stream
						using (Stream outstream = File.Create(archivefile))
						{
							// Stream the CpioGenerator into the output file, optionally passing 
							// it through one of the compression encoders
							if (compressor == null) generator.CopyTo(outstream);
							else compressor.Encode(generator, outstream);

							// Flush the changes to the output stream
							outstream.Flush();
						}

						// Output the final block count from the generator stream
						Console.WriteLine(String.Format("{0} blocks", (generator.Length) / 512));
					}

					return 0;
				}

				// Delete the created archive file on any exception during creation
				catch { Util.TryDeleteFile(archivefile); throw; }
			}

			catch (Exception ex)
			{
				// Nothing fancy, just catch anything that escapes and spit it out
				Console.WriteLine("ERROR: " + ex.Message);
				Console.WriteLine();
		
				return unchecked((int)0x80004005);				// <-- E_FAIL
			}
		}

		//-------------------------------------------------------------------
		// Private Member Functions
		//-------------------------------------------------------------------

		/// <summary>
		/// Creates an instance of the Bzip2Encoder for compression
		/// </summary>
		private static Encoder CreateBzip2Encoder()
		{
			// bzip2 -9
			//
			Bzip2Encoder encoder = new Bzip2Encoder();
			encoder.CompressionLevel = Bzip2CompressionLevel.Optimal;

			return encoder;
		}

		/// <summary>
		/// Creates an instance of the GzipEncoder for compression
		/// </summary>
		private static Encoder CreateGzipEncoder()
		{
			// gzip -9
			//
			GzipEncoder encoder = new GzipEncoder();
			encoder.CompressionLevel = GzipCompressionLevel.Optimal;

			return encoder;
		}

		/// <summary>
		/// Creates an instance of the CreateLz4Encoder for compression
		/// </summary>
		private static Encoder CreateLz4Encoder()
		{
			// lz4 -l -9
			//
			Lz4LegacyEncoder encoder = new Lz4LegacyEncoder();
			encoder.CompressionLevel = Lz4CompressionLevel.Optimal;

			return encoder;
		}

		/// <summary>
		/// Creates an instance of the LzmaEncoder for compression
		/// </summary>
		private static Encoder CreateLzmaEncoder()
		{
			// lzma -9
			//
			LzmaEncoder encoder = new LzmaEncoder();
			encoder.CompressionLevel = LzmaCompressionLevel.Optimal;

			return encoder;
		}

		/// <summary>
		/// Creates an instance of the XzEncoder for compression
		/// </summary>
		private static Encoder CreateXzEncoder()
		{
			// xz --check=crc32 --lzma2=dict=1MiB
			//
			XzEncoder encoder = new XzEncoder();
			encoder.CompressionLevel = LzmaCompressionLevel.Optimal;
			encoder.Checksum = XzChecksum.CRC32;
			encoder.DictionarySize = (1 << 20);

			return encoder;
		}

		/// <summary>
		/// Loads all of the archive file information from the manifest file
		/// </summary>
		/// <param name="manifestfile">Path to the manifest.xml file</param>
		private static List<CpioNode> LoadManifest(string manifestfile)
		{
			// Ensure that the manifest file exists
			if (!File.Exists(manifestfile)) throw new FileNotFoundException("specified manifest file [" + manifestfile + "] does not exist", manifestfile);

			// Create a new empty list of nodes
			List<CpioNode> archivenodes = new List<CpioNode>();

			// Create a new XmlDocument instance for the specified input file path
			XmlDocument document = new XmlDocument();
			document.Load(manifestfile);

			// Process each child element under the document element
			foreach (XmlNode child in document.DocumentElement.ChildNodes)
			{
				if (child.NodeType != XmlNodeType.Element) continue;
				XmlElement element = (XmlElement)child;

				try
				{
					// Get the name of the child element (<file>, <directory>, <symlink>, etc)
					string elementname = child.Name.ToLower();

					// Required: path= (Common)
					string path = element.GetAttribute("path");
					if (String.IsNullOrEmpty(path)) throw new Exception("missing or empty attribute 'path'");

					// <blockdevice path= major= minor= [mode="0777"] [uid="0"] [gid="0"]/>
					if (elementname == "blockdevice")
					{
						CpioBlockDeviceNode node = new CpioBlockDeviceNode(path);

						if (!Util.TryParseInt32(element.GetAttribute("major"), out node.Major)) throw new Exception("<blockdevice>: missing or invalid attribute 'major'");
						if (!Util.TryParseInt32(element.GetAttribute("minor"), out node.Minor)) throw new Exception("<blockdevice>: missing or invalid attribute 'minor'");

						if (!Util.TryParseInt32((element.HasAttribute("mode")) ? element.GetAttribute("mode") : "0777", out node.Mode)) throw new Exception("<blockdevice>: unable to parse attribute 'mode'");
						if (!Util.TryParseInt32((element.HasAttribute("uid")) ? element.GetAttribute("uid") : "0", out node.UserId)) throw new Exception("<blockdevice>: unable to parse attribute 'mode'");
						if (!Util.TryParseInt32((element.HasAttribute("gid")) ? element.GetAttribute("gid") : "0", out node.GroupId)) throw new Exception("<blockdevice>: unable to parse attribute 'mode'");

						archivenodes.Add(node);
					}

					// <chardevice path= major= minor= [mode="0777"] [uid="0"] [gid="0"]/>
					else if (elementname == "chardevice")
					{
						CpioCharacterDeviceNode node = new CpioCharacterDeviceNode(path);

						if (!Util.TryParseInt32(element.GetAttribute("major"), out node.Major)) throw new Exception("<chardevice>: missing or invalid attribute 'major'");
						if (!Util.TryParseInt32(element.GetAttribute("minor"), out node.Minor)) throw new Exception("<chardevice>: missing or invalid attribute 'minor'");

						if (!Util.TryParseInt32((element.HasAttribute("mode")) ? element.GetAttribute("mode") : "0777", out node.Mode)) throw new Exception("<chardevice>: unable to parse attribute 'mode'");
						if (!Util.TryParseInt32((element.HasAttribute("uid")) ? element.GetAttribute("uid") : "0", out node.UserId)) throw new Exception("<chardevice>: unable to parse attribute 'mode'");
						if (!Util.TryParseInt32((element.HasAttribute("gid")) ? element.GetAttribute("gid") : "0", out node.GroupId)) throw new Exception("<chardevice>: unable to parse attribute 'mode'");

						archivenodes.Add(node);
					}

					// <directory path= [mode="0777"] [uid="0"] [gid="0"]/>
					else if (elementname == "directory")
					{
						CpioDirectoryNode node = new CpioDirectoryNode(path);

						if (!Util.TryParseInt32((element.HasAttribute("mode")) ? element.GetAttribute("mode") : "0777", out node.Mode)) throw new Exception("<directory>: unable to parse attribute 'mode'");
						if (!Util.TryParseInt32((element.HasAttribute("uid")) ? element.GetAttribute("uid") : "0", out node.UserId)) throw new Exception("<directory>: unable to parse attribute 'mode'");
						if (!Util.TryParseInt32((element.HasAttribute("gid")) ? element.GetAttribute("gid") : "0", out node.GroupId)) throw new Exception("<directory>: unable to parse attribute 'mode'");

						archivenodes.Add(node);
					}

					// <file path= source= [mode="0777"] [uid="0"] [gid="0"] [normalize="false"]/>
					else if (elementname == "file")
					{
						CpioFileNode node = new CpioFileNode(path);

						node.Source = element.GetAttribute("source");
						if (String.IsNullOrEmpty(node.Source)) throw new Exception("<file>: missing or empty attribute 'source'");

						if (!Util.TryParseInt32((element.HasAttribute("mode")) ? element.GetAttribute("mode") : "0777", out node.Mode)) throw new Exception("<file>: unable to parse attribute 'mode'");
						if (!Util.TryParseInt32((element.HasAttribute("uid")) ? element.GetAttribute("uid") : "0", out node.UserId)) throw new Exception("<file>: unable to parse attribute 'mode'");
						if (!Util.TryParseInt32((element.HasAttribute("gid")) ? element.GetAttribute("gid") : "0", out node.GroupId)) throw new Exception("<file>: unable to parse attribute 'mode'");

						string normalize = element.GetAttribute("normalize");
						if (!String.IsNullOrEmpty(normalize) && !bool.TryParse(normalize, out node.NormalizeLineEndings)) throw new Exception("<file>: unable to parse attribute 'normalize'");

						foreach (XmlNode subchild in element.ChildNodes)
						{
							if (subchild.NodeType != XmlNodeType.Element) continue;
							XmlElement subelement = (XmlElement)subchild;

							// <link path=/>
							if (subelement.Name.ToLower() == "link")
							{
								if (!subelement.HasAttribute("path")) throw new Exception("<link>: missing attribute 'path'");
								node.Links.Add(subelement.GetAttribute("path"));
							}

							else throw new Exception("<file>: unexpected child element '" + subelement.Name + "'");
						}

						archivenodes.Add(node);
					}

					// <pipe path= [mode="0777"] [uid="0"] [gid="0"]/>
					else if (elementname == "pipe")
					{
						CpioPipeNode node = new CpioPipeNode(path);

						if (!Util.TryParseInt32((element.HasAttribute("mode")) ? element.GetAttribute("mode") : "0777", out node.Mode)) throw new Exception("<pipe>: unable to parse attribute 'mode'");
						if (!Util.TryParseInt32((element.HasAttribute("uid")) ? element.GetAttribute("uid") : "0", out node.UserId)) throw new Exception("<pipe>: unable to parse attribute 'mode'");
						if (!Util.TryParseInt32((element.HasAttribute("gid")) ? element.GetAttribute("gid") : "0", out node.GroupId)) throw new Exception("<pipe>: unable to parse attribute 'mode'");

						archivenodes.Add(node);
					}

					// <socket path= [mode="0777"] [uid="0"] [gid="0"]/>
					else if (elementname == "socket")
					{
						CpioSocketNode node = new CpioSocketNode(path);

						if (!Util.TryParseInt32((element.HasAttribute("mode")) ? element.GetAttribute("mode") : "0777", out node.Mode)) throw new Exception("<socket>: unable to parse attribute 'mode'");
						if (!Util.TryParseInt32((element.HasAttribute("uid")) ? element.GetAttribute("uid") : "0", out node.UserId)) throw new Exception("<socket>: unable to parse attribute 'mode'");
						if (!Util.TryParseInt32((element.HasAttribute("gid")) ? element.GetAttribute("gid") : "0", out node.GroupId)) throw new Exception("<socket>: unable to parse attribute 'mode'");

						archivenodes.Add(node);
					}

					// <symlink path= target= [mode="0777"] [uid="0"] [gid="0"]/>
					else if (elementname == "symlink")
					{
						CpioSymbolicLinkNode node = new CpioSymbolicLinkNode(path);

						node.Target = element.GetAttribute("target");
						if (String.IsNullOrEmpty(node.Target)) throw new Exception("missing or empty attribute 'target'");

						if (!Util.TryParseInt32((element.HasAttribute("mode")) ? element.GetAttribute("mode") : "0777", out node.Mode)) throw new Exception("<symlink>: unable to parse attribute 'mode'");
						if (!Util.TryParseInt32((element.HasAttribute("uid")) ? element.GetAttribute("uid") : "0", out node.UserId)) throw new Exception("<symlink>: unable to parse attribute 'mode'");
						if (!Util.TryParseInt32((element.HasAttribute("gid")) ? element.GetAttribute("gid") : "0", out node.GroupId)) throw new Exception("<symlink>: unable to parse attribute 'mode'");

						archivenodes.Add(node);
					}

					else throw new Exception("unexpected element <" + elementname + ">");
				}

				// Reformat the exception to include some context, the standard System.Xml.XmlDocument object
				// does not maintain line/column numbers so dump up to the first 40 characters of the line
				catch (Exception ex) { throw new Exception(String.Format("Manifest element [{0}...]: {1}", 
					element.OuterXml.Substring(0, Math.Min(40, element.OuterXml.Length)), ex.Message)); }
			}

			return archivenodes;
		}
	}
}
