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
using System.CodeDom.Compiler;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using zuki.tools.llvm.clang;
using zuki.tools.llvm.clang.extensions;

using SysFile = System.IO.File;
using ClangFile = zuki.tools.llvm.clang.File;
using ClangType = zuki.tools.llvm.clang.Type;

namespace zuki.vm.linux
{
	static class UapiHeader
	{
		/// <summary>
		/// Converts a TranslationUnit instance into a single header file that contains all
		/// of the declarations and macro definitions.  Macro definitions and enumeration
		/// constants are prefixed with "UAPI_", declarations are prefixed with "uapi_"
		/// </summary>
		/// <param name="transunit">TranslationUnit to be converted</param>
		/// <param name="clangargs">Command line arguments passed into clang</param>
		/// <param name="outheader">Output header file</param>
		/// <param name="prefix">Prefix string for declarations</param>
		/// <param name="nodecls">Flag indicating that declarations should not be emitted</param>
		public static void Generate(TranslationUnit transunit, IEnumerable<string> clangargs, string outheader, string prefix, bool nodecls)
		{
			if (transunit == null) throw new ArgumentNullException("transunit");
			if (String.IsNullOrEmpty(outheader)) throw new ArgumentNullException("outheader");
			if (!Directory.Exists(Path.GetDirectoryName(outheader))) throw new DirectoryNotFoundException(Path.GetDirectoryName(outheader));
			if (SysFile.Exists(outheader)) SysFile.Delete(outheader);

			// Set up the preamble template
			UapiHeaderPreamble preamble = new UapiHeaderPreamble();
			preamble.ClangArguments = clangargs;
			preamble.HeaderName = Path.GetFileName(outheader);

			// Set up the epilogue template
			UapiHeaderEpilogue epilogue = new UapiHeaderEpilogue();
			epilogue.HeaderName = Path.GetFileName(outheader);

			// Create the dictionary of name mappings; when dealing with macro definitions it's easier
			// to search and replace strings than it is to try and determine if something should be 
			// renamed during output processing
			var namemappings = CreateNameMappings(transunit, prefix);

			// Create an IndentedTextWriter instance to control the output
			using (IndentedTextWriter writer = new IndentedTextWriter(SysFile.CreateText(outheader)))
			{
				writer.Write(preamble.TransformText());

				// Process all of the top-level cursors within the translation unit
				transunit.Cursor.EnumerateChildren((cursor, parent) =>
				{
					// Skip over anonymous declarations that are not enumerations
					if (String.IsNullOrEmpty(cursor.DisplayName) && (cursor.Kind != CursorKind.EnumDecl)) return EnumerateChildrenResult.Continue;

					// DECLARATION
					//
					if ((!nodecls) && (cursor.Kind.IsDeclaration))
					{
						// Write the original location of the declaration for reference
						writer.WriteLine("// " + cursor.Location.ToString());

						// Process enumeration, structure, typedef and union declarations
						if (cursor.Kind == CursorKind.EnumDecl) EmitEnum(writer, cursor, prefix);
						else if (cursor.Kind == CursorKind.StructDecl) EmitStruct(writer, cursor, prefix);
						else if (cursor.Kind == CursorKind.TypedefDecl) EmitTypedef(writer, cursor, prefix);
						else if (cursor.Kind == CursorKind.UnionDecl) EmitUnion(writer, cursor, prefix);

						// Default to emitting the expected alignment of the declaration, but don't do
						// it for POD types; GCC and Visual C++ default align 8-byte integers differently
						bool emitalignment = true;
						if ((cursor.Kind == CursorKind.TypedefDecl) && (cursor.UnderlyingTypedefType.IsPOD)) emitalignment = false;

						// Apply a static_assert in the output to verify that the compiled size of
						// structures, typedefs and unions matches what the clang API calculated
						if ((cursor.Kind != CursorKind.EnumDecl) && (cursor.Type.Size != null))
						{
							writer.WriteLine();
							writer.WriteLine("#if !defined(__midl)");

							// Emitting the alignment is optional, it was removed for POD types
							if(emitalignment) writer.WriteLine("static_assert(alignof(" + prefix.ToLower() + "_" + cursor.DisplayName + ") == " + cursor.Type.Alignment.ToString() +
								", \"" + prefix.ToLower() + "_" + cursor.DisplayName + ": incorrect alignment\");");

							writer.WriteLine("static_assert(sizeof(" + prefix.ToLower() + "_" + cursor.DisplayName + ") == " + cursor.Type.Size.ToString() +
								", \"" + prefix.ToLower() + "_" + cursor.DisplayName + ": incorrect size\");");
							writer.WriteLine("#endif");
						}

						writer.WriteLine();
					}

					// MACRO DEFINITION
					//
					else if ((cursor.Kind == CursorKind.MacroDefinition) && (!ClangFile.IsNull(cursor.Location.File)))
					{
						// Write the original location of the definition for reference and emit the macro
						writer.WriteLine("// " + cursor.Location.ToString());
						EmitMacroDefinition(writer, cursor, namemappings, prefix);
						writer.WriteLine();
					}

					return EnumerateChildrenResult.Continue;
				});

				writer.Write(epilogue.TransformText());
				writer.Flush();
			}
		}

		/// <summary>
		/// Creates a dictionary of current->new declaration names, it was prohibitive to try and do
		/// this inline when processing macros in the translation unit as clang doesn't annotate macro
		/// expansion tokens separately.  A string search/replace is required instead (slow and ugly)
		/// </summary>
		/// <param name="transunit">TranslationUnit instance</param>
		/// <param name="prefix">Prefix string for declarations</param>
		private static Dictionary<string, string> CreateNameMappings(TranslationUnit transunit, string prefix)
		{
			Dictionary<string, string> dictionary = new Dictionary<string, string>();

			// MACRO DEFINITIONS (UAPI_)
			//
			foreach (var macrodecl in transunit.Cursor.FindChildren((c, p) => (c.Kind == CursorKind.MacroDefinition) && (!ClangFile.IsNull(c.Location.File))).Select(t => t.Item1))
			{
				if(!String.IsNullOrEmpty(macrodecl.DisplayName)) dictionary.Add(macrodecl.DisplayName,  prefix.ToUpper() + "_" + macrodecl.DisplayName);
			}

			// ENUMERATION CONSTANTS (UAPI_)
			//
			foreach(Cursor enumdecl in transunit.Cursor.FindChildren((c, p) => c.Kind == CursorKind.EnumDecl).Select(t => t.Item1))
			{
				var constants = enumdecl.FindChildren((c, p) => c.Kind == CursorKind.EnumConstantDecl).Select(t => t.Item1);
				foreach(Cursor constant in constants) dictionary.Add(constant.DisplayName,  prefix.ToUpper() + "_" + constant.DisplayName);
			}

			// DECLARATIONS (uapi_)
			//
			foreach (Cursor decl in transunit.Cursor.FindChildren((c, p) => c.Kind.IsDeclaration).Select(t => t.Item1))
			{
				// Declaration names override macro and enumeration constant names
				if (!String.IsNullOrEmpty(decl.DisplayName)) dictionary[decl.DisplayName] =  prefix.ToLower() + "_" + decl.DisplayName;
			}

			return dictionary;
		}

		/// <summary>
		/// Emits an enumeration declaration
		/// </summary>
		/// <param name="writer">Text writer instance</param>
		/// <param name="cursor">Enumeration cursor to be emitted</param>
		/// <param name="prefix">Prefix string for declarations</param>
		private static void EmitEnum(IndentedTextWriter writer, Cursor cursor, string prefix)
		{
			if (writer == null) throw new ArgumentNullException("writer");
			if (cursor == null) throw new ArgumentNullException("cursor");
			if (cursor.Kind != CursorKind.EnumDecl) throw new Exception("EmitEnum: Unexpected cursor kind");

			// Like structs and unions, enums can be anonymous and not have a display name
			writer.Write("enum ");
			if (!String.IsNullOrEmpty(cursor.DisplayName)) writer.Write(prefix.ToLower() + "_" + cursor.DisplayName + " ");
			writer.WriteLine("{");
			writer.WriteLine();

			// The only valid children of a enumeration declaration are enumeration constant declarations
			writer.Indent++;
			foreach (Cursor child in cursor.FindChildren((c, p) => c.Kind == CursorKind.EnumConstantDecl).Select(t => t.Item1)) EmitEnumConstant(writer, child, prefix);
			writer.Indent--;

			writer.WriteLine();
			writer.WriteLine("};");
		}

		/// <summary>
		/// Emits an enumeration constant declaration
		/// </summary>
		/// <param name="writer">Text writer instance</param>
		/// <param name="cursor">Enumeration constant cursor to be emitted</param>
		/// <param name="prefix">Prefix string for declarations</param>
		private static void EmitEnumConstant(IndentedTextWriter writer, Cursor cursor, string prefix)
		{
			if (writer == null) throw new ArgumentNullException("writer");
			if (cursor == null) throw new ArgumentNullException("cursor");
			if (cursor.Kind != CursorKind.EnumConstantDecl) throw new Exception("EmitEnumConstant: Unexpected cursor kind");

			// Enumeration constants lie in the global namespace, so they need to be prefixed with UAPI_
			writer.WriteLine(prefix.ToUpper() + "_" + cursor.DisplayName + " = " + cursor.EnumConstant.ToString() + ",");
		}

		/// <summary>
		/// Emits a field declaration
		/// </summary>
		/// <param name="writer">Text writer instance</param>
		/// <param name="cursor">Field declaration cursor to be emitted</param>
		/// <param name="prefix">Prefix string for declarations</param>
		private static void EmitField(IndentedTextWriter writer, Cursor cursor, string prefix)
		{
			if (writer == null) throw new ArgumentNullException("writer");
			if (cursor == null) throw new ArgumentNullException("cursor");
			if (cursor.Kind != CursorKind.FieldDecl) throw new Exception("EmitField: Unexpected cursor kind");

			// Determine the field type and get the declaration cursor for that type
			ClangType type = (cursor.Type.Kind == TypeKind.ConstantArray) ? cursor.Type.ArrayElementType : cursor.Type;

			// Check for zero-length constant array field declaration, which is commented out as it's not compatible with MIDL
			bool zerolength = ((cursor.Type.Kind == TypeKind.ConstantArray) && (cursor.Type.ArraySize == 0));
			if (zerolength) writer.Write("// ");

			// Check for anonymous struct / union being declared through the field
			if (String.IsNullOrEmpty(type.DeclarationCursor.DisplayName) && ((type.DeclarationCursor.Kind == CursorKind.StructDecl) || (type.DeclarationCursor.Kind == CursorKind.UnionDecl)))
			{
				if (type.DeclarationCursor.Kind == CursorKind.StructDecl) EmitStruct(writer, type.DeclarationCursor, prefix);
				else if (type.DeclarationCursor.Kind == CursorKind.UnionDecl) EmitUnion(writer, type.DeclarationCursor, prefix);
				else throw new Exception("Unexpected anonymous type detected for field " + cursor.DisplayName);
			}

			// Not an anonymous struct/union, just spit out the data type
			else EmitType(writer, type, prefix);
			
			// Field names do not receive a prefix, the display names are preserved
			writer.Write(" " + cursor.DisplayName);

			// ConstantArray fields require the element size suffix, this includes anonymous structs and unions
			if (cursor.Type.Kind == TypeKind.ConstantArray) writer.Write("[" + cursor.Type.ArraySize.ToString() + "]");

			// Fields that have a bit width require that width to be placed after the field name
			if (!(cursor.FieldBitWidth == null)) writer.Write(" : " + cursor.FieldBitWidth.ToString());
			
			writer.WriteLine(";");
		}

		/// <summary>
		/// Emits a macro definition.  The text of the macro uses a search and replace mechanism to deal with 
		/// code elements that have been renamed, it's seemingly not possible to do it any other way as the
		/// clang tokenizer will annotate each token as part of the macro definition
		/// </summary>
		/// <param name="writer">Text writer instance</param>
		/// <param name="cursor">Macro definition cursor to be emitted</param>
		/// <param name="namemappings">Dictionary of current->new code element names</param>
		/// <param name="prefix">Prefix string for declarations</param>
		private static void EmitMacroDefinition(IndentedTextWriter writer, Cursor cursor, Dictionary<string, string> namemappings, string prefix)
		{
			if (writer == null) throw new ArgumentNullException("writer");
			if (cursor == null) throw new ArgumentNullException("cursor");
			if (namemappings == null) throw new ArgumentNullException("macronames");
			if (cursor.Kind != CursorKind.MacroDefinition) throw new Exception("Unexpected cursor kind");

			// Macro names are prefixed with UAPI_
			writer.Write("#define " + prefix.ToUpper() + "_" + cursor.DisplayName);

			// Tokenize the macro so that each token string can be remapped if necessary
			using (var tokens = cursor.Extent.GetTokens())
			{
				List<string> strings = new List<string>();
				foreach (var token in tokens.Skip(1))
				{
					if (namemappings.ContainsKey(token.Spelling)) strings.Add(namemappings[token.Spelling]);
					else strings.Add(token.Spelling);
				}

				// Write the modified collection of token strings into the output file
				string str = String.Join(" ", strings).Trim();
				if (!String.IsNullOrEmpty(str)) writer.Write(" " + str);
			}

			writer.WriteLine();
		}

		/// <summary>
		/// Emits a structure declaration
		/// </summary>
		/// <param name="writer">Text writer instance</param>
		/// <param name="cursor">Structure declaration to be emitted</param>
		/// <param name="prefix">Prefix string for declarations</param>
		private static void EmitStruct(IndentedTextWriter writer, Cursor cursor, string prefix)
		{
			if (writer == null) throw new ArgumentNullException("writer");
			if (cursor == null) throw new ArgumentNullException("cursor");
			if (cursor.Kind != CursorKind.StructDecl) throw new Exception("EmitStruct: Unexpected cursor kind");

			// Determine if this is an anonymous structure, which implies that it can only appear as
			// part of another struct/union/typedef and won't get a terminating semi-colon
			bool anonymous = String.IsNullOrEmpty(cursor.DisplayName);

			// FORWARD DECLARATION
			//
			if ((!anonymous) && (cursor.Type.Size == null))
			{
				writer.WriteLine("struct " + cursor.DisplayName + ";");
				return;
			}

			// Some unions require a packing override, it appears that clang exposes it properly via alignment
			if (!anonymous) writer.WriteLine("#pragma pack(push, " + cursor.Type.Alignment.ToString() + ")");

			// Named structure declarations receive the lowercase uapi_ prefix
			writer.Write("struct ");
			if (!String.IsNullOrEmpty(cursor.DisplayName)) writer.Write(prefix.ToLower() + "_" + cursor.DisplayName + " ");
			writer.WriteLine("{");
			writer.WriteLine();

			// The only valid children of a structure declaration are field declarations
			writer.Indent++;
			foreach (Cursor child in cursor.FindChildren((c, p) => c.Kind == CursorKind.FieldDecl).Select(t => t.Item1)) EmitField(writer, child, prefix);
			writer.Indent--;

			writer.WriteLine();

			// Anonymous structures are left unterminated, named structures are terminated
			if (anonymous) writer.Write("} ");
			else writer.WriteLine("};");

			// Close out the #pragma pack for non-anonymous structures
			if(!anonymous) writer.WriteLine("#pragma pack(pop)");
		}

		/// <summary>
		/// Emits a type name; if the type is a global declaration the "uapi_" prefix is added to it
		/// </summary>
		/// <param name="writer">Text writer instance</param>
		/// <param name="type">Type name to be emitted</param>
		/// <param name="prefix">Prefix string for declarations</param>
		private static void EmitType(IndentedTextWriter writer, ClangType type, string prefix)
		{
			if (writer == null) throw new ArgumentNullException("writer");
			if (type == null) throw new ArgumentNullException("type");

			string suffix = String.Empty;

			// POINTER TYPE
			//
			if (type.Kind == TypeKind.Pointer)
			{
				suffix = " *";
				type = type.PointeeType;
			}

			// BUILT-IN TYPE
			//
			if (ClangFile.IsNull(type.DeclarationCursor.Location.File))
			{
				// Built-in types just get emitted as written in the source file, with
				// the exception of "long" and "unsigned long".  For those types use the
				// special VC++ "__int3264" data type which will be 4 bytes long on 32-bit
				// builds and 8 bytes long on 64-bit builds to match GCC

				if (type.Kind == TypeKind.Long) writer.Write("__int3264" + suffix);
				else if (type.Kind == TypeKind.ULong) writer.Write("__int3264" + suffix);
				else writer.Write(type.Spelling + suffix);

				return;
			}

			// Prefix struct and union types with the keywords 'struct' and 'union'
			if (type.DeclarationCursor.Kind == CursorKind.StructDecl) writer.Write("struct ");
			else if (type.DeclarationCursor.Kind == CursorKind.UnionDecl) writer.Write("union ");

			// Prefix global declarations with uapi_
			if (type.DeclarationCursor.SemanticParentCursor.Kind == CursorKind.TranslationUnit) writer.Write(prefix.ToLower() + "_");

			writer.Write(type.DeclarationCursor.Spelling + suffix);
		}

		/// <summary>
		/// Emits a typedef declaration
		/// </summary>
		/// <param name="writer">Text writer instance</param>
		/// <param name="cursor">typedef declaration to be emitted</param>
		/// <param name="prefix">Prefix string for declarations</param>
		private static void EmitTypedef(IndentedTextWriter writer, Cursor cursor, string prefix)
		{
			if (writer == null) throw new ArgumentNullException("writer");
			if (cursor == null) throw new ArgumentNullException("cursor");
			if (cursor.Kind != CursorKind.TypedefDecl) throw new Exception("EmitTypedef: Unexpected cursor kind");

			ClangType type = cursor.UnderlyingTypedefType;

			writer.Write("typedef ");

			// ANONYMOUS STRUCT / UNION
			//
			if (String.IsNullOrEmpty(type.DeclarationCursor.DisplayName) && ((type.DeclarationCursor.Kind == CursorKind.StructDecl) || (type.DeclarationCursor.Kind == CursorKind.UnionDecl)))
			{
				if (type.DeclarationCursor.Kind == CursorKind.StructDecl) EmitStruct(writer, type.DeclarationCursor, prefix);
				else if (type.DeclarationCursor.Kind == CursorKind.UnionDecl) EmitUnion(writer, type.DeclarationCursor, prefix);
				else throw new Exception("Unexpected anonymous type detected for typedef " + prefix.ToLower() + "_" + cursor.DisplayName);

				writer.WriteLine(prefix.ToLower() + "_" + cursor.DisplayName + ";");
			}

			// FUNCTION POINTER
			//
			else if (type.Spelling.Contains("("))
			{
				writer.WriteLine(type.Spelling.Replace("*", "* " + prefix.ToLower() + "_" + cursor.DisplayName) + ";");
			}

			// CONSTANT ARRAY
			//
			else if (type.Kind == TypeKind.ConstantArray)
			{
				EmitType(writer, type.ArrayElementType, prefix);
				writer.WriteLine(" " + prefix.ToLower() + "_" + cursor.DisplayName + "[" + type.ArraySize.ToString() + "];");
			}

			// ANYTHING ELSE
			//
			else
			{
				EmitType(writer, type, prefix);
				writer.WriteLine(" " + prefix.ToLower() + "_" + cursor.DisplayName + ";");
			}
		}

		/// <summary>
		/// Emits a union declaration
		/// </summary>
		/// <param name="writer">Text writer instance</param>
		/// <param name="cursor">Union declaration to be emitted</param>
		/// <param name="prefix">Prefix string for declarations</param>
		private static void EmitUnion(IndentedTextWriter writer, Cursor cursor, string prefix)
		{
			if (writer == null) throw new ArgumentNullException("writer");
			if (cursor == null) throw new ArgumentNullException("cursor");
			if (cursor.Kind != CursorKind.UnionDecl) throw new Exception("EmiUnion: Unexpected cursor kind");

			// Determine if this is an anonymous union, which implies that it can only appear as
			// part of another struct/union/typedef and won't get a terminating semi-colon
			bool anonymous = String.IsNullOrEmpty(cursor.DisplayName);

			// FORWARD DECLARATION
			//
			if ((!anonymous) && (cursor.Type.Size == null))
			{
				writer.WriteLine("union " + cursor.DisplayName + ";");
				return;
			}

			// Some unions require a packing override, it appears that clang exposes it properly via alignment
			if (!anonymous) writer.WriteLine("#pragma pack(push, " + cursor.Type.Alignment.ToString() + ")");

			// Named union declarations receive the lowercase uapi_ prefix
			writer.Write("union ");
			if (!String.IsNullOrEmpty(cursor.DisplayName)) writer.Write(prefix.ToLower() + "_" + cursor.DisplayName + " ");
			writer.WriteLine("{");
			writer.WriteLine();

			// The only valid children of a union declaration are field declarations
			writer.Indent++;
			foreach (Cursor child in cursor.FindChildren((c, p) => c.Kind == CursorKind.FieldDecl).Select(t => t.Item1)) EmitField(writer, child, prefix);
			writer.Indent--;

			writer.WriteLine();

			// Anonymous unions are left unterminated, named unions are terminated
			if (anonymous) writer.Write("} ");
			else writer.WriteLine("};");

			// Close out the #pragma pack for non-anonymous unions
			if (!anonymous) writer.WriteLine("#pragma pack(pop)");
		}
	}
}
