// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Types.h"

#include <QObject>
#include <QRgb>

// Abstract base class for a timeline model. Provides a way to enumerate event
// channels and iterate over events from each channel.
class TimelineModel : public QObject
{
	Q_OBJECT
public:
	using ChannelID = size_t;
	using EventID = size_t;
	using Nanoseconds = s64;

	static const constexpr ChannelID INVALID_CHANNEL = std::numeric_limits<ChannelID>::max();
	static const constexpr EventID INVALID_EVENT = std::numeric_limits<EventID>::max();
	static const constexpr Nanoseconds INVALID_NANOSECONDS = std::numeric_limits<Nanoseconds>::max();

	// Channel enumeration functions. These should always return the same value!
	virtual ChannelID rootChannel() = 0;
	virtual bool channelHasChildren(ChannelID channel) = 0;
	virtual std::vector<ChannelID> channelChildren(ChannelID channel) = 0;

	// The text to draw in the channel name column.
	virtual QString channelName(ChannelID channel) = 0;

	// The minimum begin time of any event in the model.
	virtual Nanoseconds minTime() = 0;

	// The maximum end time of any event in the model.
	virtual Nanoseconds maxTime() = 0;

	struct EventDetails
	{
		EventID id = INVALID_EVENT;
		Nanoseconds start_time = 0;
		Nanoseconds stop_time = 0;
		QRgb colour = 0;

		// Is this an event from a CachedTimelineModel that represents multiple
		// events from the base model?
		bool multiple = false;
	};

	// Retrieve basic information about an event.
	virtual EventDetails eventDetails(EventID event) = 0;

	// Retrieve the string to be displayed for an event in the timeline.
	virtual QString eventText(EventID event) = 0;

	// Find which channel an event belongs to.
	virtual ChannelID eventChannel(EventID event) = 0;

	// Tell the model the range of time that is visible has changed.
	virtual void viewChanged(Nanoseconds min_visible_time, Nanoseconds max_visible_time, int pixels) = 0;

	// Take a lock on the event data structures if applicable.
	virtual void startProcessingEvents(ChannelID channel) = 0;

	// Find the first event in the given channel that intersects the provided
	// time range. Holding the lock is optional.
	virtual std::optional<EventDetails> firstEvent(
		ChannelID channel, Nanoseconds min_time, Nanoseconds max_time) = 0;

	// Find the event immediately after the provided event if one exists that
	// starts before the provided max_time. Holding the lock is optional.
	virtual std::optional<EventDetails> nextEvent(EventID prev_event, Nanoseconds max_time) = 0;

	// Retrieve the list of placeholder events for this channel, to be drawn
	// behind the regular events. Needs the lock held.
	virtual std::optional<const std::vector<EventDetails>*> placeholderEvents(ChannelID channel) = 0;

	// Release the lock on the event data structures if applicable.
	virtual void finishProcessingEvents(ChannelID channel) = 0;

Q_SIGNALS:
	void dataChanged();
};

// Combines consecutive events from the base model together if they're too small
// to be drawn separately. This is intended to improve performance for models
// that have vast numbers of events.
//
// The base model will be accessed from both the calling thread and from two
// worker threads.
class CachedTimelineModel : public TimelineModel
{
public:
	CachedTimelineModel(TimelineModel& base_model);
	~CachedTimelineModel();

	ChannelID rootChannel() override;
	bool channelHasChildren(ChannelID channel) override;
	std::vector<ChannelID> channelChildren(ChannelID channel) override;

	QString channelName(ChannelID channel) override;

	Nanoseconds minTime() override;
	Nanoseconds maxTime() override;

	EventDetails eventDetails(EventID event) override;
	QString eventText(EventID event) override;
	ChannelID eventChannel(EventID event) override;

	void viewChanged(Nanoseconds min_visible_time, Nanoseconds max_visible_time, int pixels) override;

	void startProcessingEvents(ChannelID channel) override;
	std::optional<EventDetails> firstEvent(
		ChannelID channel, Nanoseconds min_time, Nanoseconds max_time) override;
	std::optional<EventDetails> nextEvent(EventID prev_event, Nanoseconds max_time) override;
	std::optional<const std::vector<EventDetails>*> placeholderEvents(ChannelID channel) override;
	void finishProcessingEvents(ChannelID channel) override;

private:
	void initChannel(ChannelID id);

	static std::optional<std::vector<EventDetails>> combineEvents(
		TimelineModel& base_model,
		ChannelID channel,
		Nanoseconds min_time,
		Nanoseconds max_time,
		int pixels,
		const std::atomic_bool* interrupt);

	struct Channel
	{
		std::mutex mutex;

		// The view parameters from the last time the events were updated, for
		// checking if we need to update them again.
		Nanoseconds min_visible_time = 0;
		Nanoseconds max_visible_time = 0;
		int pixels = 0;

		std::vector<EventDetails> events;
		std::thread worker;
		std::atomic_bool interrupt_worker = false;

		std::vector<EventDetails> placeholder_events;
		std::thread placeholder_worker;
		std::atomic_bool interrupt_placeholder_worker = false;

		bool outer_lock_held = false;
	};

	TimelineModel& m_base_model;
	std::map<ChannelID, std::unique_ptr<Channel>> m_channels;

	Nanoseconds m_min_visible_time;
	Nanoseconds m_max_visible_time;
	int m_pixels;
};

// Generates random events for testing purposes.
class DemoTimelineModel : public TimelineModel
{
public:
	DemoTimelineModel();

	ChannelID rootChannel() override;
	bool channelHasChildren(ChannelID channel) override;
	std::vector<ChannelID> channelChildren(ChannelID channel) override;

	QString channelName(ChannelID channel) override;

	Nanoseconds minTime() override;
	Nanoseconds maxTime() override;

	EventDetails eventDetails(EventID event) override;
	QString eventText(EventID event) override;
	ChannelID eventChannel(EventID event) override;

	void viewChanged(Nanoseconds min_visible_time, Nanoseconds max_visible_time, int pixels) override;

	void startProcessingEvents(ChannelID channel) override;
	std::optional<EventDetails> firstEvent(
		ChannelID channel, Nanoseconds min_time, Nanoseconds max_time) override;
	std::optional<EventDetails> nextEvent(EventID prev_event, Nanoseconds max_time) override;
	std::optional<const std::vector<EventDetails>*> placeholderEvents(ChannelID channel) override;
	void finishProcessingEvents(ChannelID channel) override;

private:
	std::vector<std::vector<EventDetails>> m_events;
};
