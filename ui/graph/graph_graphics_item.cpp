/*
 Copyright 2019 Google LLC

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/
#include "graph_graphics_item.h"
#include <QApplication>
#include <QGraphicsSceneHoverEvent>
#include <QPainter>
#include <QToolTip>
#include <algorithm>
#include <cmath>
#include "dive_core/capture_data.h"
#include "dive_core/data_core.h"
#include "hover_help_model.h"

//--------------------------------------------------------------------------------------------------
GraphGraphicsItem::GraphGraphicsItem(const Dive::PerfettoData &perfetto_data) :
    m_selection_region_prev_cycle(0),
    m_selection_region_start_cycle(0),
    m_selection_region_end_cycle(0),
    m_ignore_mouse_move(false),
    m_total_cycles(0),
    m_perfetto_data(perfetto_data)
{
    m_width = 0;
    m_hover_help = HoverHelp::Get();
    setAcceptHoverEvents(true);
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::ResetSelectionRegion()
{
    m_selection_region_start_cycle = 0;
    m_selection_region_end_cycle = 0;
    m_selection_region_prev_cycle = 0;

    m_nav_first_selected_event = 0;
    m_nav_last_selected_event = 0;

    SetSelectionInfo(SelectionType::kNone);
    m_ignore_mouse_move = true;
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::SetWidth(uint64_t width)
{
    m_width = width;
}

//--------------------------------------------------------------------------------------------------
uint64_t GraphGraphicsItem::GetSelectionStartCycle() const
{
    return m_selection_region_start_cycle;
}

//--------------------------------------------------------------------------------------------------
uint64_t GraphGraphicsItem::GetSelectionEndCycle() const
{
    return m_selection_region_end_cycle;
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::SetTotalCycles(uint64_t total_cycles)
{
    DIVE_ASSERT(total_cycles > 0);
    m_total_cycles = total_cycles;
}

//--------------------------------------------------------------------------------------------------
uint64_t GraphGraphicsItem::GetTotalCycles()
{
    return m_total_cycles;
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::EnableSerialChart(bool enable)
{
    m_enable_serial_chart = enable;
}

//--------------------------------------------------------------------------------------------------
bool GraphGraphicsItem::IsSerialChartEnabled()
{
    return m_enable_serial_chart;
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::SetVisibleRange(int64_t scene_x, int64_t width)
{
    // Convert to local item coordinate
    int32_t item_x = mapFromScene(scene_x, 0).x();

    m_visible_start = item_x;
    m_visible_width = width;
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::SelectNavigatorStream(uint32_t stream_id)
{
    std::fill(m_select_nav_stream.begin(), m_select_nav_stream.end(), false);
    m_select_nav_stream[stream_id] = true;
}

//--------------------------------------------------------------------------------------------------
uint32_t GraphGraphicsItem::GetNavigatorStartY(uint32_t row) const
{
    uint32_t prev_end_y = 0;
    uint32_t start_y = prev_end_y + kNavStartPadding;
    return start_y + (row * (GetNavigatorHeight() + GetRectPadding()));
}

//--------------------------------------------------------------------------------------------------
uint32_t GraphGraphicsItem::GetNavigatorLabelStartY(uint32_t row) const
{
    return GetNavigatorStartY(row);
}

//--------------------------------------------------------------------------------------------------
uint32_t GraphGraphicsItem::GetNavigatorEventStartY(uint32_t row) const
{
    uint32_t startY = GetNavigatorStartY(row);
    if (m_show_labels)
        startY += GetNavigatorLabelHeight() + GetRectPadding();
    return startY;
}

//--------------------------------------------------------------------------------------------------
uint32_t GraphGraphicsItem::GetNavigatorBarrierStartY(uint32_t row) const
{
    return GetNavigatorEventStartY(row) + GetNavigatorEventHeight() + GetRectPadding();
}

//--------------------------------------------------------------------------------------------------
uint32_t GraphGraphicsItem::GetNavigatorHeight() const
{
    uint32_t height = GetNavigatorEventHeight();
    if (m_show_labels)
        height += GetNavigatorLabelHeight() + GetRectPadding();
    if (m_show_barriers)
        height += GetNavigatorBarrierHeight() + GetRectPadding();
    return height;
}

//--------------------------------------------------------------------------------------------------
QRectF GraphGraphicsItem::boundingRect() const
{
    double height = GetRectHeight();

    // If navigator is at the bottom, then the bounding rect needs to be expanded to include it
    if (GetNavigatorBarrierStartY(GetNumNavigatorRows() - 1) + GetNavigatorBarrierHeight() > height)
    {
        height = GetNavigatorBarrierStartY(GetNumNavigatorRows() - 1) +
                 GetNavigatorBarrierHeight() + kSelectedRectExtraPadding;
    }
    return QRectF(0, 0, m_width, height);
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::paint(QPainter                       *painter,
                              const QStyleOptionGraphicsItem *option,
                              QWidget                        *widget)
{
    QFont font;
    font.setFamily(font.defaultFamily());
    font.setPixelSize((int)(GetRectHeight() * 0.65));
    painter->setFont(font);

    DrawNavigator(painter);
    DrawSelectionRegion(painter);
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    m_hover_msg_sent = false;
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    ResetSelectionRegion();

    // Pressing any non-left mouse button resets the selection only
    // In practice, this allows de-selecting anything selected by using a non-left-button
    if (event->button() != Qt::LeftButton)
    {
        // Update the scene to reflect a potential de-selection
        update();
        return;
    }

    double item_coord_to_time_range = (double)m_perfetto_data.GetTotalTimeDuration() / m_width;
    m_selection_region_prev_cycle = event->pos().x() * item_coord_to_time_range;
    m_selection_region_start_cycle = m_selection_region_end_cycle = m_selection_region_prev_cycle;
    m_selection_start_pos = event->pos();
    m_ignore_mouse_move = false;
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_ignore_mouse_move)
        return;

    // Prevent drag-select if the user hasn't moved a certain distance
    if ((event->pos() - m_selection_start_pos).manhattanLength() <=
        QApplication::startDragDistance())
        return;

    double   item_coord_to_time_range = (double)m_perfetto_data.GetTotalTimeDuration() / m_width;
    uint64_t new_cycle = event->pos().x() * item_coord_to_time_range;

    // Selection region can be left-to-right or right-to-left
    if (new_cycle < m_selection_region_start_cycle)
    {
        m_selection_region_start_cycle = new_cycle;
    }
    else if (new_cycle > m_selection_region_end_cycle)
    {
        m_selection_region_end_cycle = new_cycle;
    }
    else
    {
        if (new_cycle > m_selection_region_prev_cycle)
        {
            m_selection_region_start_cycle = new_cycle;
        }
        else if (new_cycle < m_selection_region_prev_cycle)
        {
            m_selection_region_end_cycle = new_cycle;
        }
    }
    m_selection_region_prev_cycle = new_cycle;

    // Update the scene to reflect visual changes
    update();
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (m_selection_region_start_cycle == m_selection_region_end_cycle)  // Event selection
    {
        // Calling HandleSelection() may eventually end up calling ensureVisible(), which itself can
        // somehow generate a mouseMoveEvent. This is likely because the ensureVisible() is called
        // from within a mouseReleaseEvent, before the "release" is registered with Qt. So the
        // adjustment of the mouse cursor ends up generating a mouse-move because Qt still thinks
        // the mouse is not released yet. This will then incorrectly cause a region select.
        // Workaround is to manually disable mouse-moves via a flag
        m_ignore_mouse_move = true;
        HandleSelection(event->pos().x(), event->pos().y());
        m_ignore_mouse_move = false;
    }

    // Update the scene to reflect visual changes
    update();
}

//--------------------------------------------------------------------------------------------------
// Draw horizontal grid lines
void GraphGraphicsItem::DrawHGridLines(QPainter *painter)
{
    int64_t visible_start = std::max((int64_t)0, m_visible_start);
    int64_t visible_end = std::clamp<int64_t>(m_visible_start + m_visible_width, 0, m_width);

    QPen grid_line_pen(Qt::darkGray);
    painter->setPen(grid_line_pen);
    for (uint32_t slot = 0; slot < 4; ++slot)
    {
        painter->drawLine(visible_start, GetRowY(slot), visible_end, GetRowY(slot));
    }
}

//--------------------------------------------------------------------------------------------------
QColor GraphGraphicsItem::GetSliceColor(const std::string &slice_name) const
{
    QColor slice_color = QColor(128, 128, 128);
    if (slice_name == "Binning")
        slice_color = QColor(0, 215, 215);
    else if (slice_name == "GMEM Load Color")
        slice_color = QColor(0, 150, 0);
    else if (slice_name == "GMEM Load Depth/Stencil")
        slice_color = QColor(160, 160, 160);
    else if (slice_name == "Render")
        slice_color = QColor(255, 160, 0);
    else if (slice_name == "GMEM Store Color")
        slice_color = QColor(110, 110, 255);
    else if (slice_name == "GMEM Store Depth/Stencil")
        slice_color = QColor(255, 70, 70);
    return slice_color;
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::DrawNavigator(QPainter *painter)
{
    uint32_t row = 1;
    int64_t  visible_start = std::max((int64_t)0, m_visible_start);
    int64_t  visible_end = std::clamp<int64_t>(m_visible_start + m_visible_width, 0, m_width);

    QColor border_color(198, 113, 0);  // Dark amber color

    // Conversion factors
    double item_coord_to_time_range = (double)m_perfetto_data.GetTotalTimeDuration() / m_width;

    uint64_t visible_start_ts = visible_start * item_coord_to_time_range;
    uint64_t visible_end_ts = visible_end * item_coord_to_time_range;

    for (const auto &submission : m_perfetto_data.m_submission_data)
    {
        for (const auto &slice : submission.m_data)
        {
            uint64_t slice_start_ts = m_perfetto_data.GetRelativeTime(slice.m_ts);
            uint64_t slice_end_ts = slice_start_ts + slice.m_duration;

            if (slice_end_ts < visible_start_ts)
                continue;

            if (slice_start_ts > visible_end_ts)
                break;

            uint64_t event_rect_start_x, event_rect_end_x;
            CalcRectCoord(slice_start_ts, slice_end_ts, &event_rect_start_x, &event_rect_end_x);
            DrawEventRect(painter,
                          border_color,
                          GetSliceColor(slice.m_name),
                          slice.m_name,
                          event_rect_start_x,
                          GetNavigatorEventStartY(row),
                          event_rect_end_x - event_rect_start_x,
                          GetNavigatorEventHeight(),
                          false,
                          false);
        }
    }

    // while ((uint32_t)id < events.GetNumEvents())
    //{
    //     auto     start_event_id = id;
    //     uint64_t event_start_cycle = GetEventStartCycle(id);
    //     uint64_t event_end_cycle = GetEventEndCycle(id);
    //     ++id;

    //    // Is it better to do a binary search first?
    //    if (event_end_cycle < visible_start_cycle)
    //        continue;

    //    if (event_start_cycle > visible_end_cycle)
    //        break;

    //    // Only show matching streams
    //    if (events.GetStream(start_event_id) != stream_id)
    //        continue;

    //    // Skip drawing of selected events. They will be draw in a different pass.
    //    if (IsEventHighlighted(start_event_id))
    //        continue;

    //    double conv = time_range_to_item_coord;

    //    // If a future Event fully covers over this one, then skip to that one
    //    {
    //        uint64_t event_start_x = (uint64_t)(event_start_cycle * conv);
    //        uint64_t event_end_x = (uint64_t)(event_end_cycle * conv);
    //        for (; (uint32_t)id < events.GetNumEvents(); ++id)
    //        {
    //            // skip events on other streams
    //            if (events.GetStream(id) != stream_id)
    //                continue;
    //            uint64_t next_event_start_cycle = GetEventStartCycle(id);
    //            uint64_t next_event_end_cycle = GetEventEndCycle(id);
    //            uint64_t next_event_start_x = (uint64_t)(next_event_start_cycle * conv);
    //            uint64_t next_event_end_x = (uint64_t)(next_event_end_cycle * conv);
    //            if (event_start_x != next_event_start_x || event_end_x > next_event_end_x)
    //                break;
    //            start_event_id = id;
    //            event_end_x = next_event_end_x;
    //            event_start_cycle = next_event_start_cycle;
    //            event_end_cycle = next_event_end_cycle;
    //        }
    //    }

    //    uint64_t event_rect_start_x, event_rect_end_x;
    //    CalcRectCoord(event_start_cycle, event_end_cycle, &event_rect_start_x, &event_rect_end_x);
    //    DrawEventRect(painter,
    //                  border_color,
    //                  QColor(255, 160, 0),
    //                  event_rect_start_x,
    //                  GetNavigatorEventStartY(row),
    //                  event_rect_end_x - event_rect_start_x,
    //                  GetNavigatorEventHeight(),
    //                  false,
    //                  false);

    //    // Skip over all events in different streams
    //    while ((uint32_t)id < events.GetNumEvents() && events.GetStream(id) != stream_id)
    //        ++id;
    //}

    // Draw selected/highlighted events/barriers
    DrawNavigatorHighlighted(painter);
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::DrawEventRect(QPainter   *painter,
                                      QColor      pen_color,
                                      QColor      brush_color,
                                      std::string slice_name,
                                      double      x,
                                      double      y,
                                      double      width,
                                      double      height,
                                      bool        darken,
                                      bool        draw_shader_stages)
{
    if (darken)
    {
        painter->setPen(pen_color.darker(300));
        painter->setBrush(brush_color.darker(300));
    }
    else
    {
        painter->setPen(pen_color);
        painter->setBrush(brush_color);
    }

    // Draw Event rect
    QRectF event_rect(x, y, width, height);
    painter->drawRect(event_rect);

    // Draw event id, but only if the text size is smaller than the wavefront rect size
    {
        QString event_id_str = QString::fromStdString(slice_name);
        QRect   string_rect = painter->boundingRect(QRect(0, 0, 0, 0), Qt::AlignLeft, event_id_str);
        int     text_width = string_rect.width();
        DIVE_ASSERT(text_width >= 0);
        if (width > (uint64_t)text_width)
        {
            painter->setPen(QColor(0, 0, 0));

            // At really large coordinates, drawText fails. This is a known Qt problem
            // that will not be fixed. So use the worldTransform as a workaround.
            double     coord_x = event_rect.x();
            QTransform transform = painter->worldTransform();
            transform.translate(coord_x, 0);
            painter->setWorldTransform(transform);
            QRectF text_rect = QRectF(0, event_rect.y(), event_rect.width(), event_rect.height());
            painter->drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, event_id_str);
            transform.translate(-coord_x, 0);
            painter->setWorldTransform(transform);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::DrawSelectionRegion(QPainter *painter)
{
    if (m_selection_region_end_cycle > m_selection_region_start_cycle)
    {
        uint64_t selection_region_start_x = 0, selection_region_end_x = 0;

        CalcRectCoord(m_selection_region_start_cycle,
                      m_selection_region_end_cycle,
                      &selection_region_start_x,
                      &selection_region_end_x);

        // If we have a negative region, it means it is not visible and we should not draw it.
        if (selection_region_end_x <= selection_region_start_x)
        {
            return;
        }

        QRectF select_region = QRectF(selection_region_start_x,
                                      0,
                                      selection_region_end_x - selection_region_start_x,
                                      boundingRect().height());
        painter->setOpacity(0.4);
        painter->setBrush(QColor(225, 245, 254));
        painter->drawRect(select_region);
        painter->setOpacity(1.0);
        SetSelectionInfo(SelectionType::kDrag);
    }
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::DrawNavigatorHighlighted(QPainter *painter)
{
    const auto &events = m_sqtt_data_ptr->GetEvents();

    std::set<Dive::SqttEventId> selected_events;
    uint64_t                    selected_start_cycle = UINT64_MAX;
    uint64_t                    selected_end_cycle = 0;
    Dive::SqttStreamId          selected_stream = Dive::SqttStreamId::kNone;
    if (m_nav_first_selected_event != Dive::SqttEventId() &&
        m_nav_last_selected_event != Dive::SqttEventId())
    {
        auto stream_id = events.GetStream(m_nav_first_selected_event);
        DIVE_ASSERT(selected_stream == Dive::SqttStreamId::kNone || selected_stream == stream_id);
        selected_stream = stream_id;
        for (auto event_id = m_nav_first_selected_event; event_id <= m_nav_last_selected_event;
             ++event_id)
        {
            if (events.GetStream(event_id) == stream_id)
            {
                selected_events.insert(event_id);
                selected_start_cycle = std::min<uint64_t>(GetEventStartCycle(event_id),
                                                          selected_start_cycle);
                selected_end_cycle = std::max<uint64_t>(GetEventEndCycle(event_id),
                                                        selected_end_cycle);
            }
        }
    }
    std::set<Dive::SqttEventId> hovered_events;
    uint64_t                    hovered_start_cycle = UINT64_MAX;
    uint64_t                    hovered_end_cycle = 0;
    Dive::SqttStreamId          hovered_stream = Dive::SqttStreamId::kNone;
    bool                        hovered_show_shader_stages = false;
    if (m_nav_hovered_event != Dive::SqttEventId())
    {
        hovered_show_shader_stages = true;
        hovered_events.insert(m_nav_hovered_event);
        selected_events.erase(m_nav_hovered_event);
        hovered_start_cycle = std::min<uint64_t>(GetEventStartCycle(m_nav_hovered_event),
                                                 hovered_start_cycle);
        hovered_end_cycle = std::max<uint64_t>(GetEventEndCycle(m_nav_hovered_event),
                                               hovered_end_cycle);
        auto stream_id = events.GetStream(m_nav_hovered_event);
        DIVE_ASSERT(hovered_stream == Dive::SqttStreamId::kNone || hovered_stream == stream_id);
        hovered_stream = stream_id;
    }

    std::vector<Dive::LabelMarkerId> highlighted_labels;
    if (m_nav_hovered_label != Dive::LabelMarkerId())
    {
        highlighted_labels.push_back(m_nav_hovered_label);
        auto label_it = labels.find(m_nav_hovered_label);
        auto event_marker_begin = event_markers.find(label_it->FirstEvent());
        auto event_marker_end = event_marker_begin + label_it->EventCount();
        for (auto event_marker_it = event_marker_begin; event_marker_it != event_marker_end;
             ++event_marker_it)
        {
            auto event_id = event_marker_it->SqttEvent();
            hovered_events.insert(event_id);
            selected_events.erase(event_id);
        }

        auto barrier_begin = barriers.find(label_it->FirstBarrier());
        auto barrier_end = barrier_begin + label_it->BarrierCount();
        for (auto barrier_it = barrier_begin; barrier_it != barrier_end; ++barrier_it)
        {
            auto barrier_id = barrier_it->id();
            highlighted_barriers.push_back(barrier_id);
        }

        hovered_start_cycle = std::min<uint64_t>(GetLabelStartCycle(m_nav_hovered_label),
                                                 hovered_start_cycle);
        hovered_end_cycle = std::max<uint64_t>(GetLabelEndCycle(m_nav_hovered_label),
                                               hovered_end_cycle);
    }
    if (m_nav_selected_label != Dive::LabelMarkerId())
    {
        highlighted_labels.push_back(m_nav_selected_label);
    }

    DrawNavigatorHighlightedEvents(painter, hovered_events, hovered_show_shader_stages);
    DrawNavigatorHighlightedEvents(painter, selected_events, false);

    // Draw vertical lines to show begin and end of selected/hovered events/barriers
    QPen grid_line_pen(Qt::darkGray);
    painter->setPen(grid_line_pen);
    if (selected_stream != Dive::SqttStreamId::kNone)
    {
        uint64_t start_x, end_x;
        CalcRectCoord(selected_start_cycle, selected_end_cycle, &start_x, &end_x);
        uint32_t row = GetStreamNavigatorRow(selected_stream);
        uint32_t bottom = GetNavigatorStartY(row) + GetNavigatorHeight();
        painter->drawLine(start_x, 0, start_x, bottom);
        painter->drawLine(end_x, 0, end_x, bottom);
    }
    if (hovered_stream != Dive::SqttStreamId::kNone)
    {
        uint64_t start_x, end_x;
        CalcRectCoord(hovered_start_cycle, hovered_end_cycle, &start_x, &end_x);
        uint32_t row = GetStreamNavigatorRow(hovered_stream);
        uint32_t bottom = GetNavigatorStartY(row) + GetNavigatorHeight();
        painter->drawLine(start_x, 0, start_x, bottom);
        painter->drawLine(end_x, 0, end_x, bottom);
    }
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::DrawNavigatorHighlightedEvents(QPainter *painter, bool draw_shader_stages)
{
}

//--------------------------------------------------------------------------------------------------
GraphGraphicsItem::NavigatorHit GraphGraphicsItem::FindNavigatorHit(double item_x,
                                                                    double item_y) const
{
    NavigatorHit hit = { kInvalidSlice, kInvalidSlice, kInvalidSlice };
    if (m_perfetto_data.IsEmpty())
        return hit;

    // Conversion factors
    double   item_coord_to_time_range = (double)m_perfetto_data.GetTotalTimeDuration() / m_width;
    uint64_t hover_time = (uint64_t)(item_x * item_coord_to_time_range);

    uint32_t slice_id = 0;
    uint64_t prev_slice_end = 0;

    for (const auto &submission : m_perfetto_data.m_submission_data)
    {
        for (const auto &slice : submission.m_data)
        {
            uint64_t slice_start = slice.m_ts;
            uint64_t slice_end = slice_start + slice.m_duration;

            // Events appear in order of start-cycle. Early out.
            if (slice_start > hover_time)
            {
                hit.m_next_slice = slice_id;
                break;
            }
            else if (slice_end < hover_time)
            {
                if (prev_slice_end < slice_end)
                {
                    hit.m_prev_slice = slice_id;
                    prev_slice_end = slice_end;
                }
            }
            else
            {
                hit.m_slice = slice_id;
            }

            ++slice_id;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::HandleNavigatorHover(double item_x, double item_y) {}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::HandleSelection(double item_x, double item_y)
{
    double navigator_row_start_y = GetNavigatorStartY(0);
    double navigator_row_end_y = GetNavigatorStartY(GetNumNavigatorRows() - 1) +
                                 GetNavigatorHeight();

    if (item_y >= navigator_row_start_y && item_y < navigator_row_end_y && !m_enable_serial_chart)
    {
        HandleEventSelection(item_x, item_y);
    }
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::HandleEventSelection(double item_x, double item_y)
{
    auto hit = FindNavigatorHit(item_x, item_y);

    if (hit.m_slice != kInvalidSlice)
    {
        m_nav_first_selected_event = m_nav_last_selected_event = hit.m_slice;

        // Set event selection info
        SetSelectionInfo(SelectionType::kEvent);

        // Trigger event selection on command heirarchy view
        emit SelectedEventChanged(hit.m_slice);
    }
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::CalcRectCoord(uint64_t  start_cycle,
                                      uint64_t  end_cycle,
                                      uint64_t *start_x,
                                      uint64_t *end_x)
{
    double time_range_to_item_coord = (double)m_width /
                                      (double)m_perfetto_data.GetTotalTimeDuration();

    int64_t visible_start = std::max((int64_t)0, m_visible_start);
    int64_t visible_end = std::max((int64_t)0, m_visible_start + m_visible_width);
    int64_t start_coord = start_cycle * time_range_to_item_coord;
    int64_t end_coord = end_cycle * time_range_to_item_coord;

    // Clamp to visible range
    if (start_coord < visible_start)
        start_coord = visible_start;
    if (end_coord > visible_end)
        end_coord = visible_end;
    *start_x = start_coord;
    *end_x = end_coord;
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::SetSelectionInfo(SelectionType selection_type)
{
    std::ostringstream ostr;

    if (selection_type == SelectionType::kDrag)  // Drag-select
    {
        PrintDragSelectionInfo(ostr);
    }
    else if (selection_type == SelectionType::kEvent)  // Event/Barrier select
    {
        PrintEventSelectionInfo(ostr);
    }

    emit SendSelectionInfo(QString::fromStdString(ostr.str()));
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::PrintDragSelectionInfo(std::ostream &ostr) const
{
    PrintPropertyPanelHead(ostr, "Region Selection");
    ostr << "<p>";
    PrintPropertyPanelInfo(ostr,
                           "Start cycle",
                           std::to_string(m_selection_region_start_cycle) + " cycles");
    PrintPropertyPanelInfo(ostr,
                           "End cycle",
                           std::to_string(m_selection_region_end_cycle) + " cycles");
    uint64_t duration = m_selection_region_end_cycle - m_selection_region_start_cycle;
    PrintPropertyPanelInfo(ostr, "Duration (cycles)", std::to_string(duration) + " cycles");
    double duration_time = duration / 1138000.0;
    if (duration_time < 0.01)
    {
        duration_time *= 1000.0;
        PrintPropertyPanelInfo(ostr,
                               "Duration (time)",
                               double_to_string(duration_time) + " us (@1138Mhz)");
    }
    else
    {
        PrintPropertyPanelInfo(ostr,
                               "Duration (time)",
                               double_to_string(duration_time) + " ms (@1138Mhz)");
    }

    double frame_percentage = duration / (double)m_perfetto_data.GetTotalTimeDuration();
    frame_percentage *= 100;
    PrintPropertyPanelInfo(ostr, "Frame percentage", double_to_string(frame_percentage) + " %");
    ostr << "</p>";
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsItem::PrintEventSelectionInfo(std::ostream &ostr) const {}
