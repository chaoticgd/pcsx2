// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "DockUtils.h"

#include "DebugTools/DebugInterface.h"

#include <kddockwidgets/KDDockWidgets.h>

class MD5Digest;

class DebuggerView;
struct DebuggerViewParameters;

namespace DockTables
{
	struct DebuggerViewDescription
	{
		DebuggerView* (*create_widget)(const DebuggerViewParameters& parameters);

		// The untranslated string displayed as the dock widget tab text.
		const char* display_name;

		// This is used to determine which group dock widgets of this type are
		// added to when they're opened from the Windows menu.
		DockUtils::PreferredLocation preferred_location;
	};

	extern const std::map<std::string, DebuggerViewDescription> DEBUGGER_VIEWS;

	enum DefaultDockGroup
	{
		DDG_ROOT = -1,
		DDG_TOP_RIGHT = 0,
		DDG_BOTTOM = 1,
		DDG_TOP_LEFT = 2
	};

	enum TimelineDockGroup
	{
		TDG_ROOT = -1,
		TDG_TOP = 0,
		TDG_BOTTOM_LEFT = 1,
		TDG_BOTTOM_RIGHT = 2
	};

	struct DefaultDockGroupDescription
	{
		KDDockWidgets::Location location;
		s32 parent;
	};

	extern const std::vector<DefaultDockGroupDescription> DEFAULT_DOCK_GROUPS;

	struct DefaultDockWidgetDescription
	{
		std::string type;
		s32 group;
	};

	struct DefaultDockLayout
	{
		std::string name;
		BreakPointCpu cpu;
		std::vector<DefaultDockGroupDescription> groups;
		std::vector<DefaultDockWidgetDescription> widgets;
		std::set<std::string> toolbars;
	};

	extern const std::vector<DefaultDockLayout> DEFAULT_DOCK_LAYOUTS;

	const DefaultDockLayout* defaultLayout(const std::string& name);

	// This is used to determine if the user has updated and we need to recreate
	// the default layouts.
	u32 hashDefaultLayouts();
} // namespace DockTables
