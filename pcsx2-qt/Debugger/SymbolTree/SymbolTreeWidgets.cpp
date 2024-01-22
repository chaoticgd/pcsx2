// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SymbolTreeWidgets.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>

#include "SymbolTreeValueDelegate.h"

SymbolTreeWidget::SymbolTreeWidget(u32 flags, QWidget* parent)
	: QWidget(parent)
	, m_flags(flags)
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

		QModelIndex index = m_ui.treeView->currentIndex().siblingAtRow(SymbolTreeModel::NAME);
		QVariant data = m_model->data(index, Qt::DisplayRole);
		if (data.isValid())
			QApplication::clipboard()->setText(data.toString());
	});
	m_context_menu->addAction(copy_name);

	QAction* copy_location = new QAction("Copy Location", this);
	connect(copy_location, &QAction::triggered, [this]() {
		if (!m_model)
			return;

		QModelIndex index = m_ui.treeView->currentIndex().siblingAtRow(SymbolTreeModel::LOCATION);
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

	if (m_flags & ENABLE_GROUPING)
	{
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

		connect(m_group_by_module, &QAction::toggled, this, &SymbolTreeWidget::update);
		connect(m_group_by_section, &QAction::toggled, this, &SymbolTreeWidget::update);
		connect(m_group_by_source_file, &QAction::toggled, this, &SymbolTreeWidget::update);
	}

	connect(m_ui.refreshButton, &QPushButton::pressed, this, &SymbolTreeWidget::update);
	connect(m_ui.filterBox, &QLineEdit::textEdited, this, &SymbolTreeWidget::update);

	m_ui.treeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.treeView, &QTreeView::customContextMenuRequested, [this](QPoint pos) {
		m_context_menu->exec(m_ui.treeView->viewport()->mapToGlobal(pos));
	});
}

void SymbolTreeWidget::setCPU(DebugInterface* cpu)
{
	m_cpu = cpu;

	if (m_group_by_module && m_cpu->getCpuType() == BREAKPOINT_IOP)
		m_group_by_module->setChecked(true);
}

void SymbolTreeWidget::update()
{
	assert(m_cpu);

	SymbolGuardian& guardian = m_cpu->GetSymbolGuardian();
	guardian.Read([&](const ccc::SymbolDatabase& database) -> void {
		SymbolFilters filters;
		filters.group_by_module = m_group_by_module && m_group_by_module->isChecked();
		filters.group_by_section = m_group_by_section && m_group_by_section->isChecked();
		filters.group_by_source_file = m_group_by_source_file && m_group_by_source_file->isChecked();
		filters.string = m_ui.filterBox->text();

		std::unique_ptr<SymbolTreeNode> root = std::make_unique<SymbolTreeNode>();
		root->set_children(populateModules(filters, database));
		root->sortChildrenRecursively();

		m_model = new SymbolTreeModel(std::move(root), guardian, this);
		m_ui.treeView->setModel(m_model);

		auto delegate = new SymbolTreeValueDelegate(guardian, this);
		m_ui.treeView->setItemDelegateForColumn(SymbolTreeModel::VALUE, delegate);
		m_ui.treeView->setAlternatingRowColors(true);
	});
}

std::vector<std::unique_ptr<SymbolTreeNode>> SymbolTreeWidget::populateModules(
	SymbolFilters& filters, const ccc::SymbolDatabase& database) const
{
	std::vector<std::unique_ptr<SymbolTreeNode>> nodes;

	filters.module_handle = ccc::ModuleHandle();

	if (filters.group_by_module)
	{
		auto module_children = populateSections(filters, database);
		if (!module_children.empty())
		{
			std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
			node->name = "(unknown module)";
			node->set_children(std::move(module_children));
			nodes.emplace_back(std::move(node));
		}

		for (const ccc::Module& module_symbol : database.modules)
		{
			filters.module_handle = module_symbol.handle();

			auto module_children = populateSections(filters, database);
			if (!module_children.empty())
			{
				std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
				node->name = QString::fromStdString(module_symbol.name());
				if (module_symbol.is_irx)
				{
					s32 major = module_symbol.version_major;
					s32 minor = module_symbol.version_minor;
					node->name += QString(" v%1.%2").arg(major).arg(minor);
				}
				node->set_children(std::move(module_children));
				nodes.emplace_back(std::move(node));
			}
		}
	}
	else
	{
		nodes = populateSections(filters, database);
	}

	return nodes;
}

std::vector<std::unique_ptr<SymbolTreeNode>> SymbolTreeWidget::populateSections(
	SymbolFilters& filters, const ccc::SymbolDatabase& database) const
{
	std::vector<std::unique_ptr<SymbolTreeNode>> nodes;

	filters.section = ccc::SectionHandle();

	if (filters.group_by_section)
	{
		auto section_children = populateSourceFiles(filters, database);
		if (!section_children.empty())
		{
			std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
			node->name = "(unknown section)";
			node->set_children(std::move(section_children));
			nodes.emplace_back(std::move(node));
		}

		for (const ccc::Section& section : database.sections)
		{
			if (section.address().valid())
			{
				filters.section = section.handle();

				auto section_children = populateSourceFiles(filters, database);
				if (section_children.empty())
					continue;

				std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
				node->name = QString::fromStdString(section.name());
				node->set_children(std::move(section_children));
				nodes.emplace_back(std::move(node));
			}
		}
	}
	else
	{
		nodes = populateSourceFiles(filters, database);
	}

	return nodes;
}

std::vector<std::unique_ptr<SymbolTreeNode>> SymbolTreeWidget::populateSourceFiles(
	SymbolFilters& filters, const ccc::SymbolDatabase& database) const
{
	std::vector<std::unique_ptr<SymbolTreeNode>> nodes;

	filters.source_file = nullptr;

	if (filters.group_by_source_file)
	{
		auto source_file_children = populateSymbols(filters, database);
		if (!source_file_children.empty())
		{
			std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
			node->name = "(unknown source file)";
			node->set_children(std::move(source_file_children));
			nodes.emplace_back(std::move(node));
		}

		for (const ccc::SourceFile& source_file : database.source_files)
		{
			filters.source_file = &source_file;

			auto source_file_children = populateSymbols(filters, database);
			if (source_file_children.empty())
				continue;

			std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
			if (!source_file.command_line_path.empty())
				node->name = QString::fromStdString(source_file.command_line_path);
			else
				node->name = QString::fromStdString(source_file.name());
			node->set_children(populateSymbols(filters, database));
			nodes.emplace_back(std::move(node));
		}
	}
	else
	{
		nodes = populateSymbols(filters, database);
	}

	return nodes;
}

FunctionTreeWidget::FunctionTreeWidget(QWidget* parent)
	: SymbolTreeWidget(ENABLE_GROUPING, parent)
{
	m_ui.treeView->hideColumn(SymbolTreeModel::TYPE);
	m_ui.treeView->hideColumn(SymbolTreeModel::LIVENESS);
	m_ui.treeView->hideColumn(SymbolTreeModel::VALUE);
}

FunctionTreeWidget::~FunctionTreeWidget() = default;

std::vector<std::unique_ptr<SymbolTreeNode>> FunctionTreeWidget::populateSymbols(
	const SymbolFilters& filters, const ccc::SymbolDatabase& database) const
{
	std::vector<std::unique_ptr<SymbolTreeNode>> nodes;

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

		std::unique_ptr<SymbolTreeNode> function_node = std::make_unique<SymbolTreeNode>();

		function_node->name = std::move(name);
		function_node->location = SymbolTreeLocation(m_cpu, function.address().value);

		for (auto pair : database.labels.handles_from_address_range(function.address_range()))
		{
			const ccc::Label* label = database.labels.symbol_from_handle(pair.second);
			if (!label || label->address() == function.address())
				continue;

			std::unique_ptr<SymbolTreeNode> label_node = std::make_unique<SymbolTreeNode>();
			label_node->name = QString::fromStdString(label->name());
			label_node->location = SymbolTreeLocation(m_cpu, label->address().value);
			function_node->emplace_child(std::move(label_node));
		}

		nodes.emplace_back(std::move(function_node));
	}

	return nodes;
}

GlobalVariableTreeWidget::GlobalVariableTreeWidget(QWidget* parent)
	: SymbolTreeWidget(ENABLE_GROUPING, parent)
{
	m_ui.treeView->hideColumn(SymbolTreeModel::LIVENESS);
}

GlobalVariableTreeWidget::~GlobalVariableTreeWidget() = default;

std::vector<std::unique_ptr<SymbolTreeNode>> GlobalVariableTreeWidget::populateSymbols(
	const SymbolFilters& filters, const ccc::SymbolDatabase& database) const
{
	std::vector<std::unique_ptr<SymbolTreeNode>> nodes;

	std::span<const ccc::Function> functions;
	std::span<const ccc::GlobalVariable> global_variables;
	if (filters.group_by_source_file && filters.source_file)
	{
		functions = database.functions.span(filters.source_file->functions());
		global_variables = database.global_variables.span(filters.source_file->global_variables());
	}
	else
	{
		functions = database.functions;
		global_variables = database.global_variables;
	}

	for (const ccc::GlobalVariable& global_variable : global_variables)
	{
		QString name;
		if (!filters.test(global_variable, global_variable.source_file(), database, name))
			continue;

		std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
		node->name = std::move(name);
		node->type = ccc::NodeHandle(global_variable, global_variable.type());
		node->location = SymbolTreeLocation(m_cpu, global_variable.address().value);
		nodes.emplace_back(std::move(node));
	}

	// We also include static local variables in the global variable tree
	// because they have global storage. Why not.
	for (const ccc::Function& function : functions)
	{
		std::vector<std::unique_ptr<SymbolTreeNode>> local_variable_nodes;
		for (const ccc::LocalVariable& local_variable : database.local_variables.optional_span(function.local_variables()))
		{
			if(!std::holds_alternative<ccc::GlobalStorage>(local_variable.storage))
				continue;
			
			QString name;
			if (!filters.test(local_variable, function.source_file(), database, name))
				continue;

			std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
			node->name = std::move(name);
			node->type = ccc::NodeHandle(local_variable, local_variable.type());
			node->location = SymbolTreeLocation(m_cpu, local_variable.address().value);
			local_variable_nodes.emplace_back(std::move(node));
		}

		if (local_variable_nodes.empty())
			continue;

		std::unique_ptr<SymbolTreeNode> function_node = std::make_unique<SymbolTreeNode>();
		function_node->name = QString::fromStdString(function.name());
		function_node->set_children(std::move(local_variable_nodes));
		nodes.emplace_back(std::move(function_node));
	}

	return nodes;
}

LocalVariableTreeWidget::LocalVariableTreeWidget(QWidget* parent)
	: SymbolTreeWidget(NO_SYMBOL_TREE_FLAGS, parent)
{
	m_ui.treeView->hideColumn(SymbolTreeModel::LIVENESS);
}

LocalVariableTreeWidget::~LocalVariableTreeWidget() = default;

std::vector<std::unique_ptr<SymbolTreeNode>> LocalVariableTreeWidget::populateSymbols(
	const SymbolFilters& filters, const ccc::SymbolDatabase& database) const
{
	std::vector<std::unique_ptr<SymbolTreeNode>> nodes;

	u32 program_counter = m_cpu->getPC();
	u32 stack_pointer = m_cpu->getRegister(EECAT_GPR, 29);

	const ccc::Function* function = database.functions.symbol_from_contained_address(program_counter);
	if (!function)
		return nodes;

	for (const ccc::LocalVariable& local_variable : database.local_variables.optional_span(function->local_variables()))
	{
		std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
		node->name = QString::fromStdString(local_variable.name());
		node->type = ccc::NodeHandle(local_variable, local_variable.type());

		if (const ccc::GlobalStorage* storage = std::get_if<ccc::GlobalStorage>(&local_variable.storage))
		{
			if (!local_variable.address().valid())
				continue;

			node->location = SymbolTreeLocation(m_cpu, stack_pointer + local_variable.address().value);
		}
		else if (const ccc::RegisterStorage* storage = std::get_if<ccc::RegisterStorage>(&local_variable.storage))
		{
			if (m_cpu->getCpuType() == BREAKPOINT_EE)
				node->location.type = SymbolTreeLocation::EE_REGISTER;
			else
				node->location.type = SymbolTreeLocation::IOP_REGISTER;
			node->location.address = storage->dbx_register_number;
		}
		else if (const ccc::StackStorage* storage = std::get_if<ccc::StackStorage>(&local_variable.storage))
		{
			node->location = SymbolTreeLocation(m_cpu, stack_pointer + storage->stack_pointer_offset);
		}

		nodes.emplace_back(std::move(node));
	}

	return nodes;
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
