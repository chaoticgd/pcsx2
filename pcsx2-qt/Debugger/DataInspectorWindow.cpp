#include "DataInspectorWindow.h"

#include "../QtHost.h"
#include "common/Error.h"
#include "CDVD/CDVD.h"
#include "DebugTools/ccc/analysis.h"
#include "Debugger/Delegates/DataInspectorValueColumnDelegate.h"

DataInspectorWindow::DataInspectorWindow(QWidget* parent)
	: QMainWindow(parent)
{
	m_ui.setupUi(this);

	// probably a race condition
	std::string elfPath = VMManager::GetELFPath();
	if (elfPath.empty())
	{
		m_ui.statusBar->showMessage("ERROR: No ELF file loaded");
		return;
	}

	ElfObject elf;
	Error error;
	if (!cdvdLoadElf(&elf, elfPath, false, &error))
	{
		QString message = QString("ERROR: Failed to load ELF file %1 (%2)").arg(elfPath.c_str()).arg(error.GetDescription().c_str());
		m_ui.statusBar->showMessage(message);
		return;
	}

	m_sectionHeaders = elf.GetSectionHeaders();

	// Checking the section name is better than checking the section type since
	// both STABS and DWARF symbols have a type of MIPS_DEBUG.
	ELF_SHR* mdebugSectionHeader = nullptr;
	for (auto& [sectionHeader, sectionName] : m_sectionHeaders)
		if (sectionName == ".mdebug")
			mdebugSectionHeader = &sectionHeader;

	if (mdebugSectionHeader == nullptr)
	{
		m_ui.statusBar->showMessage("ERROR: No .mdebug symbol table section present");
		return;
	}

	m_ui.statusBar->showMessage("Loading symbols...");

	ccc::Result<ccc::mdebug::SymbolTable> mdebugSymbolTable = ccc::mdebug::parse_symbol_table(elf.GetData(), mdebugSectionHeader->sh_offset);
	if (!mdebugSymbolTable.success())
	{
		m_ui.statusBar->showMessage(mdebugSymbolTable.error().message);
		return;
	}

	ccc::Result<ccc::HighSymbolTable> symbolTable = ccc::analyse(*mdebugSymbolTable, ccc::DEDUPLICATE_TYPES);
	if (!symbolTable.success())
	{
		m_ui.statusBar->showMessage(symbolTable.error().message);
		return;
	}
	m_symbolTable = std::move(*symbolTable);

	m_typeNameToDeduplicatedTypeIndex = ccc::build_type_name_to_deduplicated_type_index_map(m_symbolTable);

	bool sortBySection = m_ui.globalsGroupBySection->isChecked();
	bool sortByTranslationUnit = m_ui.globalsGroupByTranslationUnit->isChecked();

	m_globalModel = new DataInspectorModel(populateGlobals(sortBySection, sortByTranslationUnit), m_symbolTable, m_typeNameToDeduplicatedTypeIndex, this);
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
	bool sortBySection = m_ui.globalsGroupBySection->isChecked();
	bool sortByTranslationUnit = m_ui.globalsGroupByTranslationUnit->isChecked();
	QString filter = m_ui.globalsFilter->text();

	m_globalModel->reset(populateGlobals(sortBySection, sortByTranslationUnit));
	if (!filter.isEmpty())
	{
		m_globalModel->fetchAllExceptThroughPointers();
		m_globalModel->removeRowsNotMatchingFilter(filter);
	}
}

std::unique_ptr<DataInspectorWindow::TreeNode> DataInspectorWindow::populateGlobals(bool groupBySection, bool groupByTranslationUnit)
{
	std::unique_ptr<TreeNode> root = std::make_unique<TreeNode>();
	root->childrenFetched = true;

	if (groupBySection)
	{
		for (auto& [sectionHeader, sectionName] : m_sectionHeaders)
		{
			if (sectionHeader.sh_addr > 0)
			{
				u32 minAddress = sectionHeader.sh_addr;
				u32 maxAddress = sectionHeader.sh_addr + sectionHeader.sh_size;
				auto sectionChildren = populateInnerGlobals(minAddress, maxAddress, groupByTranslationUnit);
				if (!sectionChildren.empty())
				{
					std::unique_ptr<TreeNode> node = std::make_unique<TreeNode>();
					node->name = QString::fromStdString(sectionName);
					node->children = std::move(sectionChildren);

					for (std::unique_ptr<TreeNode>& child : node->children)
						child->parent = node.get();

					root->children.emplace_back(std::move(node));
				}
			}
		}
	}
	else
	{
		auto rootChildren = populateInnerGlobals(0, UINT32_MAX, groupByTranslationUnit);
		root->children.insert(root->children.end(),
			std::make_move_iterator(rootChildren.begin()),
			std::make_move_iterator(rootChildren.end()));
	}

	for (std::unique_ptr<TreeNode>& child : root->children)
		child->parent = root.get();

	return root;
}

std::vector<std::unique_ptr<DataInspectorWindow::TreeNode>> DataInspectorWindow::populateInnerGlobals(u32 min_address, u32 max_address, bool groupByTU)
{
	std::vector<std::unique_ptr<TreeNode>> children;

	if (groupByTU)
	{
		for (const std::unique_ptr<ccc::ast::SourceFile>& sourceFile : m_symbolTable.source_files)
		{
			std::unique_ptr<TreeNode> node = std::make_unique<TreeNode>();
			if (!sourceFile->relative_path.empty())
				node->name = QString::fromStdString(sourceFile->relative_path);
			else
				node->name = QString::fromStdString(sourceFile->full_path);
			node->type = sourceFile.get();

			for (const std::unique_ptr<ccc::ast::Node>& global : sourceFile->globals)
			{
				const ccc::ast::Variable& variable = global->as<ccc::ast::Variable>();
				if ((u32)variable.storage.global_address >= min_address && (u32)variable.storage.global_address < max_address)
				{
					children.emplace_back(std::move(node));
					break;
				}
			}
		}
	}
	else
	{
		for (const std::unique_ptr<ccc::ast::SourceFile>& sourceFile : m_symbolTable.source_files)
		{
			for (const std::unique_ptr<ccc::ast::Node>& global : sourceFile->globals)
			{
				const ccc::ast::Variable& variable = global->as<ccc::ast::Variable>();
				if (variable.storage.global_address > -1)
				{
					std::unique_ptr<TreeNode> node = std::make_unique<TreeNode>();
					node->name = QString::fromStdString(global->name);
					node->type = global.get();
					node->address = variable.storage.global_address;
					if ((u32)variable.storage.global_address >= min_address && (u32)variable.storage.global_address < max_address)
					{
						children.emplace_back(std::move(node));
					}
				}
			}
		}
	}

	return children;
}
