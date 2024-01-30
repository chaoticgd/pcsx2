// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtWidgets/QAbstractSpinBox>

class Int64SpinBox : public QAbstractSpinBox
{
	Q_OBJECT

public:
	explicit Int64SpinBox(qlonglong minimum, qlonglong maximum, QWidget* parent = nullptr);

	void stepBy(int steps) override;
	qlonglong value();

public Q_SLOTS:
	void setValue(qlonglong value);
	void onEditingFinished();

Q_SIGNALS:
	void valueChanged(qlonglong v);

protected:
	StepEnabled stepEnabled() const override;

	qlonglong m_value = 0;
	qlonglong m_minimum;
	qlonglong m_maximum;
};

class UInt64SpinBox : public QAbstractSpinBox
{
	Q_OBJECT

public:
	explicit UInt64SpinBox(qulonglong minimum, qulonglong maximum, QWidget* parent = nullptr);

	void stepBy(int steps) override;
	qulonglong value();

public Q_SLOTS:
	void setValue(qulonglong value);
	void onEditingFinished();

Q_SIGNALS:
	void valueChanged(qulonglong v);

protected:
	StepEnabled stepEnabled() const override;

	qulonglong m_value = 0;
	qulonglong m_minimum;
	qulonglong m_maximum;
};
