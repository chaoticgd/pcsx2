#include "DataInspectorModel.h"

DataInspectorModel::DataInspectorModel(std::unique_ptr<TreeNode> initialRoot, const ccc::HighSymbolTable& symbolTable, QObject* parent)
	: QAbstractItemModel(parent)
	, m_root(std::move(initialRoot))
	, m_symbolTable(symbolTable)
	, m_typeNameToDeduplicatedTypeIndex(ccc::build_type_name_to_deduplicated_type_index_map(m_symbolTable))
{
}

DataInspectorModel::~DataInspectorModel() {}

QModelIndex DataInspectorModel::index(int row, int column, const QModelIndex& parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	TreeNode* parentNode;
	if (parent.isValid())
		parentNode = static_cast<TreeNode*>(parent.internalPointer());
	else
		parentNode = m_root.get();

	TreeNode* childNode = parentNode->children.at(row).get();
	if (!childNode)
		return QModelIndex();

	return createIndex(row, column, childNode);
}

QModelIndex DataInspectorModel::parent(const QModelIndex& index) const
{
	if (!index.isValid())
		return QModelIndex();

	TreeNode* childNode = static_cast<TreeNode*>(index.internalPointer());
	TreeNode* parentNode = childNode->parent;
	if (!parentNode)
		return QModelIndex();

	return indexFromNode(*parentNode);
}

int DataInspectorModel::rowCount(const QModelIndex& parent) const
{
	if (parent.column() > 0)
		return 0;

	TreeNode* node;
	if (parent.isValid())
		node = static_cast<TreeNode*>(parent.internalPointer());
	else
		node = m_root.get();

	return (int)node->children.size();
}

int DataInspectorModel::columnCount(const QModelIndex& parent) const
{
	return 3;
}

bool DataInspectorModel::hasChildren(const QModelIndex& parent) const
{
	if (!parent.isValid())
		return true;

	TreeNode* parentNode = static_cast<TreeNode*>(parent.internalPointer());
	return nodeHasChildren(*parentNode->type);
}

QVariant DataInspectorModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || role != Qt::DisplayRole)
		return QVariant();

	TreeNode* node = static_cast<TreeNode*>(index.internalPointer());
	if (!node->type)
		return QVariant();

	switch (index.column())
	{
		case NAME:
		{
			return node->name;
		}
		case TYPE:
		{
			return typeToString(*node->type);
		}
		case VALUE:
		{
			return readData(node->address, *node->type);
		}
	}

	return QVariant();
}

bool DataInspectorModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (!index.isValid())
		return false;

	TreeNode* node = static_cast<TreeNode*>(index.internalPointer());
	if (!node->type)
		return false;

	writeData(node->address, value, *node->type);
	emit dataChanged(index, index);

	return true;
}

void DataInspectorModel::fetchMore(const QModelIndex& parent)
{
	if (!parent.isValid())
		return;

	TreeNode* parentNode = static_cast<TreeNode*>(parent.internalPointer());
	if (!parentNode->type)
		return;

	std::vector<std::unique_ptr<TreeNode>> children = createChildren(*parentNode->type, *parentNode);
	if (children.empty())
	{
		parentNode->childrenFetched = true;
		return;
	}

	for (std::unique_ptr<TreeNode>& childNode : children)
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

	TreeNode* parentNode = static_cast<TreeNode*>(parent.internalPointer());
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

QVariant DataInspectorModel::readData(u32 address, const ccc::ast::Node& type) const
{
	QVariant result;

	switch (type.descriptor)
	{
		case ccc::ast::ARRAY:
		{
		}
		case ccc::ast::BITFIELD:
		{
			break;
		}
		case ccc::ast::BUILTIN:
		{
			const ccc::ast::BuiltIn& builtIn = type.as<ccc::ast::BuiltIn>();

			switch (builtIn.bclass)
			{
				case ccc::BuiltInClass::UNSIGNED_8:
				{
					result = static_cast<u8>(r5900Debug.read8(address));
					break;
				}
				case ccc::BuiltInClass::SIGNED_8:
				{
					result = static_cast<s8>(r5900Debug.read8(address));
					break;
				}
				case ccc::BuiltInClass::UNQUALIFIED_8:
				{
					result = static_cast<char>(r5900Debug.read8(address));
					break;
				}
				case ccc::BuiltInClass::BOOL_8:
				{
					result = static_cast<bool>(r5900Debug.read8(address));
					break;
				}
				case ccc::BuiltInClass::UNSIGNED_16:
				{
					result = static_cast<u16>(r5900Debug.read16(address));
					break;
				}
				case ccc::BuiltInClass::SIGNED_16:
				{
					result = static_cast<s16>(r5900Debug.read16(address));
					break;
				}
				case ccc::BuiltInClass::UNSIGNED_32:
				{
					result = static_cast<u32>(r5900Debug.read32(address));
					break;
				}
				case ccc::BuiltInClass::SIGNED_32:
				{
					result = static_cast<s32>(r5900Debug.read32(address));
					break;
				}
				case ccc::BuiltInClass::FLOAT_32:
				{
					result = static_cast<float>(r5900Debug.read32(address));
					break;
				}
				case ccc::BuiltInClass::UNSIGNED_64:
				{
					break;
				}
				case ccc::BuiltInClass::SIGNED_64:
				{
					break;
				}
				case ccc::BuiltInClass::FLOAT_64:
				{
					break;
				}
				case ccc::BuiltInClass::UNSIGNED_128:
				{
					break;
				}
				case ccc::BuiltInClass::SIGNED_128:
				{
					break;
				}
				case ccc::BuiltInClass::UNQUALIFIED_128:
				{
					break;
				}
				case ccc::BuiltInClass::FLOAT_128:
				{
					break;
				}
				default:
				{
				}
			}
			break;
		}
		case ccc::ast::DATA:
		{
			break;
		}
		case ccc::ast::FUNCTION_DEFINITION:
		{
			break;
		}
		case ccc::ast::FUNCTION_TYPE:
		{
			break;
		}
		case ccc::ast::INITIALIZER_LIST:
		{
			break;
		}
		case ccc::ast::INLINE_ENUM:
		{
			break;
		}
		case ccc::ast::INLINE_STRUCT_OR_UNION:
		{
			break;
		}
		case ccc::ast::POINTER:
		{
			result = r5900Debug.read32(address);
			break;
		}
		case ccc::ast::POINTER_TO_DATA_MEMBER:
		{
			break;
		}
		case ccc::ast::REFERENCE:
		{
			result = r5900Debug.read32(address);
			break;
		}
		case ccc::ast::SOURCE_FILE:
		{
			break;
		}
		case ccc::ast::TYPE_NAME:
		{
			const ccc::ast::TypeName& typeName = type.as<ccc::ast::TypeName>();
			s32 index = ccc::lookup_type(typeName, m_symbolTable, &m_typeNameToDeduplicatedTypeIndex);
			if (index > -1)
			{
				result = readData(address, *m_symbolTable.deduplicated_types.at(index));
			}
			break;
		}
		case ccc::ast::VARIABLE:
		{
			const ccc::ast::Variable& variable = type.as<ccc::ast::Variable>();
			result = readData(address, *variable.type);
			break;
		}
	}

	return result;
}

void DataInspectorModel::writeData(u32 address, const QVariant& value, const ccc::ast::Node& type) const
{
	switch (type.descriptor)
	{
		case ccc::ast::ARRAY:
		{
		}
		case ccc::ast::BITFIELD:
		{
			break;
		}
		case ccc::ast::BUILTIN:
		{
			const ccc::ast::BuiltIn& builtIn = type.as<ccc::ast::BuiltIn>();

			switch (builtIn.bclass)
			{
				case ccc::BuiltInClass::UNSIGNED_8:
				{
					r5900Debug.write8(address, value.toInt());
					break;
				}
				case ccc::BuiltInClass::SIGNED_8:
				{
					r5900Debug.write8(address, value.toInt());
					break;
				}
				case ccc::BuiltInClass::UNQUALIFIED_8:
				{
					r5900Debug.write8(address, value.toInt());
					break;
				}
				case ccc::BuiltInClass::BOOL_8:
				{
					r5900Debug.write8(address, value.toInt());
					break;
				}
				case ccc::BuiltInClass::UNSIGNED_16:
				{
					r5900Debug.write8(address + 0, value.toInt());
					r5900Debug.write8(address + 1, value.toInt() >> 8);
					break;
				}
				case ccc::BuiltInClass::SIGNED_16:
				{
					r5900Debug.write8(address + 0, value.toInt());
					r5900Debug.write8(address + 1, value.toInt() >> 8);
					break;
				}
				case ccc::BuiltInClass::UNSIGNED_32:
				{
					r5900Debug.write32(address, value.toInt());
					break;
				}
				case ccc::BuiltInClass::SIGNED_32:
				{
					r5900Debug.write32(address, value.toInt());
					break;
				}
				case ccc::BuiltInClass::FLOAT_32:
				{
					r5900Debug.write32(address, value.toInt());
					break;
				}
				case ccc::BuiltInClass::UNSIGNED_64:
				{
					break;
				}
				case ccc::BuiltInClass::SIGNED_64:
				{
					break;
				}
				case ccc::BuiltInClass::FLOAT_64:
				{
					break;
				}
				case ccc::BuiltInClass::UNSIGNED_128:
				{
					break;
				}
				case ccc::BuiltInClass::SIGNED_128:
				{
					break;
				}
				case ccc::BuiltInClass::UNQUALIFIED_128:
				{
					break;
				}
				case ccc::BuiltInClass::FLOAT_128:
				{
					break;
				}
				default:
				{
				}
			}
			break;
		}
		case ccc::ast::DATA:
		{
			break;
		}
		case ccc::ast::FUNCTION_DEFINITION:
		{
			break;
		}
		case ccc::ast::FUNCTION_TYPE:
		{
			break;
		}
		case ccc::ast::INITIALIZER_LIST:
		{
			break;
		}
		case ccc::ast::INLINE_ENUM:
		{
			break;
		}
		case ccc::ast::INLINE_STRUCT_OR_UNION:
		{
			break;
		}
		case ccc::ast::POINTER:
		{
			r5900Debug.write32(address, value.toInt());
			break;
		}
		case ccc::ast::POINTER_TO_DATA_MEMBER:
		{
			break;
		}
		case ccc::ast::REFERENCE:
		{
			r5900Debug.write32(address, value.toInt());
			break;
		}
		case ccc::ast::SOURCE_FILE:
		{
			break;
		}
		case ccc::ast::TYPE_NAME:
		{
			const ccc::ast::TypeName& typeName = type.as<ccc::ast::TypeName>();
			s32 index = ccc::lookup_type(typeName, m_symbolTable, &m_typeNameToDeduplicatedTypeIndex);
			if (index > -1)
			{
				writeData(address, value, *m_symbolTable.deduplicated_types.at(index));
			}
			break;
		}
		case ccc::ast::VARIABLE:
		{
			const ccc::ast::Variable& variable = type.as<ccc::ast::Variable>();
			writeData(address, value, *variable.type);
			break;
		}
	}
}

bool DataInspectorModel::nodeHasChildren(const ccc::ast::Node& type) const
{
	bool result = false;

	switch (type.descriptor)
	{
		case ccc::ast::ARRAY:
		{
			const ccc::ast::Array& array = type.as<ccc::ast::Array>();
			result = array.element_count > 0;
		}
		case ccc::ast::BITFIELD:
		{
			break;
		}
		case ccc::ast::BUILTIN:
		{
			break;
		}
		case ccc::ast::DATA:
		{
			break;
		}
		case ccc::ast::FUNCTION_DEFINITION:
		{
			break;
		}
		case ccc::ast::FUNCTION_TYPE:
		{
			break;
		}
		case ccc::ast::INITIALIZER_LIST:
		{
			break;
		}
		case ccc::ast::INLINE_ENUM:
		{
			break;
		}
		case ccc::ast::INLINE_STRUCT_OR_UNION:
		{
			const ccc::ast::InlineStructOrUnion& structOrUnion = type.as<ccc::ast::InlineStructOrUnion>();
			result = !structOrUnion.fields.empty() || !structOrUnion.base_classes.empty();
			break;
		}
		case ccc::ast::POINTER:
		{
			result = true;
			break;
		}
		case ccc::ast::POINTER_TO_DATA_MEMBER:
		{
			break;
		}
		case ccc::ast::REFERENCE:
		{
			result = true;
			break;
		}
		case ccc::ast::SOURCE_FILE:
		{
			const ccc::ast::SourceFile& sourceFile = type.as<ccc::ast::SourceFile>();
			result = !sourceFile.globals.empty();
			break;
		}
		case ccc::ast::TYPE_NAME:
		{
			const ccc::ast::TypeName& typeName = type.as<ccc::ast::TypeName>();
			s32 index = ccc::lookup_type(typeName, m_symbolTable, &m_typeNameToDeduplicatedTypeIndex);
			if (index > -1)
			{
				result = nodeHasChildren(*m_symbolTable.deduplicated_types.at(index));
			}
			break;
		}
		case ccc::ast::VARIABLE:
		{
			const ccc::ast::Variable& variable = type.as<ccc::ast::Variable>();
			result = nodeHasChildren(*variable.type);
			break;
		}
	}

	return result;
}

QModelIndex DataInspectorModel::indexFromNode(const TreeNode& node) const
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

std::vector<std::unique_ptr<DataInspectorModel::TreeNode>> DataInspectorModel::createChildren(const ccc::ast::Node& type, const TreeNode& parentNode)
{
	std::vector<std::unique_ptr<TreeNode>> result;

	switch (type.descriptor)
	{
		case ccc::ast::ARRAY:
		{
			const ccc::ast::Array& array = type.as<ccc::ast::Array>();
			for (s32 i = 0; i < array.element_count; i++)
			{
				std::unique_ptr<TreeNode> element = std::make_unique<TreeNode>();
				element->name = QString("[%1]").arg(i);
				element->type = array.element_type.get();
				element->address = parentNode.address + i * array.element_type->computed_size_bytes;
				result.emplace_back(std::move(element));
			}
			break;
		}
		case ccc::ast::BITFIELD:
		{
			break;
		}
		case ccc::ast::BUILTIN:
		{
			break;
		}
		case ccc::ast::DATA:
		{
			break;
		}
		case ccc::ast::FUNCTION_DEFINITION:
		{
			break;
		}
		case ccc::ast::FUNCTION_TYPE:
		{
			break;
		}
		case ccc::ast::INITIALIZER_LIST:
		{
			break;
		}
		case ccc::ast::INLINE_ENUM:
		{
			break;
		}
		case ccc::ast::INLINE_STRUCT_OR_UNION:
		{
			const ccc::ast::InlineStructOrUnion& structOrUnion = type.as<ccc::ast::InlineStructOrUnion>();
			for (const std::unique_ptr<ccc::ast::Node>& field : structOrUnion.fields)
			{
				std::unique_ptr<TreeNode> childNode = std::make_unique<TreeNode>();
				childNode->name = QString::fromStdString(field->name);
				childNode->type = field.get();
				childNode->address = parentNode.address + field->relative_offset_bytes;
				result.emplace_back(std::move(childNode));
			}
			break;
		}
		case ccc::ast::POINTER:
		{
			u32 address = r5900Debug.read32(parentNode.address);
			if(r5900Debug.isValidAddress(address)) {
				const ccc::ast::Pointer& pointer = type.as<ccc::ast::Pointer>();
				std::unique_ptr<TreeNode> element = std::make_unique<TreeNode>();
				element->name = QString("*%1").arg(address);
				element->type = pointer.value_type.get();
				element->address = address;
				result.emplace_back(std::move(element));
			}
			break;
		}
		case ccc::ast::POINTER_TO_DATA_MEMBER:
		{
			break;
		}
		case ccc::ast::REFERENCE:
		{
			u32 address = r5900Debug.read32(parentNode.address);
			if(r5900Debug.isValidAddress(address)) {
				const ccc::ast::Reference& reference = type.as<ccc::ast::Reference>();
				std::unique_ptr<TreeNode> element = std::make_unique<TreeNode>();
				element->name = QString("*%1").arg(address);
				element->type = reference.value_type.get();
				element->address = address;
				result.emplace_back(std::move(element));
			}
			break;
		}
		case ccc::ast::SOURCE_FILE:
		{
			const ccc::ast::SourceFile& sourceFile = type.as<ccc::ast::SourceFile>();
			for (const std::unique_ptr<ccc::ast::Node>& global : sourceFile.globals)
			{
				std::unique_ptr<TreeNode> node = std::make_unique<TreeNode>();
				node->name = QString::fromStdString(global->name);
				node->type = global.get();
				node->address = global->as<ccc::ast::Variable>().storage.global_address;
				result.emplace_back(std::move(node));
			}
			break;
		}
		case ccc::ast::TYPE_NAME:
		{
			const ccc::ast::TypeName& typeName = type.as<ccc::ast::TypeName>();
			s32 index = ccc::lookup_type(typeName, m_symbolTable, &m_typeNameToDeduplicatedTypeIndex);
			if (index > -1)
			{
				result = createChildren(*m_symbolTable.deduplicated_types.at(index), parentNode);
			}
			break;
		}
		case ccc::ast::VARIABLE:
		{
			const ccc::ast::Variable& variable = type.as<ccc::ast::Variable>();
			result = createChildren(*variable.type, parentNode);
			break;
		}
	}

	return result;
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
