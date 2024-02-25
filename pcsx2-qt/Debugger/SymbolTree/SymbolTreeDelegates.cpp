// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SymbolTreeDelegates.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include "Debugger/SymbolTree/SymbolTreeModel.h"
#include "Debugger/SymbolTree/TypeString.h"

SymbolTreeLocationDelegate::SymbolTreeLocationDelegate(
	SymbolGuardian& guardian,
	QObject* parent)
	: QStyledItemDelegate(parent)
	, m_guardian(guardian)
{
}

QWidget* SymbolTreeLocationDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	if (!index.isValid())
		return nullptr;

	SymbolTreeNode* node = static_cast<SymbolTreeNode*>(index.internalPointer());
	if (!node->symbol.valid() || !node->symbol.is_flag_set(ccc::WITH_ADDRESS_MAP))
		return nullptr;

	if (m_guardian.IsBusy())
		return nullptr;

	return new QLineEdit(parent);
}

void SymbolTreeLocationDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	if (!index.isValid())
		return;

	SymbolTreeNode* node = static_cast<SymbolTreeNode*>(index.internalPointer());
	if (!node->symbol.valid())
		return;

	QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
	Q_ASSERT(lineEdit);

	m_guardian.TryRead([&](const ccc::SymbolDatabase& database) {
		const ccc::Symbol* symbol = node->symbol.lookup_symbol(database);
		if (!symbol || !symbol->address().valid())
			return;

		lineEdit->setText(QString::number(symbol->address().value, 16));
	});
}

void SymbolTreeLocationDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
	if (!index.isValid())
		return;

	SymbolTreeNode* node = static_cast<SymbolTreeNode*>(index.internalPointer());
	if (!node->symbol.valid() || !node->symbol.is_flag_set(ccc::WITH_ADDRESS_MAP))
		return;

	QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
	Q_ASSERT(lineEdit);

	SymbolTreeModel* symbolTreeModel = qobject_cast<SymbolTreeModel*>(model);
	Q_ASSERT(symbolTreeModel);

	bool ok;
	u32 address = lineEdit->text().toUInt(&ok, 16);
	if (!ok)
		return;

	bool success = false;
	m_guardian.BlockingReadWrite([&](ccc::SymbolDatabase& database) {
		if (node->symbol.move_symbol(address, database))
			success = true;
	});

	if (success)
	{
		node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, address);
		symbolTreeModel->resetChildren(index);
	}
}

// *****************************************************************************

SymbolTreeTypeDelegate::SymbolTreeTypeDelegate(
	SymbolGuardian& guardian,
	QObject* parent)
	: QStyledItemDelegate(parent)
	, m_guardian(guardian)
{
}

QWidget* SymbolTreeTypeDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	if (!index.isValid())
		return nullptr;

	SymbolTreeNode* node = static_cast<SymbolTreeNode*>(index.internalPointer());
	if (!node->symbol.valid())
		return nullptr;

	if (m_guardian.IsBusy())
		return nullptr;

	return new QLineEdit(parent);
}

void SymbolTreeTypeDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	if (!index.isValid())
		return;

	SymbolTreeNode* node = static_cast<SymbolTreeNode*>(index.internalPointer());
	if (!node->symbol.valid())
		return;

	QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
	Q_ASSERT(lineEdit);

	m_guardian.TryRead([&](const ccc::SymbolDatabase& database) {
		const ccc::Symbol* symbol = node->symbol.lookup_symbol(database);
		if (!symbol || !symbol->type())
			return;

		lineEdit->setText(typeToString(symbol->type(), database));
	});
}

void SymbolTreeTypeDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
	if (!index.isValid())
		return;

	SymbolTreeNode* node = static_cast<SymbolTreeNode*>(index.internalPointer());
	if (!node->symbol.valid())
		return;

	QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
	Q_ASSERT(lineEdit);

	QString text = lineEdit->text();
	if (text.isEmpty())
		return;

	SymbolTreeModel* symbolTreeModel = qobject_cast<SymbolTreeModel*>(model);
	Q_ASSERT(symbolTreeModel);

	QString error_message;
	m_guardian.BlockingReadWrite([&](ccc::SymbolDatabase& database) {
		ccc::Symbol* symbol = node->symbol.lookup_symbol(database);
		if (!symbol)
		{
			error_message = tr("Symbol no longer exists.");
			return;
		}

		std::unique_ptr<ccc::ast::Node> type = stringToType(text.toStdString(), database, error_message);
		if (!type)
			return;

		symbol->set_type(std::move(type));
		node->type = ccc::NodeHandle(node->symbol.descriptor(), *symbol, symbol->type());
	});

	if (error_message.isEmpty())
		symbolTreeModel->resetChildren(index);
	else
		QMessageBox::warning(editor, tr("Cannot Change Type"), error_message);
}

// *****************************************************************************

SymbolTreeValueDelegate::SymbolTreeValueDelegate(
	SymbolGuardian& guardian,
	QObject* parent)
	: QStyledItemDelegate(parent)
	, m_guardian(guardian)
{
}

QWidget* SymbolTreeValueDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	if (!index.isValid())
		return nullptr;

	SymbolTreeNode* node = static_cast<SymbolTreeNode*>(index.internalPointer());
	if (!node->type.valid())
		return nullptr;

	QWidget* result = nullptr;

	m_guardian.TryRead([&](const ccc::SymbolDatabase& database) {
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
					case ccc::ast::BuiltInClass::SIGNED_8:
					case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					case ccc::ast::BuiltInClass::UNSIGNED_16:
					case ccc::ast::BuiltInClass::SIGNED_16:
					case ccc::ast::BuiltInClass::UNSIGNED_32:
					case ccc::ast::BuiltInClass::SIGNED_32:
					case ccc::ast::BuiltInClass::FLOAT_32:
					case ccc::ast::BuiltInClass::UNSIGNED_64:
					case ccc::ast::BuiltInClass::SIGNED_64:
					case ccc::ast::BuiltInClass::FLOAT_64:
						result = new QLineEdit(parent);
						break;
					case ccc::ast::BuiltInClass::BOOL_8:
						result = new QCheckBox(parent);
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
					comboBox->addItem(QString::fromStdString(string));
				connect(comboBox, &QComboBox::currentIndexChanged, this, &SymbolTreeValueDelegate::onComboBoxIndexChanged);
				result = comboBox;
				break;
			}
			case ccc::ast::POINTER_OR_REFERENCE:
			case ccc::ast::POINTER_TO_DATA_MEMBER:
			{
				result = new QLineEdit(parent);
				break;
			}
			default:
			{
			}
		}
	});

	return result;
}

void SymbolTreeValueDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	if (!index.isValid())
		return;

	SymbolTreeNode* node = static_cast<SymbolTreeNode*>(index.internalPointer());
	if (!node->type.valid())
		return;

	m_guardian.TryRead([&](const ccc::SymbolDatabase& database) {
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
					case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					case ccc::ast::BuiltInClass::UNSIGNED_16:
					case ccc::ast::BuiltInClass::UNSIGNED_32:
					case ccc::ast::BuiltInClass::UNSIGNED_64:
					{
						QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
						Q_ASSERT(lineEdit);

						lineEdit->setText(QString::number(index.data(Qt::UserRole).toULongLong()));

						break;
					}
					case ccc::ast::BuiltInClass::SIGNED_8:
					case ccc::ast::BuiltInClass::SIGNED_16:
					case ccc::ast::BuiltInClass::SIGNED_32:
					case ccc::ast::BuiltInClass::SIGNED_64:
					{
						QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
						Q_ASSERT(lineEdit);

						lineEdit->setText(QString::number(index.data(Qt::UserRole).toLongLong()));

						break;
					}
					case ccc::ast::BuiltInClass::BOOL_8:
					{
						QCheckBox* checkBox = qobject_cast<QCheckBox*>(editor);
						Q_ASSERT(checkBox);

						checkBox->setChecked(index.data(Qt::UserRole).toBool());

						break;
					}
					case ccc::ast::BuiltInClass::FLOAT_32:
					{
						QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
						Q_ASSERT(lineEdit);

						lineEdit->setText(QString::number(index.data(Qt::UserRole).toFloat()));

						break;
					}
					case ccc::ast::BuiltInClass::FLOAT_64:
					{
						QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
						Q_ASSERT(lineEdit);

						lineEdit->setText(QString::number(index.data(Qt::UserRole).toDouble()));

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
				QVariant data = index.data(Qt::UserRole);
				for (s32 i = 0; i < (s32)enumeration.constants.size(); i++)
				{
					if (enumeration.constants[i].first == data.toInt())
					{
						comboBox->setCurrentIndex(i);
						break;
					}
				}
				break;
			}
			case ccc::ast::POINTER_OR_REFERENCE:
			case ccc::ast::POINTER_TO_DATA_MEMBER:
			{
				QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
				Q_ASSERT(lineEdit);

				lineEdit->setText(QString::number(index.data(Qt::UserRole).toULongLong(), 16));

				break;
			}
			default:
			{
			}
		}
	});
}

void SymbolTreeValueDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
	if (!index.isValid())
		return;

	SymbolTreeNode* node = static_cast<SymbolTreeNode*>(index.internalPointer());
	if (!node->type.valid())
		return;

	m_guardian.TryRead([&](const ccc::SymbolDatabase& database) {
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
					case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					case ccc::ast::BuiltInClass::UNSIGNED_16:
					case ccc::ast::BuiltInClass::UNSIGNED_32:
					case ccc::ast::BuiltInClass::UNSIGNED_64:
					{
						QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
						Q_ASSERT(lineEdit);

						bool ok;
						qulonglong value = lineEdit->text().toULongLong(&ok);
						if (ok)
							model->setData(index, value, Qt::UserRole);

						break;
					}
					case ccc::ast::BuiltInClass::SIGNED_8:
					case ccc::ast::BuiltInClass::SIGNED_16:
					case ccc::ast::BuiltInClass::SIGNED_32:
					case ccc::ast::BuiltInClass::SIGNED_64:
					{
						QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
						Q_ASSERT(lineEdit);

						bool ok;
						qlonglong value = lineEdit->text().toLongLong(&ok);
						if (ok)
							model->setData(index, value, Qt::UserRole);

						break;
					}
					case ccc::ast::BuiltInClass::BOOL_8:
					{
						QCheckBox* checkBox = qobject_cast<QCheckBox*>(editor);
						model->setData(index, checkBox->isChecked(), Qt::UserRole);

						break;
					}
					case ccc::ast::BuiltInClass::FLOAT_32:
					{
						QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
						Q_ASSERT(lineEdit);

						bool ok;
						float value = lineEdit->text().toFloat(&ok);
						if (ok)
							model->setData(index, value, Qt::UserRole);

						break;
					}
					case ccc::ast::BuiltInClass::FLOAT_64:
					{
						QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
						Q_ASSERT(lineEdit);

						bool ok;
						double value = lineEdit->text().toDouble(&ok);
						if (ok)
							model->setData(index, value, Qt::UserRole);

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
					model->setData(index, QVariant(value), Qt::UserRole);
				}
				break;
			}
			case ccc::ast::POINTER_OR_REFERENCE:
			case ccc::ast::POINTER_TO_DATA_MEMBER:
			{
				QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
				Q_ASSERT(lineEdit);

				bool ok;
				qulonglong address = lineEdit->text().toUInt(&ok, 16);
				if (ok)
					model->setData(index, address, Qt::UserRole);

				break;
			}
			default:
			{
			}
		}
	});
}

void SymbolTreeValueDelegate::onComboBoxIndexChanged(int index)
{
	QComboBox* comboBox = qobject_cast<QComboBox*>(sender());
	if (comboBox)
		commitData(comboBox);
}
