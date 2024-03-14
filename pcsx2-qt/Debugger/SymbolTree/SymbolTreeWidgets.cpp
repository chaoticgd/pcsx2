// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SymbolTreeWidgets.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QScrollBar>

#include "NewSymbolDialogs.h"
#include "SymbolTreeDelegates.h"

static bool testName(const QString& name, const QString& filter);

SymbolTreeWidget::SymbolTreeWidget(u32 flags, s32 symbol_address_alignment, DebugInterface& cpu, QWidget* parent)
	: QWidget(parent)
	, m_cpu(cpu)
	, m_flags(flags)
	, m_symbol_address_alignment(symbol_address_alignment)
{
	m_ui.setupUi(this);

	setupMenu();

	connect(m_ui.refreshButton, &QPushButton::clicked, this, &SymbolTreeWidget::reset);
	connect(m_ui.filterBox, &QLineEdit::textEdited, this, &SymbolTreeWidget::reset);

	connect(m_ui.newButton, &QPushButton::clicked, this, &SymbolTreeWidget::onNewButtonPressed);
	connect(m_ui.deleteButton, &QPushButton::clicked, this, &SymbolTreeWidget::onDeleteButtonPressed);

	connect(m_ui.treeView->verticalScrollBar(), &QScrollBar::valueChanged, this, &SymbolTreeWidget::updateVisibleNodes);

	m_ui.treeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.treeView, &QTreeView::customContextMenuRequested, this, &SymbolTreeWidget::openMenu);
}

SymbolTreeWidget::~SymbolTreeWidget() = default;

void SymbolTreeWidget::reset()
{
	if (!m_model)
		setupTree();

	m_cpu.GetSymbolGuardian().UpdateFunctionHashes(m_cpu);

	std::unique_ptr<SymbolTreeNode> root;
	m_cpu.GetSymbolGuardian().TryRead([&](const ccc::SymbolDatabase& database) -> void {
		SymbolFilters filters;
		filters.group_by_module = m_group_by_module && m_group_by_module->isChecked();
		filters.group_by_section = m_group_by_section && m_group_by_section->isChecked();
		filters.group_by_source_file = m_group_by_source_file && m_group_by_source_file->isChecked();
		filters.string = m_ui.filterBox->text();

		root = buildTree(filters, database);
	});

	if (root)
	{
		root->sortChildrenRecursively(m_sort_by_if_type_is_known && m_sort_by_if_type_is_known->isChecked());
		m_model->reset(std::move(root));

		// Read the initial values for all the nodes.
		updateChildren(QModelIndex());
	}
}

void SymbolTreeWidget::updateVisibleNodes()
{
	if (!m_model)
		return;

	QModelIndex first_visible = m_ui.treeView->indexAt(m_ui.treeView->rect().topLeft());
	QModelIndex last_visible = m_ui.treeView->indexAt(m_ui.treeView->rect().bottomLeft());

	if (!first_visible.isValid() || !last_visible.isValid())
		return;

	// Update all the visible nodes with the current contents of memory.
	for (QModelIndex index = first_visible; index.isValid() && index != last_visible; index = m_ui.treeView->indexBelow(index))
		m_model->setData(index, QVariant(), Qt::UserRole);

	m_ui.treeView->update();
}

void SymbolTreeWidget::updateChildren(QModelIndex index)
{
	if (!m_model)
		return;

	m_model->setData(index, QVariant(), Qt::UserRole);

	int child_count = m_model->rowCount(index);
	for (int i = 0; i < child_count; i++)
	{
		QModelIndex child = m_model->index(i, 0, index);
		updateChildren(child);
	}
}

void SymbolTreeWidget::setupTree()
{
	m_model = new SymbolTreeModel(m_cpu, this);
	m_ui.treeView->setModel(m_model);

	auto location_delegate = new SymbolTreeLocationDelegate(m_cpu.GetSymbolGuardian(), m_symbol_address_alignment, this);
	m_ui.treeView->setItemDelegateForColumn(SymbolTreeModel::LOCATION, location_delegate);

	auto type_delegate = new SymbolTreeTypeDelegate(m_cpu.GetSymbolGuardian(), this);
	m_ui.treeView->setItemDelegateForColumn(SymbolTreeModel::TYPE, type_delegate);

	auto value_delegate = new SymbolTreeValueDelegate(m_cpu.GetSymbolGuardian(), this);
	m_ui.treeView->setItemDelegateForColumn(SymbolTreeModel::VALUE, value_delegate);

	m_ui.treeView->setAlternatingRowColors(true);
	m_ui.treeView->setEditTriggers(QTreeView::AllEditTriggers);

	configureColumns();

	connect(m_ui.treeView, &QTreeView::pressed, this, &SymbolTreeWidget::onTreeViewClicked);
}

std::unique_ptr<SymbolTreeNode> SymbolTreeWidget::buildTree(const SymbolFilters& filters, const ccc::SymbolDatabase& database)
{
	std::vector<SymbolWork> symbols = getSymbols(filters.string, database);

	// We should be able to compare the pointers directly, but lets compare the
	// handles instead in case the implementation changes.

	auto source_file_comparator = [](const SymbolWork& lhs, const SymbolWork& rhs) -> bool {
		if (lhs.source_file)
			return rhs.source_file && lhs.source_file->handle() < rhs.source_file->handle();
		else
			return rhs.source_file;
	};

	auto section_comparator = [](const SymbolWork& lhs, const SymbolWork& rhs) -> bool {
		if (lhs.section)
			return rhs.section && lhs.section->handle() < rhs.section->handle();
		else
			return rhs.section;
	};

	auto module_comparator = [](const SymbolWork& lhs, const SymbolWork& rhs) -> bool {
		if (lhs.module_symbol)
			return rhs.module_symbol && lhs.module_symbol->handle() < rhs.module_symbol->handle();
		else
			return rhs.module_symbol;
	};

	if (filters.group_by_source_file)
		std::stable_sort(symbols.begin(), symbols.end(), source_file_comparator);

	if (filters.group_by_section)
		std::stable_sort(symbols.begin(), symbols.end(), section_comparator);

	if (filters.group_by_module)
		std::stable_sort(symbols.begin(), symbols.end(), module_comparator);

	std::unique_ptr<SymbolTreeNode> root = std::make_unique<SymbolTreeNode>();

	SymbolTreeNode* source_file_node = nullptr;
	SymbolTreeNode* section_node = nullptr;
	SymbolTreeNode* module_node = nullptr;

	const SymbolWork* source_file_work = nullptr;
	const SymbolWork* section_work = nullptr;
	const SymbolWork* module_work = nullptr;

	for (SymbolWork& work : symbols)
	{
		std::unique_ptr<SymbolTreeNode> node = buildNode(work, database);

		if (filters.group_by_source_file)
		{
			node = groupBySourceFile(std::move(node), work, source_file_node, source_file_work);
			if (!node)
				continue;
		}

		if (filters.group_by_section)
		{
			node = groupBySection(std::move(node), work, section_node, section_work);
			if (!node)
				continue;
		}

		if (filters.group_by_module)
		{
			node = groupByModule(std::move(node), work, module_node, module_work);
			if (!node)
				continue;
		}

		root->emplaceChild(std::move(node));
	}

	return root;
}

std::unique_ptr<SymbolTreeNode> SymbolTreeWidget::groupBySourceFile(
	std::unique_ptr<SymbolTreeNode> child, const SymbolWork& child_work, SymbolTreeNode*& prev_group, const SymbolWork*& prev_work)
{
	if (prev_group && child_work.source_file == prev_work->source_file)
	{
		prev_group->emplaceChild(std::move(child));
		return nullptr;
	}

	std::unique_ptr<SymbolTreeNode> group_node = std::make_unique<SymbolTreeNode>();
	if (child_work.source_file)
	{
		group_node->tag = SymbolTreeNode::GROUP;
		if (!child_work.source_file->command_line_path.empty())
			group_node->name = QString::fromStdString(child_work.source_file->command_line_path);
		else
			group_node->name = QString::fromStdString(child_work.source_file->name());
	}
	else
	{
		group_node->tag = SymbolTreeNode::UNKNOWN_GROUP;
		group_node->name = tr("(unknown source file)");
	}

	group_node->emplaceChild(std::move(child));
	child = std::move(group_node);

	prev_group = child.get();
	prev_work = &child_work;

	return child;
}

std::unique_ptr<SymbolTreeNode> SymbolTreeWidget::groupBySection(
	std::unique_ptr<SymbolTreeNode> child, const SymbolWork& child_work, SymbolTreeNode*& prev_group, const SymbolWork*& prev_work)
{
	if (prev_group && child_work.section == prev_work->section)
	{
		prev_group->emplaceChild(std::move(child));
		return nullptr;
	}

	std::unique_ptr<SymbolTreeNode> group_node = std::make_unique<SymbolTreeNode>();
	if (child_work.section)
	{
		group_node->tag = SymbolTreeNode::GROUP;
		group_node->name = QString::fromStdString(child_work.section->name());
	}
	else
	{
		group_node->tag = SymbolTreeNode::UNKNOWN_GROUP;
		group_node->name = tr("(unknown section)");
	}

	group_node->emplaceChild(std::move(child));
	child = std::move(group_node);

	prev_group = child.get();
	prev_work = &child_work;

	return child;
}

std::unique_ptr<SymbolTreeNode> SymbolTreeWidget::groupByModule(
	std::unique_ptr<SymbolTreeNode> child, const SymbolWork& child_work, SymbolTreeNode*& prev_group, const SymbolWork*& prev_work)
{
	if (prev_group && child_work.module_symbol == prev_work->module_symbol)
	{
		prev_group->emplaceChild(std::move(child));
		return nullptr;
	}

	std::unique_ptr<SymbolTreeNode> group_node = std::make_unique<SymbolTreeNode>();
	if (child_work.module_symbol)
	{

		group_node->tag = SymbolTreeNode::GROUP;
		group_node->name = QString::fromStdString(child_work.module_symbol->name());
		if (child_work.module_symbol->is_irx)
		{
			s32 major = child_work.module_symbol->version_major;
			s32 minor = child_work.module_symbol->version_minor;
			group_node->name += QString(" v%1.%2").arg(major).arg(minor);
		}
	}
	else
	{
		group_node->tag = SymbolTreeNode::UNKNOWN_GROUP;
		group_node->name = tr("(unknown module)");
	}

	group_node->emplaceChild(std::move(child));
	child = std::move(group_node);

	prev_group = child.get();
	prev_work = &child_work;

	return child;
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

	m_rename_symbol = new QAction(tr("Rename Symbol"), this);
	connect(m_rename_symbol, &QAction::triggered, this, &SymbolTreeWidget::onRenameSymbol);
	m_context_menu->addAction(m_rename_symbol);

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

		connect(m_group_by_module, &QAction::toggled, this, &SymbolTreeWidget::reset);
		connect(m_group_by_section, &QAction::toggled, this, &SymbolTreeWidget::reset);
		connect(m_group_by_source_file, &QAction::toggled, this, &SymbolTreeWidget::reset);
	}

	if (m_flags & ALLOW_SORTING_BY_IF_TYPE_IS_KNOWN)
	{
		m_context_menu->addSeparator();

		m_sort_by_if_type_is_known = new QAction(tr("Sort by if type is known"), this);
		m_sort_by_if_type_is_known->setCheckable(true);
		m_context_menu->addAction(m_sort_by_if_type_is_known);

		connect(m_sort_by_if_type_is_known, &QAction::toggled, this, &SymbolTreeWidget::reset);
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
}

void SymbolTreeWidget::openMenu(QPoint pos)
{
	bool node_is_object = currentNodeIsObject();
	bool node_is_symbol = currentNodeIsSymbol();

	m_rename_symbol->setEnabled(node_is_symbol);

	if (m_reset_children)
		m_reset_children->setEnabled(node_is_object);

	if (m_change_type_temporarily)
		m_change_type_temporarily->setEnabled(node_is_object);

	m_context_menu->exec(m_ui.treeView->viewport()->mapToGlobal(pos));
}

void SymbolTreeWidget::onCopyName()
{
	SymbolTreeNode* node = currentNode();
	if (!node)
		return;

	QApplication::clipboard()->setText(node->name);
}

void SymbolTreeWidget::onCopyLocation()
{
	SymbolTreeNode* node = currentNode();
	if (!node)
		return;

	QApplication::clipboard()->setText(node->location.toString(m_cpu));
}

void SymbolTreeWidget::onRenameSymbol()
{
	SymbolTreeNode* node = currentNode();
	if (!node || !node->symbol.valid())
		return;

	QString title = tr("Rename Symbol");
	QString label = tr("Name:");

	QString text;
	m_cpu.GetSymbolGuardian().BlockingRead([&](const ccc::SymbolDatabase& database) {
		const ccc::Symbol* symbol = node->symbol.lookup_symbol(database);
		if (!symbol || !symbol->address().valid())
			return;

		text = QString::fromStdString(symbol->name());
	});

	bool ok;
	std::string name = QInputDialog::getText(this, title, label, QLineEdit::Normal, text, &ok).toStdString();
	if (!ok)
		return;

	m_cpu.GetSymbolGuardian().BlockingReadWrite([&](ccc::SymbolDatabase& database) {
		node->symbol.rename_symbol(name, database);
	});
}

void SymbolTreeWidget::onGoToInDisassembly()
{
	SymbolTreeNode* node = currentNode();
	if (!node)
		return;

	goToInDisassembly(node->location.address);
}

void SymbolTreeWidget::onGoToInMemoryView()
{
	SymbolTreeNode* node = currentNode();
	if (!node)
		return;

	goToInMemoryView(node->location.address);
}

void SymbolTreeWidget::onResetChildren()
{
	if (!m_model)
		return;

	QModelIndex index = m_ui.treeView->currentIndex();
	if (!index.isValid())
		return;

	m_model->resetChildren(index);
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
	std::optional<QString> old_type = m_model->typeFromModelIndexToString(index);
	if (!old_type.has_value())
	{
		QMessageBox::warning(this, tr("Cannot Change Type"), tr("That node cannot have a type."));
		return;
	}

	bool ok;
	QString type_string = QInputDialog::getText(this, title, label, QLineEdit::Normal, *old_type, &ok);
	if (!ok)
		return;

	std::optional<QString> error_message = m_model->changeTypeTemporarily(index, type_string.toStdString());
	if (error_message.has_value() && !error_message->isEmpty())
		QMessageBox::warning(this, tr("Cannot Change Type"), *error_message);
}

bool SymbolTreeWidget::currentNodeIsObject()
{
	SymbolTreeNode* node = currentNode();
	if (!node)
		return false;

	return node->tag == SymbolTreeNode::OBJECT;
}

bool SymbolTreeWidget::currentNodeIsSymbol()
{
	SymbolTreeNode* node = currentNode();
	if (!node)
		return false;

	return node->symbol.valid();
}

void SymbolTreeWidget::onTreeViewClicked(const QModelIndex& index)
{
	if (!index.isValid())
		return;

	SymbolTreeNode* node = m_model->nodeFromIndex(index);
	if (!node)
		return;

	switch (index.column())
	{
		case SymbolTreeModel::NAME:
			nameColumnClicked(node->location.address);
			break;
		case SymbolTreeModel::LOCATION:
			locationColumnClicked(node->location.address);
			break;
	}
}

SymbolTreeNode* SymbolTreeWidget::currentNode()
{
	if (!m_model)
		return nullptr;

	QModelIndex index = m_ui.treeView->currentIndex();
	return m_model->nodeFromIndex(index);
}

// *****************************************************************************

FunctionTreeWidget::FunctionTreeWidget(DebugInterface& cpu, QWidget* parent)
	: SymbolTreeWidget(ALLOW_GROUPING, 4, cpu, parent)
{
}

FunctionTreeWidget::~FunctionTreeWidget() = default;

std::vector<SymbolTreeWidget::SymbolWork> FunctionTreeWidget::getSymbols(
	const QString& filter, const ccc::SymbolDatabase& database)
{
	std::vector<SymbolTreeWidget::SymbolWork> symbols;

	for (const ccc::Function& function : database.functions)
	{
		if (!function.address().valid())
			continue;

		QString name = QString::fromStdString(function.name());
		if (!testName(name, filter))
			continue;

		SymbolWork& work = symbols.emplace_back();

		work.name = std::move(name);
		work.descriptor = ccc::FUNCTION;
		work.symbol = &function;

		work.module_symbol = database.modules.symbol_from_handle(function.module_handle());
		work.section = database.sections.symbol_overlapping_address(function.address());
		work.source_file = database.source_files.symbol_from_handle(function.source_file());
	}

	return symbols;
}

std::unique_ptr<SymbolTreeNode> FunctionTreeWidget::buildNode(
	SymbolWork& work, const ccc::SymbolDatabase& database) const
{
	const ccc::Function& function = static_cast<const ccc::Function&>(*work.symbol);

	std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
	node->name = std::move(work.name);
	node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, function.address().value);
	node->symbol = ccc::MultiSymbolHandle(function);

	for (auto address_handle : database.labels.handles_from_address_range(function.address_range()))
	{
		const ccc::Label* label = database.labels.symbol_from_handle(address_handle.second);
		if (!label || label->address() == function.address())
			continue;

		std::unique_ptr<SymbolTreeNode> label_node = std::make_unique<SymbolTreeNode>();
		label_node->name = QString::fromStdString(label->name());
		label_node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, label->address().value);
		node->emplaceChild(std::move(label_node));
	}

	return node;
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

void FunctionTreeWidget::onNewButtonPressed()
{
	NewFunctionDialog* dialog = new NewFunctionDialog(m_cpu, this);
	if (dialog->exec() == QDialog::Accepted)
		update();
}

void FunctionTreeWidget::onDeleteButtonPressed()
{
	SymbolTreeNode* node = currentNode();
	if (!node)
		return;

	if (!node->symbol.valid() || node->symbol.descriptor() != ccc::SymbolDescriptor::FUNCTION)
		return;

	if (QMessageBox::question(this, tr("Confirm Deletion"), tr("Delete '%1'?").arg(node->name)) != QMessageBox::Yes)
		return;

	m_cpu.GetSymbolGuardian().BlockingReadWrite([&](ccc::SymbolDatabase& database) {
		database.destroy_function(node->symbol.handle());
	});

	update();
}

// *****************************************************************************

GlobalVariableTreeWidget::GlobalVariableTreeWidget(DebugInterface& cpu, QWidget* parent)
	: SymbolTreeWidget(ALLOW_GROUPING | ALLOW_SORTING_BY_IF_TYPE_IS_KNOWN | ALLOW_TYPE_ACTIONS, 1, cpu, parent)
{
}

GlobalVariableTreeWidget::~GlobalVariableTreeWidget() = default;

std::vector<SymbolTreeWidget::SymbolWork> GlobalVariableTreeWidget::getSymbols(
	const QString& filter, const ccc::SymbolDatabase& database)
{
	std::vector<SymbolTreeWidget::SymbolWork> symbols;

	for (const ccc::GlobalVariable& global_variable : database.global_variables)
	{
		if (!global_variable.address().valid())
			continue;

		QString name = QString::fromStdString(global_variable.name());
		if (!testName(name, filter))
			continue;

		SymbolWork& work = symbols.emplace_back();

		work.name = std::move(name);
		work.descriptor = ccc::GLOBAL_VARIABLE;
		work.symbol = &global_variable;

		work.module_symbol = database.modules.symbol_from_handle(global_variable.module_handle());
		work.section = database.sections.symbol_overlapping_address(global_variable.address());
		work.source_file = database.source_files.symbol_from_handle(global_variable.source_file());
	}

	// We also include static local variables in the global variable tree
	// because they have global storage. Why not.
	for (const ccc::LocalVariable& local_variable : database.local_variables)
	{
		if (!std::holds_alternative<ccc::GlobalStorage>(local_variable.storage))
			continue;

		if (!local_variable.address().valid())
			continue;

		ccc::FunctionHandle function_handle = local_variable.function();
		const ccc::Function* function = database.functions.symbol_from_handle(function_handle);

		QString function_name;
		if (function)
			function_name = QString::fromStdString(function->name());
		else
			function_name = tr("unknown function");

		QString name = QString("%1 (%2)")
						   .arg(QString::fromStdString(local_variable.name()))
						   .arg(function_name);
		if (!testName(name, filter))
			continue;

		SymbolWork& work = symbols.emplace_back();

		work.name = std::move(name);
		work.descriptor = ccc::LOCAL_VARIABLE;
		work.symbol = &local_variable;

		work.module_symbol = database.modules.symbol_from_handle(local_variable.module_handle());
		work.section = database.sections.symbol_overlapping_address(local_variable.address());
		if (function)
			work.source_file = database.source_files.symbol_from_handle(function->source_file());
	}

	return symbols;
}

std::unique_ptr<SymbolTreeNode> GlobalVariableTreeWidget::buildNode(
	SymbolWork& work, const ccc::SymbolDatabase& database) const
{
	std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
	node->name = std::move(work.name);

	switch (work.descriptor)
	{
		case ccc::GLOBAL_VARIABLE:
		{
			const ccc::GlobalVariable& global_variable = static_cast<const ccc::GlobalVariable&>(*work.symbol);

			if (global_variable.type())
				node->type = ccc::NodeHandle(global_variable, global_variable.type());
			node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, global_variable.address().value);
			node->symbol = ccc::MultiSymbolHandle(global_variable);

			break;
		}
		case ccc::LOCAL_VARIABLE:
		{
			const ccc::LocalVariable& local_variable = static_cast<const ccc::LocalVariable&>(*work.symbol);

			if (local_variable.type())
				node->type = ccc::NodeHandle(local_variable, local_variable.type());
			node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, local_variable.address().value);
			node->symbol = ccc::MultiSymbolHandle(local_variable);

			break;
		}
		default:
		{
		}
	}

	return node;
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

void GlobalVariableTreeWidget::onNewButtonPressed()
{
	NewGlobalVariableDialog* dialog = new NewGlobalVariableDialog(m_cpu, this);
	if (dialog->exec() == QDialog::Accepted)
		update();
}

void GlobalVariableTreeWidget::onDeleteButtonPressed()
{
	SymbolTreeNode* node = currentNode();
	if (!node)
		return;

	if (!node->symbol.valid() || node->symbol.descriptor() != ccc::SymbolDescriptor::GLOBAL_VARIABLE)
		return;

	if (QMessageBox::question(this, tr("Confirm Deletion"), tr("Delete '%1'?").arg(node->name)) != QMessageBox::Yes)
		return;

	m_cpu.GetSymbolGuardian().BlockingReadWrite([&](ccc::SymbolDatabase& database) {
		database.global_variables.destroy_symbol(node->symbol.handle());
	});

	update();
}

// *****************************************************************************

LocalVariableTreeWidget::LocalVariableTreeWidget(DebugInterface& cpu, QWidget* parent)
	: SymbolTreeWidget(ALLOW_TYPE_ACTIONS, 1, cpu, parent)
{
}

LocalVariableTreeWidget::~LocalVariableTreeWidget() = default;

std::vector<SymbolTreeWidget::SymbolWork> LocalVariableTreeWidget::getSymbols(
	const QString& filter, const ccc::SymbolDatabase& database)
{
	m_stack_pointer = m_cpu.getRegister(EECAT_GPR, 29);

	u32 program_counter = m_cpu.getPC();
	const ccc::Function* function = database.functions.symbol_overlapping_address(program_counter);
	if (!function || !function->local_variables().has_value())
		return std::vector<SymbolWork>();

	std::vector<SymbolTreeWidget::SymbolWork> symbols;

	for (const ccc::LocalVariableHandle local_variable_handle : *function->local_variables())
	{
		const ccc::LocalVariable* local_variable = database.local_variables.symbol_from_handle(local_variable_handle);

		if (std::holds_alternative<ccc::GlobalStorage>(local_variable->storage) && !local_variable->address().valid())
			continue;

		QString name = QString::fromStdString(local_variable->name());
		if (!testName(name, filter))
			continue;

		SymbolWork& work = symbols.emplace_back();

		work.name = std::move(name);
		work.descriptor = ccc::LOCAL_VARIABLE;
		work.symbol = local_variable;

		work.module_symbol = database.modules.symbol_from_handle(local_variable->module_handle());
		work.section = database.sections.symbol_overlapping_address(local_variable->address());
		work.source_file = database.source_files.symbol_from_handle(function->source_file());
	}

	return symbols;
}

std::unique_ptr<SymbolTreeNode> LocalVariableTreeWidget::buildNode(
	SymbolWork& work, const ccc::SymbolDatabase& database) const
{
	const ccc::LocalVariable& local_variable = static_cast<const ccc::LocalVariable&>(*work.symbol);

	std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
	node->name = QString::fromStdString(local_variable.name());
	if (local_variable.type())
		node->type = ccc::NodeHandle(local_variable, local_variable.type());
	node->live_range = local_variable.live_range;
	node->symbol = ccc::MultiSymbolHandle(local_variable);

	if (const ccc::GlobalStorage* storage = std::get_if<ccc::GlobalStorage>(&local_variable.storage))
		node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, m_stack_pointer + local_variable.address().value);
	else if (const ccc::RegisterStorage* storage = std::get_if<ccc::RegisterStorage>(&local_variable.storage))
		node->location = SymbolTreeLocation(SymbolTreeLocation::REGISTER, storage->dbx_register_number);
	else if (const ccc::StackStorage* storage = std::get_if<ccc::StackStorage>(&local_variable.storage))
		node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, m_stack_pointer + storage->stack_pointer_offset);

	return node;
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

void LocalVariableTreeWidget::onNewButtonPressed()
{
	NewLocalVariableDialog* dialog = new NewLocalVariableDialog(m_cpu, this);
	if (dialog->exec() == QDialog::Accepted)
		update();
}

void LocalVariableTreeWidget::onDeleteButtonPressed()
{
	SymbolTreeNode* node = currentNode();
	if (!node)
		return;

	if (!node->symbol.valid() || node->symbol.descriptor() != ccc::SymbolDescriptor::LOCAL_VARIABLE)
		return;

	if (QMessageBox::question(this, tr("Confirm Deletion"), tr("Delete '%1'?").arg(node->name)) != QMessageBox::Yes)
		return;

	m_cpu.GetSymbolGuardian().BlockingReadWrite([&](ccc::SymbolDatabase& database) {
		database.local_variables.destroy_symbol(node->symbol.handle());
	});

	update();
}

// *****************************************************************************

ParameterVariableTreeWidget::ParameterVariableTreeWidget(DebugInterface& cpu, QWidget* parent)
	: SymbolTreeWidget(ALLOW_TYPE_ACTIONS, 1, cpu, parent)
{
}

ParameterVariableTreeWidget::~ParameterVariableTreeWidget() = default;

std::vector<SymbolTreeWidget::SymbolWork> ParameterVariableTreeWidget::getSymbols(
	const QString& filter, const ccc::SymbolDatabase& database)
{
	m_stack_pointer = m_cpu.getRegister(EECAT_GPR, 29);

	std::vector<SymbolTreeWidget::SymbolWork> symbols;

	u32 program_counter = m_cpu.getPC();
	const ccc::Function* function = database.functions.symbol_overlapping_address(program_counter);
	if (!function || !function->parameter_variables().has_value())
		return std::vector<SymbolWork>();

	for (const ccc::ParameterVariableHandle parameter_variable_handle : *function->parameter_variables())
	{
		const ccc::ParameterVariable* parameter_variable = database.parameter_variables.symbol_from_handle(parameter_variable_handle);

		QString name = QString::fromStdString(parameter_variable->name());
		if (!testName(name, filter))
			continue;

		ccc::FunctionHandle function_handle = parameter_variable->function();
		const ccc::Function* function = database.functions.symbol_from_handle(function_handle);

		SymbolWork& work = symbols.emplace_back();

		work.name = std::move(name);
		work.descriptor = ccc::PARAMETER_VARIABLE;
		work.symbol = parameter_variable;

		work.module_symbol = database.modules.symbol_from_handle(parameter_variable->module_handle());
		work.section = database.sections.symbol_overlapping_address(parameter_variable->address());
		work.source_file = database.source_files.symbol_from_handle(function->source_file());
	}

	return symbols;
}

std::unique_ptr<SymbolTreeNode> ParameterVariableTreeWidget::buildNode(
	SymbolWork& work, const ccc::SymbolDatabase& database) const
{
	const ccc::ParameterVariable& parameter_variable = static_cast<const ccc::ParameterVariable&>(*work.symbol);

	std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
	node->name = QString::fromStdString(parameter_variable.name());
	if (parameter_variable.type())
		node->type = ccc::NodeHandle(parameter_variable, parameter_variable.type());
	node->symbol = ccc::MultiSymbolHandle(parameter_variable);

	if (const ccc::RegisterStorage* storage = std::get_if<ccc::RegisterStorage>(&parameter_variable.storage))
		node->location = SymbolTreeLocation(SymbolTreeLocation::REGISTER, storage->dbx_register_number);
	else if (const ccc::StackStorage* storage = std::get_if<ccc::StackStorage>(&parameter_variable.storage))
		node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, m_stack_pointer + storage->stack_pointer_offset);


	return node;
}

void ParameterVariableTreeWidget::configureColumns()
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

void ParameterVariableTreeWidget::onNewButtonPressed()
{
	NewParameterVariableDialog* dialog = new NewParameterVariableDialog(m_cpu, this);
	if (dialog->exec() == QDialog::Accepted)
		update();
}

void ParameterVariableTreeWidget::onDeleteButtonPressed()
{
	SymbolTreeNode* node = currentNode();
	if (!node)
		return;

	if (!node->symbol.valid() || node->symbol.descriptor() != ccc::SymbolDescriptor::PARAMETER_VARIABLE)
		return;

	if (QMessageBox::question(this, tr("Confirm Deletion"), tr("Delete '%1'?").arg(node->name)) != QMessageBox::Yes)
		return;

	m_cpu.GetSymbolGuardian().BlockingReadWrite([&](ccc::SymbolDatabase& database) {
		database.parameter_variables.destroy_symbol(node->symbol.handle());
	});

	update();
}

static bool testName(const QString& name, const QString& filter)
{
	return filter.isEmpty() || name.contains(filter, Qt::CaseInsensitive);
}
