// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "DockManager.h"

#include "DebugTools/DebugInterface.h"

class DebuggerWindow;

class DockLayoutManager
{
public:
	struct Layout
	{
		std::string name;
		BreakPointCpu cpu;
		bool user_defined = false;
		ads::CDockManager* dock_manager = nullptr;
	};

	DockLayoutManager(DebuggerWindow* window);

	const std::vector<Layout>& layouts();
	void switchToLayout(size_t layout);
	size_t cloneLayout(size_t existing_layout, std::string new_name);
	bool deleteLayout(size_t layout);

	void loadLayouts();
	void saveLayouts();

protected:
	size_t createDefaultLayout(const char* name, DebugInterface& cpu);
	void setupStyleSheet(ads::CDockManager* dock_manager);

	DebuggerWindow* m_window;

	std::vector<Layout> m_layouts;
	size_t m_current_layout = 0;
};
