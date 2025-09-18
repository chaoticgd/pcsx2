// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ProfilerView.h"

#include "common/Threading.h"

#include <QtCore/QPointer>
#include <QtWidgets/QMessageBox>

ProfilerView::ProfilerView(const DebuggerViewParameters& parameters)
	: DebuggerView(parameters, NO_DEBUGGER_FLAGS)
	, m_function_model(new ProfilerModel(cpu(), this))
	, m_address_model(new ProfilerModel(cpu(), this))
{
	m_ui.setupUi(this);

	m_ui.sampleCount->addItem("1,000", 1000);
	m_ui.sampleCount->addItem("10,000", 10000);
	m_ui.sampleCount->addItem("100,000", 100000);
	m_ui.sampleCount->addItem("1,000,000", 1000000);
	m_ui.sampleCount->setCurrentIndex(2);

	m_ui.duration->addItem("~1s", 1);
	m_ui.duration->addItem("~5s", 5);
	m_ui.duration->addItem("~10s", 10);
	m_ui.duration->addItem("~30s", 30);

	m_ui.functions->setModel(m_function_model);
	m_ui.addresses->setModel(m_address_model);

	m_ui.functions->setColumnHidden(ProfilerModel::IN_FUNCTION, true);
	m_ui.addresses->setColumnHidden(ProfilerModel::NAME, true);

	m_ui.functions->horizontalHeader()->setSectionResizeMode(ProfilerModel::NAME, QHeaderView::Stretch);
	m_ui.functions->horizontalHeader()->setSectionResizeMode(ProfilerModel::ADDRESS, QHeaderView::ResizeToContents);
	m_ui.functions->horizontalHeader()->setSectionResizeMode(ProfilerModel::SAMPLES, QHeaderView::ResizeToContents);
	m_ui.functions->horizontalHeader()->setSectionResizeMode(ProfilerModel::PERCENTAGE, QHeaderView::ResizeToContents);

	m_ui.addresses->horizontalHeader()->setSectionResizeMode(ProfilerModel::ADDRESS, QHeaderView::ResizeToContents);
	m_ui.addresses->horizontalHeader()->setSectionResizeMode(ProfilerModel::IN_FUNCTION, QHeaderView::Stretch);
	m_ui.addresses->horizontalHeader()->setSectionResizeMode(ProfilerModel::SAMPLES, QHeaderView::ResizeToContents);
	m_ui.addresses->horizontalHeader()->setSectionResizeMode(ProfilerModel::PERCENTAGE, QHeaderView::ResizeToContents);

	for (const auto& view : {m_ui.functions, m_ui.addresses})
	{
		view->setSelectionBehavior(QTableView::SelectRows);
		view->setSelectionMode(QAbstractItemView::SingleSelection);
		view->setAlternatingRowColors(true);
		view->setShowGrid(false);
		view->horizontalHeader()->setSectionsMovable(true);
		view->horizontalHeader()->setHighlightSections(false);
		view->verticalHeader()->setVisible(false);
	}


	connect(m_ui.functions, &QTableView::pressed, this, [&](const QModelIndex& index) {
		std::optional<u32> address = m_function_model->address(index);
		if (!address.has_value())
			return;

		goToInDisassembler(*address, false);
	});

	connect(m_ui.addresses, &QTableView::pressed, this, [&](const QModelIndex& index) {
		std::optional<u32> address = m_address_model->address(index);
		if (!address.has_value())
			return;

		goToInDisassembler(*address, false);
	});

	connect(m_ui.runButton, &QPushButton::clicked, this, [&]() {
		shutdownWorker();

		size_t sample_count = m_ui.sampleCount->currentData().toUInt();
		std::chrono::seconds duration(m_ui.duration->currentData().toUInt());

		m_thread = std::thread([this, sample_count, duration, &cpu = cpu()]() {
			worker(sample_count, duration, cpu);
		});
	});
}

ProfilerView::~ProfilerView()
{
	shutdownWorker();
}

void ProfilerView::worker(size_t sample_count, std::chrono::nanoseconds duration, DebugInterface& cpu)
{
	Threading::SetNameOfCurrentThread("Sampling Profiler");

	std::chrono::nanoseconds interval = duration / sample_count;

	// Collect samples.
	std::map<u32, u64> samples = cpu.RunSamplingProfiler(sample_count, interval, &m_interrupt);

	// Enumerate entries to display in the Functions tab.
	std::vector<ProfilerModelEntry> function_entries;
	u64 function_sample_count = 0;

	cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		std::map<ccc::FunctionHandle, u64> functions;
		for (const auto& [address, hits] : samples)
		{
			const ccc::Function* function = database.functions.symbol_overlapping_address(address);
			if (function)
				functions[function->handle()] += hits;
		}

		for (const auto& [handle, sample_count] : functions)
		{
			const ccc::Function* function = database.functions.symbol_from_handle(handle);
			if (!function)
				continue;

			ProfilerModelEntry& entry = function_entries.emplace_back();
			entry.address = function->address().value;
			entry.sample_count = sample_count;

			function_sample_count += sample_count;
		}
	});

	if (m_interrupt)
		return;

	std::sort(function_entries.begin(), function_entries.end(),
		[](const ProfilerModelEntry& lhs, const ProfilerModelEntry& rhs) {
			return lhs.sample_count > rhs.sample_count;
		});

	if (m_interrupt)
		return;

	// Enumerate entries to display in the Addresses tab.
	std::vector<ProfilerModelEntry> address_entries;
	u64 total_sample_count = 0;
	for (const auto& [address, sample_count] : samples)
	{
		ProfilerModelEntry& entry = address_entries.emplace_back();
		entry.address = address;
		entry.sample_count = sample_count;

		total_sample_count += sample_count;
	}

	if (m_interrupt)
		return;

	std::sort(address_entries.begin(), address_entries.end(),
		[](const ProfilerModelEntry& lhs, const ProfilerModelEntry& rhs) {
			return lhs.sample_count > rhs.sample_count;
		});

	if (m_interrupt)
		return;

	QtHost::RunOnUIThread([profiler = QPointer<ProfilerView>(this),
							  functions = std::move(function_entries),
							  addresses = std::move(address_entries),
							  function_sample_count,
							  total_sample_count]() {
		if (!profiler)
			return;

		profiler->m_function_model->reset(std::move(functions), total_sample_count);
		profiler->m_address_model->reset(std::move(addresses), total_sample_count);

		QString sampleText = QString(tr("%1 sample%2 in functions, %3 total sample%4"))
		                         .arg(function_sample_count)
		                         .arg((function_sample_count != 1) ? "s" : "")
		                         .arg(total_sample_count)
		                         .arg((total_sample_count != 1) ? "s" : "");
		profiler->m_ui.samplesLabel->setText(sampleText);
	});
}

void ProfilerView::shutdownWorker()
{
	m_interrupt = true;

	if (m_thread.joinable())
		m_thread.join();

	m_interrupt = false;
}
