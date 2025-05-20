// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Types.h"

#include <QRgb>

// Abstract base class for a timeline model. Provides a way to enumerate event
// channels and iterate over events from each channel.
class TimelineModel
{
public:
	using ChannelID = size_t;
	using EventID = size_t;
	using Nanoseconds = s64;

	static const constexpr ChannelID INVALID_CHANNEL = std::numeric_limits<ChannelID>::max();
	static const constexpr EventID INVALID_EVENT = std::numeric_limits<EventID>::max();
	static const constexpr Nanoseconds INVALID_NANOSECONDS = std::numeric_limits<Nanoseconds>::max();

	virtual ChannelID rootChannel() = 0;
	virtual bool channelHasChildren(ChannelID channel) = 0;
	virtual std::vector<ChannelID> channelChildren(ChannelID channel) = 0;

	virtual QString channelName(ChannelID channel) = 0;

	virtual Nanoseconds minTime() = 0;
	virtual Nanoseconds maxTime() = 0;

	struct EventDetails
	{
		EventID id;
		Nanoseconds start_time;
		Nanoseconds stop_time;
		QRgb colour;
	};

	virtual std::optional<EventDetails> firstEvent(ChannelID channel, Nanoseconds min_time, Nanoseconds max_time) = 0;
	virtual std::optional<EventDetails> nextEvent(EventID prev_event, Nanoseconds max_time) = 0;

	virtual EventDetails eventDetails(EventID event) = 0;
	virtual QString eventName(EventID event) = 0;
};

// Generates random events for testing purposes.
class DemoTimelineModel : public TimelineModel
{
public:
	DemoTimelineModel();

	ChannelID rootChannel() override;
	bool channelHasChildren(ChannelID channel) override;
	std::vector<TimelineModel::ChannelID> channelChildren(ChannelID channel) override;

	QString channelName(ChannelID channel) override;

	Nanoseconds minTime() override;
	Nanoseconds maxTime() override;

	std::optional<EventDetails> firstEvent(ChannelID channel, Nanoseconds min_time, Nanoseconds max_time) override;
	std::optional<EventDetails> nextEvent(EventID prev_event, Nanoseconds max_time) override;

	EventDetails eventDetails(EventID event) override;
	QString eventName(EventID event) override;

private:
	std::vector<std::vector<EventDetails>> m_events;
};
