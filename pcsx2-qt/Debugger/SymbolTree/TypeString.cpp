// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "TypeString.h"

#include <QtWidgets/QApplication>

std::unique_ptr<ccc::ast::Node> stringToType(std::string_view string, const ccc::SymbolDatabase& database, QString& error_out)
{
	size_t i = string.size();

	// Parse array subscripts e.g. 'float[4][4]'.
	std::vector<s32> array_subscripts;
	for (; i > 0; i--)
	{
		if (string[i - 1] != ']' || i < 2)
			break;

		size_t j = i - 1;
		for (; j > 0; j--)
			if (string[j - 1] < '0' || string[j - 1] > '9')
				break;

		if (string[j - 1] != '[')
			break;

		s32 element_count = atoi(&string[j]);
		if (element_count < 0 || element_count > 1024 * 1024)
		{
			error_out = QCoreApplication::tr("Invalid array subscript.");
			return nullptr;
		}
		array_subscripts.emplace_back(element_count);

		i = j;
	}

	// Parse pointer characters e.g. 'char*&'.
	std::vector<char> pointer_characters;
	for (; i > 0 && string[i - 1] == '*' || string[i - 1] == '&'; i--)
	{
		pointer_characters.emplace_back(string[i - 1]);
	}

	// Lookup the type.
	std::string type_name_string(string.data(), string.data() + i);
	if (type_name_string.empty())
	{
		error_out = QCoreApplication::tr("No type name provided.");
		return nullptr;
	}

	ccc::DataTypeHandle handle = database.data_types.first_handle_from_name(type_name_string);
	const ccc::DataType* data_type = database.data_types.symbol_from_handle(handle);
	if (!data_type || !data_type->type())
	{
		error_out = QCoreApplication::tr("Type '%1' not found.").arg(QString::fromStdString(type_name_string));
		return nullptr;
	}

	std::unique_ptr<ccc::ast::Node> result;

	// Create the AST.
	std::unique_ptr<ccc::ast::TypeName> type_name = std::make_unique<ccc::ast::TypeName>();
	type_name->computed_size_bytes = data_type->type()->computed_size_bytes;
	type_name->data_type_handle = data_type->handle();
	type_name->source = ccc::ast::TypeNameSource::REFERENCE;
	result = std::move(type_name);

	for (i = pointer_characters.size(); i > 0; i--)
	{
		std::unique_ptr<ccc::ast::PointerOrReference> pointer_or_reference = std::make_unique<ccc::ast::PointerOrReference>();
		pointer_or_reference->computed_size_bytes = 4;
		pointer_or_reference->is_pointer = pointer_characters[i - 1] == '*';
		pointer_or_reference->value_type = std::move(result);
		result = std::move(pointer_or_reference);
	}

	for (s32 element_count : array_subscripts)
	{
		std::unique_ptr<ccc::ast::Array> array = std::make_unique<ccc::ast::Array>();
		array->computed_size_bytes = element_count * result->computed_size_bytes;
		array->element_type = std::move(result);
		array->element_count = element_count;
		result = std::move(array);
	}

	return result;
}

QString typeToString(const ccc::ast::Node* type, const ccc::SymbolDatabase& database)
{
	QString suffix;

	// Traverse through arrays, pointers and references, and build a string
	// to be appended to the end of the type name.
	bool done_finding_arrays_pointers = false;
	bool found_pointer = false;
	while (!done_finding_arrays_pointers)
	{
		switch (type->descriptor)
		{
			case ccc::ast::ARRAY:
			{
				// If we see a pointer to an array we can't print that properly
				// with C-like syntax so we just print the node type instead.
				if (found_pointer)
				{
					done_finding_arrays_pointers = true;
					break;
				}

				const ccc::ast::Array& array = type->as<ccc::ast::Array>();
				suffix.append(QString("[%1]").arg(array.element_count));
				type = array.element_type.get();
				break;
			}
			case ccc::ast::POINTER_OR_REFERENCE:
			{
				const ccc::ast::PointerOrReference& pointer_or_reference = type->as<ccc::ast::PointerOrReference>();
				suffix.prepend(pointer_or_reference.is_pointer ? '*' : '&');
				type = pointer_or_reference.value_type.get();
				found_pointer = true;
				break;
			}
			default:
			{
				done_finding_arrays_pointers = true;
				break;
			}
		}
	}

	// Determine the actual type name, or at the very least the node type.
	QString name;
	switch (type->descriptor)
	{
		case ccc::ast::BUILTIN:
		{
			const ccc::ast::BuiltIn& built_in = type->as<ccc::ast::BuiltIn>();
			name = ccc::ast::builtin_class_to_string(built_in.bclass);
			break;
		}
		case ccc::ast::TYPE_NAME:
		{
			const ccc::ast::TypeName& type_name = type->as<ccc::ast::TypeName>();
			const ccc::DataType* data_type = database.data_types.symbol_from_handle(type_name.data_type_handle);
			if (data_type)
			{
				name = QString::fromStdString(data_type->name());
			}
			break;
		}
		default:
		{
			name = ccc::ast::node_type_to_string(*type);
		}
	}

	return name + suffix;
}
