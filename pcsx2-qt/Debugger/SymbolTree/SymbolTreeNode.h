// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QString>

#include "SymbolTreeLocation.h"

class DebugInterface;

enum class SymbolTreeNodeState
{
	NORMAL,
	ARRAY,
	STRING
};

struct SymbolTreeNode
{
public:
	QString name;
	SymbolTreeNodeState state = SymbolTreeNodeState::NORMAL;
	s32 element_count = -1;
	ccc::NodeHandle type;
	SymbolTreeLocation location;
	ccc::AddressRange live_range;
	std::unique_ptr<ccc::ast::Node> temporary_type;
	
	QString toString(const ccc::ast::Node& type);
	QVariant toVariant(const ccc::ast::Node& type);
	bool fromVariant(QVariant value, const ccc::ast::Node& type);
	
	const SymbolTreeNode* parent() const;
	
	const std::vector<std::unique_ptr<SymbolTreeNode>>& children() const;
	bool childrenFetched() const;
	void setChildren(std::vector<std::unique_ptr<SymbolTreeNode>> new_children);
	void insertChildren(std::vector<std::unique_ptr<SymbolTreeNode>> new_children);
	void emplaceChild(std::unique_ptr<SymbolTreeNode> new_child);
	void clearChildren();
	
	void sortChildrenRecursively(bool sort_by_if_type_is_known);

protected:
	SymbolTreeNode* m_parent = nullptr;
	std::vector<std::unique_ptr<SymbolTreeNode>> m_children;
	bool m_children_fetched = false;
};
