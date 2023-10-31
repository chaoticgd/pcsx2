#include "DataInspectorModel.h"

DataInspectorModel::DataInspectorModel(std::unique_ptr<TreeNode> initialRoot, const ccc::HighSymbolTable& symbolTable, QObject* parent)
	: m_root(std::move(initialRoot))
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
		parentNode = (TreeNode*)parent.internalPointer();
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

	TreeNode* childNode = (TreeNode*)index.internalPointer();
	TreeNode* parentNode = childNode->parent;
	if (!parentNode)
		return QModelIndex();

	int row = 0;
	if (parentNode->parent)
		for (int i = 0; i < (int)parentNode->parent->children.size(); i++)
		{
			if (parentNode->parent->children[i].get() == parentNode)
				row = i;
		}
	else
		row = 0;

	return createIndex(row, 0, parentNode);
}

int DataInspectorModel::rowCount(const QModelIndex& parent) const
{
	if (parent.column() > 0)
		return 0;

	TreeNode* node;
	if (parent.isValid())
		node = (TreeNode*)parent.internalPointer();
	else
		node = m_root.get();

	return (int)node->children.size();
}

int DataInspectorModel::columnCount(const QModelIndex& parent) const
{
	return 2;
}

bool DataInspectorModel::hasChildren(const QModelIndex& parent) const
{
	if (!parent.isValid())
		return true;

	TreeNode* parentNode = (TreeNode*)parent.internalPointer();
	return !parentNode->isLeaf;
}

QVariant DataInspectorModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	if (role != Qt::DisplayRole)
		return QVariant();

	TreeNode* node = (TreeNode*)index.internalPointer();
	if (!node->type)
		return QVariant();

	if (index.column() == 0)
	{
		if (node->type->descriptor == ccc::ast::SOURCE_FILE)
		{
			const ccc::ast::SourceFile& sourceFile = node->type->as<ccc::ast::SourceFile>();
			if (!sourceFile.relative_path.empty())
				return QString::fromStdString(node->type->as<ccc::ast::SourceFile>().relative_path);
			else
				return QString::fromStdString(node->type->as<ccc::ast::SourceFile>().full_path);
		}
		else
			return QString::fromStdString(node->type->name);
	}
	else
	{
		return QVariant("data");
	}
}

void DataInspectorModel::fetchMore(const QModelIndex& parent)
{
	if (!parent.isValid())
		return;

	TreeNode* parentNode = (TreeNode*)parent.internalPointer();
	if (!parentNode->type)
		return;

	std::vector<std::unique_ptr<TreeNode>> children = createChildren(*parentNode->type, parent);
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

	TreeNode* parentNode = (TreeNode*)parent.internalPointer();
	return !parentNode->isLeaf && !parentNode->childrenFetched;
}

Qt::ItemFlags DataInspectorModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;

	return QAbstractItemModel::flags(index);
}

QVariant DataInspectorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
		return QVariant();

	if (section == 0)
		return QString("Name");
	else
		return QString("Value");
}

std::vector<std::unique_ptr<DataInspectorModel::TreeNode>> DataInspectorModel::createChildren(const ccc::ast::Node& type, const QModelIndex& parent)
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
				std::unique_ptr<TreeNode> node = std::make_unique<TreeNode>();
				node->name = QString::fromStdString(field->name);
				node->type = field.get();
				result.emplace_back(std::move(node));
			}
			break;
		}
		case ccc::ast::POINTER:
		{
			break;
		}
		case ccc::ast::POINTER_TO_DATA_MEMBER:
		{
			break;
		}
		case ccc::ast::REFERENCE:
		{
			break;
		}
		case ccc::ast::SOURCE_FILE:
		{
			const ccc::ast::SourceFile& sourceFile = type.as<ccc::ast::SourceFile>();
			for (const std::unique_ptr<ccc::ast::Node>& global : sourceFile.globals)
			{
				std::unique_ptr<TreeNode> node = std::make_unique<TreeNode>();
				node->name = QString::fromStdString(sourceFile.name);
				node->type = global.get();
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
				result = createChildren(*m_symbolTable.deduplicated_types.at(index).get(), parent);
			}
			break;
		}
		case ccc::ast::VARIABLE:
		{
			const ccc::ast::Variable& variable = type.as<ccc::ast::Variable>();
			result = createChildren(*variable.type.get(), parent);
			break;
		}
	}

	return result;
}
