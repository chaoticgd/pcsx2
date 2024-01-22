// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SymbolTreeModel.h"

SymbolTreeModel::SymbolTreeModel(
	std::unique_ptr<SymbolTreeNode> initial_root,
	const SymbolGuardian& guardian,
	QObject* parent)
	: QAbstractItemModel(parent)
	, m_root(std::move(initial_root))
	, m_guardian(guardian)
{
}

QModelIndex SymbolTreeModel::index(int row, int column, const QModelIndex& parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	SymbolTreeNode* parent_node;
	if (parent.isValid())
		parent_node = static_cast<SymbolTreeNode*>(parent.internalPointer());
	else
		parent_node = m_root.get();

	const SymbolTreeNode* child_node = parent_node->children().at(row).get();
	if (!child_node)
		return QModelIndex();

	return createIndex(row, column, child_node);
}

QModelIndex SymbolTreeModel::parent(const QModelIndex& index) const
{
	if (!index.isValid())
		return QModelIndex();

	SymbolTreeNode* child_node = static_cast<SymbolTreeNode*>(index.internalPointer());
	const SymbolTreeNode* parent_node = child_node->parent();
	if (!parent_node)
		return QModelIndex();

	return indexFromNode(*parent_node);
}

int SymbolTreeModel::rowCount(const QModelIndex& parent) const
{
	if (parent.column() > 0)
		return 0;

	SymbolTreeNode* node;
	if (parent.isValid())
		node = static_cast<SymbolTreeNode*>(parent.internalPointer());
	else
		node = m_root.get();

	return (int)node->children().size();
}

int SymbolTreeModel::columnCount(const QModelIndex& parent) const
{
	return COLUMN_COUNT;
}

bool SymbolTreeModel::hasChildren(const QModelIndex& parent) const
{
	bool result = true;

	if (!parent.isValid())
		return result;

	SymbolTreeNode* parent_node = static_cast<SymbolTreeNode*>(parent.internalPointer());
	if (!parent_node->type.valid())
		return result;

	m_guardian.Read([&](const ccc::SymbolDatabase& database) -> void {
		const ccc::ast::Node* type = parent_node->type.lookup_node(database);
		if (!type)
			return;

		result = nodeHasChildren(*type, database);
	});

	return result;
}

QVariant SymbolTreeModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || role != Qt::DisplayRole)
		return QVariant();

	SymbolTreeNode* node = static_cast<SymbolTreeNode*>(index.internalPointer());

	switch (index.column())
	{
		case NAME:
		{
			return node->name;
		}
		case LOCATION:
		{
			return node->location.name();
		}
		case TYPE:
		{
			QVariant result;
			m_guardian.Read([&](const ccc::SymbolDatabase& database) -> void {
				const ccc::ast::Node* type = node->type.lookup_node(database);
				if (!type)
					return;

				result = typeToString(type, database);
			});
			return result;
		}
		case LIVENESS:
		{
			//if (node->type && node->type->descriptor == ccc::ast::VARIABLE)
			//{
			//	const ccc::ast::Variable& variable = node->type->as<ccc::ast::Variable>();
			//	if (variable.storage.type != ccc::ast::VariableStorageType::GLOBAL)
			//	{
			//		bool alive = pc >= variable.block.low && pc < variable.block.high;
			//		return alive ? "Alive" : "Dead";
			//	}
			//}
			return QVariant();
		}
		default:
		{
		}
	}

	Q_ASSERT(index.column() == VALUE);

	QVariant result;

	if (!node->type.valid())
		return result;

	m_guardian.Read([&](const ccc::SymbolDatabase& database) -> void {
		const ccc::ast::Node* logical_type = node->type.lookup_node(database);
		if (!logical_type)
			return;

		const ccc::ast::Node& type = *resolvePhysicalType(logical_type, database).first;

		switch (type.descriptor)
		{
			case ccc::ast::BUILTIN:
			{
				const ccc::ast::BuiltIn& builtIn = type.as<ccc::ast::BuiltIn>();
				switch (builtIn.bclass)
				{
					case ccc::ast::BuiltInClass::UNSIGNED_8:
						result = (qulonglong)node->location.read8();
						break;
					case ccc::ast::BuiltInClass::SIGNED_8:
						result = (qlonglong)node->location.read8();
						break;
					case ccc::ast::BuiltInClass::UNQUALIFIED_8:
						result = (qulonglong)node->location.read8();
						break;
					case ccc::ast::BuiltInClass::BOOL_8:
						result = (bool)node->location.read8();
						break;
					case ccc::ast::BuiltInClass::UNSIGNED_16:
						result = (qulonglong)node->location.read16();
						break;
					case ccc::ast::BuiltInClass::SIGNED_16:
						result = (qlonglong)node->location.read16();
						break;
					case ccc::ast::BuiltInClass::UNSIGNED_32:
						result = (qulonglong)node->location.read32();
						break;
					case ccc::ast::BuiltInClass::SIGNED_32:
						result = (qlonglong)node->location.read32();
						break;
					case ccc::ast::BuiltInClass::FLOAT_32:
					{
						u32 value = node->location.read32();
						result = *reinterpret_cast<float*>(&value);
						break;
					}
					case ccc::ast::BuiltInClass::UNSIGNED_64:
						result = (qulonglong)node->location.read64();
						break;
					case ccc::ast::BuiltInClass::SIGNED_64:
						result = (qlonglong)node->location.read64();
						break;
					case ccc::ast::BuiltInClass::FLOAT_64:
					{
						u64 value = node->location.read64();
						result = *reinterpret_cast<double*>(&value);
						break;
					}
					default:
					{
					}
				}
				break;
			}
			case ccc::ast::ENUM:
				result = node->location.read32();
				break;
			case ccc::ast::POINTER_OR_REFERENCE:
				result = node->location.read32();
				break;
			default:
			{
			}
		}
	});

	return result;
}

bool SymbolTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	bool result = false;

	if (!index.isValid())
		return result;

	SymbolTreeNode* node = static_cast<SymbolTreeNode*>(index.internalPointer());
	if (node->type.valid())
		return result;

	m_guardian.Read([&](const ccc::SymbolDatabase& database) -> void {
		const ccc::ast::Node* logical_type = node->type.lookup_node(database);
		if (!logical_type)
			return;

		const ccc::ast::Node& type = *resolvePhysicalType(logical_type, database).first;
		switch (type.descriptor)
		{
			case ccc::ast::BUILTIN:
			{
				const ccc::ast::BuiltIn& built_in = type.as<ccc::ast::BuiltIn>();

				switch (built_in.bclass)
				{
					case ccc::ast::BuiltInClass::UNSIGNED_8:
						node->location.write8((u8)value.toULongLong());
						break;
					case ccc::ast::BuiltInClass::SIGNED_8:
						node->location.write8((u8)value.toLongLong());
						break;
					case ccc::ast::BuiltInClass::UNQUALIFIED_8:
						node->location.write8((u8)value.toULongLong());
						break;
					case ccc::ast::BuiltInClass::BOOL_8:
						node->location.write8((u8)value.toBool());
						break;
					case ccc::ast::BuiltInClass::UNSIGNED_16:
						node->location.write16((u16)value.toULongLong());
						break;
					case ccc::ast::BuiltInClass::SIGNED_16:
						node->location.write16((u16)value.toLongLong());
						break;
					case ccc::ast::BuiltInClass::UNSIGNED_32:
						node->location.write32((u32)value.toULongLong());
						break;
					case ccc::ast::BuiltInClass::SIGNED_32:
						node->location.write32((u32)value.toLongLong());
						break;
					case ccc::ast::BuiltInClass::FLOAT_32:
					{
						float f = value.toFloat();
						node->location.write32(*reinterpret_cast<u32*>(&f));
						break;
					}
					case ccc::ast::BuiltInClass::UNSIGNED_64:
						node->location.write64((u64)value.toULongLong());
						break;
					case ccc::ast::BuiltInClass::SIGNED_64:
						node->location.write64((u64)value.toLongLong());
						break;
					case ccc::ast::BuiltInClass::FLOAT_64:
					{
						double d = value.toDouble();
						node->location.write64(*reinterpret_cast<u64*>(&d));
						break;
					}
					default:
					{
						return;
					}
				}
				break;
			}
			case ccc::ast::ENUM:
				node->location.write32((u32)value.toULongLong());
				break;
			case ccc::ast::POINTER_OR_REFERENCE:
				node->location.write32((u32)value.toULongLong());
				break;
			default:
			{
				return;
			}
		}

		result = true;
	});

	if (result)
		emit dataChanged(index, index);

	return result;
}

void SymbolTreeModel::fetchMore(const QModelIndex& parent)
{
	if (!parent.isValid())
		return;

	SymbolTreeNode* parent_node = static_cast<SymbolTreeNode*>(parent.internalPointer());
	if (!parent_node->type.valid())
		return;

	std::vector<std::unique_ptr<SymbolTreeNode>> children;
	m_guardian.Read([&](const ccc::SymbolDatabase& database) -> void {
		const ccc::ast::Node* logical_parent_type = parent_node->type.lookup_node(database);
		if (!logical_parent_type)
			return;

		children = populateChildren(parent_node->location, *logical_parent_type, parent_node->type, database);
	});

	if (!children.empty())
		beginInsertRows(parent, 0, children.size() - 1);
	parent_node->set_children(std::move(children));
	if (!children.empty())
		endInsertRows();
}

bool SymbolTreeModel::canFetchMore(const QModelIndex& parent) const
{
	bool result = false;

	if (!parent.isValid())
		return result;

	SymbolTreeNode* parent_node = static_cast<SymbolTreeNode*>(parent.internalPointer());
	if (!parent_node->type.valid())
		return result;

	m_guardian.Read([&](const ccc::SymbolDatabase& database) -> void {
		const ccc::ast::Node* parent_type = parent_node->type.lookup_node(database);
		if (!parent_type)
			return;

		result = nodeHasChildren(*parent_type, database) && !parent_node->children_fetched();
	});

	return result;
}

Qt::ItemFlags SymbolTreeModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;

	Qt::ItemFlags flags = QAbstractItemModel::flags(index);

	if (index.column() == VALUE)
		flags |= Qt::ItemIsEditable;

	return flags;
}

QVariant SymbolTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
		return QVariant();

	switch (section)
	{
		case NAME:
		{
			return "Name";
		}
		case LOCATION:
		{
			return "Location";
		}
		case TYPE:
		{
			return "Type";
		}
		case LIVENESS:
		{
			return "Liveness";
		}
		case VALUE:
		{
			return "Value";
		}
	}

	return QVariant();
}

void SymbolTreeModel::reset(std::unique_ptr<SymbolTreeNode> new_root)
{
	beginResetModel();
	m_root = std::move(new_root);
	endResetModel();
}

std::vector<std::unique_ptr<SymbolTreeNode>> SymbolTreeModel::populateChildren(
	SymbolTreeLocation location,
	const ccc::ast::Node& logical_type,
	ccc::NodeHandle parent_handle,
	const ccc::SymbolDatabase& database)
{
	auto [type, symbol] = resolvePhysicalType(&logical_type, database);

	// If we went through a type name, we need to make the node handles for the
	// children point to the new symbol instead of the original one.
	if (symbol)
		parent_handle = ccc::NodeHandle(*symbol, nullptr);

	std::vector<std::unique_ptr<SymbolTreeNode>> children;

	switch (type->descriptor)
	{
		case ccc::ast::ARRAY:
		{
			const ccc::ast::Array& array = type->as<ccc::ast::Array>();
			for (s32 i = 0; i < array.element_count; i++)
			{
				std::unique_ptr<SymbolTreeNode> element = std::make_unique<SymbolTreeNode>();
				element->name = QString("[%1]").arg(i);
				element->type = parent_handle.handle_for_child(array.element_type.get());
				element->location = location.addOffset(i * array.element_type->computed_size_bytes);
				if (element->location.type != SymbolTreeLocation::NONE)
					children.emplace_back(std::move(element));
			}
			break;
		}
		case ccc::ast::POINTER_OR_REFERENCE:
		{
			u32 address = location.read32();
			if (location.cpu().isValidAddress(address))
			{
				const ccc::ast::PointerOrReference& pointer_or_reference = type->as<ccc::ast::PointerOrReference>();
				std::unique_ptr<SymbolTreeNode> element = std::make_unique<SymbolTreeNode>();
				element->name = QString("*%1").arg(address);
				element->type = parent_handle.handle_for_child(pointer_or_reference.value_type.get());
				element->location = location.createAddress(address);
				children.emplace_back(std::move(element));
			}
			break;
		}
		case ccc::ast::STRUCT_OR_UNION:
		{
			const ccc::ast::StructOrUnion& struct_or_union = type->as<ccc::ast::StructOrUnion>();
			for (const std::unique_ptr<ccc::ast::Node>& base_class : struct_or_union.base_classes)
			{
				SymbolTreeLocation base_class_location = location.addOffset(base_class->offset_bytes);
				if (base_class_location.type != SymbolTreeLocation::NONE)
				{
					std::vector<std::unique_ptr<SymbolTreeNode>> fields = populateChildren(
						base_class_location, *base_class.get(), parent_handle, database);
					children.insert(children.end(),
						std::make_move_iterator(fields.begin()),
						std::make_move_iterator(fields.end()));
				}
			}
			for (const std::unique_ptr<ccc::ast::Node>& field : struct_or_union.fields)
			{
				std::unique_ptr<SymbolTreeNode> child_node = std::make_unique<SymbolTreeNode>();
				child_node->name = QString::fromStdString(field->name);
				child_node->type = parent_handle.handle_for_child(field.get());
				child_node->location = location.addOffset(field->offset_bytes);
				if (child_node->location.type != SymbolTreeLocation::NONE)
					children.emplace_back(std::move(child_node));
			}
			break;
		}
		default:
		{
		}
	}

	return children;
}

bool SymbolTreeModel::nodeHasChildren(const ccc::ast::Node& logical_type, const ccc::SymbolDatabase& database)
{
	const ccc::ast::Node& type = *resolvePhysicalType(&logical_type, database).first;

	bool result = false;
	switch (type.descriptor)
	{
		case ccc::ast::ARRAY:
		{
			const ccc::ast::Array& array = type.as<ccc::ast::Array>();
			result = array.element_count > 0;
			break;
		}
		case ccc::ast::POINTER_OR_REFERENCE:
		{
			result = true;
			break;
		}
		case ccc::ast::STRUCT_OR_UNION:
		{
			const ccc::ast::StructOrUnion& struct_or_union = type.as<ccc::ast::StructOrUnion>();
			result = !struct_or_union.base_classes.empty() || !struct_or_union.fields.empty();
			break;
		}
		default:
		{
		}
	}

	return result;
}

QModelIndex SymbolTreeModel::indexFromNode(const SymbolTreeNode& node) const
{
	int row = 0;
	if (node.parent())
	{
		for (int i = 0; i < (int)node.parent()->children().size(); i++)
			if (node.parent()->children()[i].get() == &node)
				row = i;
	}
	else
		row = 0;

	return createIndex(row, 0, &node);
}

QString SymbolTreeModel::typeToString(const ccc::ast::Node* type, const ccc::SymbolDatabase& database)
{
	std::vector<char> pointer_chars;
	std::vector<s32> array_indices;

	// Traverse through arrays, pointers and references, keeping track of what
	// needs to be appended after the type name.
	bool done_finding_arrays_pointers = false;
	while (!done_finding_arrays_pointers)
	{
		switch (type->descriptor)
		{
			case ccc::ast::ARRAY:
			{
				const ccc::ast::Array& array = type->as<ccc::ast::Array>();
				array_indices.emplace_back(array.element_count);
				type = array.element_type.get();
				break;
			}
			case ccc::ast::POINTER_OR_REFERENCE:
			{
				const ccc::ast::PointerOrReference& pointer_or_reference = type->as<ccc::ast::PointerOrReference>();
				pointer_chars.emplace_back(pointer_or_reference.is_pointer ? '*' : '&');
				type = pointer_or_reference.value_type.get();
				break;
			}
			default:
			{
				done_finding_arrays_pointers = true;
				break;
			}
		}
	}

	QString result;

	// Append the actual type name, or at the very least the node type.
	switch (type->descriptor)
	{
		case ccc::ast::TYPE_NAME:
		{
			const ccc::ast::TypeName& type_name = type->as<ccc::ast::TypeName>();
			const ccc::DataType* data_type = database.data_types.symbol_from_handle(type_name.data_type_handle);
			if (data_type)
			{
				result = QString::fromStdString(data_type->name());
			}
			break;
		}
		default:
		{
			result = ccc::ast::node_type_to_string(*type);
		}
	}

	// Append pointer characters and array indices at the end.
	for (size_t i = pointer_chars.size(); i > 0; i--)
		result += pointer_chars[i - 1];

	for (s32 index : array_indices)
		result += QString("[%1]").arg(index);

	return result;
}

std::pair<const ccc::ast::Node*, const ccc::DataType*> resolvePhysicalType(const ccc::ast::Node* type, const ccc::SymbolDatabase& database)
{
	const ccc::DataType* symbol = nullptr;
	for (s32 i = 0; i < 10 && type->descriptor == ccc::ast::TYPE_NAME; i++)
	{
		const ccc::DataType* data_type = database.data_types.symbol_from_handle(type->as<ccc::ast::TypeName>().data_type_handle);
		if (!data_type && data_type->type())
			break;
		type = data_type->type();
		symbol = data_type;
	}
	return std::pair(type, symbol);
}
