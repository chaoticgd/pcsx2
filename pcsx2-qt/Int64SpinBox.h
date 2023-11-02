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

#pragma once

#include <QtWidgets/QAbstractSpinBox>

#include "common/Pcsx2Types.h"

class Int64SpinBox : public QAbstractSpinBox
{
	Q_OBJECT

	Q_PROPERTY(qlonglong value READ value WRITE setValue NOTIFY valueChanged USER true)
public:
	explicit Int64SpinBox(qlonglong minimum, qlonglong maximum, QWidget* parent = nullptr);

	QValidator::State validate(QString& input, int& pos) const override;
	void fixup(QString& input) const override;
	void stepBy(int steps) override;

	qlonglong value();

public Q_SLOTS:
	void setValue(qlonglong value);
	void onEditFinished();

Q_SIGNALS:
	void valueChanged(qlonglong v);

protected:
	StepEnabled stepEnabled() const override;

	qlonglong m_value = 0;
	qlonglong m_minimum;
	qlonglong m_maximum;
};

using UInt64SpinBox = Int64SpinBox;
