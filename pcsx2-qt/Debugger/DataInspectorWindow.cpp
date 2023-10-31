#include "DataInspectorWindow.h"

#include <QtCore/QTimer>

#include "../QtHost.h"
#include "common/Error.h"
#include "Elfheader.h"
#include "CDVD/CDVD.h"
#include "DebugTools/ccc/analysis.h"

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

	auto [data, mdebugSectionOffset] = elf.GetSectionContentsByType(ELF_SECTION_TYPE_MIPS_DEBUG);
	if (data == nullptr)
	{
		m_ui.statusBar->showMessage("ERROR: No MIPS_DEBUG symbol table section present");
		return;
	}

	m_ui.statusBar->showMessage("Loading symbols...");

	ccc::Result<ccc::mdebug::SymbolTable> mdebugSymbolTable = ccc::mdebug::parse_symbol_table(*data, mdebugSectionOffset);
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

	m_globalModel = new DataInspectorModel(populateGlobals(), m_symbolTable, this);

	m_ui.globalsTreeView->setModel(m_globalModel);

	m_ui.globalsTreeView->setAlternatingRowColors(true);

	m_ui.statusBar->showMessage(QString("Loaded %1 data types").arg(m_symbolTable.deduplicated_types.size()));
}

DataInspectorWindow::~DataInspectorWindow() = default;

std::unique_ptr<DataInspectorModel::TreeNode> DataInspectorWindow::populateGlobals()
{
	std::unique_ptr<TreeNode> root = std::make_unique<TreeNode>();
	root->childrenFetched = true;

	for (const std::unique_ptr<ccc::ast::SourceFile>& sourceFile : m_symbolTable.source_files)
	{
		std::unique_ptr<TreeNode> node = std::make_unique<TreeNode>();
		if (!sourceFile->relative_path.empty())
			// Display the path of the source file relative to the working
			// directory of the compiler.
			node->name = QString::fromStdString(sourceFile->relative_path);
		else
			// If we don't have that information, display the full path instead.
			node->name = QString::fromStdString(sourceFile->full_path);
		node->type = sourceFile.get();
		node->parent = root.get();
		root->children.emplace_back(std::move(node));
	}

	return root;
}
