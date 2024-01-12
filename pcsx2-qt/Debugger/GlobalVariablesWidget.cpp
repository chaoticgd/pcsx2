// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GlobalVariablesWidget.h"

#include "Delegates/DataInspectorValueColumnDelegate.h"

GlobalVariablesWidget::GlobalVariablesWidget(DebugInterface& cpu, QWidget* parent)
	: QWidget(parent)
	, m_cpu(cpu)
{
	m_ui.setupUi(this);
	
	connect(m_ui.refresh_button, &QPushButton::pressed, this, &GlobalVariablesWidget::update);
	connect(m_ui.filter_box, &QLineEdit::textEdited, this, &GlobalVariablesWidget::update);
	connect(m_ui.group_by_section, &QCheckBox::toggled, this, &GlobalVariablesWidget::update);
	connect(m_ui.group_by_source_file, &QCheckBox::toggled, this, &GlobalVariablesWidget::update);
}

GlobalVariablesWidget::~GlobalVariablesWidget() = default;

void GlobalVariablesWidget::update()
{
	SymbolGuardian& guardian = m_cpu.GetSymbolGuardian();

	guardian.Read([&](const ccc::SymbolDatabase& database) {
		bool group_by_section = m_ui.group_by_section->isChecked();
		bool group_by_source_file = m_ui.group_by_source_file->isChecked();
		QString filter = m_ui.filter_box->text().toLower();

		auto initialGlobalRoot = populateGlobalSections(group_by_section, group_by_source_file, filter, database);
		m_model = new DataInspectorModel(std::move(initialGlobalRoot), guardian, this);
		m_ui.tree_view->setModel(m_model);

		auto delegate = new DataInspectorValueColumnDelegate(guardian, this);
		m_ui.tree_view->setItemDelegateForColumn(DataInspectorModel::VALUE, delegate);
		m_ui.tree_view->setAlternatingRowColors(true);
	});
}

std::unique_ptr<DataInspectorNode> GlobalVariablesWidget::populateGlobalSections(
	bool group_by_section, bool group_by_source_file, const QString& filter, const ccc::SymbolDatabase& database)
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
				auto section_children = populateGlobalSourceFiles(
					min_address, max_address, group_by_source_file, filter, database);
				if (!section_children.empty())
				{
					std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
					node->name = QString::fromStdString(section.name());
					node->children = std::move(section_children);

					for (std::unique_ptr<DataInspectorNode>& child : node->children)
						child->parent = node.get();

					root->children.emplace_back(std::move(node));
				}
			}
		}
	}
	else
	{
		auto root_children = populateGlobalSourceFiles(0, UINT32_MAX, group_by_source_file, filter, database);
		root->children.insert(root->children.end(),
			std::make_move_iterator(root_children.begin()),
			std::make_move_iterator(root_children.end()));
	}

	for (std::unique_ptr<DataInspectorNode>& child : root->children)
		child->parent = root.get();

	return root;
}

std::vector<std::unique_ptr<DataInspectorNode>> GlobalVariablesWidget::populateGlobalSourceFiles(
	u32 min_address, u32 max_address, bool group_by_source_file, const QString& filter, const ccc::SymbolDatabase& database)
{
	std::vector<std::unique_ptr<DataInspectorNode>> children;

	if (group_by_source_file)
	{
		for (const ccc::SourceFile& source_file : database.source_files)
		{
			std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
			if (!source_file.command_line_path.empty())
				node->name = QString::fromStdString(source_file.command_line_path);
			else
				node->name = QString::fromStdString(source_file.name());
			node->children = populateGlobalVariables(source_file, min_address, max_address, filter, database);

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
		for (const ccc::SourceFile& source_file : database.source_files)
		{
			std::vector<std::unique_ptr<DataInspectorNode>> variables = populateGlobalVariables(
				source_file, min_address, max_address, filter, database);
			children.insert(children.end(),
				std::make_move_iterator(variables.begin()),
				std::make_move_iterator(variables.end()));
		}
	}

	return children;
}

std::vector<std::unique_ptr<DataInspectorNode>> GlobalVariablesWidget::populateGlobalVariables(
	const ccc::SourceFile& source_file, u32 min_address, u32 max_address, const QString& filter, const ccc::SymbolDatabase& database)
{
	std::vector<std::unique_ptr<DataInspectorNode>> variables;

	for (const ccc::GlobalVariable& global_variable : database.global_variables.span(source_file.globals_variables()))
	{
		if (global_variable.address().valid())
		{
			std::unique_ptr<DataInspectorNode> node = std::make_unique<DataInspectorNode>();
			node->name = QString::fromStdString(global_variable.name());
			node->type = ccc::NodeHandle(global_variable, global_variable.type());
			node->location.type = DataInspectorLocation::EE_MEMORY;
			node->location.address = global_variable.address().value;
			bool addressInRange = global_variable.address().value >= min_address && global_variable.address().value < max_address;
			bool containsFilterString = filter.isEmpty() || node->name.toLower().contains(filter);
			if (addressInRange && containsFilterString)
			{
				variables.emplace_back(std::move(node));
			}
		}
	}

	return variables;
}
