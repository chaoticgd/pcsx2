// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Int64SpinBox.h"

#include <QtWidgets/QLineEdit>

Int64SpinBox::Int64SpinBox(qlonglong minimum, qlonglong maximum, QWidget* parent)
	: QAbstractSpinBox(parent)
	, m_minimum(minimum)
	, m_maximum(maximum)
{
	connect(this, &QAbstractSpinBox::editingFinished, this, &Int64SpinBox::onEditingFinished);
}
void Int64SpinBox::stepBy(int steps)
{
	m_value += steps;
	m_value = std::max(m_value, m_minimum);
	m_value = std::min(m_value, m_maximum);
}

qlonglong Int64SpinBox::value()
{
	return m_value;
}

void Int64SpinBox::setValue(qlonglong value)
{
	m_value = value;
	m_value = std::max(m_value, m_minimum);
	m_value = std::min(m_value, m_maximum);
	lineEdit()->setText(QString::number(value));
}

void Int64SpinBox::onEditingFinished()
{
	setValue(lineEdit()->text().toLongLong(nullptr, 0));
}

Int64SpinBox::StepEnabled Int64SpinBox::stepEnabled() const
{
	StepEnabled result = StepNone;
	if (m_value > m_minimum)
		result |= StepUpEnabled;
	if (m_value < m_maximum)
		result |= StepDownEnabled;
	return result;
}

UInt64SpinBox::UInt64SpinBox(qulonglong minimum, qulonglong maximum, QWidget* parent)
	: QAbstractSpinBox(parent)
	, m_minimum(minimum)
	, m_maximum(maximum)
{
	connect(this, &QAbstractSpinBox::editingFinished, this, &UInt64SpinBox::onEditingFinished);
}

void UInt64SpinBox::stepBy(int steps)
{
	m_value += steps;
	m_value = std::max(m_value, m_minimum);
	m_value = std::min(m_value, m_maximum);
}

qulonglong UInt64SpinBox::value()
{
	return m_value;
}

void UInt64SpinBox::setValue(qulonglong value)
{
	m_value = value;
	m_value = std::max(m_value, m_minimum);
	m_value = std::min(m_value, m_maximum);
	lineEdit()->setText(QString::number(value));
}

void UInt64SpinBox::onEditingFinished()
{
	setValue(lineEdit()->text().toULongLong(nullptr, 0));
}

UInt64SpinBox::StepEnabled UInt64SpinBox::stepEnabled() const
{
	StepEnabled result = StepNone;
	if (m_value > m_minimum)
		result |= StepUpEnabled;
	if (m_value < m_maximum)
		result |= StepDownEnabled;
	return result;
}
