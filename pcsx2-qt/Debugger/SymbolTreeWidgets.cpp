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

	setupMenu();
}

SymbolTreeWidget::~SymbolTreeWidget() = default;

void SymbolTreeWidget::setupMenu()
{
	m_context_menu = new QMenu(this);

	QAction* copy_name = new QAction("Copy Name", this);
	connect(copy_name, &QAction::triggered, [this]() {
		if (!m_model)
			return;

		QModelIndex index = m_ui.treeView->currentIndex().siblingAtRow(DataInspectorModel::NAME);
		QVariant data = m_model->data(index, Qt::DisplayRole);
		if (data.isValid())
			QApplication::clipboard()->setText(data.toString());
	});
	m_context_menu->addAction(copy_name);

	QAction* copy_location = new QAction("Copy Location", this);
	connect(copy_location, &QAction::triggered, [this]() {
		if (!m_model)
			return;

		QModelIndex index = m_ui.treeView->currentIndex().siblingAtRow(DataInspectorModel::LOCATION);
		QVariant data = m_model->data(index, Qt::DisplayRole);
		if (data.isValid())
			QApplication::clipboard()->setText(data.toString());
	});
	m_context_menu->addAction(copy_location);

	m_context_menu->addSeparator();

	QAction* go_to_in_disassembly = new QAction("Go to in Disassembly", this);
	connect(go_to_in_disassembly, &QAction::triggered, [this]() {
	});
	m_context_menu->addAction(go_to_in_disassembly);

	QAction* go_to_in_memory_view = new QAction("Go to in Memory View", this);
	connect(go_to_in_memory_view, &QAction::triggered, [this]() {
	});
	m_context_menu->addAction(go_to_in_memory_view);

	m_context_menu->addSeparator();

	m_group_by_module = new QAction("Group by module", this);
	m_group_by_module->setCheckable(true);
	m_context_menu->addAction(m_group_by_module);

	m_group_by_section = new QAction("Group by section", this);
	m_group_by_section->setCheckable(true);
	m_context_menu->addAction(m_group_by_section);

	m_group_by_source_file = new QAction("Group by source file", this);
	m_group_by_source_file->setCheckable(true);
	m_context_menu->addAction(m_group_by_source_file);

	connect(m_ui.refreshButton, &QPushButton::pressed, this, &SymbolTreeWidget::update);
	connect(m_ui.filterBox, &QLineEdit::textEdited, this, &SymbolTreeWidget::update);
	connect(m_group_by_module, &QAction::toggled, this, &SymbolTreeWidget::update);
	connect(m_group_by_section, &QAction::toggled, this, &SymbolTreeWidget::update);
	connect(m_group_by_source_file, &QAction::toggled, this, &SymbolTreeWidget::update);

	m_ui.treeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.treeView, &QTreeView::customContextMenuRequested, [this](QPoint pos) {
		m_context_menu->exec(m_ui.treeView->viewport()->mapToGlobal(pos));
	});
}

void SymbolTreeWidget::setCPU(DebugInterface* cpu)
{
	m_cpu = cpu;

	if (m_cpu->getCpuType() == BREAKPOINT_IOP)
		m_group_by_module->setChecked(true);
}

void SymbolTreeWidget::update()
{
	Q_ASSERT(m_cpu);

	SymbolGuardian& guardian = m_cpu->GetSymbolGuardian();
	guardian.Read([&](const ccc::SymbolDatabase& database) {
		SymbolFilters filters;
		filters.group_by_module = m_group_by_module->isChecked();
		filters.group_by_section = m_group_by_section->isChecked();
		filters.group_by_source_file = m_group_by_source_file->isChecked();
		filters.string = m_ui.filterBox->text();

		std::unique_ptr<DataInspectorNode> root = std::make_unique<DataInspectorNode>();
		root->set_children(populateModules(filters, database));
		root->sortChildrenRecursively();

		m_model = new DataInspectorModel(std::move(root), guardian, this);
		m_ui.treeView->setModel(m_model);

		auto delegate = new DataInspectorValueColumnDelegate(guardian, this);
		m_ui.treeView->setItemDelegateForColumn(DataInspectorModel::VALUE, delegate);
		m_ui.treeView->setAlternatingRowColors(true);
	});
}

std::vector<std::unique_ptr<DataInspectorNode>> SymbolTreeWidget::populateModules(
	SymbolFilters& filters, const ccc::SymbolDatabase& database) const
{
	std::vector<std::unique_ptr<DataInspectorNode>> children;

	filters.module_handle = ccc::ModuleHandle();

	if (filters.group_by_module)
	{
		auto module_children = populateSections(filters, database);
		if (!module_children.empty())
		{
			std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
			node->name = "(unknown module)";
			node->set_children(std::move(module_children));
			children.emplace_back(std::move(node));
		}

		for (const ccc::Module& module_symbol : database.modules)
		{
			filters.module_handle = module_symbol.handle();

			auto module_children = populateSections(filters, database);
			if (!module_children.empty())
			{
				std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
				node->name = QString::fromStdString(module_symbol.name());
				if (module_symbol.is_irx)
				{
					node->name += " v";
					node->name += QString::number(module_symbol.version_major);
					node->name += ".";
					node->name += QString::number(module_symbol.version_minor);
				}
				node->set_children(std::move(module_children));
				children.emplace_back(std::move(node));
			}
		}
	}
	else
	{
		children = populateSections(filters, database);
	}

	return children;
}

std::vector<std::unique_ptr<DataInspectorNode>> SymbolTreeWidget::populateSections(
	SymbolFilters& filters, const ccc::SymbolDatabase& database) const
{
	std::vector<std::unique_ptr<DataInspectorNode>> children;

	filters.section = ccc::SectionHandle();

	if (filters.group_by_section)
	{
		auto section_children = populateSourceFiles(filters, database);
		if (!section_children.empty())
		{
			std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
			node->name = "(unknown section)";
			node->set_children(std::move(section_children));
			children.emplace_back(std::move(node));
		}

		for (const ccc::Section& section : database.sections)
		{
			if (section.address().valid())
			{
				filters.section = section.handle();

				auto section_children = populateSourceFiles(filters, database);
				if (!section_children.empty())
				{
					std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
					node->name = QString::fromStdString(section.name());
					node->set_children(std::move(section_children));
					children.emplace_back(std::move(node));
				}
			}
		}
	}
	else
	{
		children = populateSourceFiles(filters, database);
	}

	return children;
}

std::vector<std::unique_ptr<DataInspectorNode>> SymbolTreeWidget::populateSourceFiles(
	SymbolFilters& filters, const ccc::SymbolDatabase& database) const
{
	std::vector<std::unique_ptr<DataInspectorNode>> children;

	filters.source_file = nullptr;

	if (filters.group_by_source_file)
	{
		auto source_file_children = populateSymbols(filters, database);
		if (!source_file_children.empty())
		{
			std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
			node->name = "(unknown source file)";
			node->set_children(std::move(source_file_children));
			children.emplace_back(std::move(node));
		}

		for (const ccc::SourceFile& source_file : database.source_files)
		{
			filters.source_file = &source_file;

			auto source_file_children = populateSymbols(filters, database);
			if (!source_file_children.empty())
			{
				std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
				if (!source_file.command_line_path.empty())
					node->name = QString::fromStdString(source_file.command_line_path);
				else
					node->name = QString::fromStdString(source_file.name());
				node->set_children(populateSymbols(filters, database));
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
	if (filters.group_by_source_file && filters.source_file)
		functions = database.functions.span(filters.source_file->functions());
	else
		functions = database.functions;

	for (const ccc::Function& function : functions)
	{
		QString name;
		if (!filters.test(function, function.source_file(), database, name))
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
	if (filters.group_by_source_file && filters.source_file)
		global_variables = database.global_variables.span(filters.source_file->global_variables());
	else
		global_variables = database.global_variables;

	for (const ccc::GlobalVariable& global_variable : global_variables)
	{
		QString name;
		if (!filters.test(global_variable, global_variable.source_file(), database, name))
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

bool SymbolFilters::test(
	const ccc::Symbol& test_symbol,
	ccc::SourceFileHandle test_source_file,
	const ccc::SymbolDatabase& database,
	QString& name_out) const
{
	if (!test_symbol.address().valid())
		return false;

	if (group_by_module && test_symbol.module_handle() != module_handle)
		return false;

	if (group_by_section)
	{
		const ccc::Section* test_section = database.sections.symbol_from_contained_address(test_symbol.address());
		if (section.valid())
		{
			if (!test_section || test_section->handle() != section)
				return false;
		}
		else
		{
			if (test_section)
				return false;
		}
	}

	if (group_by_source_file)
	{
		if (source_file)
		{
			if (test_source_file != source_file->handle())
				return false;
		}
		else
		{
			if (test_source_file.valid())
				return false;
		}
	}

	name_out = QString::fromStdString(test_symbol.name());

	if (!string.isEmpty() && !name_out.contains(string, Qt::CaseInsensitive))
		return false;

	return true;
}
