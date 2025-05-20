// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "TimelineModel.h"

#include <QtGui/QColor>

enum DemoChannel
{
	ROOT,
	DMAC,
	D0_VIF0,
	D1_VIF1,
	D2_GIF,
	D3_IPU_FROM,
	D4_IPU_TO,
	D5_SIF0,
	D6_SIF1,
	D7_SIF2,
	D8_SPR_FROM,
	D9_SPR_TO,
	DEMO_CHANNEL_COUNT
};

TimelineModel::ChannelID DemoTimelineModel::rootChannel()
{
	return DemoChannel::ROOT;
}

std::vector<TimelineModel::ChannelID> DemoTimelineModel::channelChildren(ChannelID channel)
{
	switch (channel)
	{
		case ROOT:
			return {
				DemoChannel::DMAC,
			};
		case DMAC:
			return {
				D0_VIF0,
				D1_VIF1,
				D2_GIF,
				D3_IPU_FROM,
				D4_IPU_TO,
				D5_SIF0,
				D6_SIF1,
				D7_SIF2,
				D8_SPR_FROM,
				D9_SPR_TO,
			};
	}

	return {};
}

bool DemoTimelineModel::channelHasChildren(ChannelID channel)
{
	return channel == ROOT || channel == DMAC;
}

QString DemoTimelineModel::channelName(ChannelID channel)
{
	switch (channel)
	{
		case ROOT:
			return "ROOT";
		case DMAC:
			return "DMAC";
		case D0_VIF0:
			return "Channel 0 VIF0";
		case D1_VIF1:
			return "Channel 1 VIF1";
		case D2_GIF:
			return "Channel 2 GIF";
		case D3_IPU_FROM:
			return "Channel 3 IPU From";
		case D4_IPU_TO:
			return "Channel 4 IPU To";
		case D5_SIF0:
			return "Channel 5 SIF0";
		case D6_SIF1:
			return "Channel 6 SIF1";
		case D7_SIF2:
			return "Channel 7 SIF2";
		case D8_SPR_FROM:
			return "Channel 8 SPR From";
		case D9_SPR_TO:
			return "Channel 9 SPR To";
	}

	return "Error";
}

DemoTimelineModel::DemoTimelineModel()
{
	srand(time(NULL));

	for (size_t i = 0; i < DEMO_CHANNEL_COUNT; i++)
	{
		u64 current_time = 0;

		std::vector<EventDetails>& events = m_events.emplace_back();
		for (size_t j = 0; j < 100; j++)
		{
			if (rand() % 2 == 0)
				current_time += rand() % 10000;

			std::array<QColor, 3> colours = {
				QColor(255, 0, 0),
				QColor(0, 255, 0),
				QColor(0, 0, 255),
			};

			EventDetails& event = events.emplace_back();
			event.id = j | (i << 32);
			event.start_time = current_time;
			event.stop_time = event.start_time + (rand() % 10000);
			event.colour = colours[rand() % colours.size()].rgb();

			current_time = event.stop_time;
		}
	}
}

TimelineModel::Nanoseconds DemoTimelineModel::minTime()
{
	Nanoseconds min = INVALID_NANOSECONDS;

	for (size_t i = 0; i < DEMO_CHANNEL_COUNT; i++)
	{
		if (!m_events.empty() && (min == INVALID_NANOSECONDS || m_events[i].front().start_time < min))
			min = m_events[i].front().start_time;
	}

	return min;
}

TimelineModel::Nanoseconds DemoTimelineModel::maxTime()
{
	Nanoseconds max = INVALID_NANOSECONDS;

	for (size_t i = 0; i < DEMO_CHANNEL_COUNT; i++)
	{
		if (!m_events.empty() && (max == INVALID_NANOSECONDS || m_events[i].back().stop_time > max))
			max = m_events[i].back().stop_time;
	}

	return max;
}

std::optional<TimelineModel::EventDetails> DemoTimelineModel::firstEvent(
	ChannelID channel, Nanoseconds min_time, Nanoseconds max_time)
{
	std::vector<EventDetails>& events = m_events.at(channel);

	for (EventDetails& event : events)
	{
		if (event.stop_time > min_time)
			return event;
	}

	return std::nullopt;
}

std::optional<TimelineModel::EventDetails> DemoTimelineModel::nextEvent(EventID prev_event, Nanoseconds max_time)
{
	u32 channel = static_cast<u32>(prev_event >> 32);
	u32 prev_event_index = static_cast<u32>(prev_event & 0xffffffff);

	std::vector<EventDetails>& events = m_events.at(channel);
	if (prev_event_index + 1 >= events.size())
		return std::nullopt;

	EventDetails& next_event = events[prev_event_index + 1];
	if (next_event.start_time >= max_time)
		return std::nullopt;

	return next_event;
}

TimelineModel::EventDetails DemoTimelineModel::eventDetails(EventID event)
{
	u32 channel = static_cast<u32>(event >> 32);
	u32 event_index = static_cast<u32>(event & 0xffffffff);
	return m_events.at(channel).at(event_index);
}

QString DemoTimelineModel::eventName(EventID event)
{
	return QString::number(event);
}
