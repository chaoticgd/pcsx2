// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockLayoutManager.h"

#include "DebuggerWindow.h"
#include "DisassemblyWidget.h"
#include "MemoryViewWidget.h"
#include "RegisterWidget.h"
#include "SymbolTree/SymbolTreeWidgets.h"

#define FOR_EACH_DEBUGGER_DOCK_WIDGET \
	X(DisassemblyWidget, tr("Disassembly"), CenterDockWidgetArea, root) \
	X(MemoryViewWidget, tr("Memory"), BottomDockWidgetArea, DisassemblyWidget) \
	X(RegisterWidget, tr("Registers"), LeftDockWidgetArea, DisassemblyWidget) \
	X(FunctionTreeWidget, tr("Functions"), CenterDockWidgetArea, RegisterWidget) \
	X(GlobalVariableTreeWidget, tr("Globals"), CenterDockWidgetArea, MemoryViewWidget) \
	X(LocalVariableTreeWidget, tr("Locals"), CenterDockWidgetArea, MemoryViewWidget) \
	X(ParameterVariableTreeWidget, tr("Parameters"), CenterDockWidgetArea, MemoryViewWidget)

DockLayoutManager::DockLayoutManager(DebuggerWindow* window)
	: m_window(window)
{
	ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);

	createDefaultLayout("R5900", r5900Debug);
	createDefaultLayout("R3000", r3000Debug);
	loadLayouts();
}

const std::vector<DockLayoutManager::Layout>& DockLayoutManager::layouts()
{
	return m_layouts;
}

void DockLayoutManager::switchToLayout(size_t layout)
{
	m_layouts.at(m_current_layout).dock_manager->setParent(nullptr);
	m_window->setCentralWidget(m_layouts.at(layout).dock_manager);
	m_current_layout = layout;
}

size_t DockLayoutManager::cloneLayout(size_t existing_layout, std::string new_name)
{
	return 0;
}

bool DockLayoutManager::deleteLayout(size_t layout)
{
	return false;
}

void DockLayoutManager::loadLayouts()
{
}

void DockLayoutManager::saveLayouts()
{
}

size_t DockLayoutManager::createDefaultLayout(const char* name, DebugInterface& cpu)
{
	size_t index = m_layouts.size();

	Layout& layout = m_layouts.emplace_back();
	layout.name = name;
	layout.cpu = cpu.getCpuType();
	layout.user_defined = false;
	layout.dock_manager = new ads::CDockManager();

	layout.dock_manager->setStyleSheet("");

	ads::CDockAreaWidget* node_root = nullptr;
#define X(Type, title, Area, parent) \
	ads::CDockAreaWidget* node_##Type = layout.dock_manager->addDockWidget(ads::Area, new Type(cpu), node_##parent); \
	static_cast<void>(node_##Type);
	FOR_EACH_DEBUGGER_DOCK_WIDGET
#undef X

	return index;
}
