// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QString>

#include "common/Pcsx2Defs.h"
#include "SymbolTreeLocation.h"

class DebugInterface;

struct SymbolTreeNode
{
public:
	QString name;
	ccc::NodeHandle type;
	SymbolTreeLocation location;
	
	const SymbolTreeNode* parent() const;
	
	const std::vector<std::unique_ptr<SymbolTreeNode>>& children() const;
	bool children_fetched() const;
	void set_children(std::vector<std::unique_ptr<SymbolTreeNode>> new_children);
	void insert_children(std::vector<std::unique_ptr<SymbolTreeNode>> new_children);
	void emplace_child(std::unique_ptr<SymbolTreeNode> new_child);
	
	void sortChildrenRecursively(bool sort_by_if_type_is_known);

protected:
	SymbolTreeNode* m_parent = nullptr;
	std::vector<std::unique_ptr<SymbolTreeNode>> m_children;
	bool m_children_fetched = false;
};
