/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QtCore/QAbstractItemModel>

#include "common/Pcsx2Defs.h"
#include "DebugTools/DebugInterface.h"
#include "DebugTools/ccc/ast.h"
#include "DebugTools/ccc/symbol_database.h"
#include "SymbolTreeNode.h"

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

	SymbolTreeModel(
		std::unique_ptr<SymbolTreeNode> initial_root,
		const SymbolGuardian& guardian,
		QObject* parent = nullptr);

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

	void reset(std::unique_ptr<SymbolTreeNode> new_root);

protected:
	bool nodeHasChildren(const ccc::ast::Node& type, const ccc::SymbolDatabase& database) const;
	QModelIndex indexFromNode(const SymbolTreeNode& node) const;
	QString typeToString(const ccc::ast::Node& type, const ccc::SymbolDatabase& database) const;

	std::unique_ptr<SymbolTreeNode> m_root;
	QString m_filter;
	const SymbolGuardian& m_guardian;
};

const ccc::ast::Node& resolvePhysicalType(const ccc::ast::Node& type, const ccc::SymbolDatabase& database);
