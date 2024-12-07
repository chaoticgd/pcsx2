// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockLayoutManager.h"

#include "DebuggerWindow.h"
#include "DisassemblyWidget.h"
#include "MemoryViewWidget.h"
#include "RegisterWidget.h"
#include "SymbolTree/SymbolTreeWidgets.h"

#include <DockAreaWidget.h>
#include <DockWidgetTab.h>

#include <QtCore/QTimer>
#include <QtCore/QtTranslation>

#define FOR_EACH_DEBUGGER_DOCK_WIDGET \
	X(DisassemblyWidget, QT_TRANSLATE_NOOP("DockTitle", "Disassembly"), Center, root) \
	X(MemoryViewWidget, QT_TRANSLATE_NOOP("DockTitle", "Memory"), Bottom, DisassemblyWidget) \
	X(RegisterWidget, QT_TRANSLATE_NOOP("DockTitle", "Registers"), Left, DisassemblyWidget) \
	X(FunctionTreeWidget, QT_TRANSLATE_NOOP("DockTitle", "Functions"), Center, RegisterWidget) \
	X(GlobalVariableTreeWidget, QT_TRANSLATE_NOOP("DockTitle", "Globals"), Center, MemoryViewWidget) \
	X(LocalVariableTreeWidget, QT_TRANSLATE_NOOP("DockTitle", "Locals"), Center, MemoryViewWidget) \
	X(ParameterVariableTreeWidget, QT_TRANSLATE_NOOP("DockTitle", "Parameters"), Center, MemoryViewWidget)

//#define LIVE_RELOAD_DEBUGGER_STYLESHEET "pcsx2-qt/resources/stylesheets/debugger.qss"

DockLayoutManager::DockLayoutManager(DebuggerWindow* window)
	: m_window(window)
{
	ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
	ads::CDockManager::setConfigFlag(ads::CDockManager::AllTabsHaveCloseButton, true);
	ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting, true);

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

	setupStyleSheet(layout.dock_manager);

	ads::CDockAreaWidget* area_root = nullptr;
#define X(Type, title, Area, parent) \
	ads::CDockWidget* dock_##Type = new ads::CDockWidget(title); \
	dock_##Type->setWidget(new Type(cpu)); \
	ads::CDockAreaWidget* area_##Type = layout.dock_manager->addDockWidget( \
		ads::Area##DockWidgetArea, dock_##Type, area_##parent); \
	static_cast<void>(area_##Type);
	FOR_EACH_DEBUGGER_DOCK_WIDGET
#undef X

	return index;
}

void DockLayoutManager::setupStyleSheet(ads::CDockManager* dock_manager)
{
#ifdef LIVE_RELOAD_DEBUGGER_STYLESHEET
	// For development purposes, reload the stylesheet every second.
	QTimer* timer = new QTimer(m_window);
	QObject::connect(timer, &QTimer::timeout, [&]() {
		QFile style_sheet(LIVE_RELOAD_DEBUGGER_STYLESHEET);
		if (style_sheet.open(QFile::ReadOnly))
			m_layouts[m_current_layout].dock_manager->setStyleSheet(style_sheet.readAll());
	});
	timer->setInterval(1000);
	timer->start();
#else
	QFile style_sheet(":/stylesheets/debugger.qss");
	if (style_sheet.open(QFile::ReadOnly))
		dock_manager->setStyleSheet(style_sheet.readAll());
#endif

	// This can't be done from the main stylesheet since the selectors wouldn't
	// be properly re-evaluated when the focus changes. In the future it might
	// be better to handle all of this by subclassing QStyle but this will have
	// to do for now.
	QObject::connect(
		dock_manager, &ads::CDockManager::focusedDockWidgetChanged,
		[](ads::CDockWidget* old, ads::CDockWidget* now) {
			if (old && old->dockManager())
				for (ads::CDockAreaWidget* area : old->dockManager()->openedDockAreas())
					area->setStyleSheet(
						"ads--CDockWidgetTab[activeTab=true] { background: palette(window); }");

			if (now && now->dockAreaWidget())
				now->dockAreaWidget()->setStyleSheet(
					"ads--CDockWidgetTab[focused=true] { background: palette(highlight); }"
					"ads--CDockWidget { border: 1px solid palette(highlight); }");
		});
}
