// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Debugger/DebuggerWidget.h"

#include "DebugTools/DebugInterface.h"

#include <kddockwidgets/MainWindow.h>
#include <kddockwidgets/DockWidget.h>

#include <QtWidgets/QTabBar>

class DebuggerWindow;

extern const u32 DEBUGGER_LAYOUT_FILE_VERSION;

class DockManager : public QObject
{
	Q_OBJECT

public:
	DockManager(DebuggerWindow* window);
	~DockManager();

	static void configureDockingSystem();

	enum LayoutCreationMode
	{
		DEFAULT_LAYOUT,
		CLONE_LAYOUT,
		BLANK_LAYOUT,
	};

	s32 createLayout(std::string name, BreakPointCpu cpu, LayoutCreationMode mode);
	bool deleteLayout(s32 layout_index);

	void switchToLayout(s32 layout_index);

	void loadLayouts();
	s32 loadLayout(const std::string& path);

	bool saveLayouts();
	bool saveLayout(s32 layout_index);

	void renameLayout(s32 layout_index, std::string new_name);

	void setupDefaultLayouts();

	void createWindowsMenu(QMenu* menu);

	QWidget* createLayoutSwitcher(QWidget* menu_bar);
	void updateLayoutSwitcher();
	void layoutSwitcherTabChanged(s32 index);
	void layoutSwitcherTabMoved(s32 from, s32 to);
	void layoutSwitcherContextMenu(QPoint pos);

private:
	struct Layout
	{
		// The name displayed in the user interface. Also used to determine the
		// file name for the layout file.
		std::string name;

		// The default target for dock widgets in this layout. This can be
		// overriden on a per-widget basis.
		BreakPointCpu cpu;

		// All the dock widgets currently open in this layout. If this is the
		// active layout then these will be owned by the docking system,
		// otherwise they won't be and will need to be cleaned up separately.
		std::vector<QPointer<DebuggerWidget>> widgets;

		int switcher_tab_index = -1;

		// The geometry of all the dock widgets, converted to JSON by the
		// LayoutSaver class from KDDockWidgets.
		QByteArray geometry;
		bool geometry_modified = false;

		// The absolute file path of the corresponding layout file as it
		// currently exists exists on disk, or empty if no such file exists.
		std::string layout_file_path;

		bool is_frozen = true;
	};

	// Save the current state of all the dock widgets to a layout.
	void freezeLayout(Layout& layout);

	// Restore the state of all the dock widgets from a layout.
	void thawLayout(Layout& layout);

	KDDockWidgets::Core::DockWidget* createDockWidget(const QString& name);

	void populateDefaultLayout();

	KDDockWidgets::QtWidgets::MainWindow* m_window;

	std::vector<Layout> m_layouts;
	s32 m_current_layout = -1;

	QTabBar* m_switcher = nullptr;
	s32 m_plus_tab_index = -1;
	s32 m_current_tab_index = -1;

	QMetaObject::Connection m_tab_connection;
};
