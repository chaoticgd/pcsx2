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

#include "Int64SpinBox.h"

#include <QtWidgets/QLineEdit>

Int64SpinBox::Int64SpinBox(qlonglong minimum, qlonglong maximum, QWidget* parent)
	: QAbstractSpinBox(parent)
	, m_minimum(minimum)
	, m_maximum(maximum)
{
	connect(lineEdit(), SIGNAL(textEdited(QString)), this, SLOT(onEditFinished()));
}

QValidator::State Int64SpinBox::validate(QString& input, int& pos) const
{
	for (QChar& c : input)
	{
		if (!c.isDigit())
		{
			return QValidator::Invalid;
		}
	}
	return QValidator::Acceptable;
}

void Int64SpinBox::onEditFinished()
{
	QString input = lineEdit()->text();
	int pos = 0;
	if (QValidator::Acceptable == validate(input, pos))
		setValue(input.toLongLong());
	else
		lineEdit()->setText(QString::number(m_value));
}

void Int64SpinBox::fixup(QString& input) const
{
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

Int64SpinBox::StepEnabled Int64SpinBox::stepEnabled() const
{
	StepEnabled result = StepNone;
	if (m_value > m_minimum)
		result |= StepUpEnabled;
	if (m_value < m_maximum)
		result |= StepDownEnabled;
	return result;
}
