// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Error.h"

#include "zip.h"

#include <optional>
#include <vector>

class ZipFile;
class ZipSource;

/// An index of a file in the central directory of a ZIP archive.
using ZipFileIndex = u64;

/// A ZIP archive. Thin RAII wrapper around a zip_t object from libzip.
class ZipArchive
{
	friend ZipFile;

public:
	ZipArchive();
	~ZipArchive();

	/// Don't allow copying.
	ZipArchive(const ZipArchive& rhs) = delete;
	ZipArchive& operator=(const ZipArchive& rhs) = delete;

	/// Allow moving.
	ZipArchive(ZipArchive&& rhs);
	ZipArchive& operator=(ZipArchive&& rhs);

	/// Open the archive with zip_open.
	bool Open(const char* path, int flags, Error* error);

	/// Write changes made to the archive back to the file and close it.
	bool WriteChangesAndClose(Error* error);

	/// Discard changes made to the archive and close it with zip_discard.
	void DiscardChangesAndClose();

	/// Check if the archive is open.
	bool IsValid();

	/// Find a file inside the archive given its name using zip_name_locate.
	std::optional<ZipFileIndex> LocateFile(const char* name, zip_flags_t flags, Error* error);

	/// Retrieve file metadata using zip_stat_index. Ownership of the name
	/// string is NOT transferred to the caller.
	std::optional<zip_stat_t> StatFile(ZipFileIndex index, zip_flags_t flags, Error* error);

	/// Read the entire contents of a file into a std::string.
	std::optional<std::string> ReadTextFile(ZipFileIndex index, zip_flags_t flags, Error* error);

	/// Read the entire contents of a file into a std::vector<u8>.
	std::optional<std::vector<u8>> ReadBinaryFile(ZipFileIndex index, zip_flags_t flags, Error* error);

	/// Add a file to the zip using zip_source_buffer_create and zip_file_add.
	std::optional<ZipFileIndex> AddFile(
		const char* name, const void* data, u64 length, zip_flags_t flags, int freep, Error* error);

	/// Add a file to the zip using zip_file_add.
	/// On success, ownership of the underlying source will be transferred to
	/// the file and the source object will be made empty.
	std::optional<ZipFileIndex> AddFileFromSource(
		const char* name, ZipSource& source, zip_flags_t flags, Error* error);

	/// Set the compression method for a file using zip_set_file_compression.
	bool SetFileCompression(ZipFileIndex index, s32 comp, u32 comp_flags, Error* error);

private:
	zip_t* m_zip = nullptr;
};

/// An individual file within a ZIP archive.
/// Thin RAII wrapper around a zip_file_t object from libzip.
class ZipFile
{
public:
	ZipFile();
	~ZipFile();

	/// Don't allow copying.
	ZipFile(const ZipFile& rhs) = delete;
	ZipFile& operator=(const ZipFile& rhs) = delete;

	/// Allow moving.
	ZipFile(ZipFile&& rhs);
	ZipFile& operator=(ZipFile&& rhs);

	/// Open the file from the given archive using zip_fopen_index.
	bool Open(ZipArchive& archive, ZipFileIndex index, zip_flags_t flags, Error* error);

	/// Close the file using zip_fclose.
	void Close();

	/// Check if the file is open.
	bool IsValid();

	/// Read nbytes bytes into buffer using zip_fread.
	bool Read(void* buffer, u64 nbytes, Error* error);

	/// Set the position indicator using zip_fseek.
	bool Seek(s64 offset, int whence, Error* error);

	/// Retrieve the position indicator using zip_ftell.
	std::optional<u64> Tell(Error* error);

	/// Retrieve the uncompressed size of the file.
	std::optional<u64> Size(Error* error);

	/// Read the entire file into a std::string.
	std::optional<std::string> ReadText(Error* error);

	/// Read the entire file into a std::vector.
	std::optional<std::vector<u8>> ReadBinary(Error* error);

	/// Read sizeof(Value) bytes into a variable of type Value and return it.
	template <typename Value>
	std::optional<Value> ReadValue(Error* error)
	{
		Value value;

		if (!Read(&value, sizeof(Value), error))
			return std::nullopt;

		return value;
	}

private:
	ZipArchive* m_archive = nullptr;
	u64 m_index = 0;
	zip_file_t* m_file = nullptr;
};

/// Data source for the contents of a file in a ZIP archive.
/// Thin RAII wrapper around a zip_source_t object from libzip.
/// Users of this class shouldn't have to worry about zip_source_t being
/// reference counted.
class ZipSource
{
	friend ZipArchive;

public:
	ZipSource();
	~ZipSource();

	/// Don't allow copying.
	ZipSource(const ZipSource& rhs) = delete;
	ZipSource& operator=(const ZipSource& rhs) = delete;

	/// Allow moving.
	ZipSource(ZipSource&& rhs);
	ZipSource& operator=(ZipSource&& rhs);

	/// Create an in-memory source using zip_source_buffer_create.
	bool CreateBuffer(const void* data, u64 length, int freep, Error* error);

	/// Free the source using zip_source_free.
	void Free();

	/// Check if the underlying source exists.
	bool IsValid();

	/// Start writing data to the source using zip_source_begin_write.
	bool BeginWrite(Error* error);

	/// Write data to the source using zip_source_write.
	/// The source takes a copy of the data.
	bool Write(const void* data, u64 length, Error* error);

	/// Stop writing data to the source using zip_source_commit_write.
	bool CommitWrite(Error* error);

private:
	zip_source_t* m_source = nullptr;
};
