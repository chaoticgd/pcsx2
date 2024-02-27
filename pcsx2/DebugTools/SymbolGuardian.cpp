// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SymbolGuardian.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/StringUtil.h"

#include "demangle.h"
#include "ccc/ast.h"
#include "ccc/elf.h"
#include "ccc/importer_flags.h"
#include "ccc/symbol_file.h"
#include "DebugInterface.h"

SymbolGuardian R5900SymbolGuardian;
SymbolGuardian R3000SymbolGuardian;

static void CreateDefaultBuiltInDataTypes(ccc::SymbolDatabase& database);
static void CreateBuiltInDataType(
	ccc::SymbolDatabase& database, ccc::SymbolSourceHandle source, const char* name, ccc::ast::BuiltInClass bclass);
static ccc::ModuleHandle ImportSymbolTables(
	ccc::SymbolDatabase& database, const ccc::SymbolFile& symbol_file, const std::atomic_bool* interrupt);
static void ComputeOriginalFunctionHashes(
	ccc::SymbolDatabase& database, const ccc::ElfFile& elf, ccc::ModuleHandle module);

static void error_callback(const ccc::Error& error, ccc::ErrorLevel level)
{
	switch (level)
	{
		case ccc::ERROR_LEVEL_ERROR:
			Console.Error("Error while importing symbol table: %s", error.message.c_str());
			break;
		case ccc::ERROR_LEVEL_WARNING:
			Console.Warning("Warning while importing symbol table: %s", error.message.c_str());
			break;
	}
}

SymbolGuardian::SymbolGuardian()
{
	ccc::set_custom_error_callback(error_callback);

	m_import_thread = std::thread([this]() {
		while (!m_shutdown_import_thread)
		{
			ReadWriteCallback callback;
			m_work_queue_lock.lock();
			if (!m_work_queue.empty() && !m_interrupt_import_thread)
			{
				ReadWriteCallback& front = m_work_queue.front();
				callback = std::move(front);
				m_work_queue.pop();
			}
			m_work_queue_lock.unlock();

			if (callback)
			{
				m_busy = true;
				m_big_symbol_lock.lock();
				callback(m_database, m_interrupt_import_thread);
				m_big_symbol_lock.unlock();
				m_busy = false;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	});
}

SymbolGuardian::~SymbolGuardian()
{
	m_shutdown_import_thread = true;
	m_interrupt_import_thread = true;
	if (m_import_thread.joinable())
		m_import_thread.join();
}

bool SymbolGuardian::TryRead(ReadCallback callback) const noexcept
{
	return Read(SDA_TRY, std::move(callback));
}

void SymbolGuardian::BlockingRead(ReadCallback callback) const noexcept
{
	Read(SDA_BLOCK, std::move(callback));
}

bool SymbolGuardian::Read(SymbolDatabaseAccessMode mode, ReadCallback callback) const noexcept
{
	pxAssertRel(mode != SDA_ASYNC, "SDA_ASYNC not supported for read operations.");

	if (mode == SDA_TRY && m_busy)
		return false;

	m_big_symbol_lock.lock_shared();
	callback(m_database);
	m_big_symbol_lock.unlock_shared();

	return true;
}

bool SymbolGuardian::TryReadWrite(SynchronousReadWriteCallback callback) noexcept
{
	return ReadWrite(SDA_TRY, [callback](ccc::SymbolDatabase& database, const std::atomic_bool&) {
		return callback(database);
	});
}

void SymbolGuardian::BlockingReadWrite(std::function<void(ccc::SymbolDatabase&)> callback) noexcept
{
	ReadWrite(SDA_BLOCK, [callback](ccc::SymbolDatabase& database, const std::atomic_bool&) {
		callback(database);
	});
}

void SymbolGuardian::AsyncReadWrite(ReadWriteCallback callback) noexcept
{
	ReadWrite(SDA_ASYNC, callback);
}

bool SymbolGuardian::ReadWrite(SymbolDatabaseAccessMode mode, ReadWriteCallback callback) noexcept
{
	if (mode == SDA_ASYNC)
	{
		m_work_queue_lock.lock();
		m_work_queue.emplace(std::move(callback));
		m_work_queue_lock.unlock();
		return true;
	}

	if (mode == SDA_TRY && m_busy)
		return false;

	m_big_symbol_lock.lock();
	callback(m_database, m_interrupt_import_thread);
	m_big_symbol_lock.unlock();

	return true;
}

bool SymbolGuardian::IsBusy() const
{
	return m_busy;
}

void SymbolGuardian::Reset()
{
	// Since the clear command is going to delete everything in the database, we
	// can discard any pending async read/write operations.
	m_work_queue_lock.lock();
	m_work_queue = std::queue<ReadWriteCallback>();
	m_work_queue_lock.unlock();

	m_interrupt_import_thread = true;

	BlockingReadWrite([&](ccc::SymbolDatabase& database) {
		database.clear();
		m_interrupt_import_thread = false;

		CreateDefaultBuiltInDataTypes(database);
	});
}

static void CreateDefaultBuiltInDataTypes(ccc::SymbolDatabase& database)
{
	ccc::Result<ccc::SymbolSourceHandle> source = database.get_symbol_source("Built-in");
	if (!source.success())
		return;

	// Create some built-in data type symbols so that users still have some
	// types to use even if there isn't a symbol table loaded. Maybe in the
	// future we could add PS2-specific types like DMA tags here too.
	CreateBuiltInDataType(database, *source, "char", ccc::ast::BuiltInClass::UNQUALIFIED_8);
	CreateBuiltInDataType(database, *source, "signed char", ccc::ast::BuiltInClass::SIGNED_8);
	CreateBuiltInDataType(database, *source, "unsigned char", ccc::ast::BuiltInClass::UNSIGNED_8);
	CreateBuiltInDataType(database, *source, "short", ccc::ast::BuiltInClass::SIGNED_16);
	CreateBuiltInDataType(database, *source, "unsigned short", ccc::ast::BuiltInClass::UNSIGNED_16);
	CreateBuiltInDataType(database, *source, "int", ccc::ast::BuiltInClass::SIGNED_32);
	CreateBuiltInDataType(database, *source, "unsigned int", ccc::ast::BuiltInClass::UNSIGNED_32);
	CreateBuiltInDataType(database, *source, "unsigned", ccc::ast::BuiltInClass::UNSIGNED_32);
	CreateBuiltInDataType(database, *source, "long", ccc::ast::BuiltInClass::SIGNED_64);
	CreateBuiltInDataType(database, *source, "unsigned long", ccc::ast::BuiltInClass::UNSIGNED_64);
	CreateBuiltInDataType(database, *source, "long long", ccc::ast::BuiltInClass::SIGNED_64);
	CreateBuiltInDataType(database, *source, "unsigned long long", ccc::ast::BuiltInClass::UNSIGNED_64);
	CreateBuiltInDataType(database, *source, "float", ccc::ast::BuiltInClass::FLOAT_32);
	CreateBuiltInDataType(database, *source, "double", ccc::ast::BuiltInClass::FLOAT_64);
	CreateBuiltInDataType(database, *source, "void", ccc::ast::BuiltInClass::VOID_TYPE);
	CreateBuiltInDataType(database, *source, "s8", ccc::ast::BuiltInClass::SIGNED_8);
	CreateBuiltInDataType(database, *source, "u8", ccc::ast::BuiltInClass::UNSIGNED_8);
	CreateBuiltInDataType(database, *source, "s16", ccc::ast::BuiltInClass::SIGNED_16);
	CreateBuiltInDataType(database, *source, "u16", ccc::ast::BuiltInClass::UNSIGNED_16);
	CreateBuiltInDataType(database, *source, "s32", ccc::ast::BuiltInClass::SIGNED_32);
	CreateBuiltInDataType(database, *source, "u32", ccc::ast::BuiltInClass::UNSIGNED_32);
	CreateBuiltInDataType(database, *source, "s64", ccc::ast::BuiltInClass::SIGNED_64);
	CreateBuiltInDataType(database, *source, "u64", ccc::ast::BuiltInClass::UNSIGNED_64);
	CreateBuiltInDataType(database, *source, "s128", ccc::ast::BuiltInClass::SIGNED_128);
	CreateBuiltInDataType(database, *source, "u128", ccc::ast::BuiltInClass::UNSIGNED_128);
	CreateBuiltInDataType(database, *source, "f32", ccc::ast::BuiltInClass::FLOAT_32);
	CreateBuiltInDataType(database, *source, "f64", ccc::ast::BuiltInClass::FLOAT_64);
}

static void CreateBuiltInDataType(
	ccc::SymbolDatabase& database, ccc::SymbolSourceHandle source, const char* name, ccc::ast::BuiltInClass bclass)
{
	ccc::Result<ccc::DataType*> symbol = database.data_types.create_symbol(name, source, nullptr);
	if (!symbol.success())
		return;

	std::unique_ptr<ccc::ast::BuiltIn> type = std::make_unique<ccc::ast::BuiltIn>();
	type->computed_size_bytes = ccc::ast::builtin_class_size(bclass);
	type->bclass = bclass;
	(*symbol)->set_type(std::move(type));
}

void SymbolGuardian::ImportElf(std::vector<u8> elf, std::string elf_file_name)
{
	ccc::Result<ccc::ElfFile> parsed_elf = ccc::ElfFile::parse(std::move(elf));
	if (!parsed_elf.success())
	{
		ccc::report_error(parsed_elf.error());
		return;
	}

	std::unique_ptr<ccc::ElfSymbolFile> symbol_file =
		std::make_unique<ccc::ElfSymbolFile>(std::move(*parsed_elf), std::move(elf_file_name));

	AsyncReadWrite([file_pointer = symbol_file.release()](ccc::SymbolDatabase& database, const std::atomic_bool& interrupt) {
		std::unique_ptr<ccc::ElfSymbolFile> symbol_file(file_pointer);

		ccc::ModuleHandle module_handle = ImportSymbolTables(database, *symbol_file, &interrupt);

		if (module_handle.valid())
			ComputeOriginalFunctionHashes(database, symbol_file->elf(), module_handle);
	});
}

ccc::ModuleHandle SymbolGuardian::ImportSymbolTables(
	ccc::SymbolDatabase& database, const ccc::SymbolFile& symbol_file, const std::atomic_bool* interrupt)
{
	ccc::Result<std::vector<std::unique_ptr<ccc::SymbolTable>>> symbol_tables = symbol_file.get_all_symbol_tables();
	if (!symbol_tables.success())
	{
		ccc::report_error(symbol_tables.error());
		return ccc::ModuleHandle();
	}

	ccc::DemanglerFunctions demangler;
	demangler.cplus_demangle = cplus_demangle;
	demangler.cplus_demangle_opname = cplus_demangle_opname;

	u32 importer_flags = ccc::DEMANGLE_PARAMETERS | ccc::DEMANGLE_RETURN_TYPE;

	ccc::Result<ccc::ModuleHandle> module_handle = ccc::import_symbol_tables(
		database, symbol_file.name(), *symbol_tables, importer_flags, demangler, interrupt);
	if (!module_handle.success())
	{
		ccc::report_error(module_handle.error());
		return ccc::ModuleHandle();
	}

	Console.WriteLn("Imported %d symbols.", database.symbol_count());

	return *module_handle;
}

bool SymbolGuardian::ImportNocashSymbols(ccc::SymbolDatabase& database, const std::string& file_name)
{
	ccc::Result<ccc::SymbolSourceHandle> source = database.get_symbol_source("Nocash Symbols");
	if (!source.success())
		return false;

	FILE* f = FileSystem::OpenCFile(file_name.c_str(), "r");
	if (!f)
		return false;

	while (!feof(f))
	{
		char line[256], value[256] = {0};
		char* p = fgets(line, 256, f);
		if (p == NULL)
			break;

		u32 address;
		if (sscanf(line, "%08x %255s", &address, value) != 2)
			continue;
		if (address == 0 && strcmp(value, "0") == 0)
			continue;

		if (value[0] == '.')
		{
			// data directives
			char* s = strchr(value, ':');
			if (s != NULL)
			{
				*s = 0;

				u32 size = 0;
				if (sscanf(s + 1, "%04x", &size) != 1)
					continue;

				std::unique_ptr<ccc::ast::BuiltIn> scalar_type = std::make_unique<ccc::ast::BuiltIn>();
				if (StringUtil::Strcasecmp(value, ".byt") == 0)
				{
					scalar_type->computed_size_bytes = 1;
					scalar_type->bclass = ccc::ast::BuiltInClass::UNSIGNED_8;
				}
				else if (StringUtil::Strcasecmp(value, ".wrd") == 0)
				{
					scalar_type->computed_size_bytes = 2;
					scalar_type->bclass = ccc::ast::BuiltInClass::UNSIGNED_16;
				}
				else if (StringUtil::Strcasecmp(value, ".dbl") == 0)
				{
					scalar_type->computed_size_bytes = 4;
					scalar_type->bclass = ccc::ast::BuiltInClass::UNSIGNED_32;
				}
				else if (StringUtil::Strcasecmp(value, ".asc") == 0)
				{
					scalar_type->computed_size_bytes = 1;
					scalar_type->bclass = ccc::ast::BuiltInClass::UNQUALIFIED_8;
				}
				else
				{
					continue;
				}

				ccc::Result<ccc::GlobalVariable*> global_variable = database.global_variables.create_symbol(
					line, address, *source, nullptr);
				if (!global_variable.success())
				{
					fclose(f);
					return false;
				}

				if (scalar_type->computed_size_bytes == (s32)size)
				{
					(*global_variable)->set_type(std::move(scalar_type));
				}
				else
				{
					std::unique_ptr<ccc::ast::Array> array = std::make_unique<ccc::ast::Array>();
					array->computed_size_bytes = (s32)size;
					array->element_type = std::move(scalar_type);
					array->element_count = size / array->element_type->computed_size_bytes;
					(*global_variable)->set_type(std::move(array));
				}
			}
		}
		else
		{ // labels
			u32 size = 1;
			char* seperator = strchr(value, ',');
			if (seperator != NULL)
			{
				*seperator = 0;
				sscanf(seperator + 1, "%08x", &size);
			}

			if (size != 1)
			{
				ccc::Result<ccc::Function*> function = database.functions.create_symbol(value, address, *source, nullptr);
				if (!function.success())
				{
					fclose(f);
					return false;
				}

				(*function)->set_size(size);
			}
			else
			{
				ccc::Result<ccc::Label*> label = database.labels.create_symbol(value, address, *source, nullptr);
				if (!label.success())
				{
					fclose(f);
					return false;
				}
			}
		}
	}

	fclose(f);
	return true;
}

static void ComputeOriginalFunctionHashes(ccc::SymbolDatabase& database, const ccc::ElfFile& elf, ccc::ModuleHandle module)
{
	for (ccc::Function& function : database.functions)
	{

		if (function.module_handle() != module)
			continue;

		if (!function.address().valid())
			continue;

		if (function.size() == 0)
			continue;

		ccc::Result<std::span<const u32>> text = elf.get_array_virtual<u32>(
			function.address().value, function.size() / 4);
		if (!text.success())
		{
			ccc::report_warning(text.error());
			break;
		}

		ccc::FunctionHash hash;
		for (u32 instruction : *text)
			hash.update(instruction);

		function.set_original_hash(hash.get());
	}
}

void SymbolGuardian::UpdateFunctionHashes(DebugInterface& cpu)
{
	TryReadWrite([&](ccc::SymbolDatabase& database) {
		for (ccc::Function& function : database.functions)
		{
			if (!function.address().valid())
				continue;

			if (function.size() == 0)
				continue;

			ccc::FunctionHash hash;
			for (u32 i = 0; i < function.size() / 4; i++)
				hash.update(cpu.read32(function.address().value + i * 4));

			function.set_current_hash(hash);
		}

		for (ccc::SourceFile& source_file : database.source_files)
			source_file.check_functions_match(database);
	});
}

void SymbolGuardian::ClearIrxModules()
{
	BlockingReadWrite([&](ccc::SymbolDatabase& database) {
		std::vector<ccc::ModuleHandle> irx_modules;
		for (const ccc::Module& module : m_database.modules)
			if (module.is_irx)
				irx_modules.emplace_back(module.handle());

		for (ccc::ModuleHandle module : irx_modules)
			m_database.destroy_symbols_from_modules(module);
	});
}

SymbolInfo SymbolGuardian::SymbolStartingAtAddress(
	u32 address, SymbolDatabaseAccessMode mode, u32 descriptors) const
{
	SymbolInfo info;
	Read(mode, [&](const ccc::SymbolDatabase& database) {
		ccc::SymbolDescriptor descriptor;
		const ccc::Symbol* symbol = database.symbol_starting_at_address(address, descriptors, &descriptor);
		if (!symbol)
			return;

		info.descriptor = descriptor;
		info.handle = symbol->raw_handle();
		info.address = symbol->address();
		info.size = symbol->size();
	});
	return info;
}

SymbolInfo SymbolGuardian::SymbolAfterAddress(
	u32 address, SymbolDatabaseAccessMode mode, u32 descriptors) const
{
	SymbolInfo info;
	Read(mode, [&](const ccc::SymbolDatabase& database) {
		ccc::SymbolDescriptor descriptor;
		const ccc::Symbol* symbol = database.symbol_after_address(address, descriptors, &descriptor);
		if (!symbol)
			return;

		info.descriptor = descriptor;
		info.handle = symbol->raw_handle();
		info.address = symbol->address();
		info.size = symbol->size();
	});
	return info;
}

SymbolInfo SymbolGuardian::SymbolOverlappingAddress(
	u32 address, SymbolDatabaseAccessMode mode, u32 descriptors) const
{
	SymbolInfo info;
	Read(mode, [&](const ccc::SymbolDatabase& database) {
		ccc::SymbolDescriptor descriptor;
		const ccc::Symbol* symbol = database.symbol_overlapping_address(address, descriptors, &descriptor);
		if (!symbol)
			return;

		info.descriptor = descriptor;
		info.handle = symbol->raw_handle();
		info.address = symbol->address();
		info.size = symbol->size();
	});
	return info;
}

SymbolInfo SymbolGuardian::SymbolWithName(
	const std::string& name, SymbolDatabaseAccessMode mode, u32 descriptors) const
{
	SymbolInfo info;
	Read(mode, [&](const ccc::SymbolDatabase& database) {
		ccc::SymbolDescriptor descriptor;
		const ccc::Symbol* symbol = database.symbol_with_name(name, descriptors, &descriptor);
		if (!symbol)
			return;

		info.descriptor = descriptor;
		info.handle = symbol->raw_handle();
		info.address = symbol->address();
		info.size = symbol->size();
	});
	return info;
}

bool SymbolGuardian::FunctionExistsWithStartingAddress(u32 address, SymbolDatabaseAccessMode mode) const
{
	bool exists = false;
	Read(mode, [&](const ccc::SymbolDatabase& database) {
		ccc::FunctionHandle handle = database.functions.first_handle_from_starting_address(address);
		exists = handle.valid();
	});
	return exists;
}

bool SymbolGuardian::FunctionExistsThatOverlapsAddress(u32 address, SymbolDatabaseAccessMode mode) const
{
	bool exists = false;
	Read(mode, [&](const ccc::SymbolDatabase& database) {
		const ccc::Function* function = database.functions.symbol_overlapping_address(address);
		exists = function != nullptr;
	});
	return exists;
}

FunctionInfo SymbolGuardian::FunctionStartingAtAddress(u32 address, SymbolDatabaseAccessMode mode) const
{
	FunctionInfo info;
	Read(mode, [&](const ccc::SymbolDatabase& database) {
		ccc::FunctionHandle handle = database.functions.first_handle_from_starting_address(address);
		const ccc::Function* function = database.functions.symbol_from_handle(handle);
		if (!function)
			return;

		info.handle = function->handle();
		info.name = function->name();
		info.address = function->address();
		info.size = function->size();
	});
	return info;
}

FunctionInfo SymbolGuardian::FunctionOverlappingAddress(u32 address, SymbolDatabaseAccessMode mode) const
{
	FunctionInfo info;
	Read(mode, [&](const ccc::SymbolDatabase& database) {
		const ccc::Function* function = database.functions.symbol_overlapping_address(address);
		if (!function)
			return;

		info.handle = function->handle();
		info.name = function->name();
		info.address = function->address();
		info.size = function->size();
	});
	return info;
}
