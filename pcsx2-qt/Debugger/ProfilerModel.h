// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "DebugTools/DebugInterface.h"

#include <QtCore/QAbstractTableModel>
#include <QtWidgets/QHeaderView>

struct ProfilerModelEntry
{
	u32 address;
	u64 sample_count;
};

class ProfilerModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	enum Column
	{
		NAME,
		ADDRESS,
		IN_FUNCTION,
		SAMPLES,
		PERCENTAGE,
		COLUMN_COUNT
	};

	ProfilerModel(DebugInterface& cpu, QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

	std::optional<u32> address(QModelIndex index) const;

	void reset(std::vector<ProfilerModelEntry> entries, u64 total_sample_count);

private:
	DebugInterface& m_cpu;
	std::vector<ProfilerModelEntry> m_entries;
	u64 m_total_sample_count;
};
