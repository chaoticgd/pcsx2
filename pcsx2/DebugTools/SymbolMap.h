// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <atomic>
#include <thread>
#include <functional>
#include <shared_mutex>

#include "common/Pcsx2Types.h"
#include "DebugTools/ccc/symbol_database.h"
#include "DebugTools/ccc/symbol_file.h"

struct FunctionStat
{
	ccc::FunctionHandle handle;
	std::string name;
	ccc::Address address;
	u32 size = 0;
};

struct SymbolGuardian
{
public:
	SymbolGuardian();
	SymbolGuardian(const SymbolGuardian& rhs) = delete;
	SymbolGuardian(SymbolGuardian&& rhs) = delete;
	SymbolGuardian& operator=(const SymbolGuardian& rhs) = delete;
	SymbolGuardian& operator=(SymbolGuardian&& rhs) = delete;

	// Take a shared lock on the symbol database and run the callback. If the
	// symbol database is busy, nothing happens and we return false.
	bool Read(std::function<void(const ccc::SymbolDatabase&)> callback) const noexcept;
	
	// Take an exclusive lock on the symbol database. Read calls will block
	// until the lock is released when the function returns.
	void ShortReadWrite(std::function<void(ccc::SymbolDatabase&)> callback) noexcept;
	
	// Take an exclusive lock on the symbol database and set the busy flag while
	// the callback in being run. Read calls will fail and return false.
	void LongReadWrite(std::function<void(ccc::SymbolDatabase&)> callback) noexcept;

	// This should only be called from the CPU thread.
	void ImportElfSymbolTablesAsync(std::vector<u8> elf, std::string file_name);
	
	// This should only be called from the CPU thread.
	void ImportSymbolTablesAsync(std::unique_ptr<ccc::SymbolFile> symbol_file);
	
	// This should only be called from the CPU thread.
	void InterruptImportThread();

	// Delete all symbols, including user-defined symbols. It is important to do
	// it this way rather than assigning a new SymbolDatabase object so we
	// don't get dangling symbol handles.
	void Clear();
	
	// Delete all symbols from modules that have the "is_irx" flag set.
	void ClearIrxModules();
	
	bool FunctionExistsWithStartingAddress(u32 address) const;
	bool FunctionExistsThatOverlapsAddress(u32 address) const;
	
	// Copy commonly used attributes of a function so they can be used by the
	// calling thread without needing to keep the lock held.
	FunctionStat StatFunctionStartingAtAddress(u32 address) const;
	FunctionStat StatFunctionOverlappingAddress(u32 address) const;

	ccc::SymbolSourceHandle GetUserDefinedSymbolSource() const;

protected:
	
	ccc::SymbolDatabase m_database;
	ccc::SymbolSourceHandle m_user_defined;
	ccc::ModuleHandle m_main_elf;
	mutable std::shared_mutex m_big_symbol_lock;
	std::atomic_bool m_busy;
	
	std::thread m_import_thread;
	std::atomic_bool m_interrupt_import_thread;
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

	bool LoadNocashSym(const std::string& filename);

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
