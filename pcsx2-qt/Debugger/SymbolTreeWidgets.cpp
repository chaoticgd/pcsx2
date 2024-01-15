// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SymbolTreeWidgets.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>

#include "Delegates/DataInspectorValueColumnDelegate.h"

SymbolTreeWidget::SymbolTreeWidget(QWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);

	m_context_menu = new QMenu(this);
	
	m_context_menu->addAction(m_ui.actionCopyName);
	connect(m_ui.actionCopyName, &QAction::triggered, [this]() {
		QModelIndex index = m_ui.treeView->currentIndex().siblingAtRow(DataInspectorModel::NAME);
		QVariant data = m_model->data(index, Qt::DisplayRole);
		if(data.isValid())
			QApplication::clipboard()->setText(data.toString());
	});
	
	m_context_menu->addAction(m_ui.actionCopyLocation);
	connect(m_ui.actionCopyLocation, &QAction::triggered, [this]() {
		QModelIndex index = m_ui.treeView->currentIndex().siblingAtRow(DataInspectorModel::LOCATION);
		QVariant data = m_model->data(index, Qt::DisplayRole);
		if(data.isValid())
			QApplication::clipboard()->setText(data.toString());
	});
	
	m_context_menu->addSeparator();
	
	m_context_menu->addAction(m_ui.actionGoToInDisassembly);
	connect(m_ui.actionGoToInDisassembly, &QAction::triggered, [this]() {
	});
	
	m_context_menu->addAction(m_ui.actionGoToInMemoryView);
	connect(m_ui.actionGoToInMemoryView, &QAction::triggered, [this]() {
	});
	
	m_context_menu->addSeparator();
	
	m_context_menu->addAction(m_ui.actionGroupBySection);
	m_context_menu->addAction(m_ui.actionGroupBySourceFile);

	connect(m_ui.refreshButton, &QPushButton::pressed, this, &SymbolTreeWidget::update);
	connect(m_ui.filterBox, &QLineEdit::textEdited, this, &SymbolTreeWidget::update);
	connect(m_ui.actionGroupBySection, &QAction::toggled, this, &SymbolTreeWidget::update);
	connect(m_ui.actionGroupBySourceFile, &QAction::toggled, this, &SymbolTreeWidget::update);

	m_ui.treeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.treeView, &QTreeView::customContextMenuRequested, [this](QPoint pos) {
		m_context_menu->exec(m_ui.treeView->viewport()->mapToGlobal(pos));
	});
}

SymbolTreeWidget::~SymbolTreeWidget() = default;

void SymbolTreeWidget::setCPU(DebugInterface* cpu)
{
	m_cpu = cpu;
}

void SymbolTreeWidget::update()
{
	Q_ASSERT(m_cpu);
	SymbolGuardian& guardian = m_cpu->GetSymbolGuardian();

	guardian.Read([&](const ccc::SymbolDatabase& database) {
		bool group_by_section = m_ui.actionGroupBySection->isChecked();
		bool group_by_source_file = m_ui.actionGroupBySourceFile->isChecked();
		QString filter = m_ui.filterBox->text().toLower();

		auto initialRoot = populateSections(group_by_section, group_by_source_file, filter, database);
		m_model = new DataInspectorModel(std::move(initialRoot), guardian, this);
		m_ui.treeView->setModel(m_model);

		auto delegate = new DataInspectorValueColumnDelegate(guardian, this);
		m_ui.treeView->setItemDelegateForColumn(DataInspectorModel::VALUE, delegate);
		m_ui.treeView->setAlternatingRowColors(true);
	});
}

std::unique_ptr<DataInspectorNode> SymbolTreeWidget::populateSections(
	bool group_by_section, bool group_by_source_file, const QString& filter, const ccc::SymbolDatabase& database) const
{
	std::unique_ptr<DataInspectorNode> root = std::make_unique<DataInspectorNode>();
	root->children_fetched = true;

	if (group_by_section)
	{
		for (const ccc::Section& section : database.sections)
		{
			if (section.address().valid())
			{
				u32 min_address = section.address().value;
				u32 max_address = section.address().value + section.size();
				auto section_children = populateSourceFiles(
					min_address, max_address, group_by_source_file, filter, database);
				if (!section_children.empty())
				{
					std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
					node->name = QString::fromStdString(section.name());
					node->set_children(std::move(section_children));
					root->emplace_child(std::move(node));
				}
			}
		}
	}
	else
	{
		root->set_children(populateSourceFiles(0, UINT32_MAX, group_by_source_file, filter, database));
	}

	return root;
}

std::vector<std::unique_ptr<DataInspectorNode>> SymbolTreeWidget::populateSourceFiles(
	u32 min_address, u32 max_address, bool group_by_source_file, const QString& filter, const ccc::SymbolDatabase& database) const
{
	std::vector<std::unique_ptr<DataInspectorNode>> children;

	SymbolFilters filters;
	filters.min_address = min_address;
	filters.max_address = max_address;
	filters.string = filter;

	if (group_by_source_file)
	{
		filters.source_file = nullptr;

		{
			std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
			node->name = "(unknown source file)";
			node->set_children(populateSymbols(filters, database));

			if (!node->children().empty())
			{
				children.emplace_back(std::move(node));
			}
		}

		for (const ccc::SourceFile& source_file : database.source_files)
		{
			filters.source_file = &source_file;

			std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
			if (!source_file.command_line_path.empty())
				node->name = QString::fromStdString(source_file.command_line_path);
			else
				node->name = QString::fromStdString(source_file.name());
			node->set_children(populateSymbols(filters, database));

			if (!node->children().empty())
			{
				children.emplace_back(std::move(node));
			}
		}
	}
	else
	{
		children = populateSymbols(filters, database);
	}

	return children;
}

FunctionTreeWidget::FunctionTreeWidget(QWidget* parent)
	: SymbolTreeWidget(parent)
{
	m_ui.treeView->hideColumn(DataInspectorModel::TYPE);
	m_ui.treeView->hideColumn(DataInspectorModel::LIVENESS);
	m_ui.treeView->hideColumn(DataInspectorModel::VALUE);
}

FunctionTreeWidget::~FunctionTreeWidget() = default;

std::vector<std::unique_ptr<DataInspectorNode>> FunctionTreeWidget::populateSymbols(
	const SymbolFilters& filters, const ccc::SymbolDatabase& database) const
{
	std::vector<std::unique_ptr<DataInspectorNode>> variables;

	std::span<const ccc::Function> functions;
	if (filters.source_file.has_value() && *filters.source_file)
		functions = database.functions.span((*filters.source_file)->functions());
	else
		functions = database.functions;

	for (const ccc::Function& function : functions)
	{
		QString name;
		if (!filters.test(function, function.source_file(), name))
			continue;

		std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
		node->name = std::move(name);
		node->location.type = DataInspectorLocation::EE_MEMORY;
		node->location.address = function.address().value;
		variables.emplace_back(std::move(node));
	}

	return variables;
}

GlobalVariableTreeWidget::GlobalVariableTreeWidget(QWidget* parent)
	: SymbolTreeWidget(parent)
{
	m_ui.treeView->hideColumn(DataInspectorModel::LIVENESS);
}

GlobalVariableTreeWidget::~GlobalVariableTreeWidget() = default;

std::vector<std::unique_ptr<DataInspectorNode>> GlobalVariableTreeWidget::populateSymbols(
	const SymbolFilters& filters, const ccc::SymbolDatabase& database) const
{
	std::vector<std::unique_ptr<DataInspectorNode>> variables;

	std::span<const ccc::GlobalVariable> global_variables;
	if (filters.source_file.has_value() && *filters.source_file)
		global_variables = database.global_variables.span((*filters.source_file)->global_variables());
	else
		global_variables = database.global_variables;

	for (const ccc::GlobalVariable& global_variable : global_variables)
	{
		QString name;
		if (!filters.test(global_variable, global_variable.source_file(), name))
			continue;

		std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
		node->name = std::move(name);
		node->type = ccc::NodeHandle(global_variable, global_variable.type());
		node->location.type = DataInspectorLocation::EE_MEMORY;
		node->location.address = global_variable.address().value;
		variables.emplace_back(std::move(node));
	}

	return variables;
}

bool SymbolFilters::test(const ccc::Symbol& test_symbol, ccc::SourceFileHandle test_source_file, QString& name_out) const
{
	if (!test_symbol.address().valid())
		return false;

	if (source_file.has_value())
	{
		if (*source_file)
		{
			if ((*source_file)->handle() != test_source_file)
				return false;
		}
		else
		{
			if (test_source_file.valid())
				return false;
		}
	}

	if (test_symbol.address().value < min_address || test_symbol.address().value > max_address)
		return false;

	name_out = QString::fromStdString(test_symbol.name());

	if (!string.isEmpty() && !name_out.contains(string, Qt::CaseInsensitive))
		return false;

	return true;
}
