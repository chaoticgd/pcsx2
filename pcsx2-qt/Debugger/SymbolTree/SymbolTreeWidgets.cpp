// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SymbolTreeWidgets.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>

#include "common/Assertions.h"
#include "SymbolTreeValueDelegate.h"

SymbolTreeWidget::SymbolTreeWidget(u32 flags, DebugInterface& cpu, QWidget* parent)
	: QWidget(parent)
	, m_cpu(cpu)
	, m_flags(flags)
{
	m_ui.setupUi(this);

	setupMenu();
}

SymbolTreeWidget::~SymbolTreeWidget() = default;

void SymbolTreeWidget::update()
{
	if (!m_model)
		setupTree();

	std::unique_ptr<SymbolTreeNode> root;
	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) -> void {
		SymbolFilters filters;
		filters.group_by_module = m_group_by_module && m_group_by_module->isChecked();
		filters.group_by_section = m_group_by_section && m_group_by_section->isChecked();
		filters.group_by_source_file = m_group_by_source_file && m_group_by_source_file->isChecked();
		filters.string = m_ui.filterBox->text();

		root = std::make_unique<SymbolTreeNode>();
		root->setChildren(populateModules(filters, database));
	});

	if (root)
	{
		root->sortChildrenRecursively(m_sort_by_if_type_is_known && m_sort_by_if_type_is_known->isChecked());
		m_model->reset(std::move(root));
	}
}

void SymbolTreeWidget::setupTree()
{
	m_model = new SymbolTreeModel(m_cpu, this);
	m_ui.treeView->setModel(m_model);

	auto delegate = new SymbolTreeValueDelegate(m_cpu.GetSymbolGuardian(), this);
	m_ui.treeView->setItemDelegateForColumn(SymbolTreeModel::VALUE, delegate);

	m_ui.treeView->setAlternatingRowColors(true);
	m_ui.treeView->setEditTriggers(QTreeView::AllEditTriggers);

	configureColumns();
}

void SymbolTreeWidget::setupMenu()
{
	m_context_menu = new QMenu(this);

	QAction* copy_name = new QAction(tr("Copy Name"), this);
	connect(copy_name, &QAction::triggered, this, &SymbolTreeWidget::onCopyName);
	m_context_menu->addAction(copy_name);

	QAction* copy_location = new QAction(tr("Copy Location"), this);
	connect(copy_location, &QAction::triggered, this, &SymbolTreeWidget::onCopyLocation);
	m_context_menu->addAction(copy_location);

	m_context_menu->addSeparator();

	QAction* go_to_in_disassembly = new QAction(tr("Go to in Disassembly"), this);
	connect(go_to_in_disassembly, &QAction::triggered, this, &SymbolTreeWidget::onGoToInDisassembly);
	m_context_menu->addAction(go_to_in_disassembly);

	QAction* go_to_in_memory_view = new QAction(tr("Go to in Memory View"), this);
	connect(go_to_in_memory_view, &QAction::triggered, this, &SymbolTreeWidget::onGoToInMemoryView);
	m_context_menu->addAction(go_to_in_memory_view);

	if (m_flags & ALLOW_GROUPING)
	{
		m_context_menu->addSeparator();

		m_group_by_module = new QAction(tr("Group by module"), this);
		m_group_by_module->setCheckable(true);
		if (m_cpu.getCpuType() == BREAKPOINT_IOP)
			m_group_by_module->setChecked(true);
		m_context_menu->addAction(m_group_by_module);

		m_group_by_section = new QAction(tr("Group by section"), this);
		m_group_by_section->setCheckable(true);
		m_context_menu->addAction(m_group_by_section);

		m_group_by_source_file = new QAction(tr("Group by source file"), this);
		m_group_by_source_file->setCheckable(true);
		m_context_menu->addAction(m_group_by_source_file);

		connect(m_group_by_module, &QAction::toggled, this, &SymbolTreeWidget::update);
		connect(m_group_by_section, &QAction::toggled, this, &SymbolTreeWidget::update);
		connect(m_group_by_source_file, &QAction::toggled, this, &SymbolTreeWidget::update);
	}

	if (m_flags & ALLOW_SORTING_BY_IF_TYPE_IS_KNOWN)
	{
		m_context_menu->addSeparator();

		m_sort_by_if_type_is_known = new QAction(tr("Sort by if type is known"), this);
		m_sort_by_if_type_is_known->setCheckable(true);
		m_sort_by_if_type_is_known->setChecked(true);
		m_context_menu->addAction(m_sort_by_if_type_is_known);

		connect(m_sort_by_if_type_is_known, &QAction::toggled, this, &SymbolTreeWidget::update);
	}

	if (m_flags & ALLOW_TYPE_ACTIONS)
	{
		m_context_menu->addSeparator();

		m_reset_children = new QAction(tr("Reset children"), this);
		m_context_menu->addAction(m_reset_children);

		m_change_type_temporarily = new QAction(tr("Change type temporarily"), this);
		m_context_menu->addAction(m_change_type_temporarily);

		connect(m_reset_children, &QAction::triggered, this, &SymbolTreeWidget::onResetChildren);
		connect(m_change_type_temporarily, &QAction::triggered, this, &SymbolTreeWidget::onChangeTypeTemporarily);
	}

	connect(m_ui.refreshButton, &QPushButton::pressed, this, &SymbolTreeWidget::update);
	connect(m_ui.filterBox, &QLineEdit::textEdited, this, &SymbolTreeWidget::update);

	m_ui.treeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.treeView, &QTreeView::customContextMenuRequested, [this](QPoint pos) {
		m_context_menu->exec(m_ui.treeView->viewport()->mapToGlobal(pos));
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
			node->setChildren(std::move(module_children));
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
				node->setChildren(std::move(module_children));
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
			node->setChildren(std::move(section_children));
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
				node->setChildren(std::move(section_children));
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
			node->setChildren(std::move(source_file_children));
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
			node->setChildren(populateSymbols(filters, database));
			nodes.emplace_back(std::move(node));
		}
	}
	else
	{
		nodes = populateSymbols(filters, database);
	}

	return nodes;
}

void SymbolTreeWidget::onCopyName()
{
	if (!m_model)
		return;

	QModelIndex index = m_ui.treeView->currentIndex().siblingAtRow(SymbolTreeModel::NAME);
	QVariant data = m_model->data(index, Qt::DisplayRole);
	if (data.isValid())
		QApplication::clipboard()->setText(data.toString());
}

void SymbolTreeWidget::onCopyLocation()
{
	if (!m_model)
		return;

	QModelIndex index = m_ui.treeView->currentIndex().siblingAtRow(SymbolTreeModel::LOCATION);
	QVariant data = m_model->data(index, Qt::DisplayRole);
	if (data.isValid())
		QApplication::clipboard()->setText(data.toString());
}

void SymbolTreeWidget::onGoToInDisassembly()
{
}

void SymbolTreeWidget::onGoToInMemoryView()
{
}

void SymbolTreeWidget::onResetChildren()
{
	if (!m_model)
		return;

	QModelIndex index = m_ui.treeView->currentIndex();
	if (!index.isValid())
		return;

	if (!m_model->resetChildren(index))
		QMessageBox::warning(this, tr("Cannot Reset Children"), tr("That node doesn't have a type."));
}

void SymbolTreeWidget::onChangeTypeTemporarily()
{
	if (!m_model)
		return;

	QModelIndex index = m_ui.treeView->currentIndex();
	if (!index.isValid())
		return;

	QString title = tr("Change Type To");
	QString label = tr("Type:");
	std::optional<QString> old_type = m_model->typeToString(index);
	if (!old_type.has_value())
	{
		QMessageBox::warning(this, tr("Cannot Change Type"), tr("That node doesn't have a type."));
		return;
	}

	bool ok;
	QString type_string = QInputDialog::getText(this, title, label, QLineEdit::Normal, *old_type, &ok);
	if (!ok)
		return;

	QString error_message = m_model->changeTypeTemporarily(index, type_string.toStdString());
	if (!error_message.isEmpty())
		QMessageBox::warning(this, tr("Cannot Change Type"), error_message);
}

FunctionTreeWidget::FunctionTreeWidget(DebugInterface& cpu, QWidget* parent)
	: SymbolTreeWidget(ALLOW_GROUPING, cpu, parent)
{
}

FunctionTreeWidget::~FunctionTreeWidget() = default;

const char* FunctionTreeWidget::name() const
{
	return "Function Tree";
}

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
			function_node->emplaceChild(std::move(label_node));
		}

		nodes.emplace_back(std::move(function_node));
	}

	return nodes;
}

void FunctionTreeWidget::configureColumns()
{
	m_ui.treeView->setColumnHidden(SymbolTreeModel::NAME, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::LOCATION, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::TYPE, true);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::LIVENESS, true);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::VALUE, true);

	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::NAME, QHeaderView::Stretch);

	m_ui.treeView->header()->setStretchLastSection(false);
}

GlobalVariableTreeWidget::GlobalVariableTreeWidget(DebugInterface& cpu, QWidget* parent)
	: SymbolTreeWidget(ALLOW_GROUPING | ALLOW_SORTING_BY_IF_TYPE_IS_KNOWN | ALLOW_TYPE_ACTIONS, cpu, parent)
{
}

GlobalVariableTreeWidget::~GlobalVariableTreeWidget() = default;

const char* GlobalVariableTreeWidget::name() const
{
	return "Global Variable Tree";
}

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
		if (global_variable.type())
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
			if (!std::holds_alternative<ccc::GlobalStorage>(local_variable.storage))
				continue;

			QString name;
			if (!filters.test(local_variable, function.source_file(), database, name))
				continue;

			std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
			node->name = std::move(name);
			if (local_variable.type())
				node->type = ccc::NodeHandle(local_variable, local_variable.type());
			node->location = SymbolTreeLocation(m_cpu, local_variable.address().value);
			local_variable_nodes.emplace_back(std::move(node));
		}

		if (local_variable_nodes.empty())
			continue;

		std::unique_ptr<SymbolTreeNode> function_node = std::make_unique<SymbolTreeNode>();
		function_node->name = QString::fromStdString(function.name());
		function_node->setChildren(std::move(local_variable_nodes));
		nodes.emplace_back(std::move(function_node));
	}

	return nodes;
}

void GlobalVariableTreeWidget::configureColumns()
{
	m_ui.treeView->setColumnHidden(SymbolTreeModel::NAME, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::LOCATION, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::TYPE, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::LIVENESS, true);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::VALUE, false);

	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::NAME, QHeaderView::Stretch);
	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::TYPE, QHeaderView::Stretch);
	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::VALUE, QHeaderView::Stretch);

	m_ui.treeView->header()->setStretchLastSection(false);
}

LocalVariableTreeWidget::LocalVariableTreeWidget(DebugInterface& cpu, QWidget* parent)
	: SymbolTreeWidget(ALLOW_TYPE_ACTIONS, cpu, parent)
{
}

LocalVariableTreeWidget::~LocalVariableTreeWidget() = default;

const char* LocalVariableTreeWidget::name() const
{
	return "Local Variable Tree";
}

std::vector<std::unique_ptr<SymbolTreeNode>> LocalVariableTreeWidget::populateSymbols(
	const SymbolFilters& filters, const ccc::SymbolDatabase& database) const
{
	std::vector<std::unique_ptr<SymbolTreeNode>> nodes;

	u32 program_counter = m_cpu.getPC();
	u32 stack_pointer = m_cpu.getRegister(EECAT_GPR, 29);

	const ccc::Function* function = database.functions.symbol_overlapping_address(program_counter);
	if (!function)
		return nodes;

	for (const ccc::LocalVariable& local_variable : database.local_variables.optional_span(function->local_variables()))
	{
		std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
		node->name = QString::fromStdString(local_variable.name());
		if (local_variable.type())
			node->type = ccc::NodeHandle(local_variable, local_variable.type());

		if (const ccc::GlobalStorage* storage = std::get_if<ccc::GlobalStorage>(&local_variable.storage))
		{
			if (!local_variable.address().valid())
				continue;

			node->location = SymbolTreeLocation(m_cpu, stack_pointer + local_variable.address().value);
		}
		else if (const ccc::RegisterStorage* storage = std::get_if<ccc::RegisterStorage>(&local_variable.storage))
		{
			if (m_cpu.getCpuType() == BREAKPOINT_EE)
				node->location.type = SymbolTreeLocation::EE_REGISTER;
			else
				node->location.type = SymbolTreeLocation::IOP_REGISTER;
			node->location.address = storage->dbx_register_number;
		}
		else if (const ccc::StackStorage* storage = std::get_if<ccc::StackStorage>(&local_variable.storage))
		{
			node->location = SymbolTreeLocation(m_cpu, stack_pointer + storage->stack_pointer_offset);
		}
		node->live_range = local_variable.live_range;

		nodes.emplace_back(std::move(node));
	}

	return nodes;
}

void LocalVariableTreeWidget::configureColumns()
{
	m_ui.treeView->setColumnHidden(SymbolTreeModel::NAME, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::LOCATION, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::TYPE, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::LIVENESS, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::VALUE, false);

	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::NAME, QHeaderView::Stretch);
	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::TYPE, QHeaderView::Stretch);
	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::VALUE, QHeaderView::Stretch);

	m_ui.treeView->header()->setStretchLastSection(false);
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
		const ccc::Section* test_section = database.sections.symbol_overlapping_address(test_symbol.address());
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
