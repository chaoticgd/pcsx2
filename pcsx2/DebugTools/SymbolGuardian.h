// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <queue>
#include <atomic>
#include <thread>
#include <functional>
#include <shared_mutex>

#include "common/Pcsx2Types.h"
#include "DebugTools/ccc/symbol_database.h"
#include "DebugTools/ccc/symbol_file.h"

class DebugInterface;

// TODO: When the symbol map is gone, rename this back to SymbolInfo.
struct SymbolInfo2
{
	std::optional<ccc::SymbolDescriptor> descriptor;
	u32 handle = (u32)-1;
	std::string name;
	ccc::Address address;
	u32 size = 0;
};

struct FunctionInfo
{
	ccc::FunctionHandle handle;
	std::string name;
	ccc::Address address;
	u32 size = 0;
};

enum SymbolDatabaseAccessMode
{
	SDA_TRY, // If the symbol database is busy, do nothing and return.
	SDA_BLOCK, // If the symbol database is busy, block until it's available.
	SDA_ASYNC // Submit the callback to be run on the work thread and return immediately.
};

struct SymbolGuardian
{
public:
	SymbolGuardian();
	SymbolGuardian(const SymbolGuardian& rhs) = delete;
	SymbolGuardian(SymbolGuardian&& rhs) = delete;
	~SymbolGuardian();
	SymbolGuardian& operator=(const SymbolGuardian& rhs) = delete;
	SymbolGuardian& operator=(SymbolGuardian&& rhs) = delete;

	using ReadCallback = std::function<void(const ccc::SymbolDatabase&)>;
	using ReadWriteCallback = std::function<void(ccc::SymbolDatabase&, const std::atomic_bool& interrupt)>;
	using SynchronousReadWriteCallback = std::function<void(ccc::SymbolDatabase&)>;

	// Take a shared lock on the symbol database and run the callback. If the
	// symbol database is busy, nothing happens and we return false.
	bool TryRead(ReadCallback callback) const noexcept;

	// Take a shared lock on the symbol database and run the callback. If the
	// symbol database is busy, we block until it's available.
	void BlockingRead(ReadCallback callback) const noexcept;

	// Take an exclusive lock on the symbol database. If the symbol database is
	// busy, nothing happens and we return false. TryRead and TryReadWrite calls
	// will block until the lock is released when the function returns.
	bool TryReadWrite(SynchronousReadWriteCallback callback) noexcept;

	// Take an exclusive lock on the symbol database. If the symbol table is
	// busy, we block until it's available. TryRead and TryReadWrite calls will
	// block until the lock is released when the function returns.
	void BlockingReadWrite(SynchronousReadWriteCallback callback) noexcept;

	// Push the callback onto a work queue so it can be run from the symbol
	// table import thread. While the callback is running, TryRead and
	// TryReadWrite calls will fail and return false.
	void AsyncReadWrite(ReadWriteCallback callback) noexcept;

	// Interrupt the import thread, delete all symbols, create built-ins.
	void Reset();

	void ImportElf(std::vector<u8> elf, std::string elf_file_name);

	static ccc::ModuleHandle ImportSymbolTables(
		ccc::SymbolDatabase& database, const ccc::SymbolFile& symbol_file, const std::atomic_bool* interrupt);
	static bool ImportNocashSymbols(ccc::SymbolDatabase& database, const std::string& file_name);

	// Compute new hashes for all the functions to check if any of them have
	// been overwritten.
	void UpdateFunctionHashes(DebugInterface& cpu);

	// Delete all symbols from modules that have the "is_irx" flag set.
	void ClearIrxModules();

	// Copy commonly used attributes of a function, global variable or static
	// local variable symbol into a temporary object.
	SymbolInfo2 SymbolStartingAtAddress(u32 address, SymbolDatabaseAccessMode mode, u32 descriptors = ccc::ALL_SYMBOL_TYPES) const;
	SymbolInfo2 SymbolAfterAddress(u32 address, SymbolDatabaseAccessMode mode, u32 descriptors = ccc::ALL_SYMBOL_TYPES) const;
	SymbolInfo2 SymbolOverlappingAddress(u32 address, SymbolDatabaseAccessMode mode, u32 descriptors = ccc::ALL_SYMBOL_TYPES) const;

	bool FunctionExistsWithStartingAddress(u32 address, SymbolDatabaseAccessMode mode) const;
	bool FunctionExistsThatOverlapsAddress(u32 address, SymbolDatabaseAccessMode mode) const;

	// Copy commonly used attributes of a function so they can be used by the
	// calling thread without needing to keep the lock held.
	FunctionInfo FunctionStartingAtAddress(u32 address, SymbolDatabaseAccessMode mode) const;
	FunctionInfo FunctionOverlappingAddress(u32 address, SymbolDatabaseAccessMode mode) const;

protected:
	bool Read(SymbolDatabaseAccessMode mode, ReadCallback callback) const noexcept;
	bool ReadWrite(SymbolDatabaseAccessMode mode, ReadWriteCallback callback) noexcept;

	ccc::SymbolDatabase m_database;
	mutable std::shared_mutex m_big_symbol_lock;
	std::atomic_bool m_busy;

	std::thread m_import_thread;
	std::atomic_bool m_shutdown_import_thread = false;
	std::atomic_bool m_interrupt_import_thread = false;
	std::queue<ReadWriteCallback> m_work_queue;
	std::mutex m_work_queue_lock;
};

extern SymbolGuardian R5900SymbolGuardian;
extern SymbolGuardian R3000SymbolGuardian;

enum SymbolType
{
	ST_NONE = 0,
	ST_FUNCTION = 1,
	ST_DATA = 2,
	ST_ALL = 3,
};

struct SymbolInfo
{
	SymbolType type;
	u32 address;
	u32 size;
};

struct SymbolEntry
{
	std::string name;
	u32 address;
	u32 size;
};

enum DataType
{
	DATATYPE_NONE,
	DATATYPE_BYTE,
	DATATYPE_HALFWORD,
	DATATYPE_WORD,
	DATATYPE_ASCII
};

class SymbolMap
{
public:
	SymbolMap() {}
	void Clear();
	void SortSymbols();

	SymbolType GetSymbolType(u32 address) const;
	bool GetSymbolInfo(SymbolInfo* info, u32 address, SymbolType symmask = ST_FUNCTION) const;
	u32 GetNextSymbolAddress(u32 address, SymbolType symmask);
	std::string GetDescription(unsigned int address) const;
	std::vector<SymbolEntry> GetAllSymbols(SymbolType symmask) const;

	void AddFunction(const std::string& name, u32 address, u32 size);
	u32 GetFunctionStart(u32 address) const;
	int GetFunctionNum(u32 address) const;
	u32 GetFunctionSize(u32 startAddress) const;
	bool SetFunctionSize(u32 startAddress, u32 newSize);
	bool RemoveFunction(u32 startAddress);

	void AddLabel(const std::string& name, u32 address);
	std::string GetLabelName(u32 address) const;
	void SetLabelName(const std::string& name, u32 address);
	bool GetLabelValue(const std::string& name, u32& dest);

	void AddData(u32 address, u32 size, DataType type, int moduleIndex = -1);
	u32 GetDataStart(u32 address) const;
	u32 GetDataSize(u32 startAddress) const;
	DataType GetDataType(u32 startAddress) const;

	// Module functions for IOP symbols

	static const u32 INVALID_ADDRESS = (u32)-1;

	bool IsEmpty() const { return functions.empty() && labels.empty() && data.empty(); };

private:
	void AssignFunctionIndices();

	struct FunctionEntry
	{
		u32 start;
		u32 size;
		int index;
		std::string name;
	};

	struct LabelEntry
	{
		u32 addr;
		std::string name;
	};

	struct DataEntry
	{
		DataType type;
		u32 start;
		u32 size;
	};

	std::map<u32, FunctionEntry> functions;
	std::map<u32, LabelEntry> labels;
	std::map<u32, DataEntry> data;

	mutable std::recursive_mutex m_lock;
};

extern SymbolMap R5900SymbolMap;
extern SymbolMap R3000SymbolMap;
