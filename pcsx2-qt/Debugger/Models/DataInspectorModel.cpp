#include "DataInspectorModel.h"

DataInspectorModel::DataInspectorModel(
	std::unique_ptr<DataInspectorNode> initial_root,
	const SymbolGuardian& guardian,
	QObject* parent)
	: QAbstractItemModel(parent)
	, m_root(std::move(initial_root))
	, m_guardian(guardian)
{
}

QModelIndex DataInspectorModel::index(int row, int column, const QModelIndex& parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	DataInspectorNode* parent_node;
	if (parent.isValid())
		parent_node = static_cast<DataInspectorNode*>(parent.internalPointer());
	else
		parent_node = m_root.get();

	DataInspectorNode* child_node = parent_node->children.at(row).get();
	if (!child_node)
		return QModelIndex();

	return createIndex(row, column, child_node);
}

QModelIndex DataInspectorModel::parent(const QModelIndex& index) const
{
	if (!index.isValid())
		return QModelIndex();

	DataInspectorNode* child_node = static_cast<DataInspectorNode*>(index.internalPointer());
	DataInspectorNode* parent_node = child_node->parent;
	if (!parent_node)
		return QModelIndex();

	return indexFromNode(*parent_node);
}

int DataInspectorModel::rowCount(const QModelIndex& parent) const
{
	if (parent.column() > 0)
		return 0;

	DataInspectorNode* node;
	if (parent.isValid())
		node = static_cast<DataInspectorNode*>(parent.internalPointer());
	else
		node = m_root.get();

	return (int)node->children.size();
}

int DataInspectorModel::columnCount(const QModelIndex& parent) const
{
	return COLUMN_COUNT;
}

bool DataInspectorModel::hasChildren(const QModelIndex& parent) const
{
	bool result = true;

	if (!parent.isValid())
		return result;

	DataInspectorNode* parent_node = static_cast<DataInspectorNode*>(parent.internalPointer());
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

QVariant DataInspectorModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || role != Qt::DisplayRole)
		return QVariant();

	DataInspectorNode* node = static_cast<DataInspectorNode*>(index.internalPointer());

	u32 pc = r5900Debug.getRegister(EECAT_GPR, 32);

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

				result = typeToString(*type, database);
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

		const ccc::ast::Node& type = resolvePhysicalType(*logical_type, database);

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

bool DataInspectorModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	bool result = false;

	if (!index.isValid())
		return result;

	DataInspectorNode* node = static_cast<DataInspectorNode*>(index.internalPointer());
	if (node->type.valid())
		return result;

	m_guardian.Read([&](const ccc::SymbolDatabase& database) -> void {
		const ccc::ast::Node* logical_type = node->type.lookup_node(database);
		if (!logical_type)
			return;

		const ccc::ast::Node& type = resolvePhysicalType(*logical_type, database);
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

void DataInspectorModel::fetchMore(const QModelIndex& parent)
{
	if (!parent.isValid())
		return;

	DataInspectorNode* parent_node = static_cast<DataInspectorNode*>(parent.internalPointer());
	if (!parent_node->type.valid())
		return;

	std::vector<std::unique_ptr<DataInspectorNode>> children;
	m_guardian.Read([&](const ccc::SymbolDatabase& database) -> void {
		const ccc::ast::Node* logical_parent_type = parent_node->type.lookup_node(database);
		if (!logical_parent_type)
			return;

		const ccc::ast::Node& parent_type = resolvePhysicalType(*logical_parent_type, database);

		switch (parent_type.descriptor)
		{
			case ccc::ast::ARRAY:
			{
				const ccc::ast::Array& array = parent_type.as<ccc::ast::Array>();
				for (s32 i = 0; i < array.element_count; i++)
				{
					std::unique_ptr<DataInspectorNode> element = std::make_unique<DataInspectorNode>();
					element->name = QString("[%1]").arg(i);
					element->type = parent_node->type.handle_for_child(array.element_type.get());
					element->location = parent_node->location.addOffset(i * array.element_type->computed_size_bytes);
					children.emplace_back(std::move(element));
				}
				break;
			}
			case ccc::ast::POINTER_OR_REFERENCE:
			{
				u32 address = parent_node->location.read32();
				if (parent_node->location.cpu().isValidAddress(address))
				{
					const ccc::ast::PointerOrReference& pointer_or_reference = parent_type.as<ccc::ast::PointerOrReference>();
					std::unique_ptr<DataInspectorNode> element = std::make_unique<DataInspectorNode>();
					element->name = QString("*%1").arg(address);
					element->type = parent_node->type.handle_for_child(pointer_or_reference.value_type.get());
					element->location = parent_node->location.createAddress(address);
					children.emplace_back(std::move(element));
				}
				break;
			}
			case ccc::ast::STRUCT_OR_UNION:
			{
				const ccc::ast::StructOrUnion& structOrUnion = parent_type.as<ccc::ast::StructOrUnion>();
				for (const std::unique_ptr<ccc::ast::Node>& field : structOrUnion.fields)
				{
					std::unique_ptr<DataInspectorNode> child_node = std::make_unique<DataInspectorNode>();
					child_node->name = QString::fromStdString(field->name);
					child_node->type = parent_node->type.handle_for_child(field.get());
					child_node->location = parent_node->location.addOffset(field->offset_bytes);
					children.emplace_back(std::move(child_node));
				}
				break;
			}
			default:
			{
			}
		}
	});

	if (children.empty())
	{
		parent_node->children_fetched = true;
		return;
	}

	for (std::unique_ptr<DataInspectorNode>& child_node : children)
		child_node->parent = parent_node;

	beginInsertRows(parent, 0, children.size() - 1);
	parent_node->children = std::move(children);
	parent_node->children_fetched = true;
	endInsertRows();
}

bool DataInspectorModel::canFetchMore(const QModelIndex& parent) const
{
	bool result = false;

	if (!parent.isValid())
		return result;

	DataInspectorNode* parent_node = static_cast<DataInspectorNode*>(parent.internalPointer());
	if (!parent_node->type.valid())
		return result;

	m_guardian.Read([&](const ccc::SymbolDatabase& database) -> void {
		const ccc::ast::Node* parent_type = parent_node->type.lookup_node(database);
		if (!parent_type)
			return;

		result = nodeHasChildren(*parent_type, database) && !parent_node->children_fetched;
	});

	return result;
}

Qt::ItemFlags DataInspectorModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;

	Qt::ItemFlags flags = QAbstractItemModel::flags(index);

	if (index.column() == VALUE)
		flags |= Qt::ItemIsEditable;

	return flags;
}

QVariant DataInspectorModel::headerData(int section, Qt::Orientation orientation, int role) const
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

void DataInspectorModel::reset(std::unique_ptr<DataInspectorNode> new_root)
{
	beginResetModel();
	m_root = std::move(new_root);
	endResetModel();
}

bool DataInspectorModel::nodeHasChildren(const ccc::ast::Node& type, const ccc::SymbolDatabase& database) const
{
	const ccc::ast::Node& physical_type = resolvePhysicalType(type, database);

	bool result = false;
	switch (physical_type.descriptor)
	{
		case ccc::ast::ARRAY:
		{
			const ccc::ast::Array& array = physical_type.as<ccc::ast::Array>();
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
			const ccc::ast::StructOrUnion& struct_or_union = physical_type.as<ccc::ast::StructOrUnion>();
			result = !struct_or_union.fields.empty() || !struct_or_union.base_classes.empty();
			break;
		}
		default:
		{
		}
	}

	return result;
}

QModelIndex DataInspectorModel::indexFromNode(const DataInspectorNode& node) const
{
	int row = 0;
	if (node.parent)
	{
		for (int i = 0; i < (int)node.parent->children.size(); i++)
			if (node.parent->children[i].get() == &node)
				row = i;
	}
	else
		row = 0;

	return createIndex(row, 0, &node);
}

QString DataInspectorModel::typeToString(const ccc::ast::Node& type, const ccc::SymbolDatabase& database) const
{
	QString result;

	switch (type.descriptor)
	{
		case ccc::ast::TYPE_NAME:
		{
			const ccc::ast::TypeName& type_name = type.as<ccc::ast::TypeName>();
			const ccc::DataType* data_type = database.data_types.symbol_from_handle(type_name.data_type_handle);
			if (data_type)
			{
				result = QString::fromStdString(data_type->name());
			}
			break;
		}
		default:
		{
			result = ccc::ast::node_type_to_string(type);
		}
	}

	return result;
}

const ccc::ast::Node& resolvePhysicalType(const ccc::ast::Node& type, const ccc::SymbolDatabase& database)
{
	const ccc::ast::Node* result = &type;
	for (s32 i = 0; i < 10 && result->descriptor == ccc::ast::TYPE_NAME; i++)
	{
		const ccc::DataType* symbol = database.data_types.symbol_from_handle(type.as<ccc::ast::TypeName>().data_type_handle);
		if (!symbol)
		{
			break;
		}
		result = symbol->type();
	}
	return *result;
}
