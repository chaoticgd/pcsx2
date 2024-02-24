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

struct SymbolInfo
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
	// table import thread. TryRead and TryReadWrite calls will fail and return
	// false until the lock is released when the function returns.
	void AsyncReadWrite(ReadWriteCallback callback) noexcept;

	bool IsBusy() const;

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

	// Copy commonly used attributes of a symbol into a temporary object.
	SymbolInfo SymbolStartingAtAddress(
		u32 address, SymbolDatabaseAccessMode mode, u32 descriptors = ccc::ALL_SYMBOL_TYPES) const;
	SymbolInfo SymbolAfterAddress(
		u32 address, SymbolDatabaseAccessMode mode, u32 descriptors = ccc::ALL_SYMBOL_TYPES) const;
	SymbolInfo SymbolOverlappingAddress(
		u32 address, SymbolDatabaseAccessMode mode, u32 descriptors = ccc::ALL_SYMBOL_TYPES) const;
	SymbolInfo SymbolWithName(
		const std::string& name, SymbolDatabaseAccessMode mode, u32 descriptors = ccc::ALL_SYMBOL_TYPES) const;
	
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
