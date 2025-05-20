// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "TimelineView.h"

#include "QtHost.h"

#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QStyleOptionHeader>

TimelineView::TimelineView(QWidget* parent)
	: QAbstractScrollArea(parent)
{
	setViewport(m_viewport = new TimelineViewportWidget(this));
	m_ruler = new TimelineRulerWidget(this);
	connect(m_ruler, &TimelineRulerWidget::geometriesChanged, this, &TimelineView::updateGeometries);
}

void TimelineView::setModel(TimelineModel* model)
{
	m_model = model;

	QPointer<TimelineView> view(this);
	QTimer::singleShot(100, [view]() {
		if (!view)
			return;

		float max_delta = view->m_model->maxTime() - view->m_model->minTime();
		if (max_delta == 0)
			return;

		view->m_scroll_x = 0;
		view->m_scroll_y = 0;
		view->m_zoom_x = (view->width() - view->channelNameColumnWidth()) / max_delta;

		view->update();
	});
}

TimelineModel::Nanoseconds TimelineView::minVisibleTime() const
{
	return m_scroll_x;
}

TimelineModel::Nanoseconds TimelineView::maxVisibleTime() const
{
	return m_scroll_x + visibleTimeDelta();
}

TimelineModel::Nanoseconds TimelineView::visibleTimeDelta() const
{
	if (m_zoom_x == 0)
		return 0;

	return (width() - channelNameColumnWidth()) / m_zoom_x;
}

s64 TimelineView::pixelsFromTime(TimelineModel::Nanoseconds time) const
{
	TimelineModel::Nanoseconds delta = visibleTimeDelta();
	if (delta == 0)
		return 0;

	return (time * (m_viewport->width() - channelNameColumnWidth())) / delta;
}

TimelineModel::Nanoseconds TimelineView::timeFromPixels(s64 pixels) const
{
	int space = m_viewport->width() - channelNameColumnWidth();
	if (space == 0)
		return 0;

	return (pixels * (visibleTimeDelta())) / space;
}

int TimelineView::minVisibleChannel() const
{
	return m_scroll_y;
}

int TimelineView::maxVisibleChannel() const
{
	return m_scroll_y + maxVisibleChannelCount();
}

int TimelineView::maxVisibleChannelCount() const
{
	return static_cast<u32>(ceilf(static_cast<float>(height()) / channelHeight()));
}

void TimelineView::zoom(QPoint pixel_delta)
{
	const float old_zoom_x = m_zoom_x;

	if (pixel_delta.x() > 0)
		m_zoom_x *= 1.5f;
	else if (pixel_delta.x() < 0)
		m_zoom_x *= 2.f / 3.f;

	if (m_zoom_x != old_zoom_x)
		update();
}

void TimelineView::scroll(QPoint pixel_delta)
{
	const TimelineModel::Nanoseconds old_scroll_x = m_scroll_x;
	const int old_scroll_y = m_scroll_y;

	m_scroll_x -= timeFromPixels(pixel_delta.x());
	m_scroll_x = std::max(m_model->minTime(), m_scroll_x);
	m_scroll_x = std::min(m_model->maxTime() - visibleTimeDelta(), m_scroll_x);

	const int max_channels_fully_visible = m_viewport->height() / channelHeight();
	const int max_rows = std::max(static_cast<int>(m_viewport->visibleChannelCount()), max_channels_fully_visible);

	m_scroll_y -= pixel_delta.y() / abs(pixel_delta.y());
	m_scroll_y = std::max(0, m_scroll_y);
	m_scroll_y = std::min(max_rows - max_channels_fully_visible, m_scroll_y);

	if (m_scroll_x != old_scroll_x || m_scroll_y != old_scroll_y)
		update();
}

int TimelineView::channelNameColumnWidth() const
{
	return std::min(fontMetrics().averageCharWidth() * 30, width() / 3);
}

int TimelineView::channelHeight() const
{
	return fontMetrics().height() + m_zoom_y;
}

TimelineModel::Nanoseconds TimelineView::seekPosition()
{
	return m_seek_position;
}

void TimelineView::setSeekPosition(TimelineModel::Nanoseconds seek_position)
{
	if (seek_position == m_seek_position)
		return;

	m_seek_position = seek_position;

	emit seekPositionChanged(seek_position);
	update();
}

void TimelineView::updateGeometries()
{
	setViewportMargins(0, m_ruler->sizeHint().height() - 1, 0, 0);
	m_ruler->setGeometry(0, 0, viewport()->width(), m_ruler->height());
}

void TimelineView::resizeEvent(QResizeEvent* event)
{
	updateGeometries();
}

bool TimelineView::viewportEvent(QEvent* event)
{
	// QAbstractScrollArea installs an event filter on its viewport. We want the
	// viewport to handle certain events itself, so we handle that here.
	switch (event->type())
	{
		case QEvent::MouseButtonPress:
		case QEvent::MouseMove:
		case QEvent::KeyPress:
		case QEvent::Leave:
		case QEvent::Paint:
		case QEvent::Wheel:
			return false;
		default:
		{
		}
	}

	return QAbstractScrollArea::viewportEvent(event);
}

// *****************************************************************************

TimelineRulerWidget::TimelineRulerWidget(TimelineView* view)
	: QWidget(view)
	, m_view(view)
{
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

QSize TimelineRulerWidget::sizeHint() const
{
	return QSize(0, fontMetrics().height() + 15);
}

bool TimelineRulerWidget::event(QEvent* event)
{
	switch (event->type())
	{
		case QEvent::Resize:
		case QEvent::Show:
		case QEvent::Hide:
		case QEvent::FontChange:
		case QEvent::StyleChange:
		{
			emit geometriesChanged();
		}
		default:
		{
		}
	}

	return QWidget::event(event);
}

void TimelineRulerWidget::mousePressEvent(QMouseEvent* event)
{
	if (event->pos().x() >= m_view->channelNameColumnWidth())
	{
		TimelineModel::Nanoseconds time = m_view->timeFromPixels(event->pos().x() - m_view->channelNameColumnWidth());
		m_view->setSeekPosition(m_view->minVisibleTime() + time);
		event->accept();
	}
}

void TimelineRulerWidget::mouseMoveEvent(QMouseEvent* event)
{
	if (event->buttons().testFlag(Qt::LeftButton) && event->pos().x() >= m_view->channelNameColumnWidth())
	{
		TimelineModel::Nanoseconds time = m_view->timeFromPixels(event->pos().x() - m_view->channelNameColumnWidth());
		m_view->setSeekPosition(m_view->minVisibleTime() + time);
		event->accept();
	}
}

void TimelineRulerWidget::wheelEvent(QWheelEvent* event)
{
}

void TimelineRulerWidget::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);

	// Draw the background.
	painter.save();
	QStyleOptionHeader opt;
	opt.rect = rect();
	opt.palette = palette();
	style()->drawControl(QStyle::CE_Header, &opt, &painter, this);
	painter.restore();

	// Calculate how many hatch marks should appear.
	const s64 minimum_hatch_mark_dist = fontMetrics().averageCharWidth() * 4;
	const s64 minimum_label_dist = fontMetrics().averageCharWidth() * 20;

	TimelineModel::Nanoseconds hatch_mark_delta = m_view->timeFromPixels(minimum_hatch_mark_dist);
	hatch_mark_delta = powf(10.f, ceilf(log10f(hatch_mark_delta))) / 5;
	if (hatch_mark_delta == 0)
		return;

	painter.setPen(palette().text().color());

	const TimelineModel::Nanoseconds min_time = m_view->minVisibleTime();
	const TimelineModel::Nanoseconds max_time = m_view->maxVisibleTime();

	TimelineModel::Nanoseconds first_hatch_mark_time = min_time;
	if ((first_hatch_mark_time % hatch_mark_delta) != 0)
		first_hatch_mark_time += hatch_mark_delta - (first_hatch_mark_time % hatch_mark_delta);

	// Draw the hatch marks.
	for (TimelineModel::Nanoseconds time = first_hatch_mark_time; time < max_time; time += hatch_mark_delta)
	{
		int index = time / hatch_mark_delta;

		int hatch_mark_height;
		bool draw_label;
		if (index % 10 == 0)
		{
			hatch_mark_height = 6;
			draw_label = true;
		}
		else if (index % 2 == 0)
		{
			hatch_mark_height = 3;
			draw_label = false;
		}
		else
		{
			hatch_mark_height = 2;
			draw_label = false;
		}

		int x = m_view->channelNameColumnWidth() + static_cast<int>(m_view->pixelsFromTime(time - min_time));
		painter.drawLine(x, height() - (hatch_mark_height + 2), x, height() - 2);

		if (draw_label)
		{
			QRect label_rect(x - static_cast<int>(minimum_label_dist) / 2, 0, minimum_label_dist, height() - 9);

			float timef = time;
			QString label_text = QString("%1%2").arg(timef / 1000000.f).arg("ms");

			painter.drawText(label_rect, Qt::AlignHCenter | Qt::AlignBottom, label_text);
		}
	}

	// Draw the scrubber head.
	TimelineModel::Nanoseconds scrubber_time = m_view->seekPosition() - m_view->minVisibleTime();
	int seek_x = m_view->channelNameColumnWidth() + static_cast<int>(m_view->pixelsFromTime(scrubber_time));
	int seek_y = height();
	int size = 8;

	if (seek_x >= m_view->channelNameColumnWidth() && seek_x < width() + size)
	{
		const QPointF points[] = {
			QPointF(seek_x, seek_y),
			QPointF(seek_x - size, seek_y - size),
			QPointF(seek_x - size, seek_y - size * 2),
			QPointF(seek_x + size, seek_y - size * 2),
			QPointF(seek_x + size, seek_y - size),
		};

		painter.setPen(palette().highlight().color().lighter());
		painter.setBrush(palette().highlight().color());
		painter.drawConvexPolygon(points, std::size(points));
	}
}

// *****************************************************************************

TimelineViewportWidget::TimelineViewportWidget(TimelineView* view)
	: QWidget(view)
	, m_view(view)
{
	setMouseTracking(true);
}

void TimelineViewportWidget::updateHoveredItem(QPoint cursor_pos)
{
	TimelineModel::ChannelID new_hovered_channel = TimelineModel::INVALID_CHANNEL;
	TimelineModel::EventID new_hovered_event = TimelineModel::INVALID_EVENT;

	int index = m_view->minVisibleChannel() + (cursor_pos.y() / m_view->channelHeight());
	auto channel = m_index_to_channel.find(index);
	if (channel != m_index_to_channel.end())
	{
		if (cursor_pos.x() < m_view->channelNameColumnWidth())
		{
			if (model()->channelHasChildren(channel->second))
				new_hovered_channel = channel->second;
		}
		else
		{
			TimelineModel::Nanoseconds time =
				m_view->minVisibleTime() + m_view->timeFromPixels(cursor_pos.x() - m_view->channelNameColumnWidth());
			std::optional<TimelineModel::EventDetails> event =
				model()->firstEvent(channel->second, time, m_view->maxVisibleTime());
			if (event.has_value() && event->start_time <= time)
				new_hovered_event = event->id;
		}
	}

	if (new_hovered_channel != m_hovered_channel || new_hovered_event != m_hovered_event)
		update();

	m_hovered_channel = new_hovered_channel;
	m_hovered_event = new_hovered_event;
}

void TimelineViewportWidget::mousePressEvent(QMouseEvent* event)
{
	if (event->pos().x() < m_view->channelNameColumnWidth())
	{
		if (m_hovered_channel != TimelineModel::INVALID_CHANNEL)
		{
			bool& collapsed = m_channel_collapsed[m_hovered_channel];
			collapsed = !collapsed;
			update();
		}
	}
	else
	{
		TimelineModel::EventID old_selected_event = m_selected_event;
		m_selected_event = m_hovered_event;
		if (m_selected_event != old_selected_event)
		{
			emit m_view->selectedEventChanged(m_selected_event);
			update();
		}
	}
}

void TimelineViewportWidget::mouseMoveEvent(QMouseEvent* event)
{
	updateHoveredItem(event->pos());

	if (event->buttons().testFlag(Qt::LeftButton) && event->pos().x() >= m_view->channelNameColumnWidth())
	{
		if (m_hovered_event != m_selected_event)
			update();

		m_selected_event = m_hovered_event;
	}
}

void TimelineViewportWidget::wheelEvent(QWheelEvent* event)
{
	QPoint pixel_delta = event->pixelDelta();

	if (event->position().x() >= m_view->channelNameColumnWidth())
		pixel_delta = pixel_delta.transposed();

	Qt::KeyboardModifiers modifiers = QGuiApplication::queryKeyboardModifiers();
	if (modifiers.testFlag(Qt::ShiftModifier))
		m_view->zoom(pixel_delta);
	else
		m_view->scroll(pixel_delta);

	updateHoveredItem(event->position().toPoint());
}

void TimelineViewportWidget::keyPressEvent(QKeyEvent* event)
{
}

void TimelineViewportWidget::leaveEvent(QEvent* event)
{
	if (m_hovered_channel == TimelineModel::INVALID_CHANNEL && m_hovered_event == TimelineModel::INVALID_CHANNEL)
		return;

	m_hovered_channel = TimelineModel::INVALID_CHANNEL;
	m_hovered_event = TimelineModel::INVALID_EVENT;

	update();
}

void TimelineViewportWidget::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);

	m_index_to_channel.clear();

	// Draw all the channels and events.
	int index = 0;
	drawChannelChildren(model()->rootChannel(), index, 0, painter);
	m_visible_channel_count = index;

	// Fill in the remaining space.
	if (static_cast<int>(index * m_view->channelHeight()) < height())
	{
		QRect empty_space = channelRect(index);
		empty_space.setBottom(height());
		painter.fillRect(empty_space, palette().base());
	}

	QStyleOptionViewItem option;
	const int grid_hint = style()->styleHint(QStyle::SH_Table_GridLineColor, &option, this);
	QColor grid_color = QColor::fromRgba(static_cast<QRgb>(grid_hint));

	// Draw the dividing line between the channel name column and the events.
	QLinearGradient divider_gradient(m_view->channelNameColumnWidth(), 0, m_view->channelNameColumnWidth() + 2, 0);
	divider_gradient.setColorAt(0, grid_color);
	divider_gradient.setColorAt(1, Qt::transparent);
	painter.fillRect(QRect(m_view->channelNameColumnWidth(), 0, 2, height()), divider_gradient);

	// Draw the scrubber bar.
	TimelineModel::Nanoseconds scrubber_time = m_view->seekPosition() - m_view->minVisibleTime();
	int seek_x = m_view->channelNameColumnWidth() + static_cast<int>(m_view->pixelsFromTime(scrubber_time));
	if (seek_x >= m_view->channelNameColumnWidth())
	{
		painter.setPen(palette().highlight().color().lighter());
		painter.drawLine(seek_x - 2, 0, seek_x - 2, height());
		painter.drawLine(seek_x, 0, seek_x, height());

		painter.setPen(palette().highlight().color());
		painter.drawLine(seek_x - 1, 0, seek_x - 1, height());
	}
}

void TimelineViewportWidget::drawChannelChildren(
	TimelineModel::ChannelID parent, int& index, int depth, QPainter& painter)
{
	for (TimelineModel::ChannelID child : model()->channelChildren(parent))
	{
		drawChannel(child, index, depth, painter);
		index++;

		auto collapsed = m_channel_collapsed.find(child);
		if (collapsed == m_channel_collapsed.end() || !collapsed->second)
			drawChannelChildren(child, index, depth + 1, painter);
	}
}

void TimelineViewportWidget::drawChannel(
	TimelineModel::ChannelID channel, int index, int depth, QPainter& painter)
{
	m_index_to_channel[index] = channel;

	const QRect channel_rect = channelRect(index);
	if (!channel_rect.intersects(rect()))
		return;

	// Draw the background.
	QColor background_colour;
	if (index % 2 == 0)
		background_colour = palette().base().color();
	else
		background_colour = palette().alternateBase().color();
	painter.fillRect(channel_rect, background_colour);

	// Draw the channel name column.
	drawChannelName(channel, index, depth, painter);

	// Draw all the events.
	std::optional<TimelineModel::EventDetails> event = model()->firstEvent(
		channel, m_view->minVisibleTime(), m_view->maxVisibleTime());

	painter.setClipRect(eventsClipRect());

	QRect selected_event_rect;
	while (event.has_value())
	{
		QRect event_rect = drawEvent(*event, index, painter);
		if (event->id == m_selected_event)
			selected_event_rect = event_rect;

		event = model()->nextEvent(event->id, m_view->maxVisibleTime());
	}

	// Draw the selection highlight last so it doesn't get partially covered if
	// another event comes directly after it.
	if (!selected_event_rect.isEmpty())
	{
		QPen pen(palette().highlight().color(), 2);
		pen.setJoinStyle(Qt::MiterJoin); // Prevent rounded corners.
		painter.setPen(pen);
		painter.drawRect(selected_event_rect.marginsRemoved(QMargins(0, 2, -1, 1)));
	}

	painter.setClipRect(rect());
}

void TimelineViewportWidget::drawChannelName(
	TimelineModel::ChannelID channel, int index, int depth, QPainter& painter)
{
	const auto collapsed = m_channel_collapsed.find(channel);
	const bool channel_has_children = model()->channelHasChildren(channel);
	const bool expanded = collapsed == m_channel_collapsed.end() || !collapsed->second;

	if (channel == m_hovered_channel)
	{
		QColor selected_color = palette().highlight().color();
		selected_color.setAlpha(64);
		painter.fillRect(channelNameRect(index), selected_color);
	}

	const QRect name_column_rect = channelNameRect(index);
	const int indentation = style()->pixelMetric(QStyle::PM_TreeViewIndentation, nullptr, this);

	QRect arrow_rect = name_column_rect;
	arrow_rect.moveLeft(depth * indentation);
	arrow_rect.setWidth(indentation);

	QStyleOptionViewItem arrow_opt;
	arrow_opt.rect = arrow_rect;
	arrow_opt.state.setFlag(QStyle::State_MouseOver, channel == m_hovered_channel);
	arrow_opt.state.setFlag(QStyle::State_Open, expanded);
	arrow_opt.state.setFlag(QStyle::State_Children, channel_has_children);
	style()->drawPrimitive(QStyle::PE_IndicatorBranch, &arrow_opt, &painter, this);

	QRect name_rect = name_column_rect;
	name_rect.moveLeft((depth + 1) * indentation);
	name_rect.setWidth(name_column_rect.width() - (depth + 1) * indentation);

	painter.setPen(palette().text().color());
	painter.drawText(name_rect, Qt::AlignLeft | Qt::AlignVCenter, model()->channelName(channel));
}

static QColor getFusionHeaderColour(const QPalette& palette)
{
	// Determine colour of column header. Based on QFusionStyle.
	QColor title_line_color = palette.button().color();
	const int title_line_color_val = qGray(title_line_color.rgb());
	title_line_color = title_line_color.lighter(100 + qMax(1, (180 - title_line_color_val) / 6));
	title_line_color.setHsv(title_line_color.hue(), title_line_color.saturation() * 0.75, title_line_color.value());
	return title_line_color.lighter(104);
}

QRect TimelineViewportWidget::drawEvent(const TimelineModel::EventDetails& event, int channel_index, QPainter& painter)
{
	const int start_pos = static_cast<int>(m_view->pixelsFromTime(event.start_time - m_view->minVisibleTime()));
	const int end_pos = static_cast<int>(m_view->pixelsFromTime(event.stop_time - m_view->minVisibleTime()));
	const int event_width = end_pos - start_pos;

	QRect bounds = eventRect(channel_index, start_pos, end_pos);
	bounds = bounds.marginsRemoved(QMargins(0, 0, 1, 1));

	// The event colour will be a primary or secondary colour, so we need to
	// augment it to find a nice colour to show in the user interface.
	const QColor header_colour = getFusionHeaderColour(palette());
	QColor event_colour(
		(qRed(event.colour) * header_colour.red()) / 255,
		(qGreen(event.colour) * header_colour.green()) / 255,
		(qBlue(event.colour) * header_colour.blue()) / 255);

	if (!QtHost::IsDarkApplicationTheme())
	{
		event_colour = QColor::fromHsv(
			event_colour.hsvHue(),
			event_colour.hsvSaturation() / 3,
			event_colour.value());
	}

	// Draw the background.
	QColor event_background_colour = event_colour;
	if (event.id == m_hovered_event || event.id == m_selected_event)
		event_background_colour = event_background_colour.lighter();
	painter.fillRect(bounds.marginsRemoved(QMargins(0, 2, -1, 1)), event_background_colour);

	// Only draw the borders if there's enough space.
	if (event_width > 2)
	{
		painter.setPen(event_colour.darker());
		painter.drawRect(bounds.marginsRemoved(QMargins(0, 2, 0, 2)));

		if (event.id != m_selected_event)
		{
			painter.setPen(event_colour.lighter());
			painter.drawRect(bounds.marginsRemoved(QMargins(1, 3, 1, 3)));
		}
	}

	// Only draw text if there's enough space.
	if (event_width > 8)
	{
		painter.setPen(palette().text().color());
		QRect text_bounds = bounds.marginsRemoved(QMargins(4, 0, 4, 0));
		text_bounds.setLeft(std::max(text_bounds.left(), m_view->channelNameColumnWidth()));
		painter.drawText(text_bounds, Qt::AlignLeft | Qt::AlignVCenter, model()->eventName(event.id));
	}

	return bounds;
}

QRect TimelineViewportWidget::channelRect(int index)
{
	return QRect(
		0,
		(index - m_view->minVisibleChannel()) * m_view->channelHeight(),
		width(),
		m_view->channelHeight());
}

QRect TimelineViewportWidget::channelNameRect(int index)
{
	return QRect(
		0,
		(index - m_view->minVisibleChannel()) * m_view->channelHeight(),
		m_view->channelNameColumnWidth(),
		m_view->channelHeight());
}

QRect TimelineViewportWidget::eventRect(int channel_index, int start_pos, int end_pos)
{
	return QRect(
		m_view->channelNameColumnWidth() + start_pos,
		(channel_index - m_view->minVisibleChannel()) * m_view->channelHeight(),
		end_pos - start_pos,
		m_view->channelHeight());
}

QRect TimelineViewportWidget::eventsClipRect()
{
	return QRect(m_view->channelNameColumnWidth(), 0, width() - m_view->channelNameColumnWidth(), height());
}
