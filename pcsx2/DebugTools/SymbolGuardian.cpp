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

static ccc::ModuleHandle ImportSymbolTables(ccc::SymbolDatabase& database, const ccc::SymbolFile& symbol_file, const std::atomic_bool* interrupt);
static void ComputeOriginalFunctionHashes(ccc::SymbolDatabase& database, const ccc::ElfFile& elf, ccc::ModuleHandle module);

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

void SymbolGuardian::ImportElf(std::vector<u8> elf, std::string elf_file_name)
{
	ccc::Result<ccc::ElfFile> parsed_elf = ccc::parse_elf_file(std::move(elf));
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
	// TODO: leaking file handle here
	FILE* f = FileSystem::OpenCFile(file_name.c_str(), "r");
	if (!f)
		return false;

	ccc::Result<ccc::SymbolSourceHandle> source = database.get_symbol_source("Nocash Symbols");
	if (!source.success())
		return false;

	while (!feof(f))
	{
		char line[256], value[256] = {0};
		char* p = fgets(line, 256, f);
		if (p == NULL)
			break;

		u32 address;
		if (sscanf(line, "%08X %s", &address, value) != 2)
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
				if (sscanf(s + 1, "%04X", &size) != 1)
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
					return false;

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
			int size = 1;
			char* seperator = strchr(value, ',');
			if (seperator != NULL)
			{
				*seperator = 0;
				sscanf(seperator + 1, "%08X", &size);
			}

			if (size != 1)
			{
				ccc::Result<ccc::Function*> function = database.functions.create_symbol(value, address, *source, nullptr);
				if (!function.success())
					return false;
			}
			else
			{
				ccc::Result<ccc::Label*> label = database.labels.create_symbol(value, address, *source, nullptr);
				if (!label.success())
					return false;
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

		function.compute_original_hash(*text);
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

			std::vector<u32> text(function.size() / 4);
			for (u32 i = 0; i < function.size() / 4; i++)
				text[i] = cpu.read32(function.address().value + i * 4);

			function.compute_current_hash(text);
		}

		for (ccc::SourceFile& source_file : database.source_files)
			source_file.check_functions_match(database);
	});
}

void SymbolGuardian::Clear()
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
		if (function)
		{
			info.handle = function->handle();
			info.name = function->name();
			info.address = function->address();
			info.size = function->size();
		}
	});
	return info;
}

FunctionInfo SymbolGuardian::FunctionOverlappingAddress(u32 address, SymbolDatabaseAccessMode mode) const
{
	FunctionInfo info;
	Read(mode, [&](const ccc::SymbolDatabase& database) {
		const ccc::Function* function = database.functions.symbol_overlapping_address(address);
		if (function)
		{
			info.handle = function->handle();
			info.name = function->name();
			info.address = function->address();
			info.size = function->size();
		}
	});
	return info;
}

SymbolMap R5900SymbolMap;
SymbolMap R3000SymbolMap;

void SymbolMap::SortSymbols()
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	AssignFunctionIndices();
}

void SymbolMap::Clear()
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	functions.clear();
	labels.clear();
	data.clear();
}

SymbolType SymbolMap::GetSymbolType(u32 address) const
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	if (functions.find(address) != functions.end())
		return ST_FUNCTION;
	if (data.find(address) != data.end())
		return ST_DATA;
	return ST_NONE;
}

bool SymbolMap::GetSymbolInfo(SymbolInfo* info, u32 address, SymbolType symmask) const
{
	u32 functionAddress = INVALID_ADDRESS;
	u32 dataAddress = INVALID_ADDRESS;

	if (symmask & ST_FUNCTION)
		functionAddress = GetFunctionStart(address);

	if (symmask & ST_DATA)
		dataAddress = GetDataStart(address);

	if (functionAddress == INVALID_ADDRESS || dataAddress == INVALID_ADDRESS)
	{
		if (functionAddress != INVALID_ADDRESS)
		{
			if (info != NULL)
			{
				info->type = ST_FUNCTION;
				info->address = functionAddress;
				info->size = GetFunctionSize(functionAddress);
			}

			return true;
		}

		if (dataAddress != INVALID_ADDRESS)
		{
			if (info != NULL)
			{
				info->type = ST_DATA;
				info->address = dataAddress;
				info->size = GetDataSize(dataAddress);
			}

			return true;
		}

		return false;
	}

	// if both exist, return the function
	if (info != NULL)
	{
		info->type = ST_FUNCTION;
		info->address = functionAddress;
		info->size = GetFunctionSize(functionAddress);
	}

	return true;
}

u32 SymbolMap::GetNextSymbolAddress(u32 address, SymbolType symmask)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	const auto functionEntry = symmask & ST_FUNCTION ? functions.upper_bound(address) : functions.end();
	const auto dataEntry = symmask & ST_DATA ? data.upper_bound(address) : data.end();

	if (functionEntry == functions.end() && dataEntry == data.end())
		return INVALID_ADDRESS;

	u32 funcAddress = (functionEntry != functions.end()) ? functionEntry->first : 0xFFFFFFFF;
	u32 dataAddress = (dataEntry != data.end()) ? dataEntry->first : 0xFFFFFFFF;

	if (funcAddress <= dataAddress)
		return funcAddress;
	else
		return dataAddress;
}

std::string SymbolMap::GetDescription(unsigned int address) const
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	std::string labelName;

	u32 funcStart = GetFunctionStart(address);
	if (funcStart != INVALID_ADDRESS)
	{
		labelName = GetLabelName(funcStart);
	}
	else
	{
		u32 dataStart = GetDataStart(address);
		if (dataStart != INVALID_ADDRESS)
			labelName = GetLabelName(dataStart);
	}

	if (!labelName.empty())
		return labelName;

	char descriptionTemp[256];
	std::snprintf(descriptionTemp, std::size(descriptionTemp), "(%08x)", address);
	return descriptionTemp;
}

std::vector<SymbolEntry> SymbolMap::GetAllSymbols(SymbolType symmask) const
{
	std::vector<SymbolEntry> result;

	if (symmask & ST_FUNCTION)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		for (auto it = functions.begin(); it != functions.end(); it++)
		{
			SymbolEntry entry;
			entry.address = it->first;
			entry.size = GetFunctionSize(entry.address);
			entry.name = GetLabelName(entry.address);

			result.push_back(entry);
		}
	}

	if (symmask & ST_DATA)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		for (auto it = data.begin(); it != data.end(); it++)
		{
			SymbolEntry entry;
			entry.address = it->first;
			entry.size = GetDataSize(entry.address);
			entry.name = GetLabelName(entry.address);

			result.push_back(entry);
		}
	}

	return result;
}

void SymbolMap::AddFunction(const std::string& name, u32 address, u32 size)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);

	auto existing = functions.find(address);

	if (existing != functions.end())
	{
		existing->second.size = size;
	}
	else
	{
		FunctionEntry func;
		func.start = address;
		func.size = size;
		func.index = (int)functions.size();
		func.name = name;
		functions[address] = func;

		functions.insert(std::make_pair(address, func));
	}

	AddLabel(name, address);
}

u32 SymbolMap::GetFunctionStart(u32 address) const
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto it = functions.upper_bound(address);
	if (it == functions.end())
	{
		// check last element
		auto rit = functions.rbegin();
		if (rit != functions.rend())
		{
			u32 start = rit->first;
			u32 size = rit->second.size;
			if (start <= address && start + size > address)
				return start;
		}
		// otherwise there's no function that contains this address
		return INVALID_ADDRESS;
	}

	if (it != functions.begin())
	{
		it--;
		u32 start = it->first;
		u32 size = it->second.size;
		if (start <= address && start + size > address)
			return start;
	}

	return INVALID_ADDRESS;
}

u32 SymbolMap::GetFunctionSize(u32 startAddress) const
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto it = functions.find(startAddress);
	if (it == functions.end())
		return INVALID_ADDRESS;

	return it->second.size;
}

int SymbolMap::GetFunctionNum(u32 address) const
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	u32 start = GetFunctionStart(address);
	if (start == INVALID_ADDRESS)
		return INVALID_ADDRESS;

	auto it = functions.find(start);
	if (it == functions.end())
		return INVALID_ADDRESS;

	return it->second.index;
}

void SymbolMap::AssignFunctionIndices()
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	int index = 0;
	auto begin = functions.lower_bound(0);
	auto end = functions.upper_bound(0xFFFFFFFF);
	for (auto it = begin; it != end; ++it)
	{
		it->second.index = index++;
	}
}

bool SymbolMap::SetFunctionSize(u32 startAddress, u32 newSize)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);

	auto funcInfo = functions.find(startAddress);
	if (funcInfo != functions.end())
	{
		funcInfo->second.size = newSize;
	}

	// TODO: check for overlaps
	return true;
}

bool SymbolMap::RemoveFunction(u32 startAddress)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);

	auto it = functions.find(startAddress);
	if (it == functions.end())
		return false;

	functions.erase(it);

	labels.erase(startAddress);

	return true;
}

void SymbolMap::AddLabel(const std::string& name, u32 address)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);

	auto existing = labels.find(address);

	if (existing != labels.end())
	{
		// Adding a function will automatically call this.
		// We don't want to overwrite the name if it's already set because
		// label names are added before our functions
	}
	else
	{
		LabelEntry label;
		label.addr = address;
		label.name = name;

		labels[address] = label;
	}
}

void SymbolMap::SetLabelName(const std::string& name, u32 address)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto labelInfo = labels.find(address);
	if (labelInfo == labels.end())
	{
		AddLabel(name, address);
	}
	else
	{
		labelInfo->second.name = name;
	}
}

std::string SymbolMap::GetLabelName(u32 address) const
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto it = labels.find(address);
	if (it == labels.end())
		return "";

	return it->second.name;
}

bool SymbolMap::GetLabelValue(const std::string& name, u32& dest)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	for (auto it = labels.begin(); it != labels.end(); it++)
	{
		if (name == it->second.name)
		{
			dest = it->first;
			return true;
		}
	}

	return false;
}

void SymbolMap::AddData(u32 address, u32 size, DataType type, int moduleIndex)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);

	auto existing = data.find(address);

	if (existing != data.end())
	{
		existing->second.size = size;
		existing->second.type = type;
	}
	else
	{
		DataEntry entry;
		entry.start = address;
		entry.size = size;
		entry.type = type;

		data[address] = entry;
	}
}

u32 SymbolMap::GetDataStart(u32 address) const
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto it = data.upper_bound(address);
	if (it == data.end())
	{
		// check last element
		auto rit = data.rbegin();

		if (rit != data.rend())
		{
			u32 start = rit->first;
			u32 size = rit->second.size;
			if (start <= address && start + size > address)
				return start;
		}
		// otherwise there's no data that contains this address
		return INVALID_ADDRESS;
	}

	if (it != data.begin())
	{
		it--;
		u32 start = it->first;
		u32 size = it->second.size;
		if (start <= address && start + size > address)
			return start;
	}

	return INVALID_ADDRESS;
}

u32 SymbolMap::GetDataSize(u32 startAddress) const
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto it = data.find(startAddress);
	if (it == data.end())
		return INVALID_ADDRESS;
	return it->second.size;
}

DataType SymbolMap::GetDataType(u32 startAddress) const
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto it = data.find(startAddress);
	if (it == data.end())
		return DATATYPE_NONE;
	return it->second.type;
}
