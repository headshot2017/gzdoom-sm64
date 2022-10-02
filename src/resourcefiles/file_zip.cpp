/*
** file_zip.cpp
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
** Copyright 2005-2009 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
**
*/

#include <time.h>
#include "file_zip.h"
#include "cmdlib.h"
#include "templates.h"
#include "v_text.h"
#include "w_wad.h"
#include "w_zip.h"
#include "i_system.h"
#include "ancientzip.h"

#define BUFREADCOMMENT (0x400)

//==========================================================================
//
// Decompression subroutine
//
//==========================================================================

static bool UncompressZipLump(char *Cache, FileReader *Reader, int Method, int LumpSize, int CompressedSize, int GPFlags)
{
	try
	{
		switch (Method)
		{
		case METHOD_STORED:
		{
			Reader->Read(Cache, LumpSize);
			break;
		}

		case METHOD_DEFLATE:
		{
			FileReaderZ frz(*Reader, true);
			frz.Read(Cache, LumpSize);
			break;
		}

		case METHOD_BZIP2:
		{
			FileReaderBZ2 frz(*Reader);
			frz.Read(Cache, LumpSize);
			break;
		}

		case METHOD_LZMA:
		{
			FileReaderLZMA frz(*Reader, LumpSize, true);
			frz.Read(Cache, LumpSize);
			break;
		}

		case METHOD_IMPLODE:
		{
			FZipExploder exploder;
			exploder.Explode((unsigned char *)Cache, LumpSize, Reader, CompressedSize, GPFlags);
			break;
		}

		case METHOD_SHRINK:
		{
			ShrinkLoop((unsigned char *)Cache, LumpSize, Reader, CompressedSize);
			break;
		}

		default:
			assert(0);
			return false;
		}
	}
	catch (CRecoverableError &err)
	{
		Printf("%s\n", err.GetMessage());
		return false;
	}
	return true;
}

bool FCompressedBuffer::Decompress(char *destbuffer)
{
	MemoryReader mr(mBuffer, mCompressedSize);
	return UncompressZipLump(destbuffer, &mr, mMethod, mSize, mCompressedSize, mZipFlags);
}

//-----------------------------------------------------------------------
//
// Finds the central directory end record in the end of the file.
// Taken from Quake3 source but the file in question is not GPL'ed. ;)
//
//-----------------------------------------------------------------------

static uint32_t Zip_FindCentralDir(FileReader * fin)
{
	unsigned char buf[BUFREADCOMMENT + 4];
	uint32_t FileSize;
	uint32_t uBackRead;
	uint32_t uMaxBack; // maximum size of global comment
	uint32_t uPosFound=0;

	fin->Seek(0, SEEK_END);

	FileSize = fin->Tell();
	uMaxBack = MIN<uint32_t>(0xffff, FileSize);

	uBackRead = 4;
	while (uBackRead < uMaxBack)
	{
		uint32_t uReadSize, uReadPos;
		int i;
		if (uBackRead + BUFREADCOMMENT > uMaxBack) 
			uBackRead = uMaxBack;
		else
			uBackRead += BUFREADCOMMENT;
		uReadPos = FileSize - uBackRead;

		uReadSize = MIN<uint32_t>((BUFREADCOMMENT + 4), (FileSize - uReadPos));

		if (fin->Seek(uReadPos, SEEK_SET) != 0) break;

		if (fin->Read(buf, (int32_t)uReadSize) != (int32_t)uReadSize) break;

		for (i = (int)uReadSize - 3; (i--) > 0;)
		{
			if (buf[i] == 'P' && buf[i+1] == 'K' && buf[i+2] == 5 && buf[i+3] == 6)
			{
				uPosFound = uReadPos + i;
				break;
			}
		}

		if (uPosFound != 0)
			break;
	}
	return uPosFound;
}

//==========================================================================
//
// Zip file
//
//==========================================================================

FZipFile::FZipFile(const char * filename, FileReader *file)
: FResourceFile(filename, file)
{
	Lumps = NULL;
}

bool FZipFile::Open(bool quiet)
{
	uint32_t centraldir = Zip_FindCentralDir(Reader);
	FZipEndOfCentralDirectory info;
	int skipped = 0;

	Lumps = NULL;

	if (centraldir == 0)
	{
		if (!quiet) Printf(TEXTCOLOR_RED "\n%s: ZIP file corrupt!\n", Filename);
		return false;
	}

	// Read the central directory info.
	Reader->Seek(centraldir, SEEK_SET);
	Reader->Read(&info, sizeof(FZipEndOfCentralDirectory));

	// No multi-disk zips!
	if (info.NumEntries != info.NumEntriesOnAllDisks ||
		info.FirstDisk != 0 || info.DiskNumber != 0)
	{
		if (!quiet) Printf(TEXTCOLOR_RED "\n%s: Multipart Zip files are not supported.\n", Filename);
		return false;
	}

	NumLumps = LittleShort(info.NumEntries);
	Lumps = new FZipLump[NumLumps];

	// Load the entire central directory. Too bad that this contains variable length entries...
	int dirsize = LittleLong(info.DirectorySize);
	void *directory = malloc(dirsize);
	Reader->Seek(LittleLong(info.DirectoryOffset), SEEK_SET);
	Reader->Read(directory, dirsize);

	char *dirptr = (char*)directory;
	FZipLump *lump_p = Lumps;

	FString name0;
	bool foundspeciallump = false;

	// Check if all files have the same prefix so that this can be stripped out.
	// This will only be done if there is either a MAPINFO, ZMAPINFO or GAMEINFO lump in the subdirectory, denoting a ZDoom mod.
	if (NumLumps > 1) for (uint32_t i = 0; i < NumLumps; i++)
	{
		FZipCentralDirectoryInfo *zip_fh = (FZipCentralDirectoryInfo *)dirptr;

		int len = LittleShort(zip_fh->NameLength);
		FString name(dirptr + sizeof(FZipCentralDirectoryInfo), len);

		dirptr += sizeof(FZipCentralDirectoryInfo) +
			LittleShort(zip_fh->NameLength) +
			LittleShort(zip_fh->ExtraLength) +
			LittleShort(zip_fh->CommentLength);

		if (dirptr > ((char*)directory) + dirsize)	// This directory entry goes beyond the end of the file.
		{
			free(directory);
			if (!quiet) Printf(TEXTCOLOR_RED "\n%s: Central directory corrupted.", Filename);
			return false;
		}

		name.ToLower();
		if (i == 0)
		{
			// check for special names, if one of these gets found this must be treated as a normal zip.
			bool isspecial = !name.Compare("flats/") ||
				name.IndexOf("/") < 0 ||
				!name.Compare("textures/") ||
				!name.Compare("hires/") ||
				!name.Compare("sprites/") ||
				!name.Compare("voxels/") ||
				!name.Compare("colormaps/") ||
				!name.Compare("acs/") ||
				!name.Compare("maps/") ||
				!name.Compare("voices/") ||
				!name.Compare("patches/") ||
				!name.Compare("graphics/") ||
				!name.Compare("sounds/") ||
				!name.Compare("music/");
			if (isspecial) break;
			name0 = name;
		}
		else
		{
			if (name.IndexOf(name0) != 0)
			{
				name0 = "";
				break;
			}
			else if (!foundspeciallump)
			{
				// at least one of the more common definition lumps must be present.
				if (name.IndexOf(name0 + "mapinfo") == 0) foundspeciallump = true;
				else if (name.IndexOf(name0 + "zmapinfo") == 0) foundspeciallump = true;
				else if (name.IndexOf(name0 + "gameinfo") == 0) foundspeciallump = true;
				else if (name.IndexOf(name0 + "sndinfo") == 0) foundspeciallump = true;
				else if (name.IndexOf(name0 + "sbarinfo") == 0) foundspeciallump = true;
				else if (name.IndexOf(name0 + "menudef") == 0) foundspeciallump = true;
				else if (name.IndexOf(name0 + "gldefs") == 0) foundspeciallump = true;
				else if (name.IndexOf(name0 + "animdefs") == 0) foundspeciallump = true;
				else if (name.IndexOf(name0 + "decorate.") == 0) foundspeciallump = true;	// DECORATE is a common subdirectory name, so the check needs to be a bit different.
				else if (name.Compare(name0 + "decorate") == 0) foundspeciallump = true;
				else if (name.IndexOf(name0 + "zscript.") == 0) foundspeciallump = true;	// same here.
				else if (name.Compare(name0 + "zscript") == 0) foundspeciallump = true;
				else if (name.Compare(name0 + "maps/") == 0) foundspeciallump = true;
			}
		}
	}

	dirptr = (char*)directory;
	lump_p = Lumps;
	for (uint32_t i = 0; i < NumLumps; i++)
	{
		FZipCentralDirectoryInfo *zip_fh = (FZipCentralDirectoryInfo *)dirptr;

		int len = LittleShort(zip_fh->NameLength);
		FString name(dirptr + sizeof(FZipCentralDirectoryInfo), len);
		if (name0.IsNotEmpty()) name = name.Mid(name0.Len());
		dirptr += sizeof(FZipCentralDirectoryInfo) + 
				  LittleShort(zip_fh->NameLength) + 
				  LittleShort(zip_fh->ExtraLength) + 
				  LittleShort(zip_fh->CommentLength);

		if (dirptr > ((char*)directory) + dirsize)	// This directory entry goes beyond the end of the file.
		{
			free(directory);
			if (!quiet) Printf(TEXTCOLOR_RED "\n%s: Central directory corrupted.", Filename);
			return false;
		}
		
		// skip Directories
		if (name[len - 1] == '/' && LittleLong(zip_fh->UncompressedSize) == 0) 
		{
			skipped++;
			continue;
		}

		// Ignore unknown compression formats
		zip_fh->Method = LittleShort(zip_fh->Method);
		if (zip_fh->Method != METHOD_STORED &&
			zip_fh->Method != METHOD_DEFLATE &&
			zip_fh->Method != METHOD_LZMA &&
			zip_fh->Method != METHOD_BZIP2 &&
			zip_fh->Method != METHOD_IMPLODE &&
			zip_fh->Method != METHOD_SHRINK)
		{
			if (!quiet) Printf(TEXTCOLOR_YELLOW "\n%s: '%s' uses an unsupported compression algorithm (#%d).\n", Filename, name.GetChars(), zip_fh->Method);
			skipped++;
			continue;
		}
		// Also ignore encrypted entries
		zip_fh->Flags = LittleShort(zip_fh->Flags);
		if (zip_fh->Flags & ZF_ENCRYPTED)
		{
			if (!quiet) Printf(TEXTCOLOR_YELLOW "\n%s: '%s' is encrypted. Encryption is not supported.\n", Filename, name.GetChars());
			skipped++;
			continue;
		}

		FixPathSeperator(name);
		name.ToLower();

		lump_p->LumpNameSetup(name);
		lump_p->LumpSize = LittleLong(zip_fh->UncompressedSize);
		lump_p->Owner = this;
		// The start of the Reader will be determined the first time it is accessed.
		lump_p->Flags = LUMPF_ZIPFILE | LUMPFZIP_NEEDFILESTART;
		lump_p->Method = uint8_t(zip_fh->Method);
		lump_p->GPFlags = zip_fh->Flags;
		lump_p->CRC32 = zip_fh->CRC32;
		lump_p->CompressedSize = LittleLong(zip_fh->CompressedSize);
		lump_p->Position = LittleLong(zip_fh->LocalHeaderOffset);
		lump_p->CheckEmbedded();

		// Ignore some very specific names
		if (0 == stricmp("dehacked.exe", name))
		{
			memset(lump_p->Name, 0, sizeof(lump_p->Name));
		}

		lump_p++;
	}
	// Resize the lump record array to its actual size
	NumLumps -= skipped;
	free(directory);

	if (!quiet && !batchrun) Printf(TEXTCOLOR_NORMAL ", %d lumps\n", NumLumps);
	
	PostProcessArchive(&Lumps[0], sizeof(FZipLump));
	return true;
}

//==========================================================================
//
// Zip file
//
//==========================================================================

FZipFile::~FZipFile()
{
	if (Lumps != NULL) delete [] Lumps;
}

//==========================================================================
//
//
//
//==========================================================================

FCompressedBuffer FZipLump::GetRawData()
{
	FCompressedBuffer cbuf = { (unsigned)LumpSize, (unsigned)CompressedSize, Method, GPFlags, CRC32, new char[CompressedSize] };
	if (Flags & LUMPFZIP_NEEDFILESTART) SetLumpAddress();
	Owner->Reader->Seek(Position, SEEK_SET);
	Owner->Reader->Read(cbuf.mBuffer, CompressedSize);
	return cbuf;
}

//==========================================================================
//
// SetLumpAddress
//
//==========================================================================

void FZipLump::SetLumpAddress()
{
	// This file is inside a zip and has not been opened before.
	// Position points to the start of the local file header, which we must
	// read and skip so that we can get to the actual file data.
	FZipLocalFileHeader localHeader;
	int skiplen;

	FileReader *file = Owner->Reader;

	file->Seek(Position, SEEK_SET);
	file->Read(&localHeader, sizeof(localHeader));
	skiplen = LittleShort(localHeader.NameLength) + LittleShort(localHeader.ExtraLength);
	Position += sizeof(localHeader) + skiplen;
	Flags &= ~LUMPFZIP_NEEDFILESTART;
}

//==========================================================================
//
// Get reader (only returns non-NULL if not encrypted)
//
//==========================================================================

FileReader *FZipLump::GetReader()
{
	// Don't return the reader if this lump is encrypted
	// In that case always force caching of the lump
	if (Method == METHOD_STORED)
	{
		if (Flags & LUMPFZIP_NEEDFILESTART) SetLumpAddress();
		Owner->Reader->Seek(Position, SEEK_SET);
		return Owner->Reader;
	}
	else return NULL;	
}

//==========================================================================
//
// Fills the lump cache and performs decompression
//
//==========================================================================

int FZipLump::FillCache()
{
	if (Flags & LUMPFZIP_NEEDFILESTART) SetLumpAddress();
	const char *buffer;

	if (Method == METHOD_STORED && (buffer = Owner->Reader->GetBuffer()) != NULL)
	{
		// This is an in-memory file so the cache can point directly to the file's data.
		Cache = const_cast<char*>(buffer) + Position;
		RefCount = -1;
		return -1;
	}

	Owner->Reader->Seek(Position, SEEK_SET);
	Cache = new char[LumpSize];
	UncompressZipLump(Cache, Owner->Reader, Method, LumpSize, CompressedSize, GPFlags);
	RefCount = 1;
	return 1;
}

//==========================================================================
//
//
//
//==========================================================================

int FZipLump::GetFileOffset()
{
	if (Method != METHOD_STORED) return -1;
	if (Flags & LUMPFZIP_NEEDFILESTART) SetLumpAddress();
	return Position;
}

//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile *CheckZip(const char *filename, FileReader *file, bool quiet)
{
	char head[4];

	if (file->GetLength() >= (long)sizeof(FZipLocalFileHeader))
	{
		file->Seek(0, SEEK_SET);
		file->Read(&head, 4);
		file->Seek(0, SEEK_SET);
		if (!memcmp(head, "PK\x3\x4", 4))
		{
			FResourceFile *rf = new FZipFile(filename, file);
			if (rf->Open(quiet)) return rf;

			rf->Reader = NULL; // to avoid destruction of reader
			delete rf;
		}
	}
	return NULL;
}



//==========================================================================
//
// time_to_dos
//
// Converts time from struct tm to the DOS format used by zip files.
//
//==========================================================================

static void time_to_dos(struct tm *time, unsigned short *dosdate, unsigned short *dostime)
{
	if (time == NULL || time->tm_year < 80)
	{
		*dosdate = *dostime = 0;
	}
	else
	{
		*dosdate = LittleShort((time->tm_year - 80) * 512 + (time->tm_mon + 1) * 32 + time->tm_mday);
		*dostime = LittleShort(time->tm_hour * 2048 + time->tm_min * 32 + time->tm_sec / 2);
	}
}

//==========================================================================
//
// append_to_zip
//
// Write a given file to the zipFile.
// 
// zipfile: zip object to be written to
// 
// returns: position = success, -1 = error
//
//==========================================================================

int AppendToZip(FILE *zip_file, const char *filename, FCompressedBuffer &content, uint16_t date, uint16_t time)
{
	FZipLocalFileHeader local;
	int position;

	local.Magic = ZIP_LOCALFILE;
	local.VersionToExtract[0] = 20;
	local.VersionToExtract[1] = 0;
	local.Flags = content.mMethod == METHOD_DEFLATE ? LittleShort(2) : LittleShort((uint16_t)content.mZipFlags);
	local.Method = LittleShort(content.mMethod);
	local.ModDate = date;
	local.ModTime = time;
	local.CRC32 = content.mCRC32;
	local.UncompressedSize = LittleLong(content.mSize);
	local.CompressedSize = LittleLong(content.mCompressedSize);
	local.NameLength = LittleShort((unsigned short)strlen(filename));
	local.ExtraLength = 0;

	// Fill in local directory header.

	position = (int)ftell(zip_file);

	// Write out the header, file name, and file data.
	if (fwrite(&local, sizeof(local), 1, zip_file) != 1 ||
		fwrite(filename, strlen(filename), 1, zip_file) != 1 ||
		fwrite(content.mBuffer, 1, content.mCompressedSize, zip_file) != content.mCompressedSize)
	{
		return -1;
	}
	return position;
}


//==========================================================================
//
// write_central_dir
//
// Writes the central directory entry for a file.
//
//==========================================================================

int AppendCentralDirectory(FILE *zip_file, const char *filename, FCompressedBuffer &content, uint16_t date, uint16_t time, int position)
{
	FZipCentralDirectoryInfo dir;

	dir.Magic = ZIP_CENTRALFILE;
	dir.VersionMadeBy[0] = 20;
	dir.VersionMadeBy[1] = 0;
	dir.VersionToExtract[0] = 20;
	dir.VersionToExtract[1] = 0;
	dir.Flags = content.mMethod == METHOD_DEFLATE ? LittleShort(2) : LittleShort((uint16_t)content.mZipFlags);
	dir.Method = LittleShort(content.mMethod);
	dir.ModTime = time;
	dir.ModDate = date;
	dir.CRC32 = content.mCRC32;
	dir.CompressedSize = LittleLong(content.mCompressedSize);
	dir.UncompressedSize = LittleLong(content.mSize);
	dir.NameLength = LittleShort((unsigned short)strlen(filename));
	dir.ExtraLength = 0;
	dir.CommentLength = 0;
	dir.StartingDiskNumber = 0;
	dir.InternalAttributes = 0;
	dir.ExternalAttributes = 0;
	dir.LocalHeaderOffset = LittleLong(position);

	if (fwrite(&dir, sizeof(dir), 1, zip_file) != 1 ||
		fwrite(filename,  strlen(filename), 1, zip_file) != 1)
	{
		return -1;
	}
	return 0;
}

bool WriteZip(const char *filename, TArray<FString> &filenames, TArray<FCompressedBuffer> &content)
{
	// try to determine local time
	struct tm *ltime;
	time_t ttime;
	uint16_t mydate, mytime;
	ttime = time(nullptr);
	ltime = localtime(&ttime);
	time_to_dos(ltime, &mydate, &mytime);

	TArray<int> positions;

	if (filenames.Size() != content.Size()) return false;

	FILE *f = fopen(filename, "wb");
	if (f != nullptr)
	{
		for (unsigned i = 0; i < filenames.Size(); i++)
		{
			int pos = AppendToZip(f, filenames[i], content[i], mydate, mytime);
			if (pos == -1)
			{
				fclose(f);
				remove(filename);
				return false;
			}
			positions.Push(pos);
		}

		int dirofs = (int)ftell(f);
		for (unsigned i = 0; i < filenames.Size(); i++)
		{
			if (AppendCentralDirectory(f, filenames[i], content[i], mydate, mytime, positions[i]) < 0)
			{
				fclose(f);
				remove(filename);
				return false;
			}
		}

		// Write the directory terminator.
		FZipEndOfCentralDirectory dirend;
		dirend.Magic = ZIP_ENDOFDIR;
		dirend.DiskNumber = 0;
		dirend.FirstDisk = 0;
		dirend.NumEntriesOnAllDisks = dirend.NumEntries = LittleShort(filenames.Size());
		dirend.DirectoryOffset = LittleLong(dirofs);
		dirend.DirectorySize = LittleLong(ftell(f) - dirofs);
		dirend.ZipCommentLength = 0;
		if (fwrite(&dirend, sizeof(dirend), 1, f) != 1)
		{
			fclose(f);
			remove(filename);
			return false;
		}
		fclose(f);
		return true;
	}
	return false;
}