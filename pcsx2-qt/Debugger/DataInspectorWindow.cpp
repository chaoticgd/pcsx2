#include "DataInspectorWindow.h"

#include <QtConcurrent/QtConcurrent>

#include "Debugger/Delegates/DataInspectorValueColumnDelegate.h"
#include "MainWindow.h"

struct MdebugElfFile
{
	std::vector<u8> data;
	std::vector<std::pair<ELF_SHR, std::string>> sections;
	u32 mdebugSectionOffset;
};

DataInspectorWindow::DataInspectorWindow(QWidget* parent)
	: QMainWindow(parent)
{
	m_ui.setupUi(this);

	setAttribute(Qt::WA_DeleteOnClose);

	connect(m_ui.globalsFilter, &QLineEdit::textEdited, this, &DataInspectorWindow::createGUI);
	connect(m_ui.globalsGroupBySection, &QCheckBox::toggled, this, &DataInspectorWindow::createGUI);
	connect(m_ui.globalsGroupByTranslationUnit, &QCheckBox::toggled, this, &DataInspectorWindow::createGUI);
}

void DataInspectorWindow::createGUI()
{
	R5900SymbolGuardian.Read([&](const ccc::SymbolDatabase& database) {
		bool group_by_section = m_ui.globalsGroupBySection->isChecked();
		bool group_by_source_file = m_ui.globalsGroupByTranslationUnit->isChecked();
		QString filter = m_ui.globalsFilter->text().toLower();

		auto initialGlobalRoot = populateGlobalSections(group_by_section, group_by_source_file, filter, database);
		m_global_model = new DataInspectorModel(std::move(initialGlobalRoot), R5900SymbolGuardian, this);
		m_ui.globalsTreeView->setModel(m_global_model);

		auto initialStackRoot = populateStack(database);
		m_stackModel = new DataInspectorModel(std::move(initialStackRoot), R5900SymbolGuardian, this);
		m_ui.stackTreeView->setModel(m_stackModel);

		connect(m_ui.stackRefreshButton, &QPushButton::pressed, this, &DataInspectorWindow::resetStack);

		for (QTreeView* view : {m_ui.watchTreeView, m_ui.globalsTreeView, m_ui.stackTreeView})
		{
			auto delegate = new DataInspectorValueColumnDelegate(R5900SymbolGuardian, this);
			view->setItemDelegateForColumn(DataInspectorModel::VALUE, delegate);
			view->setAlternatingRowColors(true);
		}
	});
}

std::unique_ptr<DataInspectorNode> DataInspectorWindow::populateGlobalSections(
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
				auto section_children = populateGlobalTranslationUnits(
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
		auto root_children = populateGlobalTranslationUnits(0, UINT32_MAX, group_by_source_file, filter, database);
		root->children.insert(root->children.end(),
			std::make_move_iterator(root_children.begin()),
			std::make_move_iterator(root_children.end()));
	}

	for (std::unique_ptr<DataInspectorNode>& child : root->children)
		child->parent = root.get();

	return root;
}

std::vector<std::unique_ptr<DataInspectorNode>> DataInspectorWindow::populateGlobalTranslationUnits(
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

std::vector<std::unique_ptr<DataInspectorNode>> DataInspectorWindow::populateGlobalVariables(
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

void DataInspectorWindow::resetStack()
{
	R5900SymbolGuardian.Read([&](const ccc::SymbolDatabase& database) {
		m_stackModel->reset(populateStack(database));
	});
}

std::unique_ptr<DataInspectorNode> DataInspectorWindow::populateStack(const ccc::SymbolDatabase& database)
{
	std::unique_ptr<DataInspectorNode> root = std::make_unique<DataInspectorNode>();
	root->children_fetched = true;

	DebugInterface& cpu = r5900Debug;
	u32 ra = cpu.getRegister(0, 31);
	u32 sp = cpu.getRegister(0, 29);

	std::vector<StackFrame> stack_frames;
	for (const auto& thread : cpu.GetThreadList())
	{
		if (thread->Status() == ThreadStatus::THS_RUN)
		{
			stack_frames = MipsStackWalk::Walk(&cpu, cpu.getPC(), ra, sp, thread->EntryPoint(), thread->StackTop());
			break;
		}
	}

	for (StackFrame& frame : stack_frames)
	{
		std::unique_ptr<DataInspectorNode> function_node = std::make_unique<DataInspectorNode>();

		ccc::FunctionHandle handle = database.functions.first_handle_from_starting_address(frame.entry);
		const ccc::Function* function = database.functions.symbol_from_handle(handle);
		if (function)
		{
			function_node->name = QString::fromStdString(function->name());
			for (const ccc::LocalVariable& local_variable : database.local_variables.optional_span(function->local_variables()))
			{
				const ccc::RegisterStorage* register_storage = std::get_if<ccc::RegisterStorage>(&local_variable.storage);
				if (register_storage && frame.sp == sp)
				{

					std::unique_ptr<DataInspectorNode> local_node = std::make_unique<DataInspectorNode>();
					local_node->name = QString::fromStdString(local_variable.name());
					local_node->type = ccc::NodeHandle(local_variable, local_variable.type());
					local_node->location.type = DataInspectorLocation::EE_REGISTER;
					local_node->location.address = register_storage->dbx_register_number;
					function_node->children.emplace_back(std::move(local_node));
				}
				else if (std::holds_alternative<ccc::StackStorage>(local_variable.storage))
				{
					std::unique_ptr<DataInspectorNode> local_node = std::make_unique<DataInspectorNode>();
					local_node->name = QString::fromStdString(local_variable.name());
					local_node->type = ccc::NodeHandle(local_variable, local_variable.type());
					local_node->location.type = DataInspectorLocation::EE_MEMORY;
					local_node->location.address = frame.sp;
					function_node->children.emplace_back(std::move(local_node));
				}
			}
		}
		else
		{
			function_node->name = QString::number(frame.entry, 16);
		}

		for (std::unique_ptr<DataInspectorNode>& child : function_node->children)
			child->parent = function_node.get();

		root->children.emplace_back(std::move(function_node));
	}

	for (std::unique_ptr<DataInspectorNode>& child : root->children)
		child->parent = root.get();

	return root;
}
