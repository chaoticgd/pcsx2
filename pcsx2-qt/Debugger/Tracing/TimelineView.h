// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Debugger/Tracing/TimelineModels.h"

#include <QtWidgets/QAbstractScrollArea>

class TimelineRulerWidget;
class TimelineViewportWidget;

// Analagous to a QTreeView but for displaying a Gantt chart-like timeline of
// events rather than multiple different columns. The events shown are provided
// by a subclass of TimelineModel.
class TimelineView : public QAbstractScrollArea
{
	Q_OBJECT

public:
	TimelineView(QWidget* parent = nullptr);

	__fi TimelineModel* model() { return m_model; }
	void setModel(TimelineModel* model);

	// X-axis scroll/zoom information.
	TimelineModel::Nanoseconds minVisibleTime() const;
	TimelineModel::Nanoseconds maxVisibleTime() const;
	TimelineModel::Nanoseconds visibleTimeDelta() const;

	// Y-axis scroll/zoom information.
	int minVisibleChannel() const;
	int maxVisibleChannel() const;
	int maxVisibleChannelCount() const;

	// Convert a given duration of time into the number of pixels which will
	// represent said duration on the X-axis taking into account the current
	// zoom level, but not the scroll position or the name column offset.
	s64 pixelsFromTime(TimelineModel::Nanoseconds time) const;

	// Convert a given number of pixels into the length of time they represent
	// on the X-axis taking into account the current zoom level, but not the
	// scroll position of the name column offset.
	TimelineModel::Nanoseconds timeFromPixels(s64 pixels) const;

	void zoom(QPoint pixel_delta);
	void scroll(QPoint pixel_delta);

	int channelNameColumnWidth() const;
	int channelHeight() const;

	TimelineModel::Nanoseconds seekPosition();
	void setSeekPosition(TimelineModel::Nanoseconds seek_position);

Q_SIGNALS:
	void selectedEventChanged(TimelineModel::EventID event);
	void seekPositionChanged(TimelineModel::Nanoseconds time);

protected:
	void updateGeometries();

	void resizeEvent(QResizeEvent* event) override;
	bool viewportEvent(QEvent* event) override;

private:
	TimelineModel* m_model = nullptr;

	TimelineRulerWidget* m_ruler;
	TimelineViewportWidget* m_viewport;

	TimelineModel::Nanoseconds m_scroll_x = 0;
	int m_scroll_y = 0;

	float m_zoom_x = 0;
	int m_zoom_y = 10;

	TimelineModel::Nanoseconds m_seek_position = 0;
};

// The ruler of time.
class TimelineRulerWidget : public QWidget
{
	Q_OBJECT

public:
	TimelineRulerWidget(TimelineView* view);

	QSize sizeHint() const override;

Q_SIGNALS:
	void geometriesChanged();

protected:
	bool event(QEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void paintEvent(QPaintEvent* event) override;

private:
	__fi TimelineModel* model() { return m_view->model(); }

	TimelineView* m_view;
};

class TimelineViewportWidget : public QWidget
{
	Q_OBJECT

public:
	TimelineViewportWidget(TimelineView* view);

	__fi int visibleChannelCount() { return m_visible_channel_count; }

protected:
	void updateHoveredItem(QPoint cursor_pos);
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void leaveEvent(QEvent* event) override;
	void paintEvent(QPaintEvent* event) override;

private:
	size_t drawChannelChildren(TimelineModel::ChannelID parent, int& index, int depth, QPainter& painter);
	size_t drawChannel(TimelineModel::ChannelID channel, int index, int depth, QPainter& painter);
	void drawChannelName(TimelineModel::ChannelID channel, int index, int depth, QPainter& painter);
	QRect drawEvent(
		const TimelineModel::EventDetails& event, int channel_index, bool is_placeholder, QPainter& painter);

	QRect channelRect(int index);
	QRect channelNameRect(int index);
	QRect eventRect(int channel_index, int start_pos, int end_pos);

	QRect eventsClipRect();

	__fi TimelineModel* model() { return m_view->model(); }

	TimelineView* m_view;

	TimelineModel::ChannelID m_hovered_channel = TimelineModel::INVALID_CHANNEL;
	TimelineModel::EventID m_hovered_event = TimelineModel::INVALID_EVENT;
	TimelineModel::EventID m_selected_event = TimelineModel::INVALID_EVENT;

	std::map<u32, TimelineModel::ChannelID> m_index_to_channel;
	std::map<TimelineModel::ChannelID, bool> m_channel_collapsed;

	int m_visible_channel_count = 0;
};
