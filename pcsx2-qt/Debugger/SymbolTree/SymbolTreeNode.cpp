// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SymbolTreeNode.h"

#include "DebugTools/ccc/ast.h"

bool SymbolTreeNode::readFromVM(DebugInterface& cpu, const ccc::SymbolDatabase& database)
{
	QVariant new_value;

	const ccc::ast::Node* logical_type = type.lookup_node(database);
	if (logical_type)
	{
		const ccc::ast::Node& physical_type = *resolvePhysicalType(logical_type, database).first;
		new_value = readValueAsVariant(physical_type, cpu, database);
	}

	bool data_changed = false;

	if (new_value != value)
	{
		value = std::move(new_value);
		data_changed = true;
	}

	data_changed |= updateDisplayString(cpu, database);
	data_changed |= updateLiveness(cpu);

	return data_changed;
}

bool SymbolTreeNode::writeToVM(DebugInterface& cpu, const ccc::SymbolDatabase& database)
{
	const ccc::ast::Node* logical_type = type.lookup_node(database);
	if (!logical_type)
		return false;

	const ccc::ast::Node& physical_type = *resolvePhysicalType(logical_type, database).first;

	bool data_changed = false;

	data_changed |= writeValueFromVariant(value, physical_type, cpu);
	data_changed |= updateDisplayString(cpu, database);
	data_changed |= updateLiveness(cpu);

	return data_changed;
}

QVariant SymbolTreeNode::readValueAsVariant(const ccc::ast::Node& physical_type, DebugInterface& cpu, const ccc::SymbolDatabase& database) const
{
	switch (physical_type.descriptor)
	{
		case ccc::ast::BUILTIN:
		{
			const ccc::ast::BuiltIn& builtIn = physical_type.as<ccc::ast::BuiltIn>();
			switch (builtIn.bclass)
			{
				case ccc::ast::BuiltInClass::UNSIGNED_8:
					return (qulonglong)location.read8(cpu);
				case ccc::ast::BuiltInClass::SIGNED_8:
					return (qlonglong)(s8)location.read8(cpu);
				case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					return (qulonglong)location.read8(cpu);
				case ccc::ast::BuiltInClass::BOOL_8:
					return (bool)location.read8(cpu);
				case ccc::ast::BuiltInClass::UNSIGNED_16:
					return (qulonglong)location.read16(cpu);
				case ccc::ast::BuiltInClass::SIGNED_16:
					return (qlonglong)(s16)location.read16(cpu);
				case ccc::ast::BuiltInClass::UNSIGNED_32:
					return (qulonglong)location.read32(cpu);
				case ccc::ast::BuiltInClass::SIGNED_32:
					return (qlonglong)(s32)location.read32(cpu);
				case ccc::ast::BuiltInClass::FLOAT_32:
				{
					u32 value = location.read32(cpu);
					return *reinterpret_cast<float*>(&value);
				}
				case ccc::ast::BuiltInClass::UNSIGNED_64:
					return (qulonglong)location.read64(cpu);
				case ccc::ast::BuiltInClass::SIGNED_64:
					return (qlonglong)(s64)location.read64(cpu);
				case ccc::ast::BuiltInClass::FLOAT_64:
				{
					u64 value = location.read64(cpu);
					return *reinterpret_cast<double*>(&value);
				}
				default:
				{
				}
			}
			break;
		}
		case ccc::ast::ENUM:
			return location.read32(cpu);
		case ccc::ast::POINTER_OR_REFERENCE:
		case ccc::ast::POINTER_TO_DATA_MEMBER:
			return location.read32(cpu);
		default:
		{
		}
	}

	return QVariant();
}

bool SymbolTreeNode::writeValueFromVariant(QVariant value, const ccc::ast::Node& physical_type, DebugInterface& cpu) const
{
	switch (physical_type.descriptor)
	{
		case ccc::ast::BUILTIN:
		{
			const ccc::ast::BuiltIn& built_in = physical_type.as<ccc::ast::BuiltIn>();

			switch (built_in.bclass)
			{
				case ccc::ast::BuiltInClass::UNSIGNED_8:
					location.write8((u8)value.toULongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::SIGNED_8:
					location.write8((u8)(s8)value.toLongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					location.write8((u8)value.toULongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::BOOL_8:
					location.write8((u8)value.toBool(), cpu);
					break;
				case ccc::ast::BuiltInClass::UNSIGNED_16:
					location.write16((u16)value.toULongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::SIGNED_16:
					location.write16((u16)(s16)value.toLongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::UNSIGNED_32:
					location.write32((u32)value.toULongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::SIGNED_32:
					location.write32((u32)(s32)value.toLongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::FLOAT_32:
				{
					float f = value.toFloat();
					location.write32(*reinterpret_cast<u32*>(&f), cpu);
					break;
				}
				case ccc::ast::BuiltInClass::UNSIGNED_64:
					location.write64((u64)value.toULongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::SIGNED_64:
					location.write64((u64)(s64)value.toLongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::FLOAT_64:
				{
					double d = value.toDouble();
					location.write64(*reinterpret_cast<u64*>(&d), cpu);
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
			location.write32((u32)value.toULongLong(), cpu);
			break;
		case ccc::ast::POINTER_OR_REFERENCE:
		case ccc::ast::POINTER_TO_DATA_MEMBER:
			location.write32((u32)value.toULongLong(), cpu);
			break;
		default:
		{
			return false;
		}
	}

	return true;
}

bool SymbolTreeNode::updateDisplayString(DebugInterface& cpu, const ccc::SymbolDatabase& database)
{
	QString result;

	const ccc::ast::Node* logical_type = type.lookup_node(database);
	if (logical_type)
	{
		const ccc::ast::Node& physical_type = *resolvePhysicalType(logical_type, database).first;
		result = generateDisplayString(physical_type, cpu, database, 0);
	}

	if (result.isEmpty())
	{
		// We don't know how to display objects of this type, so just show the
		// first 4 bytes of it as a hex dump.
		u32 value = location.read32(cpu);
		result = QString("%1 %2 %3 %4")
					 .arg(value & 0xff, 2, 16, QChar('0'))
					 .arg((value >> 8) & 0xff, 2, 16, QChar('0'))
					 .arg((value >> 16) & 0xff, 2, 16, QChar('0'))
					 .arg((value >> 24) & 0xff, 2, 16, QChar('0'));
	}

	if (result == display_value)
		return false;

	display_value = std::move(result);

	return true;
}

QString SymbolTreeNode::generateDisplayString(
	const ccc::ast::Node& physical_type, DebugInterface& cpu, const ccc::SymbolDatabase& database, s32 depth) const
{
	s32 max_elements_to_display = 0;
	switch (depth)
	{
		case 0:
			max_elements_to_display = 8;
			break;
		case 1:
			max_elements_to_display = 2;
			break;
	}

	switch (physical_type.descriptor)
	{
		case ccc::ast::ARRAY:
		{
			const ccc::ast::Array& array = physical_type.as<ccc::ast::Array>();

			QString result;
			result += "{";

			s32 elements_to_display = std::min(array.element_count, max_elements_to_display);
			for (s32 i = 0; i < elements_to_display; i++)
			{
				SymbolTreeNode node;
				node.location = location.addOffset(i * array.element_type->size_bytes);

				const ccc::ast::Node& element_type = *resolvePhysicalType(array.element_type.get(), database).first;

				QString element = node.generateDisplayString(element_type, cpu, database, depth + 1);
				if (element.isEmpty())
					element = QString("(%1)").arg(ccc::ast::node_type_to_string(element_type));
				result += element;

				if (i + 1 != array.element_count)
					result += ",";
			}

			if (elements_to_display != array.element_count)
				result += "...";

			result += "}";
			return result;
		}
		case ccc::ast::BUILTIN:
		{
			const ccc::ast::BuiltIn& builtIn = physical_type.as<ccc::ast::BuiltIn>();
			switch (builtIn.bclass)
			{
				case ccc::ast::BuiltInClass::UNSIGNED_8:
					return QString::number(location.read8(cpu));
				case ccc::ast::BuiltInClass::SIGNED_8:
					return QString::number((s8)location.read8(cpu));
				case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					return QString::number(location.read8(cpu));
				case ccc::ast::BuiltInClass::BOOL_8:
					return location.read8(cpu) ? "true" : "false";
				case ccc::ast::BuiltInClass::UNSIGNED_16:
					return QString::number(location.read16(cpu));
				case ccc::ast::BuiltInClass::SIGNED_16:
					return QString::number((s16)location.read16(cpu));
				case ccc::ast::BuiltInClass::UNSIGNED_32:
					return QString::number(location.read32(cpu));
				case ccc::ast::BuiltInClass::SIGNED_32:
					return QString::number((s32)location.read32(cpu));
				case ccc::ast::BuiltInClass::FLOAT_32:
				{
					u32 value = location.read32(cpu);
					return QString::number(*reinterpret_cast<float*>(&value));
				}
				case ccc::ast::BuiltInClass::UNSIGNED_64:
					return QString::number(location.read64(cpu));
				case ccc::ast::BuiltInClass::SIGNED_64:
					return QString::number((s64)location.read64(cpu));
				case ccc::ast::BuiltInClass::FLOAT_64:
				{
					u64 value = location.read64(cpu);
					return QString::number(*reinterpret_cast<double*>(&value));
				}
				case ccc::ast::BuiltInClass::UNSIGNED_128:
				case ccc::ast::BuiltInClass::SIGNED_128:
				case ccc::ast::BuiltInClass::UNQUALIFIED_128:
				case ccc::ast::BuiltInClass::FLOAT_128:
				{
					if (depth > 0)
						return "(128-bit value)";

					QString result;
					for (s32 i = 0; i < 16; i++)
					{
						u8 value = location.addOffset(i).read8(cpu);
						result += QString("%1 ").arg(value, 2, 16, QChar('0'));
						if ((i + 1) % 4 == 0)
							result += " ";
					}

					return result;
				}
				default:
				{
				}
			}
			break;
		}
		case ccc::ast::ENUM:
		{
			s32 value = (s32)location.read32(cpu);
			const auto& enum_type = physical_type.as<ccc::ast::Enum>();
			for (auto [test_value, name] : enum_type.constants)
			{
				if (test_value == value)
					return QString::fromStdString(name);
			}

			break;
		}
		case ccc::ast::POINTER_OR_REFERENCE:
		{
			const auto& pointer_or_reference = physical_type.as<ccc::ast::PointerOrReference>();

			QString result = QString::number(location.read32(cpu), 16);

			// For char* nodes add the value of the string to the output.
			if (pointer_or_reference.is_pointer)
			{
				const ccc::ast::Node* value_type =
					resolvePhysicalType(pointer_or_reference.value_type.get(), database).first;
				if (value_type->name == "char")
				{
					u32 pointer = location.read32(cpu);
					const char* string = cpu.stringFromPointer(pointer);
					if (string)
						result += QString(" \"%1\"").arg(string);
				}
			}

			return result;
		}
		case ccc::ast::POINTER_TO_DATA_MEMBER:
		{
			return QString::number(location.read32(cpu), 16);
		}
		case ccc::ast::STRUCT_OR_UNION:
		{
			const ccc::ast::StructOrUnion& struct_or_union = physical_type.as<ccc::ast::StructOrUnion>();

			QString result;
			result += "{";

			s32 fields_to_display = std::min((s32)struct_or_union.fields.size(), max_elements_to_display);
			for (s32 i = 0; i < fields_to_display; i++)
			{
				QString field_name = QString::fromStdString(struct_or_union.fields[i]->name);

				SymbolTreeNode node;
				node.location = location.addOffset(struct_or_union.fields[i]->offset_bytes);

				const ccc::ast::Node& field_type = *resolvePhysicalType(struct_or_union.fields[i].get(), database).first;

				QString field_value = node.generateDisplayString(field_type, cpu, database, depth + 1);
				if (field_value.isEmpty())
					field_value = QString("(%1)").arg(ccc::ast::node_type_to_string(field_type));
				result += QString(".%1=%2").arg(field_name).arg(field_value);

				if (i + 1 != (s32)struct_or_union.fields.size())
					result += ",";
			}

			if (fields_to_display != (s32)struct_or_union.fields.size())
				result += "...";

			result += "}";
			return result;
		}
		default:
		{
		}
	}

	return QString();
}

bool SymbolTreeNode::updateLiveness(DebugInterface& cpu)
{
	std::optional<bool> new_liveness;
	if (live_range.low.valid() && live_range.high.valid())
	{
		u32 pc = cpu.getPC();
		new_liveness = pc >= live_range.low && pc < live_range.high;
	}

	if (new_liveness == liveness)
		return false;

	liveness = new_liveness;

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
		if (lhs->tag != rhs->tag)
			return lhs->tag < rhs->tag;
		if (sort_by_if_type_is_known && lhs->type.valid() != rhs->type.valid())
			return lhs->type.valid() > rhs->type.valid();

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
