// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockManager.h"

#include "Debugger/DebuggerWindow.h"
#include "Debugger/DisassemblyWidget.h"
#include "Debugger/JsonValueWrapper.h"
#include "Debugger/RegisterWidget.h"
#include "Debugger/StackWidget.h"
#include "Debugger/ThreadWidget.h"
#include "Debugger/Breakpoints/BreakpointWidget.h"
#include "Debugger/Docking/LayoutEditorDialog.h"
#include "Debugger/Memory/MemorySearchWidget.h"
#include "Debugger/Memory/MemoryViewWidget.h"
#include "Debugger/Memory/SavedAddressesWidget.h"
#include "Debugger/SymbolTree/SymbolTreeWidgets.h"

#include "common/Assertions.h"
#include "common/FileSystem.h"
#include "common/Path.h"

#include <kddockwidgets/Config.h>
#include <kddockwidgets/DockWidget.h>
#include <kddockwidgets/LayoutSaver.h>
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/DockWidget.h>
#include <kddockwidgets/core/Group.h>
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

#include <QtCore/QTimer>
#include <QtCore/QtTranslation>
#include <QtWidgets/QMessageBox>

// Independent of the KDDockWidgets file format version number.
const u32 DEBUGGER_LAYOUT_FILE_VERSION = 1;

enum DefaultDockGroup
{
	ROOT = -1,
	TOP_RIGHT = 0,
	TOP_LEFT = 1,
	BOTTOM = 2,
	COUNT = 3
};

struct DefaultDockGroupDescription
{
	KDDockWidgets::Location location;
	DefaultDockGroup parent;
};

static const std::vector<DefaultDockGroupDescription> DEFAULT_DOCK_GROUPS = {
	/* [DefaultDockGroup::TOP_RIGHT] = */ {KDDockWidgets::Location_OnRight, DefaultDockGroup::ROOT},
	/* [DefaultDockGroup::TOP_LEFT]  = */ {KDDockWidgets::Location_OnLeft, DefaultDockGroup::TOP_RIGHT},
	/* [DefaultDockGroup::BOTTOM]    = */ {KDDockWidgets::Location_OnBottom, DefaultDockGroup::TOP_RIGHT},
};

struct DefaultDockWidgetDescription
{
	const char* type;
	DebuggerWidget* (*create_widget)(DebugInterface& cpu);
	QString title;
	DefaultDockGroup group;
};

#define DOCK_TYPE(type) #type, [](DebugInterface& cpu) -> DebuggerWidget* { return new type(cpu); }

static const std::vector<DefaultDockWidgetDescription> DEFAULT_DOCK_WIDGETS = {
	/* DefaultDockGroup::TOP_RIGHT */
	{DOCK_TYPE(DisassemblyWidget), QT_TRANSLATE_NOOP("DockWidget", "Disassembly"), DefaultDockGroup::TOP_RIGHT},
	/* DefaultDockGroup::BOTTOM */
	{DOCK_TYPE(MemoryViewWidget), QT_TRANSLATE_NOOP("DockWidget", "Memory"), DefaultDockGroup::BOTTOM},
	{DOCK_TYPE(BreakpointWidget), QT_TRANSLATE_NOOP("DockWidget", "Breakpoints"), DefaultDockGroup::BOTTOM},
	{DOCK_TYPE(ThreadWidget), QT_TRANSLATE_NOOP("DockWidget", "Threads"), DefaultDockGroup::BOTTOM},
	{DOCK_TYPE(StackWidget), QT_TRANSLATE_NOOP("DockWidget", "Stack"), DefaultDockGroup::BOTTOM},
	{DOCK_TYPE(SavedAddressesWidget), QT_TRANSLATE_NOOP("DockWidget", "Saved Addresses"), DefaultDockGroup::BOTTOM},
	{DOCK_TYPE(GlobalVariableTreeWidget), QT_TRANSLATE_NOOP("DockWidget", "Globals"), DefaultDockGroup::BOTTOM},
	{DOCK_TYPE(LocalVariableTreeWidget), QT_TRANSLATE_NOOP("DockWidget", "Locals"), DefaultDockGroup::BOTTOM},
	{DOCK_TYPE(ParameterVariableTreeWidget), QT_TRANSLATE_NOOP("DockWidget", "Parameters"), DefaultDockGroup::BOTTOM},
	/* DefaultDockGroup::TOP_LEFT */
	{DOCK_TYPE(RegisterWidget), QT_TRANSLATE_NOOP("DockWidget", "Registers"), DefaultDockGroup::TOP_LEFT},
	{DOCK_TYPE(FunctionTreeWidget), QT_TRANSLATE_NOOP("DockWidget", "Functions"), DefaultDockGroup::TOP_LEFT},
	{DOCK_TYPE(MemorySearchWidget), QT_TRANSLATE_NOOP("DockWidget", "Memory Search"), DefaultDockGroup::TOP_LEFT},
};

#undef DOCK_TYPE

DockManager::DockManager(DebuggerWindow* window)
	: QObject(window)
	, m_window(window)
{
	loadLayouts();
}

DockManager::~DockManager()
{
	saveLayout(m_current_layout);

	for (Layout& layout : m_layouts)
		for (QPointer<DebuggerWidget> widget : layout.widgets)
			delete widget;
}

void DockManager::configureDockingSystem()
{
	KDDockWidgets::Config::self().setFlags(
		KDDockWidgets::Config::Flag_HideTitleBarWhenTabsVisible |
		KDDockWidgets::Config::Flag_AlwaysShowTabs |
		KDDockWidgets::Config::Flag_AllowReorderTabs |
		KDDockWidgets::Config::Flag_TabsHaveCloseButton |
		KDDockWidgets::Config::Flag_TitleBarIsFocusable);
}

s32 DockManager::createLayout(std::string name, BreakPointCpu cpu, LayoutCreationMode mode)
{
	s32 layout_index = static_cast<s32>(m_layouts.size());

	Layout& layout = m_layouts.emplace_back();
	layout.name = std::move(name);
	layout.cpu = cpu;

	DebugInterface& debug_interface = r5900Debug;
	if (cpu == BREAKPOINT_IOP)
		debug_interface = r3000Debug;

	switch (mode)
	{
		case DEFAULT_LAYOUT:
		{
			for (size_t i = 0; i < DEFAULT_DOCK_WIDGETS.size(); i++)
			{
				const DefaultDockWidgetDescription& desc = DEFAULT_DOCK_WIDGETS[i];

				DebuggerWidget* widget = desc.create_widget(debug_interface);
				widget->setWidgetDescriptionIndex(i);
				widget->uniqueName = desc.title;
				layout.widgets.emplace_back(widget);
			}
			break;
		}
		case CLONE_LAYOUT:
		{
			// TODO
			break;
		}
		case BLANK_LAYOUT:
		{
			// Nothing to do.
			break;
		}
	}

	return layout_index;
}

bool DockManager::deleteLayout(s32 layout_index)
{
	if (layout_index < 0 || layout_index >= static_cast<s32>(m_layouts.size()))
		return false;

	if (layout_index == m_current_layout)
	{
		int other_layout = -1;
		if (layout_index + 1 <= static_cast<s32>(m_layouts.size()))
			other_layout = layout_index + 1;
		else if (layout_index > 0)
			other_layout = layout_index - 1;

		switchToLayout(other_layout);
	}

	Layout& layout = m_layouts[layout_index];
	for (QPointer<DebuggerWidget> widget : layout.widgets)
	{
		delete widget;
	}

	m_layouts.erase(m_layouts.begin() + layout_index);

	if (m_current_layout > layout_index)
		m_current_layout--;

	return true;
}

void DockManager::switchToLayout(s32 layout_index)
{
	if (layout_index == m_current_layout || layout_index >= static_cast<s32>(m_layouts.size()))
		return;

	if (m_current_layout > -1)
	{
		freezeLayout(m_layouts[m_current_layout]);
		saveLayout(m_current_layout);
	}

	m_current_layout = layout_index;

	if (m_current_layout > -1)
	{
		thawLayout(m_layouts[m_current_layout]);
	}
}

void DockManager::loadLayouts()
{
	m_layouts.clear();

	// Load the layouts.
	FileSystem::FindResultsArray files;
	FileSystem::FindFiles(
		EmuFolders::DebuggerLayouts.c_str(),
		"*.json",
		FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES,
		&files);

	for (const FILESYSTEM_FIND_DATA& ffd : files)
		loadLayout(ffd.FileName);

	if (m_layouts.empty())
		setupDefaultLayouts();
}

s32 DockManager::loadLayout(const std::string& path)
{
	s32 layout_index = 0; // = createLayout(name);

	return layout_index;
}

bool DockManager::saveLayouts()
{
	for (s32 i = 0; i < static_cast<s32>(m_layouts.size()); i++)
		if (!saveLayout(i))
			return false;

	return true;
}

bool DockManager::saveLayout(s32 layout_index)
{
	if (layout_index < 0)
		return false;

	Layout& layout = m_layouts[layout_index];

	// Serialize the layout as JSON.
	rapidjson::Document json(rapidjson::kObjectType);
	rapidjson::Document geometry;

	json.AddMember("format", "PCSX2 Debugger User Interface Layout", json.GetAllocator());
	json.AddMember("version", DEBUGGER_LAYOUT_FILE_VERSION, json.GetAllocator());

	rapidjson::Value name;
	name.SetString(layout.name.c_str(), strlen(layout.name.c_str()));
	json.AddMember("name", name, json.GetAllocator());

	rapidjson::Value widgets(rapidjson::kArrayType);
	for (QPointer<DebuggerWidget> widget : layout.widgets)
	{
		rapidjson::Value object(rapidjson::kObjectType);

		JsonValueWrapper wrapper(object, json.GetAllocator());
		widget->toJson(wrapper);

		widgets.PushBack(object, json.GetAllocator());
	}
	json.AddMember("widgets", widgets, json.GetAllocator());

	if (!layout.geometry.isEmpty() && !geometry.Parse(layout.geometry).HasParseError())
		json.AddMember("geometry", geometry, json.GetAllocator());

	rapidjson::StringBuffer string_buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(string_buffer);
	json.Accept(writer);

	// Write out the JSON to a file.
	std::string temp_file_path = Path::Combine(EmuFolders::DebuggerLayouts, layout.name + ".tmp");

	if (!FileSystem::WriteStringToFile(temp_file_path.c_str(), string_buffer.GetString()))
		return false;

	// Generate a name if a file doesn't already exist.
	if (layout.layout_file_path.empty())
		layout.layout_file_path = Path::Combine(EmuFolders::DebuggerLayouts, layout.name + ".json");

	return FileSystem::MoveFilePath(temp_file_path.c_str(), layout.layout_file_path.c_str());
}

void DockManager::renameLayout(s32 layout_index, std::string new_name)
{
}

void DockManager::setupDefaultLayouts()
{
	switchToLayout(-1);

	m_layouts.clear();

	createLayout("R5900 (EE)", BREAKPOINT_EE, DEFAULT_LAYOUT);
	createLayout("R3000 (IOP)", BREAKPOINT_IOP, DEFAULT_LAYOUT);

	switchToLayout(0);
}

void DockManager::createWindowsMenu(QMenu* menu)
{
	menu->clear();

	QAction* reset_all_layouts_action = new QAction(tr("Reset All Layouts"), menu);
	connect(reset_all_layouts_action, &QAction::triggered, [this]() {
		QMessageBox::StandardButton result = QMessageBox::question(
			m_window, tr("Confirmation"), tr("Are you sure you want to reset all layouts?"));

		if (result == QMessageBox::Yes)
			setupDefaultLayouts();
	});
	menu->addAction(reset_all_layouts_action);

	menu->addSeparator();

	std::vector<const DefaultDockWidgetDescription*> descriptions;
	for (const DefaultDockWidgetDescription& desc : DEFAULT_DOCK_WIDGETS)
		descriptions.emplace_back(&desc);

	std::sort(descriptions.begin(), descriptions.end(),
		[](const DefaultDockWidgetDescription* lhs, const DefaultDockWidgetDescription* rhs) {
			return lhs->title < rhs->title;
		});

	for (const DefaultDockWidgetDescription* desc : descriptions)
	{
		QAction* action = new QAction(menu);
		action->setText(QCoreApplication::translate("DockWidget", desc->title.toStdString().c_str()));
		action->setCheckable(true);
		action->setChecked(true);
		menu->addAction(action);
	}
}

QWidget* DockManager::createLayoutSwitcher(QWidget* menu_bar)
{
	QWidget* container = new QWidget;
	QHBoxLayout* layout = new QHBoxLayout;
	container->setLayout(layout);

	layout->setContentsMargins(0, 2, 0, 0);

	QWidget* menu_wrapper = new QWidget;
	menu_wrapper->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	layout->addWidget(menu_wrapper);

	QHBoxLayout* menu_layout = new QHBoxLayout;
	menu_layout->setContentsMargins(0, 4, 0, 4);
	menu_wrapper->setLayout(menu_layout);

	menu_layout->addWidget(menu_bar);

	m_switcher = new QTabBar;
	m_switcher->setContentsMargins(0, 0, 0, 0);
	m_switcher->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	m_switcher->setContextMenuPolicy(Qt::CustomContextMenu);
	m_switcher->setMovable(true);
	layout->addWidget(m_switcher);

	updateLayoutSwitcher();

	connect(m_switcher, &QTabBar::tabMoved, this, &DockManager::layoutSwitcherTabMoved);
	connect(m_switcher, &QTabBar::customContextMenuRequested, this, &DockManager::layoutSwitcherContextMenu);

	QWidget* spacer = new QWidget;
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	layout->addWidget(spacer);

	return container;
}

void DockManager::updateLayoutSwitcher()
{
	if (!m_switcher)
		return;

	disconnect(m_tab_connection);

	for (int i = m_switcher->count(); i > 0; i--)
		m_switcher->removeTab(i - 1);

	for (Layout& layout : m_layouts)
		layout.switcher_tab_index = m_switcher->addTab(QString::fromStdString(layout.name));

	m_plus_tab_index = m_switcher->addTab("+");
	m_current_tab_index = m_current_layout;

	m_switcher->setCurrentIndex(m_current_layout);

	m_tab_connection = connect(m_switcher, &QTabBar::currentChanged, this, &DockManager::layoutSwitcherTabChanged);
}

void DockManager::layoutSwitcherTabChanged(s32 index)
{
	if (index == m_plus_tab_index)
	{
		if (m_current_tab_index >= 0 && m_current_tab_index < m_plus_tab_index)
			m_switcher->setCurrentIndex(m_current_tab_index);

		LayoutEditorDialog* dialog = new LayoutEditorDialog(m_window);
		if (dialog->exec() == QDialog::Accepted)
		{
			s32 layout_index = createLayout(dialog->name(), dialog->cpu(), dialog->initial_state());
			switchToLayout(layout_index);
			updateLayoutSwitcher();
		}
	}
	else
	{
		switchToLayout(index);
		m_current_tab_index = index;
	}
}

void DockManager::layoutSwitcherTabMoved(s32 from, s32 to)
{
	updateLayoutSwitcher();
}

void DockManager::layoutSwitcherContextMenu(QPoint pos)
{
	s32 layout_index = m_switcher->tabAt(pos);
	if (layout_index < 0)
		return;

	QMenu* menu = new QMenu(tr("Layout Switcher Context Menu"), m_switcher);

	QAction* edit_action = new QAction(tr("Edit Layout"), menu);
	connect(edit_action, &QAction::triggered, [this, layout_index]() {
		if (layout_index < 0 || layout_index >= static_cast<s32>(m_layouts.size()))
			return;

		Layout& layout = m_layouts[layout_index];

		LayoutEditorDialog* dialog = new LayoutEditorDialog(layout.name, layout.cpu, m_window);

		if (dialog->exec() == QDialog::Accepted)
		{
			layout.name = dialog->name();
			layout.cpu = dialog->cpu();
			updateLayoutSwitcher();
		}
	});
	menu->addAction(edit_action);

	QAction* delete_action = new QAction(tr("Delete Layout"), menu);
	connect(delete_action, &QAction::triggered, [this, layout_index]() {
		deleteLayout(layout_index);
		updateLayoutSwitcher();
	});
	menu->addAction(delete_action);

	menu->popup(m_switcher->mapToGlobal(pos));
}

void DockManager::freezeLayout(Layout& layout)
{
	pxAssertRel(!layout.is_frozen, "DockManager::freezeLayout called on already frozen layout.");
	layout.is_frozen = true;

	KDDockWidgets::LayoutSaver saver(KDDockWidgets::RestoreOption_RelativeToMainWindow);

	// Store the geometry of all the dock widgets as JSON.
	layout.geometry = saver.serializeLayout();

	// Delete the dock widgets.
	for (KDDockWidgets::Core::DockWidget* dock : KDDockWidgets::DockRegistry::self()->dockwidgets())
	{
		// Make sure the dock widget releases ownership of its content.
		KDDockWidgets::QtWidgets::DockWidget* view =
			static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock->view());
		view->setWidget(new QWidget());

		delete dock;
	}
}

void DockManager::thawLayout(Layout& layout)
{
	pxAssertRel(layout.is_frozen, "DockManager::thawLayout called on already thawed layout.");
	layout.is_frozen = false;

	KDDockWidgets::LayoutSaver saver(KDDockWidgets::RestoreOption_RelativeToMainWindow);

	if (layout.geometry.isEmpty())
	{
		// This is a newly created layout with no geometry information.
		populateDefaultLayout();
		return;
	}

	// Create any dock widgets that were previously frozen during this session.
	for (QPointer<DebuggerWidget> widget : layout.widgets)
	{
		KDDockWidgets::QtWidgets::DockWidget* view = new KDDockWidgets::QtWidgets::DockWidget(widget->uniqueName);
		view->setWidget(widget);
		m_window->addDockWidget(view, KDDockWidgets::Location_OnBottom);
	}

	// Restore the geometry of the dock widgets we just recreated.
	if (!saver.restoreLayout(layout.geometry))
	{
		for (KDDockWidgets::Core::DockWidget* dock : KDDockWidgets::DockRegistry::self()->dockwidgets())
		{
			// Make sure the dock widget releases ownership of its content.
			KDDockWidgets::QtWidgets::DockWidget* view =
				static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock->view());
			view->setWidget(new QWidget());

			delete dock;
		}

		// We failed to restore the geometry, so just setup the default layout.
		populateDefaultLayout();
	}
}

KDDockWidgets::Core::DockWidget* DockManager::createDockWidget(const QString& name)
{

	return nullptr;
}

void DockManager::populateDefaultLayout()
{
	if (m_current_layout < 0)
		return;

	Layout& layout = m_layouts[m_current_layout];

	KDDockWidgets::QtWidgets::DockWidget* groups[DefaultDockGroup::COUNT] = {};

	for (QPointer<DebuggerWidget> widget : layout.widgets)
	{
		if (widget->widgetDescriptionIndex() >= DEFAULT_DOCK_WIDGETS.size())
			continue;

		const DefaultDockWidgetDescription& desc = DEFAULT_DOCK_WIDGETS[widget->widgetDescriptionIndex()];
		const DefaultDockGroupDescription& group = DEFAULT_DOCK_GROUPS[desc.group];

		KDDockWidgets::QtWidgets::DockWidget* dock = new KDDockWidgets::QtWidgets::DockWidget(desc.title);
		dock->setWidget(widget);

		if (!groups[desc.group])
		{
			KDDockWidgets::QtWidgets::DockWidget* parent = nullptr;
			if (group.parent != DefaultDockGroup::ROOT)
				parent = groups[group.parent];

			m_window->addDockWidget(dock, group.location, parent);

			groups[desc.group] = dock;
		}
		else
		{
			groups[desc.group]->addDockWidgetAsTab(dock);
		}
	}

	for (KDDockWidgets::Core::Group* group : KDDockWidgets::DockRegistry::self()->groups())
		group->setCurrentTabIndex(0);
}
