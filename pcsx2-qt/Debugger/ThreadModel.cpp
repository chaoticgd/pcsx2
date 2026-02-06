// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ThreadModel.h"

#include "QtUtils.h"

ThreadModel::ThreadModel(DebugInterface& cpu, QObject* parent)
	: QAbstractTableModel(parent)
	, m_cpu(cpu)
{
}

int ThreadModel::rowCount(const QModelIndex&) const
{
	return static_cast<int>(m_threads.size());
}

int ThreadModel::columnCount(const QModelIndex&) const
{
	return ThreadModel::COLUMN_COUNT;
}

QVariant ThreadModel::data(const QModelIndex& index, int role) const
{
	size_t row = static_cast<size_t>(index.row());
	if (row >= m_threads.size())
		return QVariant();

	const BiosThread* thread = m_threads[row].get();

	if (role == Qt::DisplayRole)
	{
		switch (index.column())
		{
			case ThreadModel::ID:
				return thread->TID();
			case ThreadModel::PC:
			{
				if (thread->Status() == ThreadStatus::THS_RUN)
					return QtUtils::FilledQStringFromValue(m_cpu.getPC(), 16);

				return QtUtils::FilledQStringFromValue(thread->PC(), 16);
			}
			case ThreadModel::ENTRY:
				return QtUtils::FilledQStringFromValue(thread->EntryPoint(), 16);
			case ThreadModel::PRIORITY:
				return QString::number(thread->Priority());
			case ThreadModel::STATE:
			{
				const auto& state = ThreadStateStrings.find(thread->Status());
				if (state != ThreadStateStrings.end())
					return state->second;

				return tr("INVALID");
			}
			case ThreadModel::WAIT_TYPE:
			{
				const auto& waitType = ThreadWaitStrings.find(thread->Wait());
				if (waitType != ThreadWaitStrings.end())
					return waitType->second;

				return tr("INVALID");
			}
			case ThreadModel::WAIT_ID:
				return QtUtils::FilledQStringFromValue(thread->WaitId(), 16);
		}
	}
	else if (role == Qt::UserRole)
	{
		switch (index.column())
		{
			case ThreadModel::ID:
				return thread->TID();
			case ThreadModel::PC:
			{
				if (thread->Status() == ThreadStatus::THS_RUN)
					return m_cpu.getPC();

				return thread->PC();
			}
			case ThreadModel::ENTRY:
				return thread->EntryPoint();
			case ThreadModel::PRIORITY:
				return thread->Priority();
			case ThreadModel::STATE:
				return static_cast<u32>(thread->Status());
			case ThreadModel::WAIT_TYPE:
				return static_cast<u32>(thread->Wait());
			case ThreadModel::WAIT_ID:
				return QString::number(thread->WaitId());
			default:
				return QVariant();
		}
	}
	return QVariant();
}

QVariant ThreadModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
	{
		switch (section)
		{
			case ThreadColumns::ID:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("ID");
			case ThreadColumns::PC:
				//: Warning: short space limit. Abbreviate if needed. PC = Program Counter (location where the CPU is executing).
				return tr("PC");
			case ThreadColumns::ENTRY:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("ENTRY");
			case ThreadColumns::PRIORITY:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("PRIORITY");
			case ThreadColumns::STATE:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("STATE");
			case ThreadColumns::WAIT_TYPE:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("WAIT TYPE");
			case ThreadColumns::WAIT_ID:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("WAIT ID");
			default:
				return QVariant();
		}
	}
	return QVariant();
}

void ThreadModel::refreshData()
{
	daspjgapjswdg
	
	// TODO FIGURE THIS OUT
	
	std::vector<std::unique_ptr<BiosThread>> threads = m_cpu.GetThreadList();

	// Match the entries for the new threads up with the existing entries so
	// that the user doesn't lose their selection.
	size_t source_index = 0;
	size_t destination_index = 0;

	while (source_index < threads.size() && destination_index < m_threads.size())
	{
		u32 source_tid = threads[source_index]->TID();
		u32 destination_tid = m_threads[destination_index]->TID();
		if (source_tid < destination_tid)
		{
			beginInsertRows(QModelIndex(), static_cast<int>(destination_index), static_cast<int>(destination_index));
			m_threads.insert(m_threads.begin() + destination_index, std::move(threads[source_index]));
			endInsertRows();
			source_index++;
			destination_index++;
		}
		else if (source_tid > destination_tid)
		{
			beginRemoveRows(QModelIndex(), static_cast<int>(destination_index), static_cast<int>(destination_index));
			m_threads.erase(m_threads.begin() + destination_index);
			endRemoveRows();
		}
		else
		{
			if (!threads[source_index]->Equals(*m_threads[destination_index]))
			{
				*m_threads[destination_index] = std::move(*threads[source_index]);
				QModelIndex index_changed(index(destination_index, destination_index, QModelIndex()));
				emit dataChanged(index_changed, index_changed);
			}

			source_index++;
			destination_index++;
		}
	}

	if (source_index < threads.size())
	{
		int first = static_cast<int>(m_threads.size());
		int last = static_cast<int>(m_threads.size() + (threads.size() - source_index) - 1);
		beginInsertRows(QModelIndex(), first, last);
	}
	else if (destination_index < m_threads.size())
	{
		beginRemoveRows(QModelIndex(), static_cast<int>(destination_index), static_cast<int>(m_threads.size() - 1));
		m_threads.erase(m_threads.begin() + destination_index, m_threads.end());
		endRemoveRows();
	}

	beginResetModel();
	m_threads = std::move(threads);
	endResetModel();
}

#include "moc_ThreadModel.cpp"
