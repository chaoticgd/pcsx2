// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockLayout.h"

#include "Debugger/DebuggerWidget.h"
#include "Debugger/DebuggerWindow.h"
#include "Debugger/JsonValueWrapper.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"

#include <kddockwidgets/Config.h>
#include <kddockwidgets/DockWidget.h>
#include <kddockwidgets/LayoutSaver.h>
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/DockWidget.h>
#include <kddockwidgets/core/Group.h>
#include <kddockwidgets/core/Layout.h>
#include <kddockwidgets/core/ViewFactory.h>
#include <kddockwidgets/qtwidgets/Group.h>
#include <kddockwidgets/qtwidgets/MainWindow.h>

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

const char* DEBUGGER_LAYOUT_FILE_FORMAT = "PCSX2 Debugger User Interface Layout";

// Increment this version number whenever the JSON format changes (excluding the
// contents of the geometry object which is managed by KDDockWidgets).
const u32 DEBUGGER_LAYOUT_FILE_VERSION = 1;

DockLayout::DockLayout(
	std::string name,
	BreakPointCpu cpu,
	bool is_default,
	const DockTables::DefaultDockLayout& default_layout,
	DockLayout::Index index)
	: m_name(name)
	, m_cpu(cpu)
	, m_is_default(is_default)
	, m_base_layout(default_layout.name)
{
	DebugInterface& debug_interface = DebugInterface::get(cpu);

	for (size_t i = 0; i < default_layout.widgets.size(); i++)
	{
		auto iterator = DockTables::DEBUGGER_WIDGETS.find(default_layout.widgets[i].type);
		pxAssertRel(iterator != DockTables::DEBUGGER_WIDGETS.end(), "Invalid default layout.");
		const DockTables::DebuggerWidgetDescription& dock_description = iterator->second;

		DebuggerWidget* widget = dock_description.create_widget(debug_interface);
		m_widgets.emplace(default_layout.widgets[i].type, widget);
	}

	save(index);
}

DockLayout::DockLayout(
	std::string name,
	BreakPointCpu cpu,
	bool is_default,
	DockLayout::Index index)
	: m_name(name)
	, m_cpu(cpu)
	, m_is_default(is_default)
{
	save(index);
}

DockLayout::DockLayout(
	std::string name,
	BreakPointCpu cpu,
	bool is_default,
	const DockLayout& layout_to_clone,
	DockLayout::Index index)
	: m_name(name)
	, m_cpu(cpu)
	, m_is_default(is_default)
{
	for (const auto& [unique_name, widget_to_clone] : layout_to_clone.m_widgets)
	{
		auto widget_description = DockTables::DEBUGGER_WIDGETS.find(widget_to_clone->metaObject()->className());
		if (widget_description == DockTables::DEBUGGER_WIDGETS.end())
			continue;

		DebuggerWidget* new_widget = widget_description->second.create_widget(DebugInterface::get(cpu));
		m_widgets.emplace(unique_name, new_widget);
	}

	m_geometry = layout_to_clone.m_geometry;
	m_base_layout = layout_to_clone.m_base_layout;

	save(index);
}

DockLayout::DockLayout(
	const std::string& path,
	DockLayout::LoadResult& result,
	DockLayout::Index& index_last_session,
	DockLayout::Index index)
{
	load(path, result, index_last_session);
}

DockLayout::~DockLayout()
{
	for (auto& [unique_name, widget] : m_widgets)
	{
		pxAssert(widget.get());

		delete widget;
	}
}

const std::string& DockLayout::name() const
{
	return m_name;
}

void DockLayout::setName(std::string name)
{
	m_name = std::move(name);
}

BreakPointCpu DockLayout::cpu() const
{
	return m_cpu;
}

bool DockLayout::isDefault() const
{
	return m_is_default;
}

void DockLayout::setCpu(BreakPointCpu cpu)
{
	m_cpu = cpu;

	for (auto& [unique_name, widget] : m_widgets)
	{
		pxAssert(widget.get());

		if (!widget->setCpu(DebugInterface::get(cpu)))
			recreateDebuggerWidget(unique_name);
	}
}

void DockLayout::freeze()
{
	pxAssert(!m_is_frozen);
	m_is_frozen = true;

	KDDockWidgets::LayoutSaver saver(KDDockWidgets::RestoreOption_RelativeToMainWindow);

	// Store the geometry of all the dock widgets as JSON.
	m_geometry = saver.serializeLayout();


	// Delete the dock widgets.
	for (KDDockWidgets::Core::DockWidget* dock : KDDockWidgets::DockRegistry::self()->dockwidgets())
	{
		// Make sure the dock widget releases ownership of its content.
		auto view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock->view());
		view->setWidget(new QWidget());

		delete dock;
	}
}

void DockLayout::thaw(DebuggerWindow* window)
{
	pxAssert(m_is_frozen);
	m_is_frozen = false;

	KDDockWidgets::LayoutSaver saver(KDDockWidgets::RestoreOption_RelativeToMainWindow);

	if (m_geometry.isEmpty())
	{
		// This is a newly created layout with no geometry information.
		populateDefaultLayout(window);
		retranslateDockWidgets();
		return;
	}

	// Create any dock widgets that were previously frozen during this session.
	for (auto& [unique_name, widget] : m_widgets)
	{
		pxAssert(widget.get());

		auto view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(
			KDDockWidgets::Config::self().viewFactory()->createDockWidget(unique_name));
		view->setWidget(widget);
		window->addDockWidget(view, KDDockWidgets::Location_OnBottom);
	}

	m_restoring_layout = true;

	// Restore the geometry of the dock widgets we just recreated.
	if (!saver.restoreLayout(m_geometry))
	{
		m_restoring_layout = false;

		for (KDDockWidgets::Core::DockWidget* dock : KDDockWidgets::DockRegistry::self()->dockwidgets())
		{
			// Make sure the dock widget releases ownership of its content.
			auto view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock->view());
			view->setWidget(new QWidget());

			delete dock;
		}

		// We failed to restore the geometry, so just setup the default layout.
		populateDefaultLayout(window);
	}

	m_restoring_layout = false;

	retranslateDockWidgets();
}

void DockLayout::retranslateDockWidgets()
{
	for (KDDockWidgets::Core::DockWidget* widget : KDDockWidgets::DockRegistry::self()->dockwidgets())
		retranslateDockWidget(widget);
}

void DockLayout::retranslateDockWidget(KDDockWidgets::Core::DockWidget* dock_widget)
{
	pxAssert(!m_is_frozen);

	auto widget_iterator = m_widgets.find(dock_widget->uniqueName());
	if (widget_iterator == m_widgets.end())
		return;

	DebuggerWidget* widget = widget_iterator->second.get();
	if (!widget)
		return;

	auto description_iterator = DockTables::DEBUGGER_WIDGETS.find(dock_widget->uniqueName());
	if (description_iterator == DockTables::DEBUGGER_WIDGETS.end())
		return;

	const DockTables::DebuggerWidgetDescription& description = description_iterator->second;

	QString translated_title = QCoreApplication::translate("DebuggerWidget", description.title);
	std::optional<BreakPointCpu> cpu_override = widget->cpuOverride();

	if (cpu_override.has_value())
	{
		const char* cpu_name = DebugInterface::cpuName(*cpu_override);
		dock_widget->setTitle(QString("%1 (%2)").arg(translated_title).arg(cpu_name));
	}
	else
	{
		dock_widget->setTitle(std::move(translated_title));
	}
}

void DockLayout::dockWidgetClosed(KDDockWidgets::Core::DockWidget* dock_widget)
{
	// The LayoutSaver class will close a bunch of dock widgets. We only want to
	// delete the dock widgets when they're being closed by the user.
	if (m_restoring_layout)
		return;

	auto debugger_widget_iterator = m_widgets.find(dock_widget->uniqueName());
	if (debugger_widget_iterator == m_widgets.end())
		return;

	KDDockWidgets::Vector<QString> names{dock_widget->uniqueName()};
	KDDockWidgets::Vector<KDDockWidgets::Core::DockWidget*> dock_widgets =
		KDDockWidgets::DockRegistry::self()->dockWidgets(names);

	m_widgets.erase(debugger_widget_iterator);
	dock_widgets[0]->deleteLater();
}

bool DockLayout::hasDebuggerWidget(QString unique_name)
{
	return m_widgets.find(unique_name) != m_widgets.end();
}

void DockLayout::toggleDebuggerWidget(QString unique_name, DebuggerWindow* window)
{
	auto debugger_widget_iterator = m_widgets.find(unique_name);

	KDDockWidgets::Vector<QString> names{unique_name};
	KDDockWidgets::Vector<KDDockWidgets::Core::DockWidget*> dock_widgets =
		KDDockWidgets::DockRegistry::self()->dockWidgets(names);

	if (debugger_widget_iterator == m_widgets.end())
	{
		// Create the dock widget.
		if (!dock_widgets.empty())
			return;

		auto description_iterator = DockTables::DEBUGGER_WIDGETS.find(unique_name);
		if (description_iterator == DockTables::DEBUGGER_WIDGETS.end())
			return;

		const DockTables::DebuggerWidgetDescription& description = description_iterator->second;

		DebuggerWidget* widget = description.create_widget(DebugInterface::get(m_cpu));
		m_widgets.emplace(unique_name, widget);

		auto view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(
			KDDockWidgets::Config::self().viewFactory()->createDockWidget(unique_name));
		view->setWidget(widget);

		KDDockWidgets::Core::DockWidget* controller = view->asController<KDDockWidgets::Core::DockWidget>();
		if (!controller)
		{
			delete view;
			return;
		}

		DockUtils::insertDockWidgetAtPreferredLocation(controller, description.preferred_location, window);
		retranslateDockWidget(controller);
	}
	else
	{
		// Delete the dock widget.
		if (dock_widgets.size() != 1)
			return;

		m_widgets.erase(debugger_widget_iterator);
		delete dock_widgets[0];
	}
}

void DockLayout::recreateDebuggerWidget(QString unique_name)
{
	pxAssert(!m_is_frozen);

	auto debugger_widget_iterator = m_widgets.find(unique_name);
	if (debugger_widget_iterator == m_widgets.end())
		return;

	KDDockWidgets::Vector<QString> names{unique_name};
	KDDockWidgets::Vector<KDDockWidgets::Core::DockWidget*> dock_widgets =
		KDDockWidgets::DockRegistry::self()->dockWidgets(names);
	if (dock_widgets.size() != 1)
		return;

	auto description_iterator = DockTables::DEBUGGER_WIDGETS.find(unique_name);
	if (description_iterator == DockTables::DEBUGGER_WIDGETS.end())
		return;

	DebuggerWidget* old_debugger_widget = debugger_widget_iterator->second;
	KDDockWidgets::Core::DockWidget* dock_controller = dock_widgets[0];
	const DockTables::DebuggerWidgetDescription& description = description_iterator->second;

	auto dock_view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock_controller->view());
	pxAssert(dock_view->widget() == old_debugger_widget);

	DebuggerWidget* new_debugger_widget = description.create_widget(DebugInterface::get(m_cpu));
	new_debugger_widget->setCpuOverride(old_debugger_widget->cpuOverride());
	debugger_widget_iterator->second = new_debugger_widget;

	dock_view->setWidget(new_debugger_widget);

	delete old_debugger_widget;
}

void DockLayout::deleteFile()
{
	if (m_layout_file_path.empty())
		return;

	FileSystem::DeleteFilePath(m_layout_file_path.c_str());
}

bool DockLayout::save(DockLayout::Index layout_index)
{
	// Serialize the layout as JSON.
	rapidjson::Document json(rapidjson::kObjectType);
	rapidjson::Document geometry;

	const char* cpu_name = DebugInterface::cpuName(m_cpu);
	const std::string& default_layouts_hash = DockTables::hashDefaultLayouts();

	rapidjson::Value format;
	format.SetString(DEBUGGER_LAYOUT_FILE_FORMAT, strlen(DEBUGGER_LAYOUT_FILE_FORMAT));
	json.AddMember("format", format, json.GetAllocator());

	rapidjson::Value version(rapidjson::kArrayType);
	version.PushBack(DEBUGGER_LAYOUT_FILE_VERSION, json.GetAllocator());
	rapidjson::Value minor_version;
	minor_version.SetString(default_layouts_hash.c_str(), default_layouts_hash.size());
	version.PushBack(minor_version, json.GetAllocator());
	json.AddMember("version", version, json.GetAllocator());

	json.AddMember("name", rapidjson::Value().SetString(m_name.c_str(), m_name.size()), json.GetAllocator());
	json.AddMember("target", rapidjson::Value().SetString(cpu_name, strlen(cpu_name)), json.GetAllocator());
	json.AddMember("index", static_cast<int>(layout_index), json.GetAllocator());
	json.AddMember("isDefault", m_is_default, json.GetAllocator());

	rapidjson::Value widgets(rapidjson::kArrayType);
	for (auto& [unique_name, widget] : m_widgets)
	{
		pxAssert(widget.get());

		rapidjson::Value object(rapidjson::kObjectType);

		std::string name_str = unique_name.toStdString();
		rapidjson::Value name;
		name.SetString(name_str.c_str(), name_str.size(), json.GetAllocator());
		object.AddMember("uniqueName", name, json.GetAllocator());

		const char* type_str = widget->metaObject()->className();
		rapidjson::Value type;
		type.SetString(type_str, strlen(type_str), json.GetAllocator());
		object.AddMember("type", type, json.GetAllocator());

		JsonValueWrapper wrapper(object, json.GetAllocator());
		widget->toJson(wrapper);

		widgets.PushBack(object, json.GetAllocator());
	}
	json.AddMember("widgets", widgets, json.GetAllocator());

	if (!m_geometry.isEmpty() && !geometry.Parse(m_geometry).HasParseError())
		json.AddMember("geometry", geometry, json.GetAllocator());

	if (!m_base_layout.empty())
	{
		rapidjson::Value base_layout;
		rapidjson::Value().SetString(m_base_layout.c_str(), m_base_layout.size());
		json.AddMember("baseLayout", base_layout, json.GetAllocator());
	}

	rapidjson::StringBuffer string_buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(string_buffer);
	json.Accept(writer);

	std::string safe_name = Path::SanitizeFileName(m_name);

	// Write out the JSON to a file.
	std::string temp_file_path = Path::Combine(EmuFolders::DebuggerLayouts, safe_name + ".tmp");

	if (!FileSystem::WriteStringToFile(temp_file_path.c_str(), string_buffer.GetString()))
		return false;

	std::string file_path = Path::Combine(EmuFolders::DebuggerLayouts, safe_name + ".json");

	if (!FileSystem::RenamePath(temp_file_path.c_str(), file_path.c_str()))
	{
		FileSystem::DeleteFilePath(temp_file_path.c_str());
		return false;
	}

	// If the layout has been renamed we need to delete the old file.
	if (file_path != m_layout_file_path)
		FileSystem::DeleteFilePath(m_layout_file_path.c_str());

	m_layout_file_path = std::move(file_path);

	return true;
}

void DockLayout::load(
	const std::string& path,
	LoadResult& result,
	DockLayout::Index& index_last_session)
{
	pxAssert(m_is_frozen);

	result = DockLayout::SUCCESS;

	std::optional<std::string> text = FileSystem::ReadFileToString(path.c_str());
	if (!text.has_value())
	{
		Console.Warning("DockLayout: Failed to open layout file '%s'.", path.c_str());
		result = FAILED;
		return;
	}

	rapidjson::Document json;
	if (json.Parse(text->c_str()).HasParseError() || !json.IsObject())
	{
		Console.Warning("DockLayout: Failed to parse layout file '%s' as JSON.", path.c_str());
		result = FAILED;
		return;
	}

	auto format = json.FindMember("format");
	if (format == json.MemberEnd() ||
		!format->value.IsString() ||
		strcmp(format->value.GetString(), DEBUGGER_LAYOUT_FILE_FORMAT) != 0)
	{
		Console.Warning("DockLayout: Layout file '%s' has invalid format property.", path.c_str());
		result = FAILED;
		return;
	}

	auto version = json.FindMember("version");
	if (version == json.MemberEnd() ||
		!version->value.IsArray() ||
		version->value.Size() != 2 ||
		!version->value[0].IsInt() ||
		!version->value[1].IsString())
	{
		Console.Warning("DockLayout: Layout file '%s' has invalid version property.", path.c_str());
		result = FAILED;
		return;
	}

	int version_major = version->value[0].GetInt();
	if (version_major != DEBUGGER_LAYOUT_FILE_VERSION)
	{
		result = FAILED;
		return;
	}

	const char* version_minor = version->value[1].GetString();
	if (strcmp(version_minor, DockTables::hashDefaultLayouts().c_str()) != 0)
		result = MINOR_VERSION_MISMATCH;

	auto name = json.FindMember("name");
	if (name != json.MemberEnd() && name->value.IsString())
		m_name = name->value.GetString();
	else
		m_name = QCoreApplication::translate("DockLayout", "Unnamed").toStdString();

	auto target = json.FindMember("target");
	m_cpu = BREAKPOINT_EE;
	if (target != json.MemberEnd() && target->value.IsString())
	{
		for (BreakPointCpu cpu : DEBUG_CPUS)
			if (strcmp(DebugInterface::cpuName(cpu), target->value.GetString()) == 0)
				m_cpu = cpu;
	}

	auto index = json.FindMember("index");
	if (index != json.MemberEnd() && index->value.IsInt())
		index_last_session = index->value.GetInt();

	auto is_default = json.FindMember("isDefault");
	if (is_default != json.MemberEnd() && is_default->value.IsBool())
		m_is_default = is_default->value.GetBool();

	auto widgets = json.FindMember("widgets");
	if (widgets != json.MemberEnd() && widgets->value.IsArray())
	{
		for (rapidjson::Value& object : widgets->value.GetArray())
		{
			auto unique_name = object.FindMember("uniqueName");
			if (unique_name == object.MemberEnd() || !unique_name->value.IsString())
				continue;

			auto type = object.FindMember("type");
			if (type == object.MemberEnd() || !type->value.IsString())
				continue;

			auto description = DockTables::DEBUGGER_WIDGETS.find(type->value.GetString());
			if (description == DockTables::DEBUGGER_WIDGETS.end())
				continue;

			DebuggerWidget* widget = description->second.create_widget(DebugInterface::get(m_cpu));

			JsonValueWrapper wrapper(object, json.GetAllocator());
			if (!widget->fromJson(wrapper))
			{
				delete widget;
				continue;
			}

			m_widgets.emplace(unique_name->value.GetString(), widget);
		}
	}

	auto geometry = json.FindMember("geometry");
	if (geometry != json.MemberEnd() && geometry->value.IsObject())
	{
		rapidjson::StringBuffer string_buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(string_buffer);
		geometry->value.Accept(writer);

		m_geometry = QByteArray(string_buffer.GetString(), string_buffer.GetSize());
	}

	auto base_layout = json.FindMember("baseLayout");
	if (base_layout != json.MemberEnd() && base_layout->value.IsString())
		m_base_layout = base_layout->value.GetString();

	m_layout_file_path = path;
}

void DockLayout::populateDefaultLayout(DebuggerWindow* window)
{
	pxAssert(!m_is_frozen);

	if (m_base_layout.empty())
		return;

	const DockTables::DefaultDockLayout* layout = nullptr;
	for (const DockTables::DefaultDockLayout& default_layout : DockTables::DEFAULT_DOCK_LAYOUTS)
		if (default_layout.name == m_base_layout)
			layout = &default_layout;

	if (!layout)
		return;

	std::vector<KDDockWidgets::QtWidgets::DockWidget*> groups(layout->groups.size(), nullptr);

	for (const DockTables::DefaultDockWidgetDescription& dock_description : layout->widgets)
	{
		const DockTables::DefaultDockGroupDescription& group = layout->groups[static_cast<u32>(dock_description.group)];

		auto widget_iterator = m_widgets.find(dock_description.type);
		if (widget_iterator == m_widgets.end())
			continue;

		const QString& unique_name = widget_iterator->first;
		DebuggerWidget* widget = widget_iterator->second;

		auto view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(
			KDDockWidgets::Config::self().viewFactory()->createDockWidget(unique_name));
		view->setWidget(widget);

		if (!groups[static_cast<u32>(dock_description.group)])
		{
			KDDockWidgets::QtWidgets::DockWidget* parent = nullptr;
			if (group.parent != DockTables::DefaultDockGroup::ROOT)
				parent = groups[static_cast<u32>(group.parent)];

			window->addDockWidget(view, group.location, parent);

			groups[static_cast<u32>(dock_description.group)] = view;
		}
		else
		{
			groups[static_cast<u32>(dock_description.group)]->addDockWidgetAsTab(view);
		}
	}

	for (KDDockWidgets::Core::Group* group : KDDockWidgets::DockRegistry::self()->groups())
		group->setCurrentTabIndex(0);
}
