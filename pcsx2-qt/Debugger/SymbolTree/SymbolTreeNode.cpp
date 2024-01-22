// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SymbolTreeNode.h"

const SymbolTreeNode* SymbolTreeNode::parent() const
{
	return m_parent;
}

const std::vector<std::unique_ptr<SymbolTreeNode>>& SymbolTreeNode::children() const
{
	return m_children;
}

bool SymbolTreeNode::children_fetched() const
{
	return m_children_fetched;
}

void SymbolTreeNode::set_children(std::vector<std::unique_ptr<SymbolTreeNode>> new_children)
{
	for (std::unique_ptr<SymbolTreeNode>& child : new_children)
		child->m_parent = this;
	m_children = std::move(new_children);
	m_children_fetched = true;
}

void SymbolTreeNode::insert_children(std::vector<std::unique_ptr<SymbolTreeNode>> new_children)
{
	for (std::unique_ptr<SymbolTreeNode>& child : new_children)
		child->m_parent = this;
	m_children.insert(m_children.end(),
		std::make_move_iterator(new_children.begin()),
		std::make_move_iterator(new_children.end()));
	m_children_fetched = true;
}

void SymbolTreeNode::emplace_child(std::unique_ptr<SymbolTreeNode> new_child)
{
	new_child->m_parent = this;
	m_children.emplace_back(std::move(new_child));
	m_children_fetched = true;
}

void SymbolTreeNode::sortChildrenRecursively()
{
	auto comparator = [](const std::unique_ptr<SymbolTreeNode>& lhs, const std::unique_ptr<SymbolTreeNode>& rhs) -> bool {
		// Sort the nodes that actually have type information to the top since
		// these will be the most useful for editing.
		if (lhs->type.valid() != rhs->type.valid())
			return lhs->type.valid() > rhs->type.valid();
		// Next, sort nodes that have a valid location to the top.
		return lhs->location < rhs->location;
	};

	std::sort(m_children.begin(), m_children.end(), comparator);

	for (std::unique_ptr<SymbolTreeNode>& child : m_children)
		child->sortChildrenRecursively();
}
