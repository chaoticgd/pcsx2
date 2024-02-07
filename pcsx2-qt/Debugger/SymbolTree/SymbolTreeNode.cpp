// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SymbolTreeNode.h"

#include "DebugTools/ccc/ast.h"

QString SymbolTreeNode::toString(const ccc::ast::Node& type, const ccc::SymbolDatabase* database)
{
	switch (type.descriptor)
	{
		case ccc::ast::BUILTIN:
		{
			const ccc::ast::BuiltIn& builtIn = type.as<ccc::ast::BuiltIn>();
			switch (builtIn.bclass)
			{
				case ccc::ast::BuiltInClass::UNSIGNED_8:
					return QString::number(location.read8());
				case ccc::ast::BuiltInClass::SIGNED_8:
					return QString::number((s8)location.read8());
				case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					return QString::number(location.read8());
				case ccc::ast::BuiltInClass::BOOL_8:
					return QString::number(location.read8());
				case ccc::ast::BuiltInClass::UNSIGNED_16:
					return QString::number(location.read16());
				case ccc::ast::BuiltInClass::SIGNED_16:
					return QString::number((s16)location.read16());
				case ccc::ast::BuiltInClass::UNSIGNED_32:
					return QString::number(location.read32());
				case ccc::ast::BuiltInClass::SIGNED_32:
					return QString::number((s32)location.read32());
				case ccc::ast::BuiltInClass::FLOAT_32:
				{
					u32 value = location.read32();
					return QString::number(*reinterpret_cast<float*>(&value));
				}
				case ccc::ast::BuiltInClass::UNSIGNED_64:
					return QString::number(location.read64());
				case ccc::ast::BuiltInClass::SIGNED_64:
					return QString::number((s64)location.read64());
				case ccc::ast::BuiltInClass::FLOAT_64:
				{
					u64 value = location.read64();
					return QString::number(*reinterpret_cast<double*>(&value));
				}
				default:
				{
				}
			}
			break;
		}
		case ccc::ast::ENUM:
		{
			s32 value = (s32)location.read32();
			const auto& enum_type = type.as<ccc::ast::Enum>();
			for (auto [test_value, name] : enum_type.constants)
			{
				if (test_value == value)
					return QString::fromStdString(name);
			}
		}
		case ccc::ast::POINTER_OR_REFERENCE:
		{
			const auto& pointer_or_reference = type.as<ccc::ast::PointerOrReference>();

			QString result = QString::number(location.read32(), 16);

			// For char* nodes add the value of the string to the output.
			if (pointer_or_reference.is_pointer && database)
			{
				const ccc::ast::Node* value_type =
					resolvePhysicalType(pointer_or_reference.value_type.get(), *database).first;
				if (value_type->name == "char")
				{
					u32 pointer = location.read32();
					const char* string = location.cpu().stringFromPointer(pointer);
					if (string)
						result += QString(" \"%1\"").arg(string);
				}
			}

			return result;
		}
		default:
		{
		}
	}
	return QString();
}

QVariant SymbolTreeNode::toVariant(const ccc::ast::Node& type)
{
	switch (type.descriptor)
	{
		case ccc::ast::BUILTIN:
		{
			const ccc::ast::BuiltIn& builtIn = type.as<ccc::ast::BuiltIn>();
			switch (builtIn.bclass)
			{
				case ccc::ast::BuiltInClass::UNSIGNED_8:
					return (qulonglong)location.read8();
				case ccc::ast::BuiltInClass::SIGNED_8:
					return (qlonglong)(s8)location.read8();
				case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					return (qulonglong)location.read8();
				case ccc::ast::BuiltInClass::BOOL_8:
					return (bool)location.read8();
				case ccc::ast::BuiltInClass::UNSIGNED_16:
					return (qulonglong)location.read16();
				case ccc::ast::BuiltInClass::SIGNED_16:
					return (qlonglong)(s16)location.read16();
				case ccc::ast::BuiltInClass::UNSIGNED_32:
					return (qulonglong)location.read32();
				case ccc::ast::BuiltInClass::SIGNED_32:
					return (qlonglong)(s32)location.read32();
				case ccc::ast::BuiltInClass::FLOAT_32:
				{
					u32 value = location.read32();
					return *reinterpret_cast<float*>(&value);
				}
				case ccc::ast::BuiltInClass::UNSIGNED_64:
					return (qulonglong)location.read64();
				case ccc::ast::BuiltInClass::SIGNED_64:
					return (qlonglong)(s64)location.read64();
				case ccc::ast::BuiltInClass::FLOAT_64:
				{
					u64 value = location.read64();
					return *reinterpret_cast<double*>(&value);
				}
				default:
				{
				}
			}
			break;
		}
		case ccc::ast::ENUM:
			return location.read32();
		case ccc::ast::POINTER_OR_REFERENCE:
			return location.read32();
		default:
		{
		}
	}
	return QVariant();
}

bool SymbolTreeNode::fromVariant(QVariant value, const ccc::ast::Node& type)
{
	switch (type.descriptor)
	{
		case ccc::ast::BUILTIN:
		{
			const ccc::ast::BuiltIn& built_in = type.as<ccc::ast::BuiltIn>();

			switch (built_in.bclass)
			{
				case ccc::ast::BuiltInClass::UNSIGNED_8:
					location.write8((u8)value.toULongLong());
					break;
				case ccc::ast::BuiltInClass::SIGNED_8:
					location.write8((u8)(s8)value.toLongLong());
					break;
				case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					location.write8((u8)value.toULongLong());
					break;
				case ccc::ast::BuiltInClass::BOOL_8:
					location.write8((u8)value.toBool());
					break;
				case ccc::ast::BuiltInClass::UNSIGNED_16:
					location.write16((u16)value.toULongLong());
					break;
				case ccc::ast::BuiltInClass::SIGNED_16:
					location.write16((u16)(s16)value.toLongLong());
					break;
				case ccc::ast::BuiltInClass::UNSIGNED_32:
					location.write32((u32)value.toULongLong());
					break;
				case ccc::ast::BuiltInClass::SIGNED_32:
					location.write32((u32)(s32)value.toLongLong());
					break;
				case ccc::ast::BuiltInClass::FLOAT_32:
				{
					float f = value.toFloat();
					location.write32(*reinterpret_cast<u32*>(&f));
					break;
				}
				case ccc::ast::BuiltInClass::UNSIGNED_64:
					location.write64((u64)value.toULongLong());
					break;
				case ccc::ast::BuiltInClass::SIGNED_64:
					location.write64((u64)(s64)value.toLongLong());
					break;
				case ccc::ast::BuiltInClass::FLOAT_64:
				{
					double d = value.toDouble();
					location.write64(*reinterpret_cast<u64*>(&d));
					break;
				}
				default:
				{
					return false;
				}
			}
			break;
		}
		case ccc::ast::ENUM:
			location.write32((u32)value.toULongLong());
			break;
		case ccc::ast::POINTER_OR_REFERENCE:
			location.write32((u32)value.toULongLong());
			break;
		default:
		{
			return false;
		}
	}

	return true;
}

const SymbolTreeNode* SymbolTreeNode::parent() const
{
	return m_parent;
}

const std::vector<std::unique_ptr<SymbolTreeNode>>& SymbolTreeNode::children() const
{
	return m_children;
}

bool SymbolTreeNode::childrenFetched() const
{
	return m_children_fetched;
}

void SymbolTreeNode::setChildren(std::vector<std::unique_ptr<SymbolTreeNode>> new_children)
{
	for (std::unique_ptr<SymbolTreeNode>& child : new_children)
		child->m_parent = this;
	m_children = std::move(new_children);
	m_children_fetched = true;
}

void SymbolTreeNode::insertChildren(std::vector<std::unique_ptr<SymbolTreeNode>> new_children)
{
	for (std::unique_ptr<SymbolTreeNode>& child : new_children)
		child->m_parent = this;
	m_children.insert(m_children.end(),
		std::make_move_iterator(new_children.begin()),
		std::make_move_iterator(new_children.end()));
	m_children_fetched = true;
}

void SymbolTreeNode::emplaceChild(std::unique_ptr<SymbolTreeNode> new_child)
{
	new_child->m_parent = this;
	m_children.emplace_back(std::move(new_child));
	m_children_fetched = true;
}

void SymbolTreeNode::clearChildren()
{
	m_children.clear();
	m_children_fetched = false;
}

void SymbolTreeNode::sortChildrenRecursively(bool sort_by_if_type_is_known)
{
	auto comparator = [&](const std::unique_ptr<SymbolTreeNode>& lhs, const std::unique_ptr<SymbolTreeNode>& rhs) -> bool {
		// Sort the nodes that actually have type information to the top since
		// these will be the most useful for editing.
		if (sort_by_if_type_is_known && lhs->type.valid() != rhs->type.valid())
			return lhs->type.valid() > rhs->type.valid();
		// Next, sort nodes that have a valid location to the top.
		return lhs->location < rhs->location;
	};

	std::sort(m_children.begin(), m_children.end(), comparator);

	for (std::unique_ptr<SymbolTreeNode>& child : m_children)
		child->sortChildrenRecursively(sort_by_if_type_is_known);
}

std::pair<const ccc::ast::Node*, const ccc::DataType*> resolvePhysicalType(const ccc::ast::Node* type, const ccc::SymbolDatabase& database)
{
	const ccc::DataType* symbol = nullptr;
	for (s32 i = 0; i < 10 && type->descriptor == ccc::ast::TYPE_NAME; i++)
	{
		const ccc::DataType* data_type = database.data_types.symbol_from_handle(type->as<ccc::ast::TypeName>().data_type_handle);
		if (!data_type || !data_type->type())
			break;
		type = data_type->type();
		symbol = data_type;
	}
	return std::pair(type, symbol);
}
