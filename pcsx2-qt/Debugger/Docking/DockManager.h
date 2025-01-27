// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Debugger/Docking/DockLayout.h"

#include <kddockwidgets/MainWindow.h>
#include <kddockwidgets/DockWidget.h>
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/DockWidget.h>
#include <kddockwidgets/core/Draggable_p.h>

#include <QtCore/QPointer>
#include <QtWidgets/QTabBar>

#include "fmt/format.h"

class DockManager : public QObject
{
	Q_OBJECT

public:
	DockManager(QObject* parent = nullptr);

	DockManager(const DockManager& rhs) = delete;
	DockManager& operator=(const DockManager& rhs) = delete;

	DockManager(DockManager&& rhs) = delete;
	DockManager& operator=(DockManager&&) = delete;

	static void configureDockingSystem();

	template <typename... Args>
	DockLayout::Index createLayout(Args&&... args)
	{
		DockLayout::Index layout_index = m_layouts.size();

		if (m_layouts.empty())
		{
			// Delete the placeholder created in DockManager::deleteLayout.
			for (KDDockWidgets::Core::DockWidget* dock : KDDockWidgets::DockRegistry::self()->dockwidgets())
				delete dock;
		}

		DockLayout& layout = m_layouts.emplace_back(std::forward<Args>(args)..., layout_index);

		// Try to make sure the layout has a unique name.
		const std::string& name = layout.name();
		std::string new_name = name;
		for (int i = 0; hasNameConflict(new_name, layout_index) && i < 100; i++)
		{
			new_name = fmt::format("{} #{}", name, i);
		}

		if (new_name != name)
		{
			layout.setName(name);
			layout.save(layout_index);
		}

		return layout_index;
	}

	bool deleteLayout(DockLayout::Index layout_index);

	void switchToLayout(DockLayout::Index layout_index);

	void loadLayouts();
	bool saveLayouts();

	void resetAllLayouts();
	void resetDefaultLayouts();

	void createWindowsMenu(QMenu* menu);

	QWidget* createLayoutSwitcher(QWidget* menu_bar);
	void updateLayoutSwitcher();
	void layoutSwitcherTabChanged(s32 index);
	void layoutSwitcherTabMoved(s32 from, s32 to);
	void layoutSwitcherContextMenu(QPoint pos);

	bool hasNameConflict(const std::string& name, DockLayout::Index layout_index);

	void retranslateDockWidget(KDDockWidgets::Core::DockWidget* dock_widget);
	void dockWidgetClosed(KDDockWidgets::Core::DockWidget* dock_widget);

	void recreateDebuggerWidget(QString unique_name);

	bool isLayoutLocked();
	void setLayoutLocked(bool locked);

private:
	static bool dragAboutToStart(KDDockWidgets::Core::Draggable* draggable);

	std::vector<DockLayout> m_layouts;
	DockLayout::Index m_current_layout = DockLayout::INVALID_INDEX;

	QTabBar* m_switcher = nullptr;
	s32 m_plus_tab_index = -1;
	s32 m_current_tab_index = -1;

	QMetaObject::Connection m_tab_connection;

	bool m_layout_locked = true;
};
