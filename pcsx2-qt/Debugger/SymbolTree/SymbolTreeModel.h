// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtCore/QAbstractItemModel>

#include "common/Pcsx2Defs.h"
#include "DebugTools/DebugInterface.h"
#include "DebugTools/ccc/ast.h"
#include "DebugTools/ccc/symbol_database.h"
#include "SymbolTreeNode.h"

// Model for the symbol trees. It will dynamically grow itself as the user
// chooses to expand different nodes.
class SymbolTreeModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	enum Column
	{
		NAME = 0,
		LOCATION = 1,
		TYPE = 2,
		LIVENESS = 3,
		VALUE = 4,
		COLUMN_COUNT = 5
	};

	SymbolTreeModel(DebugInterface& cpu, QObject* parent = nullptr);

	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& index) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
	void fetchMore(const QModelIndex& parent) override;
	bool canFetchMore(const QModelIndex& parent) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	// Reset the whole model.
	void reset(std::unique_ptr<SymbolTreeNode> new_root);

	// Remove all the children of a given node, and allow fetching again.
	bool resetChildren(QModelIndex index);

	// Change the node of a type based on the provided string. This is a wrapper
	// around stringToType that handles taking a lock on the symbol database
	// and updating the model with the new type.
	QString changeTypeTemporarily(QModelIndex index, std::string_view type_string);

	std::optional<QString> typeToString(QModelIndex index);
	static QString typeToString(const ccc::ast::Node* type, const ccc::SymbolDatabase& database);

	// Take a string e.g. "int*[3]" and return an AST node for the type
	// specified. If pointer characters or array subscripts are specified, the
	// owner field in the result structure will be populated with the newly
	// created AST.
	std::unique_ptr<ccc::ast::Node> stringToType(std::string_view string, const ccc::SymbolDatabase& database, QString& error_out);

protected:
	static std::vector<std::unique_ptr<SymbolTreeNode>> populateChildren(
		SymbolTreeLocation location,
		const ccc::ast::Node& logical_type,
		ccc::NodeHandle parent_handle,
		const ccc::SymbolDatabase& database);

	static bool nodeHasChildren(const ccc::ast::Node& logical_type, const ccc::SymbolDatabase& database);
	QModelIndex indexFromNode(const SymbolTreeNode& node) const;

	std::unique_ptr<SymbolTreeNode> m_root;
	QString m_filter;
	DebugInterface& m_cpu;
	SymbolGuardian& m_guardian;
};

std::pair<const ccc::ast::Node*, const ccc::DataType*> resolvePhysicalType(const ccc::ast::Node* type, const ccc::SymbolDatabase& database);
