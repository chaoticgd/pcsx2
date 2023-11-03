#include "DataInspectorModel.h"

DataInspectorModel::DataInspectorModel(
	std::unique_ptr<DataInspectorNode> initialRoot,
	const ccc::HighSymbolTable& symbolTable,
	const std::map<std::string, s32>& typeNameToDeduplicatedTypeIndex,
	QObject* parent)
	: QAbstractItemModel(parent)
	, m_root(std::move(initialRoot))
	, m_symbolTable(symbolTable)
	, m_typeNameToDeduplicatedTypeIndex(typeNameToDeduplicatedTypeIndex)
{
}

QModelIndex DataInspectorModel::index(int row, int column, const QModelIndex& parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	DataInspectorNode* parentNode;
	if (parent.isValid())
		parentNode = static_cast<DataInspectorNode*>(parent.internalPointer());
	else
		parentNode = m_root.get();

	DataInspectorNode* childNode = parentNode->children.at(row).get();
	if (!childNode)
		return QModelIndex();

	return createIndex(row, column, childNode);
}

QModelIndex DataInspectorModel::parent(const QModelIndex& index) const
{
	if (!index.isValid())
		return QModelIndex();

	DataInspectorNode* childNode = static_cast<DataInspectorNode*>(index.internalPointer());
	DataInspectorNode* parentNode = childNode->parent;
	if (!parentNode)
		return QModelIndex();

	return indexFromNode(*parentNode);
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
	return 4;
}

bool DataInspectorModel::hasChildren(const QModelIndex& parent) const
{
	if (!parent.isValid())
		return true;

	DataInspectorNode* parentNode = static_cast<DataInspectorNode*>(parent.internalPointer());
	if (!parentNode->type)
		return true;

	return nodeHasChildren(*parentNode->type);
}

QVariant DataInspectorModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || role != Qt::DisplayRole)
		return QVariant();

	DataInspectorNode* node = static_cast<DataInspectorNode*>(index.internalPointer());

	switch (index.column())
	{
		case NAME:
		{
			return node->name;
		}
		case ADDRESS:
		{
			if (node->address > 0)
				return QString::number(node->address, 16);
			else
				return QVariant();
		}
		case TYPE:
		{
			if (!node->type)
				return QVariant();
			return typeToString(*node->type);
		}
	}

	Q_ASSERT(index.column() == VALUE);

	if (!node->type)
		return QVariant();

	const ccc::ast::Node& type = resolvePhysicalType(*node->type, m_symbolTable, m_typeNameToDeduplicatedTypeIndex);

	QVariant result;
	switch (type.descriptor)
	{
		case ccc::ast::BUILTIN:
		{
			const ccc::ast::BuiltIn& builtIn = type.as<ccc::ast::BuiltIn>();
			switch (builtIn.bclass)
			{
				case ccc::BuiltInClass::UNSIGNED_8:
					return (qulonglong)(u8)r5900Debug.read8(node->address);
				case ccc::BuiltInClass::SIGNED_8:
					return (qlonglong)(s8)r5900Debug.read8(node->address);
				case ccc::BuiltInClass::UNQUALIFIED_8:
					return (qulonglong)(u8)r5900Debug.read8(node->address);
				case ccc::BuiltInClass::BOOL_8:
					return (bool)r5900Debug.read8(node->address);
				case ccc::BuiltInClass::UNSIGNED_16:
					return (qulonglong)(u16)r5900Debug.read16(node->address);
				case ccc::BuiltInClass::SIGNED_16:
					return (qlonglong)(s16)r5900Debug.read16(node->address);
				case ccc::BuiltInClass::UNSIGNED_32:
					return (qulonglong)(u32)r5900Debug.read32(node->address);
				case ccc::BuiltInClass::SIGNED_32:
					return (qlonglong)(s32)r5900Debug.read32(node->address);
				case ccc::BuiltInClass::FLOAT_32:
				{
					u32 value = r5900Debug.read32(node->address);
					return *reinterpret_cast<float*>(&value);
				}
				case ccc::BuiltInClass::UNSIGNED_64:
					return (qulonglong)(u64)r5900Debug.read64(node->address);
				case ccc::BuiltInClass::SIGNED_64:
					return (qlonglong)(s64)r5900Debug.read64(node->address);
				case ccc::BuiltInClass::FLOAT_64:
				{
					u64 value = r5900Debug.read64(node->address);
					return *reinterpret_cast<double*>(&value);
				}
				default:
				{
				}
			}
			break;
		}
		case ccc::ast::INLINE_ENUM:
			return r5900Debug.read32(node->address);
		case ccc::ast::POINTER:
			return r5900Debug.read32(node->address);
		case ccc::ast::REFERENCE:
			return r5900Debug.read32(node->address);
		default:
		{
		}
	}

	return QVariant();
}

bool DataInspectorModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (!index.isValid())
		return false;

	DataInspectorNode* node = static_cast<DataInspectorNode*>(index.internalPointer());
	if (!node->type)
		return false;

	const ccc::ast::Node& type = resolvePhysicalType(*node->type, m_symbolTable, m_typeNameToDeduplicatedTypeIndex);
	switch (type.descriptor)
	{
		case ccc::ast::BUILTIN:
		{
			const ccc::ast::BuiltIn& builtIn = type.as<ccc::ast::BuiltIn>();

			switch (builtIn.bclass)
			{
				case ccc::BuiltInClass::UNSIGNED_8:
					r5900Debug.write8(node->address, value.toULongLong());
					break;
				case ccc::BuiltInClass::SIGNED_8:
					r5900Debug.write8(node->address, value.toLongLong());
					break;
				case ccc::BuiltInClass::UNQUALIFIED_8:
					r5900Debug.write8(node->address, value.toULongLong());
					break;
				case ccc::BuiltInClass::BOOL_8:
					r5900Debug.write8(node->address, value.toBool());
					break;
				case ccc::BuiltInClass::UNSIGNED_16:
					r5900Debug.write16(node->address, value.toULongLong());
					break;
				case ccc::BuiltInClass::SIGNED_16:
					r5900Debug.write16(node->address, value.toLongLong());
					break;
				case ccc::BuiltInClass::UNSIGNED_32:
					r5900Debug.write32(node->address, value.toULongLong());
					break;
				case ccc::BuiltInClass::SIGNED_32:
					r5900Debug.write32(node->address, value.toLongLong());
					break;
				case ccc::BuiltInClass::FLOAT_32:
				{
					float f = value.toFloat();
					r5900Debug.write32(node->address, *reinterpret_cast<float*>(&f));
					break;
				}
				case ccc::BuiltInClass::UNSIGNED_64:
					r5900Debug.write32(node->address, value.toULongLong());
					break;
				case ccc::BuiltInClass::SIGNED_64:
					r5900Debug.write32(node->address, value.toLongLong());
					break;
				case ccc::BuiltInClass::FLOAT_64:
				{
					double d = value.toDouble();
					r5900Debug.write32(node->address, *reinterpret_cast<double*>(&d));
					break;
				}
				default:
				{
					return false;
				}
			}
			break;
		}
		case ccc::ast::INLINE_ENUM:
			r5900Debug.write32(node->address, value.toLongLong());
			break;
		case ccc::ast::POINTER:
			r5900Debug.write32(node->address, value.toULongLong());
			break;
		case ccc::ast::REFERENCE:
			r5900Debug.write32(node->address, value.toInt());
			break;
		default:
		{
			return false;
		}
	}

	emit dataChanged(index, index);

	return true;
}

void DataInspectorModel::fetchMore(const QModelIndex& parent)
{
	if (!parent.isValid())
		return;

	DataInspectorNode* parentNode = static_cast<DataInspectorNode*>(parent.internalPointer());
	if (!parentNode->type)
		return;

	const ccc::ast::Node& parentType = resolvePhysicalType(*parentNode->type, m_symbolTable, m_typeNameToDeduplicatedTypeIndex);

	std::vector<std::unique_ptr<DataInspectorNode>> children;
	switch (parentType.descriptor)
	{
		case ccc::ast::ARRAY:
		{
			const ccc::ast::Array& array = parentType.as<ccc::ast::Array>();
			for (s32 i = 0; i < array.element_count; i++)
			{
				std::unique_ptr<DataInspectorNode> element = std::make_unique<DataInspectorNode>();
				element->name = QString("[%1]").arg(i);
				element->type = array.element_type.get();
				element->address = parentNode->address + i * array.element_type->computed_size_bytes;
				children.emplace_back(std::move(element));
			}
			break;
		}
		case ccc::ast::INLINE_STRUCT_OR_UNION:
		{
			const ccc::ast::InlineStructOrUnion& structOrUnion = parentType.as<ccc::ast::InlineStructOrUnion>();
			for (const std::unique_ptr<ccc::ast::Node>& field : structOrUnion.fields)
			{
				std::unique_ptr<DataInspectorNode> childNode = std::make_unique<DataInspectorNode>();
				childNode->name = QString::fromStdString(field->name);
				childNode->type = field.get();
				childNode->address = parentNode->address + field->relative_offset_bytes;
				children.emplace_back(std::move(childNode));
			}
			break;
		}
		case ccc::ast::POINTER:
		{
			u32 address = r5900Debug.read32(parentNode->address);
			if (r5900Debug.isValidAddress(address))
			{
				const ccc::ast::Pointer& pointer = parentType.as<ccc::ast::Pointer>();
				std::unique_ptr<DataInspectorNode> element = std::make_unique<DataInspectorNode>();
				element->name = QString("*%1").arg(address);
				element->type = pointer.value_type.get();
				element->address = address;
				children.emplace_back(std::move(element));
			}
			break;
		}
		case ccc::ast::REFERENCE:
		{
			u32 address = r5900Debug.read32(parentNode->address);
			if (r5900Debug.isValidAddress(address))
			{
				const ccc::ast::Reference& reference = parentType.as<ccc::ast::Reference>();
				std::unique_ptr<DataInspectorNode> element = std::make_unique<DataInspectorNode>();
				element->name = QString("*%1").arg(address);
				element->type = reference.value_type.get();
				element->address = address;
				children.emplace_back(std::move(element));
			}
			break;
		}
		default:
		{
		}
	}

	if (children.empty())
	{
		parentNode->childrenFetched = true;
		return;
	}

	for (std::unique_ptr<DataInspectorNode>& childNode : children)
		childNode->parent = parentNode;

	beginInsertRows(parent, 0, children.size() - 1);
	parentNode->children = std::move(children);
	parentNode->childrenFetched = true;
	endInsertRows();
}

bool DataInspectorModel::canFetchMore(const QModelIndex& parent) const
{
	if (!parent.isValid())
		return false;

	DataInspectorNode* parentNode = static_cast<DataInspectorNode*>(parent.internalPointer());
	if (!parentNode->type)
		return false;

	return nodeHasChildren(*parentNode->type) && !parentNode->childrenFetched;
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
		case ADDRESS:
		{
			return "Address";
		}
		case TYPE:
		{
			return "Type";
		}
		case VALUE:
		{
			return "Value";
		}
	}

	return QVariant();
}

void DataInspectorModel::reset(std::unique_ptr<DataInspectorNode> newRoot)
{
	beginResetModel();
	m_root = std::move(newRoot);
	endResetModel();
}

bool DataInspectorModel::nodeHasChildren(const ccc::ast::Node& type) const
{
	const ccc::ast::Node& physicalType = resolvePhysicalType(type, m_symbolTable, m_typeNameToDeduplicatedTypeIndex);

	bool result = false;
	switch (physicalType.descriptor)
	{
		case ccc::ast::ARRAY:
		{
			const ccc::ast::Array& array = physicalType.as<ccc::ast::Array>();
			result = array.element_count > 0;
			break;
		}
		case ccc::ast::INLINE_STRUCT_OR_UNION:
		{
			const ccc::ast::InlineStructOrUnion& structOrUnion = physicalType.as<ccc::ast::InlineStructOrUnion>();
			result = !structOrUnion.fields.empty() || !structOrUnion.base_classes.empty();
			break;
		}
		case ccc::ast::POINTER:
		{
			result = true;
			break;
		}
		case ccc::ast::REFERENCE:
		{
			result = true;
			break;
		}
		case ccc::ast::SOURCE_FILE:
		{
			const ccc::ast::SourceFile& sourceFile = physicalType.as<ccc::ast::SourceFile>();
			result = !sourceFile.globals.empty();
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
		for (int i = 0; i < (int)node.parent->children.size(); i++)
		{
			if (node.parent->children[i].get() == &node)
				row = i;
		}
	else
		row = 0;

	return createIndex(row, 0, &node);
}

QString DataInspectorModel::typeToString(const ccc::ast::Node& type) const
{
	QString result;

	switch (type.descriptor)
	{
		case ccc::ast::TYPE_NAME:
		{
			const ccc::ast::TypeName& typeName = type.as<ccc::ast::TypeName>();
			return QString::fromStdString(typeName.type_name);
			break;
		}
		case ccc::ast::VARIABLE:
		{
			const ccc::ast::Variable& variable = type.as<ccc::ast::Variable>();
			result = typeToString(*variable.type);
			break;
		}
		default:
		{
			result = ccc::ast::node_type_to_string(type);
		}
	}

	return result;
}

const ccc::ast::Node& resolvePhysicalType(const ccc::ast::Node& type, const ccc::HighSymbolTable& symbolTable, const std::map<std::string, s32>& nameToType)
{
	const ccc::ast::Node* result = &type;
	if (result->descriptor == ccc::ast::VARIABLE)
	{
		result = result->as<ccc::ast::Variable>().type.get();
	}
	for (s32 i = 0; i < 10 && result->descriptor == ccc::ast::TYPE_NAME; i++)
	{
		s32 type_index = ccc::lookup_type(result->as<ccc::ast::TypeName>(), symbolTable, &nameToType);
		if (type_index < 0)
		{
			break;
		}
		result = symbolTable.deduplicated_types.at(type_index).get();
	}
	return *result;
}
