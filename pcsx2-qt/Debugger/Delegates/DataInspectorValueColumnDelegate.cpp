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

#include "DataInspectorValueColumnDelegate.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include "Int64SpinBox.h"

DataInspectorValueColumnDelegate::DataInspectorValueColumnDelegate(
	const ccc::HighSymbolTable& symbolTable,
	const std::map<std::string, s32>& typeNameToDeduplicatedTypeIndex,
	QObject* parent)
	: QStyledItemDelegate(parent)
	, m_symbolTable(symbolTable)
	, m_typeNameToDeduplicatedTypeIndex(typeNameToDeduplicatedTypeIndex)
{
}

QWidget* DataInspectorValueColumnDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	DataInspectorNode* node = static_cast<DataInspectorNode*>(index.internalPointer());
	if (node->type)
	{
		const ccc::ast::Node& type = resolvePhysicalType(*node->type, m_symbolTable, m_typeNameToDeduplicatedTypeIndex);
		switch (type.descriptor)
		{
			case ccc::ast::BUILTIN:
			{
				const ccc::ast::BuiltIn& builtIn = type.as<ccc::ast::BuiltIn>();

				switch (builtIn.bclass)
				{
					case ccc::BuiltInClass::UNSIGNED_8:
						return new UInt64SpinBox(0, UINT8_MAX, parent);
					case ccc::BuiltInClass::SIGNED_8:
						return new Int64SpinBox(INT8_MIN, INT8_MAX, parent);
					case ccc::BuiltInClass::UNQUALIFIED_8:
						return new UInt64SpinBox(0, UINT8_MAX, parent);
					case ccc::BuiltInClass::BOOL_8:
						return new QCheckBox(parent);
					case ccc::BuiltInClass::UNSIGNED_16:
						return new UInt64SpinBox(0, INT16_MAX, parent);
					case ccc::BuiltInClass::SIGNED_16:
						return new Int64SpinBox(INT16_MIN, INT16_MAX, parent);
					case ccc::BuiltInClass::UNSIGNED_32:
						return new UInt64SpinBox(0, UINT32_MAX, parent);
					case ccc::BuiltInClass::SIGNED_32:
						return new Int64SpinBox(INT32_MIN, INT32_MAX, parent);
					case ccc::BuiltInClass::FLOAT_32:
						return new QDoubleSpinBox(parent);
					case ccc::BuiltInClass::UNSIGNED_64:
						return new UInt64SpinBox(0, UINT64_MAX, parent);
					case ccc::BuiltInClass::SIGNED_64:
						return new Int64SpinBox(INT64_MIN, INT64_MAX, parent);
					case ccc::BuiltInClass::FLOAT_64:
						return new QDoubleSpinBox(parent);
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
				return comboBox;
			}
			default:
			{
			}
		}
	}

	return nullptr;
}

void DataInspectorValueColumnDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	DataInspectorNode* node = static_cast<DataInspectorNode*>(index.internalPointer());
	if (node->type)
	{
		const ccc::ast::Node& type = resolvePhysicalType(*node->type, m_symbolTable, m_typeNameToDeduplicatedTypeIndex);
		switch (type.descriptor)
		{
			case ccc::ast::BUILTIN:
			{
				const ccc::ast::BuiltIn& builtIn = type.as<ccc::ast::BuiltIn>();

				switch (builtIn.bclass)
				{
					case ccc::BuiltInClass::UNSIGNED_8:
					case ccc::BuiltInClass::UNQUALIFIED_8:
					case ccc::BuiltInClass::UNSIGNED_16:
					case ccc::BuiltInClass::UNSIGNED_32:
					case ccc::BuiltInClass::UNSIGNED_64:
					{
						UInt64SpinBox* spinBox = qobject_cast<UInt64SpinBox*>(editor);
						Q_ASSERT(spinBox);
						spinBox->setValue(index.data().toULongLong());
						break;
					}
					case ccc::BuiltInClass::SIGNED_8:
					case ccc::BuiltInClass::SIGNED_16:
					case ccc::BuiltInClass::SIGNED_32:
					case ccc::BuiltInClass::SIGNED_64:
					{
						Int64SpinBox* spinBox = qobject_cast<Int64SpinBox*>(editor);
						Q_ASSERT(spinBox);
						spinBox->setValue(index.data().toLongLong());
						break;
					}
					case ccc::BuiltInClass::BOOL_8:
					{
						QCheckBox* checkBox = qobject_cast<QCheckBox*>(editor);
						Q_ASSERT(checkBox);
						checkBox->setChecked(index.data().toBool());
						break;
					}
					case ccc::BuiltInClass::FLOAT_32:
					{
						QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(editor);
						Q_ASSERT(spinBox);
						spinBox->setValue(index.data().toFloat());
						break;
					}
					case ccc::BuiltInClass::FLOAT_64:
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
	}
}

void DataInspectorValueColumnDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
	DataInspectorNode* node = static_cast<DataInspectorNode*>(index.internalPointer());
	if (node->type)
	{
		const ccc::ast::Node& type = resolvePhysicalType(*node->type, m_symbolTable, m_typeNameToDeduplicatedTypeIndex);
		switch (type.descriptor)
		{
			case ccc::ast::BUILTIN:
			{
				const ccc::ast::BuiltIn& builtIn = type.as<ccc::ast::BuiltIn>();

				switch (builtIn.bclass)
				{
					case ccc::BuiltInClass::UNSIGNED_8:
					case ccc::BuiltInClass::UNQUALIFIED_8:
					case ccc::BuiltInClass::UNSIGNED_16:
					case ccc::BuiltInClass::UNSIGNED_32:
					case ccc::BuiltInClass::UNSIGNED_64:
					{
						UInt64SpinBox* spinBox = qobject_cast<UInt64SpinBox*>(editor);
						Q_ASSERT(spinBox);
						model->setData(index, spinBox->value(), Qt::EditRole);
						break;
					}
					case ccc::BuiltInClass::SIGNED_8:
					case ccc::BuiltInClass::SIGNED_16:
					case ccc::BuiltInClass::SIGNED_32:
					case ccc::BuiltInClass::SIGNED_64:
					{
						Int64SpinBox* spinBox = qobject_cast<Int64SpinBox*>(editor);
						Q_ASSERT(spinBox);
						model->setData(index, spinBox->value(), Qt::EditRole);
						break;
					}
					case ccc::BuiltInClass::BOOL_8:
					{
						QCheckBox* checkBox = qobject_cast<QCheckBox*>(editor);
						model->setData(index, checkBox->isChecked(), Qt::EditRole);
						break;
					}
					case ccc::BuiltInClass::FLOAT_32:
					{
						QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(editor);
						Q_ASSERT(spinBox);
						model->setData(index, spinBox->value(), Qt::EditRole);
						break;
					}
					case ccc::BuiltInClass::FLOAT_64:
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
	}
}
