// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "DataInspectorValueColumnDelegate.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include "Int64SpinBox.h"

DataInspectorValueColumnDelegate::DataInspectorValueColumnDelegate(
	const SymbolGuardian& guardian,
	QObject* parent)
	: QStyledItemDelegate(parent)
	, m_guardian(guardian)
{
}

QWidget* DataInspectorValueColumnDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QWidget* result = nullptr;

	DataInspectorNode* node = static_cast<DataInspectorNode*>(index.internalPointer());
	if (!node->type.valid())
		return result;

	m_guardian.Read([&](const ccc::SymbolDatabase& database) {
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
						result = new UInt64SpinBox(0, UINT8_MAX, parent);
						break;
					case ccc::ast::BuiltInClass::SIGNED_8:
						result = new Int64SpinBox(INT8_MIN, INT8_MAX, parent);
						break;
					case ccc::ast::BuiltInClass::UNQUALIFIED_8:
						result = new UInt64SpinBox(0, UINT8_MAX, parent);
						break;
					case ccc::ast::BuiltInClass::BOOL_8:
						result = new QCheckBox(parent);
						break;
					case ccc::ast::BuiltInClass::UNSIGNED_16:
						result = new UInt64SpinBox(0, INT16_MAX, parent);
						break;
					case ccc::ast::BuiltInClass::SIGNED_16:
						result = new Int64SpinBox(INT16_MIN, INT16_MAX, parent);
						break;
					case ccc::ast::BuiltInClass::UNSIGNED_32:
						result = new UInt64SpinBox(0, UINT32_MAX, parent);
						break;
					case ccc::ast::BuiltInClass::SIGNED_32:
						result = new Int64SpinBox(INT32_MIN, INT32_MAX, parent);
						break;
					case ccc::ast::BuiltInClass::FLOAT_32:
						result = new QDoubleSpinBox(parent);
						break;
					case ccc::ast::BuiltInClass::UNSIGNED_64:
						result = new UInt64SpinBox(0, UINT64_MAX, parent);
						break;
					case ccc::ast::BuiltInClass::SIGNED_64:
						result = new Int64SpinBox(INT64_MIN, INT64_MAX, parent);
						break;
					case ccc::ast::BuiltInClass::FLOAT_64:
						result = new QDoubleSpinBox(parent);
						break;
					default:
					{
					}
				}
				break;
			}
			case ccc::ast::ENUM:
			{
				const ccc::ast::Enum& enumeration = type.as<ccc::ast::Enum>();
				QComboBox* comboBox = new QComboBox(parent);
				for (auto [value, string] : enumeration.constants)
				{
					comboBox->addItem(QString::fromStdString(string));
				}
				result = comboBox;
				break;
			}
			default:
			{
			}
		}
	});

	return result;
}

void DataInspectorValueColumnDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	DataInspectorNode* node = static_cast<DataInspectorNode*>(index.internalPointer());
	if (!node->type.valid())
		return;

	m_guardian.Read([&](const ccc::SymbolDatabase& database) {
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
					case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					case ccc::ast::BuiltInClass::UNSIGNED_16:
					case ccc::ast::BuiltInClass::UNSIGNED_32:
					case ccc::ast::BuiltInClass::UNSIGNED_64:
					{
						UInt64SpinBox* spinBox = qobject_cast<UInt64SpinBox*>(editor);
						Q_ASSERT(spinBox);
						spinBox->setValue(index.data().toULongLong());
						break;
					}
					case ccc::ast::BuiltInClass::SIGNED_8:
					case ccc::ast::BuiltInClass::SIGNED_16:
					case ccc::ast::BuiltInClass::SIGNED_32:
					case ccc::ast::BuiltInClass::SIGNED_64:
					{
						Int64SpinBox* spinBox = qobject_cast<Int64SpinBox*>(editor);
						Q_ASSERT(spinBox);
						spinBox->setValue(index.data().toLongLong());
						break;
					}
					case ccc::ast::BuiltInClass::BOOL_8:
					{
						QCheckBox* checkBox = qobject_cast<QCheckBox*>(editor);
						Q_ASSERT(checkBox);
						checkBox->setChecked(index.data().toBool());
						break;
					}
					case ccc::ast::BuiltInClass::FLOAT_32:
					{
						QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(editor);
						Q_ASSERT(spinBox);
						spinBox->setValue(index.data().toFloat());
						break;
					}
					case ccc::ast::BuiltInClass::FLOAT_64:
					{
						QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(editor);
						Q_ASSERT(spinBox);
						spinBox->setValue(index.data().toFloat());
						break;
					}
					default:
					{
					}
				}
				break;
			}
			case ccc::ast::ENUM:
			{
				const ccc::ast::Enum& enumeration = type.as<ccc::ast::Enum>();
				QComboBox* comboBox = static_cast<QComboBox*>(editor);
				QVariant data = index.data(Qt::EditRole);
				for (s32 i = 0; i < (s32)enumeration.constants.size(); i++)
				{
					if (enumeration.constants[i].first == data.toInt())
					{
						comboBox->setCurrentIndex(i);
						break;
					}
				}
			}
			default:
			{
			}
		}
	});
}

void DataInspectorValueColumnDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
	DataInspectorNode* node = static_cast<DataInspectorNode*>(index.internalPointer());
	if (!node->type.valid())
		return;

	m_guardian.Read([&](const ccc::SymbolDatabase& database) {
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
					case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					case ccc::ast::BuiltInClass::UNSIGNED_16:
					case ccc::ast::BuiltInClass::UNSIGNED_32:
					case ccc::ast::BuiltInClass::UNSIGNED_64:
					{
						UInt64SpinBox* spinBox = qobject_cast<UInt64SpinBox*>(editor);
						Q_ASSERT(spinBox);
						model->setData(index, spinBox->value(), Qt::EditRole);
						break;
					}
					case ccc::ast::BuiltInClass::SIGNED_8:
					case ccc::ast::BuiltInClass::SIGNED_16:
					case ccc::ast::BuiltInClass::SIGNED_32:
					case ccc::ast::BuiltInClass::SIGNED_64:
					{
						Int64SpinBox* spinBox = qobject_cast<Int64SpinBox*>(editor);
						Q_ASSERT(spinBox);
						model->setData(index, spinBox->value(), Qt::EditRole);
						break;
					}
					case ccc::ast::BuiltInClass::BOOL_8:
					{
						QCheckBox* checkBox = qobject_cast<QCheckBox*>(editor);
						model->setData(index, checkBox->isChecked(), Qt::EditRole);
						break;
					}
					case ccc::ast::BuiltInClass::FLOAT_32:
					{
						QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(editor);
						Q_ASSERT(spinBox);
						model->setData(index, spinBox->value(), Qt::EditRole);
						break;
					}
					case ccc::ast::BuiltInClass::FLOAT_64:
					{
						QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(editor);
						Q_ASSERT(spinBox);
						model->setData(index, spinBox->value(), Qt::EditRole);
						break;
					}
					default:
					{
					}
				}
				break;
			}
			case ccc::ast::ENUM:
			{
				const ccc::ast::Enum& enumeration = type.as<ccc::ast::Enum>();
				QComboBox* comboBox = static_cast<QComboBox*>(editor);
				s32 comboIndex = comboBox->currentIndex();
				if (comboIndex < (s32)enumeration.constants.size())
				{
					s32 value = enumeration.constants[comboIndex].first;
					model->setData(index, QVariant(value), Qt::EditRole);
				}
				break;
			}
			default:
			{
			}
		}
	});
}
