// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "TimelineModels.h"

#include "common/Assertions.h"

#include <QtGui/QColor>

#define PROFILE_TIMELINE_COMBINE_EVENTS
#ifdef PROFILE_TIMELINE_COMBINE_EVENTS
#include <common/Console.h>
#include <common/Timer.h>
#endif

CachedTimelineModel::CachedTimelineModel(TimelineModel& base_model)
	: m_base_model(base_model)
{
	initChannel(m_base_model.rootChannel());
}

CachedTimelineModel::~CachedTimelineModel()
{
	// Tell the worker threads to stop what they're doing.
	for (const auto& [id, channel] : m_channels)
	{
		channel->interrupt_worker = true;
		channel->interrupt_placeholder_worker = true;
	}

	// Wait for the worker threads to all stop.
	for (const auto& [id, channel] : m_channels)
	{
		if (channel->worker.joinable())
			channel->worker.join();
		if (channel->placeholder_worker.joinable())
			channel->placeholder_worker.join();
	}
}

TimelineModel::ChannelID CachedTimelineModel::rootChannel()
{
	return m_base_model.rootChannel();
}

bool CachedTimelineModel::channelHasChildren(ChannelID channel)
{
	return m_base_model.channelHasChildren(channel);
}

std::vector<TimelineModel::ChannelID> CachedTimelineModel::channelChildren(ChannelID channel)
{
	return m_base_model.channelChildren(channel);
}

QString CachedTimelineModel::channelName(ChannelID channel)
{
	return m_base_model.channelName(channel);
}

TimelineModel::Nanoseconds CachedTimelineModel::minTime()
{
	return m_base_model.minTime();
}

TimelineModel::Nanoseconds CachedTimelineModel::maxTime()
{
	return m_base_model.maxTime();
}

TimelineModel::EventDetails CachedTimelineModel::eventDetails(EventID event)
{
	return m_base_model.eventDetails(event);
}

QString CachedTimelineModel::eventText(EventID event)
{
	return m_base_model.eventText(event);
}

TimelineModel::ChannelID CachedTimelineModel::eventChannel(EventID event)
{
	return m_base_model.eventChannel(event);
}

void CachedTimelineModel::viewChanged(Nanoseconds min_visible_time, Nanoseconds max_visible_time, int pixels)
{
	min_visible_time = std::max(min_visible_time, minTime());
	max_visible_time = std::min(max_visible_time, maxTime());
	pixels = std::max(pixels, 0);
	pixels = std::min(pixels, 10000);

	m_min_visible_time = min_visible_time;
	m_max_visible_time = max_visible_time;
	m_pixels = pixels;

	for (auto& [id, channel] : m_channels)
		channel->interrupt_worker = true;

	for (auto& [id, channel] : m_channels)
	{
		if (channel->worker.joinable())
			channel->worker.join();

		std::atomic_bool* interrupt = &channel->interrupt_worker;
		*interrupt = false;

		channel->worker = std::thread(
			[this, base = &m_base_model, id, &channel, min_visible_time, max_visible_time, pixels, interrupt]() {
				std::optional<std::vector<EventDetails>> events = combineEvents(
					*base, id, min_visible_time, max_visible_time, pixels, interrupt);
				if (!events.has_value())
					return;

				{
					std::lock_guard g(channel->mutex);
					channel->min_visible_time = min_visible_time;
					channel->max_visible_time = max_visible_time;
					channel->pixels = pixels;
					channel->events = std::move(*events);
				}

				emit dataChanged();
			});
	}

	for (auto& [id, channel] : m_channels)
		channel->interrupt_worker = false;
}

void CachedTimelineModel::startProcessingEvents(ChannelID channel)
{
	m_channels.at(channel)->mutex.lock();
	m_channels.at(channel)->outer_lock_held = true;
}

std::optional<TimelineModel::EventDetails> CachedTimelineModel::firstEvent(
	ChannelID channel, Nanoseconds min_time, Nanoseconds max_time)
{
	std::unique_ptr<Channel>& channel_ptr = m_channels.at(channel);

	if (!channel_ptr->outer_lock_held)
		channel_ptr->mutex.lock();

	std::optional<EventDetails> result;
	for (EventDetails& event : channel_ptr->events)
	{
		if (event.stop_time > min_time)
		{
			result = event;
			break;
		}
	}

	if (!channel_ptr->outer_lock_held)
		channel_ptr->mutex.unlock();

	return result;
}

std::optional<TimelineModel::EventDetails> CachedTimelineModel::nextEvent(EventID prev_event, Nanoseconds max_time)
{
	std::unique_ptr<Channel>& channel_ptr = m_channels.at(eventChannel(prev_event));

	if (!channel_ptr->outer_lock_held)
		channel_ptr->mutex.lock();

	std::optional<EventDetails> result;

	bool next = false;
	for (EventDetails& event : channel_ptr->events)
	{
		if (next && event.id != prev_event)
		{
			result = event;
			break;
		}
		else if (event.id == prev_event)
		{
			next = true;
		}
	}

	if (!channel_ptr->outer_lock_held)
		channel_ptr->mutex.unlock();

	return result;
}

std::optional<const std::vector<TimelineModel::EventDetails>*> CachedTimelineModel::placeholderEvents(ChannelID channel)
{
	std::unique_ptr<Channel>& channel_ptr = m_channels.at(channel);
	if (channel_ptr->min_visible_time == m_min_visible_time &&
		channel_ptr->max_visible_time == m_max_visible_time &&
		channel_ptr->pixels == m_pixels)
		// The data is fresh, so there's no need for placeholders.
		return std::nullopt;

	return &m_channels.at(channel)->placeholder_events;
}

void CachedTimelineModel::finishProcessingEvents(ChannelID channel)
{
	m_channels.at(channel)->mutex.unlock();
	m_channels.at(channel)->outer_lock_held = false;
}

void CachedTimelineModel::initChannel(ChannelID id)
{
	std::unique_ptr<Channel> channel = std::make_unique<Channel>();
	channel->placeholder_worker = std::thread(
		[this, base = &m_base_model, id, channel = channel.get(), interrupt = &channel->interrupt_placeholder_worker]() {
			std::optional<std::vector<EventDetails>> events = combineEvents(
				*base, id, m_base_model.minTime(), m_base_model.maxTime(), 1000, interrupt);
			if (!events.has_value())
				return;

			{
				std::lock_guard g(channel->mutex);
				channel->placeholder_events = std::move(*events);
			}

			emit dataChanged();
		});
	m_channels.emplace(id, std::move(channel));

	for (ChannelID child : m_base_model.channelChildren(id))
		initChannel(child);
}

std::optional<std::vector<TimelineModel::EventDetails>> CachedTimelineModel::combineEvents(
	TimelineModel& base_model,
	ChannelID channel,
	Nanoseconds min_time,
	Nanoseconds max_time,
	int pixels,
	const std::atomic_bool* interrupt)
{
#ifdef PROFILE_TIMELINE_COMBINE_EVENTS
	Common::Timer timer;
#endif

	std::vector<TimelineModel::EventDetails> events;

	// Iterate over each horizontal pixel and decide if we need to generate a
	// new event for it or if we just need to extend the previous event.
	for (int i = 0; i < pixels; i++)
	{
		if (interrupt && *interrupt)
			return std::nullopt;

		Nanoseconds start_time = min_time + ((max_time - min_time) * i) / pixels;
		Nanoseconds stop_time = min_time + ((max_time - min_time) * (i + 1)) / pixels;

		EventID id = INVALID_EVENT;
		s64 h = 0;
		s64 s = 0;
		s64 v = 0;
		s64 count = 0;

		std::optional<EventDetails> event = base_model.firstEvent(channel, start_time, stop_time);
		while (event.has_value())
		{
			QColor colour(event->colour);

			id = event->id;
			h += colour.hsvHue();
			s += colour.hsvSaturation();
			v += colour.value();
			count++;

			event = base_model.nextEvent(event->id, stop_time);
		}

		if (count == 1 && !events.empty())
		{
			EventDetails& prev_event = events.back();
			if (prev_event.id == id && !prev_event.multiple)
			{
				// Merge this event with the previous one.
				events.back().stop_time = stop_time;
				continue;
			}
		}

		if (count >= 1)
		{
			EventDetails& combined_event = events.emplace_back();
			combined_event.id = id;
			combined_event.start_time = start_time;
			combined_event.stop_time = stop_time;
			// Mix the colours of all the combined events together.
			combined_event.colour = QColor::fromHsv(h / count, s / count, v / count).rgb();
			combined_event.multiple = count > 1;
		}
	}

#ifdef PROFILE_TIMELINE_COMBINE_EVENTS
	Console.WriteLn("CachedTimelineModel::combineEvents took %fms to generate %zu events",
		timer.GetTimeMilliseconds(), events.size());
#endif

	return events;
}

// *****************************************************************************

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

DemoTimelineModel::DemoTimelineModel()
{
	srand(time(NULL));

	for (size_t i = 0; i < DEMO_CHANNEL_COUNT; i++)
	{
		u64 current_time = 0;

		std::vector<EventDetails>& events = m_events.emplace_back();
		for (size_t j = 0; j < 100000; j++)
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

TimelineModel::EventDetails DemoTimelineModel::eventDetails(EventID event)
{
	u32 channel = static_cast<u32>(event >> 32);
	u32 event_index = static_cast<u32>(event & 0xffffffff);
	return m_events.at(channel).at(event_index);
}

QString DemoTimelineModel::eventText(EventID event)
{
	return QString::number(event);
}

TimelineModel::ChannelID DemoTimelineModel::eventChannel(EventID event)
{
	return static_cast<u32>(event >> 32);
}

void DemoTimelineModel::viewChanged(Nanoseconds min_visible_time, Nanoseconds max_visible_time, int pixels)
{
	// Nothing to do.
}

void DemoTimelineModel::startProcessingEvents(ChannelID channel)
{
	// Nothing to do.
}

std::optional<TimelineModel::EventDetails> DemoTimelineModel::firstEvent(
	ChannelID channel, Nanoseconds min_time, Nanoseconds max_time)
{
	for (EventDetails& event : m_events.at(channel))
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

std::optional<const std::vector<TimelineModel::EventDetails>*> DemoTimelineModel::placeholderEvents(ChannelID channel)
{
	return std::nullopt;
}

void DemoTimelineModel::finishProcessingEvents(ChannelID channel)
{
	// Nothing to do.
}
