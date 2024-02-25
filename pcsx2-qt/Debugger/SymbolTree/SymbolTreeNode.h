// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtCore/QString>
#include <QtCore/QVariant>

#include "SymbolTreeLocation.h"

class DebugInterface;

enum class SymbolTreeTag
{
	GROUP,
	OBJECT
};

// A node in a symbol tree model.
struct SymbolTreeNode
{
public:
	SymbolTreeTag tag = SymbolTreeTag::OBJECT;
	QString name;
	ccc::NodeHandle type;
	SymbolTreeLocation location;
	ccc::AddressRange live_range;
	ccc::MultiSymbolHandle symbol;
	std::unique_ptr<ccc::ast::Node> temporary_type;

	QString toString(const ccc::ast::Node& type, DebugInterface& cpu, const ccc::SymbolDatabase* database);
	QVariant toVariant(const ccc::ast::Node& type, DebugInterface& cpu);
	bool fromVariant(QVariant value, const ccc::ast::Node& type, DebugInterface& cpu);

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

std::pair<const ccc::ast::Node*, const ccc::DataType*> resolvePhysicalType(const ccc::ast::Node* type, const ccc::SymbolDatabase& database);
