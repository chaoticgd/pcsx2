// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ProfilerModel.h"

ProfilerModel::ProfilerModel(DebugInterface& cpu, QObject* parent)
	: QAbstractTableModel(parent)
	, m_cpu(cpu)
{
}

int ProfilerModel::rowCount(const QModelIndex& parent) const
{
	return static_cast<int>(m_entries.size());
}

int ProfilerModel::columnCount(const QModelIndex& parent) const
{
	return COLUMN_COUNT;
}

QVariant ProfilerModel::data(const QModelIndex& index, int role) const
{
	if (role != Qt::DisplayRole)
		return QVariant();

	size_t row = static_cast<size_t>(index.row());
	if (row >= m_entries.size())
		return QVariant();

	const ProfilerModelEntry& entry = m_entries[row];

	switch (index.column())
	{
		case NAME:
		{
			QVariant name;
			m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
				const ccc::FunctionHandle handle = database.functions.first_handle_from_starting_address(entry.address);
				const ccc::Function* function = database.functions.symbol_from_handle(handle);
				if (!function)
					return;

				name = QString::fromStdString(function->name());
			});

			return name;
		}
		case ADDRESS:
			return QString::number(entry.address, 16);
		case IN_FUNCTION:
		{
			QVariant enclosing_function_name;
			m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
				const ccc::Function* function = database.functions.symbol_overlapping_address(entry.address);
				if (!function)
					return;

				enclosing_function_name = QString::fromStdString(function->name());
			});

			return enclosing_function_name;
		}
		case SAMPLES:
			return QString::number(entry.sample_count);
		case PERCENTAGE:
			return QString::number(static_cast<double>(entry.sample_count * 100) / m_total_sample_count, 'f', 2);
	}

	return QVariant();
}

QVariant ProfilerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
		return QVariant();

	switch (section)
	{
		case NAME:
			return tr("Name");
		case ADDRESS:
			return tr("Address");
		case IN_FUNCTION:
			return tr("In Function");
		case SAMPLES:
			return tr("Samples");
		case PERCENTAGE:
			return tr("Percentage");
	}

	return QVariant();
}

std::optional<u32> ProfilerModel::address(QModelIndex index) const
{
	size_t row = static_cast<size_t>(index.row());
	if (row >= m_entries.size())
		return std::nullopt;

	return m_entries[row].address;
}

void ProfilerModel::reset(std::vector<ProfilerModelEntry> entries, u64 total_sample_count)
{
	beginResetModel();
	m_entries = std::move(entries);
	m_total_sample_count = total_sample_count;
	endResetModel();
}
