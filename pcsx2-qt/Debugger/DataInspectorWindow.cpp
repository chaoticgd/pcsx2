#include "DataInspectorWindow.h"

#include <QtConcurrent/QtConcurrent>

#include "../QtHost.h"
#include "common/Error.h"
#include "CDVD/CDVD.h"
#include "DebugTools/ccc/importer_flags.h"
#include "DebugTools/ccc/symbol_file.h"
#include "DebugTools/ccc/symbol_table.h"
#include "Debugger/Delegates/DataInspectorValueColumnDelegate.h"
#include "MainWindow.h"

struct MdebugElfFile
{
	std::vector<u8> data;
	std::vector<std::pair<ELF_SHR, std::string>> sections;
	u32 mdebugSectionOffset;
};

static ccc::Result<MdebugElfFile> readMdebugElfFile();
static void reportErrorOnUiThread(const ccc::Error& error);

DataInspectorWindow::DataInspectorWindow(QWidget* parent)
	: QMainWindow(parent)
{
	m_ui.setupUi(this);

	setAttribute(Qt::WA_DeleteOnClose);

	m_ui.statusBar->showMessage("Loading...");

	Host::RunOnCPUThread([]() {
		// Read the contents of the currently loaded ELF file into memory.
		ccc::Result<MdebugElfFile> raw_elf = readMdebugElfFile();
		if (!raw_elf.success())
		{
			reportErrorOnUiThread(raw_elf.error());
			return;
		}

		ccc::Result<ccc::ElfFile> parsed_elf = ccc::parse_elf_file(std::move(raw_elf->data));
		if (!parsed_elf.success())
		{
			reportErrorOnUiThread(parsed_elf.error());
			return;
		}

		std::unique_ptr<ccc::SymbolFile> symbol_file = std::make_unique<ccc::ElfSymbolFile>(std::move(*parsed_elf));

		// Parsing the symbol table can take a while, so we spin off a worker
		// thread to do it on.
		QtConcurrent::run(
			[](ccc::SymbolFile* raw_symbol_file) {
				std::unique_ptr<ccc::SymbolFile> symbol_file(raw_symbol_file);

				ccc::SymbolDatabase database;

				ccc::Result<std::vector<std::unique_ptr<ccc::SymbolTable>>> symbol_tables = symbol_file->get_all_symbol_tables();
				if (!symbol_tables.success())
				{
					reportErrorOnUiThread(symbol_tables.error());
					return;
				}

				// TODO: Add GNU demangler.
				ccc::DemanglerFunctions demangler;

				ccc::Result<ccc::SymbolSourceRange> symbol_sources = ccc::import_symbol_tables(
					database, *symbol_tables, ccc::NO_IMPORTER_FLAGS, demangler);
				if (!symbol_sources.success())
				{
					reportErrorOnUiThread(symbol_sources.error());
					return;
				}

				QtHost::RunOnUIThread(
					[&database]() {
						DataInspectorWindow* window = g_main_window->getDataInspectorWindow();
						if (window)
						{
							window->m_database = std::move(database);
							window->createGUI();
						}
					},
					true);
			},
			symbol_file.release());
	});
}

static ccc::Result<MdebugElfFile> readMdebugElfFile()
{
	std::string elfPath = VMManager::GetELFPath();
	CCC_CHECK(!elfPath.empty(), "No ELF file loaded");

	ElfObject elf;
	Error error;
	bool success = cdvdLoadElf(&elf, elfPath, false, &error);
	CCC_CHECK(success, "Failed to load ELF file %s (%s)", elfPath.c_str(), error.GetDescription().c_str());

	std::vector<std::pair<ELF_SHR, std::string>> elfSections = elf.GetSectionHeaders();

	// Checking the section name is better than checking the section type
	// since both STABS and DWARF symbols have a type of MIPS_DEBUG.
	ELF_SHR* mdebugSectionHeader = nullptr;
	for (auto& [sectionHeader, sectionName] : elfSections)
		if (sectionName == ".mdebug")
			mdebugSectionHeader = &sectionHeader;

	CCC_CHECK(mdebugSectionHeader, "No .mdebug symbol table section present");

	MdebugElfFile result;
	result.data = std::move(elf.GetData());
	result.sections = std::move(elfSections);
	result.mdebugSectionOffset = mdebugSectionHeader->sh_offset;
	return result;
}

void DataInspectorWindow::reportErrorOnUiThread(const ccc::Error& error)
{
	QtHost::RunOnUIThread([error]() {
		DataInspectorWindow* window = g_main_window->getDataInspectorWindow();
		if (window)
		{
			window->m_ui.statusBar->showMessage(QString::fromStdString(error.message));
		}
	});
}

void DataInspectorWindow::createGUI()
{
	bool groupBySection = m_ui.globalsGroupBySection->isChecked();
	bool groupByTranslationUnit = m_ui.globalsGroupByTranslationUnit->isChecked();
	QString filter = m_ui.globalsFilter->text().toLower();

	auto initialGlobalRoot = populateGlobalSections(groupBySection, groupByTranslationUnit, filter);
	m_globalModel = new DataInspectorModel(std::move(initialGlobalRoot), m_database, this);
	m_ui.globalsTreeView->setModel(m_globalModel);

	connect(m_ui.globalsFilter, &QLineEdit::textEdited, this, &DataInspectorWindow::resetGlobals);
	connect(m_ui.globalsGroupBySection, &QCheckBox::toggled, this, &DataInspectorWindow::resetGlobals);
	connect(m_ui.globalsGroupByTranslationUnit, &QCheckBox::toggled, this, &DataInspectorWindow::resetGlobals);

	auto initialStackRoot = populateStack();
	m_stackModel = new DataInspectorModel(std::move(initialStackRoot), m_database, this);
	m_ui.stackTreeView->setModel(m_stackModel);

	connect(m_ui.stackRefreshButton, &QPushButton::pressed, this, &DataInspectorWindow::resetStack);

	for (QTreeView* view : {m_ui.watchTreeView, m_ui.globalsTreeView, m_ui.stackTreeView})
	{
		auto delegate = new DataInspectorValueColumnDelegate(m_database, this);
		view->setItemDelegateForColumn(DataInspectorModel::VALUE, delegate);
		view->setAlternatingRowColors(true);
	}

	s32 symbol_count = 0;
#define CCC_X(SymbolType, symbol_list) symbol_count += m_database.symbol_list.size();
	CCC_FOR_EACH_SYMBOL_TYPE_DO_X
#undef CCC_X

	m_ui.statusBar->showMessage(QString("Loaded %1 symbols").arg(symbol_count));
}

void DataInspectorWindow::resetGlobals()
{
	bool groupBySection = m_ui.globalsGroupBySection->isChecked();
	bool groupByTranslationUnit = m_ui.globalsGroupByTranslationUnit->isChecked();
	QString filter = m_ui.globalsFilter->text().toLower();

	m_globalModel->reset(populateGlobalSections(groupBySection, groupByTranslationUnit, filter));
}

std::unique_ptr<DataInspectorNode> DataInspectorWindow::populateGlobalSections(
	bool groupBySection, bool groupByTranslationUnit, const QString& filter)
{
	std::unique_ptr<DataInspectorNode> root = std::make_unique<DataInspectorNode>();
	root->childrenFetched = true;

	if (groupBySection)
	{
		for (ccc::Section& section : m_database.sections)
		{
			if (section.address().valid())
			{
				u32 minAddress = section.address().value;
				u32 maxAddress = section.address().value + section.size();
				auto sectionChildren = populateGlobalTranslationUnits(minAddress, maxAddress, groupByTranslationUnit, filter);
				if (!sectionChildren.empty())
				{
					std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
					node->name = QString::fromStdString(section.name());
					node->children = std::move(sectionChildren);

					for (std::unique_ptr<DataInspectorNode>& child : node->children)
						child->parent = node.get();

					root->children.emplace_back(std::move(node));
				}
			}
		}
	}
	else
	{
		auto rootChildren = populateGlobalTranslationUnits(0, UINT32_MAX, groupByTranslationUnit, filter);
		root->children.insert(root->children.end(),
			std::make_move_iterator(rootChildren.begin()),
			std::make_move_iterator(rootChildren.end()));
	}

	for (std::unique_ptr<DataInspectorNode>& child : root->children)
		child->parent = root.get();

	return root;
}

std::vector<std::unique_ptr<DataInspectorNode>> DataInspectorWindow::populateGlobalTranslationUnits(
	u32 minAddress, u32 maxAddress, bool group_by_source_file, const QString& filter)
{
	std::vector<std::unique_ptr<DataInspectorNode>> children;

	if (group_by_source_file)
	{
		for (const ccc::SourceFile& source_file : m_database.source_files)
		{
			std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
			if (!source_file.command_line_path.empty())
				node->name = QString::fromStdString(source_file.command_line_path);
			else
				node->name = QString::fromStdString(source_file.name());
			node->type = source_file.type();
			node->children = populateGlobalVariables(source_file, minAddress, maxAddress, filter);

			if (!node->children.empty())
			{
				for (std::unique_ptr<DataInspectorNode>& child : node->children)
					child->parent = node.get();
				children.emplace_back(std::move(node));
			}
		}
	}
	else
	{
		for (const ccc::SourceFile& source_file : m_database.source_files)
		{
			std::vector<std::unique_ptr<DataInspectorNode>> variables = populateGlobalVariables(source_file, minAddress, maxAddress, filter);
			children.insert(children.end(),
				std::make_move_iterator(variables.begin()),
				std::make_move_iterator(variables.end()));
		}
	}

	return children;
}

std::vector<std::unique_ptr<DataInspectorNode>> DataInspectorWindow::populateGlobalVariables(
	const ccc::SourceFile& source_file, u32 minAddress, u32 maxAddress, const QString& filter)
{
	std::vector<std::unique_ptr<DataInspectorNode>> variables;

	for (const ccc::GlobalVariable& global_variable : m_database.global_variables.span(source_file.globals_variables()))
	{
		if (global_variable.address().valid())
		{
			std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
			node->name = QString::fromStdString(global_variable.name());
			node->type = global_variable.type();
			node->location.type = DataInspectorLocation::EE_MEMORY;
			node->location.address = global_variable.address().value;
			bool addressInRange = global_variable.address().value >= minAddress && global_variable.address().value < maxAddress;
			bool containsFilterString = filter.isEmpty() || node->name.toLower().contains(filter);
			if (addressInRange && containsFilterString)
			{
				variables.emplace_back(std::move(node));
			}
		}
	}

	return variables;
}

void DataInspectorWindow::resetStack()
{
	m_stackModel->reset(populateStack());
}

std::unique_ptr<DataInspectorNode> DataInspectorWindow::populateStack()
{
	std::unique_ptr<DataInspectorNode> root = std::make_unique<DataInspectorNode>();
	root->childrenFetched = true;

	DebugInterface& cpu = r5900Debug;
	u32 ra = cpu.getRegister(0, 31);
	u32 sp = cpu.getRegister(0, 29);

	std::vector<StackFrame> stackFrames;
	for (const auto& thread : cpu.GetThreadList())
	{
		if (thread->Status() == ThreadStatus::THS_RUN)
		{
			stackFrames = MipsStackWalk::Walk(&cpu, cpu.getPC(), ra, sp, thread->EntryPoint(), thread->StackTop());
			break;
		}
	}

	for (StackFrame& frame : stackFrames)
	{
		std::unique_ptr<DataInspectorNode> functionNode = std::make_unique<DataInspectorNode>();

		ccc::FunctionHandle handle = m_database.functions.first_handle_from_starting_address(frame.entry);
		const ccc::Function* function = m_database.functions.symbol_from_handle(handle);
		if (function)
		{
			functionNode->name = QString::fromStdString(function->name());
			for (const ccc::LocalVariable& local_variable : m_database.local_variables.optional_span(function->local_variables()))
			{
				const ccc::RegisterStorage* register_storage = std::get_if<ccc::RegisterStorage>(&local_variable.storage);
				if (register_storage && frame.sp == sp)
				{
					
					std::unique_ptr<DataInspectorNode> localNode = std::make_unique<DataInspectorNode>();
					localNode->name = QString::fromStdString(local_variable.name());
					localNode->type = local_variable.type();
					localNode->location.type = DataInspectorLocation::EE_REGISTER;
					localNode->location.address = register_storage->dbx_register_number;
					functionNode->children.emplace_back(std::move(localNode));
				}
				else if (std::holds_alternative<ccc::StackStorage>(local_variable.storage))
				{
					std::unique_ptr<DataInspectorNode> localNode = std::make_unique<DataInspectorNode>();
					localNode->name = QString::fromStdString(local_variable.name());
					localNode->type = local_variable.type();
					localNode->location.type = DataInspectorLocation::EE_MEMORY;
					localNode->location.address = frame.sp;
					functionNode->children.emplace_back(std::move(localNode));
				}
			}
		}
		else
		{
			functionNode->name = QString::number(frame.entry, 16);
		}

		for (std::unique_ptr<DataInspectorNode>& child : functionNode->children)
			child->parent = functionNode.get();

		root->children.emplace_back(std::move(functionNode));
	}

	for (std::unique_ptr<DataInspectorNode>& child : root->children)
		child->parent = root.get();

	return root;
}
