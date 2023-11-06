#include "DataInspectorWindow.h"

#include <QtConcurrent/QtConcurrent>

#include "../QtHost.h"
#include "common/Error.h"
#include "CDVD/CDVD.h"
#include "DebugTools/ccc/analysis.h"
#include "Debugger/Delegates/DataInspectorValueColumnDelegate.h"
#include "MainWindow.h"

struct MdebugElfFile
{
	std::vector<u8> data;
	std::vector<std::pair<ELF_SHR, std::string>> sections;
	u32 mdebugSectionOffset;
};

static ccc::Result<MdebugElfFile> readMdebugElfFile();
static ccc::Result<ccc::HighSymbolTable> parseMdebugSection(const MdebugElfFile& input);

DataInspectorWindow::DataInspectorWindow(QWidget* parent)
	: QMainWindow(parent)
{
	m_ui.setupUi(this);

	setAttribute(Qt::WA_DeleteOnClose);

	m_ui.statusBar->showMessage("Loading...");

	Host::RunOnCPUThread([]() {
		// Read the contents of the currently loaded ELF file into memory.
		ccc::Result<MdebugElfFile> elfFile = readMdebugElfFile();
		if (!elfFile.success())
		{
			QtHost::RunOnUIThread([error = elfFile.error()]() {
				DataInspectorWindow* window = g_main_window->getDataInspectorWindow();
				if (window)
				{
					window->m_ui.statusBar->showMessage(QString::fromStdString(error.message));
				}
			});
			return;
		}

		// Parsing the symbol table can take a while, so we spin off a worker
		// thread to do it on.
		QtConcurrent::run(
			[](MdebugElfFile elfFile) {
				ccc::Result<ccc::HighSymbolTable> symbolTable = parseMdebugSection(elfFile);
				if (!symbolTable.success())
				{
					QtHost::RunOnUIThread([error = symbolTable.error()]() {
						DataInspectorWindow* window = g_main_window->getDataInspectorWindow();
						if (window)
						{
							window->m_ui.statusBar->showMessage(QString::fromStdString(error.message));
						}
					});
					return;
				}

				QtHost::RunOnUIThread(
					[&symbolTable, &elfFile]() {
						DataInspectorWindow* window = g_main_window->getDataInspectorWindow();
						if (window)
						{
							window->m_symbolTable = std::move(*symbolTable);
							window->m_elfSections = std::move(elfFile.sections);
							window->createGUI();
						}
					},
					true);
			},
			std::move(*elfFile));
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

static ccc::Result<ccc::HighSymbolTable> parseMdebugSection(const MdebugElfFile& input)
{
	ccc::mdebug::SymbolTable symbolTable;
	ccc::Result<void> symbolTableResult = symbolTable.init(input.data, input.mdebugSectionOffset);
	CCC_RETURN_IF_ERROR(symbolTableResult);

	ccc::Result<ccc::HighSymbolTable> highSymbolTable =
		ccc::analyse(symbolTable, ccc::DEDUPLICATE_TYPES);
	CCC_RETURN_IF_ERROR(highSymbolTable);

	return highSymbolTable;
}

void DataInspectorWindow::createGUI()
{
	m_typeNameToDeduplicatedTypeIndex = ccc::build_type_name_to_deduplicated_type_index_map(m_symbolTable);

	bool groupBySection = m_ui.globalsGroupBySection->isChecked();
	bool groupByTranslationUnit = m_ui.globalsGroupByTranslationUnit->isChecked();
	QString filter = m_ui.globalsFilter->text().toLower();

	auto initialGlobalRoot = populateGlobalSections(groupBySection, groupByTranslationUnit, filter);
	m_globalModel = new DataInspectorModel(std::move(initialGlobalRoot), m_symbolTable, m_typeNameToDeduplicatedTypeIndex, this);
	m_ui.globalsTreeView->setModel(m_globalModel);

	connect(m_ui.globalsFilter, &QLineEdit::textEdited, this, &DataInspectorWindow::resetGlobals);
	connect(m_ui.globalsGroupBySection, &QCheckBox::toggled, this, &DataInspectorWindow::resetGlobals);
	connect(m_ui.globalsGroupByTranslationUnit, &QCheckBox::toggled, this, &DataInspectorWindow::resetGlobals);

	for (QTreeView* view : {m_ui.watchTreeView, m_ui.globalsTreeView, m_ui.stackTreeView})
	{
		auto delegate = new DataInspectorValueColumnDelegate(m_symbolTable, m_typeNameToDeduplicatedTypeIndex, this);
		view->setItemDelegateForColumn(DataInspectorModel::VALUE, delegate);
		view->setAlternatingRowColors(true);
	}

	m_ui.statusBar->showMessage(QString("Loaded %1 data types").arg(m_symbolTable.deduplicated_types.size()));
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
		for (auto& [sectionHeader, sectionName] : m_elfSections)
		{
			if (sectionHeader.sh_addr > 0)
			{
				u32 minAddress = sectionHeader.sh_addr;
				u32 maxAddress = sectionHeader.sh_addr + sectionHeader.sh_size;
				auto sectionChildren = populateGlobalTranslationUnits(minAddress, maxAddress, groupByTranslationUnit, filter);
				if (!sectionChildren.empty())
				{
					std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
					node->name = QString::fromStdString(sectionName);
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
	u32 minAddress, u32 maxAddress, bool groupByTranlationUnit, const QString& filter)
{
	std::vector<std::unique_ptr<DataInspectorNode>> children;

	if (groupByTranlationUnit)
	{
		for (const std::unique_ptr<ccc::ast::SourceFile>& sourceFile : m_symbolTable.source_files)
		{
			std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
			if (!sourceFile->relative_path.empty())
				node->name = QString::fromStdString(sourceFile->relative_path);
			else
				node->name = QString::fromStdString(sourceFile->full_path);
			node->type = sourceFile.get();
			node->children = populateGlobalVariables(*sourceFile, minAddress, maxAddress, filter);

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
		for (const std::unique_ptr<ccc::ast::SourceFile>& sourceFile : m_symbolTable.source_files)
		{
			std::vector<std::unique_ptr<DataInspectorNode>> variables = populateGlobalVariables(*sourceFile, minAddress, maxAddress, filter);
			children.insert(children.end(),
				std::make_move_iterator(variables.begin()),
				std::make_move_iterator(variables.end()));
		}
	}

	return children;
}

std::vector<std::unique_ptr<DataInspectorNode>> DataInspectorWindow::populateGlobalVariables(
	const ccc::ast::SourceFile& sourceFile, u32 minAddress, u32 maxAddress, const QString& filter)
{
	std::vector<std::unique_ptr<DataInspectorNode>> variables;

	for (const std::unique_ptr<ccc::ast::Node>& global : sourceFile.globals)
	{
		const ccc::ast::Variable& variable = global->as<ccc::ast::Variable>();
		if (variable.storage.global_address != (u32)-1)
		{
			std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
			node->name = QString::fromStdString(global->name);
			node->type = global.get();
			node->address = variable.storage.global_address;
			bool addressInRange = (u32)variable.storage.global_address >= minAddress && (u32)variable.storage.global_address < maxAddress;
			bool containsFilterString = filter.isEmpty() || node->name.toLower().contains(filter);
			if (addressInRange && containsFilterString)
			{
				variables.emplace_back(std::move(node));
			}
		}
	}

	return variables;
}
