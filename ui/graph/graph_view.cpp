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
#include "graph/graph_view.h"
#include "dive_core/capture_data.h"
#include "graph/graph_graphics_item.h"
#include "graph/graph_graphics_view.h"
#include "graph/ruler_graphics_item.h"
#include "hover_help_model.h"

#include <math.h>

#include <QApplication>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSplitter>
#include <QStandardItemModel>
#include <QVBoxLayout>

static const uint32_t kLabelViewMinimumWidth = 170;

// =================================================================================================
// GraphView
// =================================================================================================
GraphView::GraphView(const Dive::PerfettoData &perfetto_data) :
    m_perfetto_data(perfetto_data)
{
    QVBoxLayout *main_layout_ptr = new QVBoxLayout();

    QWidget     *first_panel_wgt = new QWidget();
    QVBoxLayout *first_panel_layout = new QVBoxLayout();

    // Create and add options
    QHBoxLayout *options_layout_ptr = new QHBoxLayout();
    m_zoom_to_fit_button_ptr = new TrackablePushButton("Zoom to fit", this);
    m_zoom_to_fit_button_ptr->setEnabled(false);
    m_zoom_full_button_ptr = new TrackablePushButton("Zoom full", this);

    // Add a visual vertical separator line
    QFrame *line_ptr = new QFrame();
    line_ptr->setFrameShape(QFrame::VLine);
    line_ptr->setFrameShadow(QFrame::Sunken);

    options_layout_ptr->addWidget(line_ptr);
    options_layout_ptr->addStretch();
    options_layout_ptr->addWidget(m_zoom_to_fit_button_ptr);
    options_layout_ptr->addWidget(m_zoom_full_button_ptr);
    first_panel_layout->addLayout(options_layout_ptr);

    QHBoxLayout *graphics_view_layout_ptr = new QHBoxLayout();
    {
        // Creation of wavefront scene
        {
            m_scene_ptr = new QGraphicsScene();
            m_view_ptr = new GraphGraphicsView();
            m_ruler_item_ptr = new RulerGraphicsItem();

            m_ruler_item_ptr->SetRange(m_perfetto_data.GetTotalTimeDuration());

            m_item_ptr = new GraphGraphicsItem(m_perfetto_data);

            // Set position of ruler, then the wavefront view right below that
            int offset_y = m_ruler_item_ptr->boundingRect().height();
            m_ruler_item_ptr->setPos(0, 0);
            m_item_ptr->setPos(0, offset_y);

            // Align it so top-left of scene is at coordinate(0,0)
            m_view_ptr->setAlignment(Qt::AlignTop | Qt::AlignLeft);
            m_view_ptr->setScene(m_scene_ptr);
            m_scene_ptr->addItem(m_ruler_item_ptr);
            m_scene_ptr->addItem(m_item_ptr);
        }

        {
            QGraphicsScene *slot_scene_ptr = new QGraphicsScene();
            m_slot_view_ptr = new QGraphicsView();
            m_slot_view_ptr->setScene(slot_scene_ptr);
            m_slot_view_ptr->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

            // Bottom of ruler
            // int offset_y = m_ruler_item_ptr->boundingRect().height();

            // Navigator labels

            QGraphicsTextItem *text_item_ptr = new QGraphicsTextItem();
            text_item_ptr->setPlainText("GPU Timing");
            slot_scene_ptr->addItem(text_item_ptr);

            // Align it so top-left of scene is at coordinate(0,0)
            m_slot_view_ptr->setAlignment(Qt::AlignTop | Qt::AlignLeft);
            m_slot_view_ptr->setMinimumWidth(kLabelViewMinimumWidth);
            m_slot_view_ptr->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

            // Since slot item start at an offset, need to arbitrarily make sure (0,0) is part of
            // scene rect
            QRectF scene_rect = m_slot_view_ptr->sceneRect();
            scene_rect.setY(0);
            m_slot_view_ptr->setSceneRect(scene_rect);
            m_slot_view_ptr->centerOn(scene_rect.center());

            graphics_view_layout_ptr->addWidget(m_slot_view_ptr);
            graphics_view_layout_ptr->addWidget(m_view_ptr);
        }
    }
    first_panel_layout->addLayout(graphics_view_layout_ptr);
    first_panel_wgt->setLayout(first_panel_layout);

    QSplitter *spiltter = new QSplitter();
    spiltter->setOrientation(Qt::Vertical);
    spiltter->addWidget(first_panel_wgt);
    spiltter->setCollapsible(0, false);
    main_layout_ptr->addWidget(spiltter);
    setLayout(main_layout_ptr);

    // Initialize hover help
    m_hover_help = HoverHelp::Get();

    Reset();

    // Qt bug? This seems only affect Linux.
    // Set visibility of widget offscreen will not update nested layout calculation.
    // For example, m_back_button->setVisible(false) only invalidate first_panel_layout.
    options_layout_ptr->invalidate();

    QObject::connect(m_zoom_to_fit_button_ptr, SIGNAL(clicked()), this, SLOT(OnZoomToFitClick()));
    QObject::connect(m_zoom_full_button_ptr, SIGNAL(clicked()), this, SLOT(OnZoomFullClick()));
    QObject::connect(m_view_ptr->horizontalScrollBar(),
                     SIGNAL(valueChanged(int)),
                     this,
                     SLOT(Update()));
    QObject::connect(m_view_ptr,
                     SIGNAL(OnMouseWheel(QPoint, int)),
                     this,
                     SLOT(OnMouseWheel(QPoint, int)));

    // Hover help signal connections
    QObject::connect(m_item_ptr,
                     SIGNAL(SendSelectionInfo(const QString &)),
                     this,
                     SLOT(OnSelectionChange(const QString &)));

    QScrollBar *scrollbar = new QScrollBar(Qt::Vertical);
    m_slot_view_ptr->setVerticalScrollBar(scrollbar);
    m_view_ptr->setVerticalScrollBar(scrollbar);
}

//--------------------------------------------------------------------------------------------------
void GraphView::OnLoadFile()
{
    m_item_ptr->SetTotalCycles(m_perfetto_data.GetTotalTimeDuration());

    Reset(true);
}

//--------------------------------------------------------------------------------------------------
void GraphView::Reset(bool preserve_viewport)
{
    m_item_ptr->ResetSelectionRegion();
    uint64_t visible_width = m_view_ptr->contentsRect().width();
    // TODO(wangra): fix me!!!
    double   prev_scene_center = m_view_ptr->mapToScene(QPoint(visible_width / 2, 0)).x();
    int64_t  prev_cycle_center = m_ruler_item_ptr->MapToTimeRange(prev_scene_center);
    uint64_t prev_width = m_ruler_item_ptr->GetWidth();

    m_ruler_item_ptr->SetRange(m_perfetto_data.GetTotalTimeDuration());
    m_ruler_item_ptr->SetWidth(visible_width);
    m_item_ptr->SetWidth(visible_width);

    if (preserve_viewport)
    {
        m_ruler_item_ptr->SetWidth(prev_width);
        m_item_ptr->SetWidth(prev_width);

        // Center the view on the previous cycle center
        // Note: Have to update width before calling MapToScene()
        double new_scene_center = m_ruler_item_ptr->MapToScene(prev_cycle_center);
        m_view_ptr->centerOn(new_scene_center, 0);
    }

    Update();
}

//--------------------------------------------------------------------------------------------------
void GraphView::Update()
{
    int64_t visible_width = m_view_ptr->contentsRect().width();

    // Scrolling the window or zooming will affect the range that is visible
    int32_t scene_left = m_view_ptr->mapToScene(QPoint(0, 0)).x();
    m_ruler_item_ptr->SetVisibleRange(scene_left, visible_width);
    m_item_ptr->SetVisibleRange(scene_left, visible_width);

    // Update viewport (or else paint() of new region might not take into effect)
    m_view_ptr->viewport()->update();
}

//--------------------------------------------------------------------------------------------------
void GraphView::OnMouseWheel(QPoint mouse_pos, int angle_delta)
{
    DIVE_ASSERT(angle_delta != 0);

    // Note: angle_delta is relative amount the wheel was rotated, in eighths of a degree. Most
    // mouse wheels work in steps of 15 degrees, but there are some finer-resolution wheels that
    // work in less than 120 units (ie. less than 15 degrees). Trackpads can also work in less than
    // 120 units

    // Determine what cycle is pointed to by the mouse
    double  prev_scene_mouse_pt = m_view_ptr->mapToScene(QPoint(mouse_pos.x(), 0)).x();
    int64_t prev_cycle_mouse_pt = m_ruler_item_ptr->MapToTimeRange(prev_scene_mouse_pt);

    // Determine how off-center, in scene-coordinates, mouse position is
    uint64_t visible_width = m_view_ptr->contentsRect().width();
    double   prev_scene_center = m_view_ptr->mapToScene(QPoint(visible_width / 2, 0)).x();
    double   scene_distance = prev_scene_mouse_pt - prev_scene_center;

    // Allow the user to zoom by 0.1x to 2x depending on how quickly they turn the wheel.
    int wheel_steps = std::clamp(angle_delta / 10, -8, 8);  // 1.1 ^ 8 ~= 2.14
    if (wheel_steps == 0)
        wheel_steps = (angle_delta < 0) ? -1 : 1;
    double old_width = m_ruler_item_ptr->GetWidth();
    double new_width = pow(1.1, wheel_steps) * old_width;

    // Do not allow the user to zoom out past the point where everything is visible.
    new_width = std::max<double>(visible_width, new_width);

    // Maximum width is bounded by INT32_MAX (assuming position of (0,0)), since the Qt code
    // uses code like this:
    //      viewBoundingRect.adjust(-int(rectAdjust), -int(rectAdjust), rectAdjust, rectAdjust);
    if ((m_ruler_item_ptr->pos().x() + new_width) > INT32_MAX)
        return;

    // If number of cycles visible would get below a certain threshold, then do not allow a zoom-in
    const uint64_t kMinCyclesVisible = 50;
    if (angle_delta > 0)
    {
        if (m_ruler_item_ptr->GetTimeRangeVisible(visible_width, new_width) < kMinCyclesVisible)
            return;
    }

    m_ruler_item_ptr->SetWidth(new_width);
    m_item_ptr->SetWidth(new_width);

    // Update the scene with the new ruler item size
    QRectF wavefront_bounding_rect = m_scene_ptr->itemsBoundingRect();
    m_scene_ptr->setSceneRect(wavefront_bounding_rect);

    // Adjust scene center so that the mouse cursor is at the same cycle as before.
    double new_scene_mouse_pt = m_ruler_item_ptr->MapToScene(prev_cycle_mouse_pt);
    m_view_ptr->centerOn(new_scene_mouse_pt - scene_distance, 0);

    // Update visible range in the graphics items
    // Note: Sometimes centerOn() will implicitly call Update() if the new center point causes
    // a horizontal bar update, but this is not guaranteed
    Update();
}

//--------------------------------------------------------------------------------------------------
void GraphView::OnSelectionChange(const QString &str)
{
    m_zoom_to_fit_button_ptr->setEnabled(!str.isEmpty());
    emit SendSelectionInfo(str);
}

//--------------------------------------------------------------------------------------------------
void GraphView::OnZoomToFitClick()
{
    uint64_t start_cycle = m_item_ptr->GetSelectionStartCycle();
    uint64_t end_cycle = m_item_ptr->GetSelectionEndCycle();
    uint64_t start_scene = m_ruler_item_ptr->MapToScene(start_cycle);
    uint64_t end_scene = m_ruler_item_ptr->MapToScene(end_cycle);
    double   old_width = m_ruler_item_ptr->GetWidth();
    double   scale_factor = old_width / (end_scene - start_scene);
    uint64_t visible_width = m_view_ptr->contentsRect().width();
    double   new_width = visible_width * scale_factor;
    new_width = std::max<double>(visible_width, new_width);

    // Maximum width is bounded by INT32_MAX (assuming position of (0,0)), since the Qt code
    // uses code like this:
    //      viewBoundingRect.adjust(-int(rectAdjust), -int(rectAdjust), rectAdjust, rectAdjust);
    if ((m_ruler_item_ptr->pos().x() + new_width) > INT32_MAX)
    {
        // We can't zoom, but we can adjust the scene center.
        m_view_ptr->centerOn(m_ruler_item_ptr->MapToScene((start_cycle + end_cycle) / 2), 0);
        return;
    }

    // If number of cycles visible would get below a certain threshold, then do not allow a zoom-in
    const uint64_t kMinCyclesVisible = 50;
    if (m_ruler_item_ptr->GetTimeRangeVisible(visible_width, new_width) < kMinCyclesVisible)
    {
        // We can't zoom, but we can adjust the scene center.
        m_view_ptr->centerOn(m_ruler_item_ptr->MapToScene((start_cycle + end_cycle) / 2), 0);
        return;
    }

    m_ruler_item_ptr->SetWidth(new_width);
    m_item_ptr->SetWidth(new_width);

    // Update the scene with the new ruler item size
    QRectF wavefront_bounding_rect = m_scene_ptr->itemsBoundingRect();
    m_scene_ptr->setSceneRect(wavefront_bounding_rect);

    // Adjust scene center.
    m_view_ptr->centerOn(m_ruler_item_ptr->MapToScene((start_cycle + end_cycle) / 2), 0);

    Update();
}

//--------------------------------------------------------------------------------------------------
void GraphView::OnZoomFullClick()
{
    double new_width = m_view_ptr->contentsRect().width();
    m_ruler_item_ptr->SetWidth(new_width);
    m_item_ptr->SetWidth(new_width);

    QRectF wavefront_bounding_rect = m_scene_ptr->itemsBoundingRect();
    m_scene_ptr->setSceneRect(wavefront_bounding_rect);

    Update();
}

//--------------------------------------------------------------------------------------------------
void GraphView::resizeEvent(QResizeEvent *event)
{
    Update();
}

//--------------------------------------------------------------------------------------------------
TrackablePushButton::TrackablePushButton(const QString &text, QWidget *parent) :
    QPushButton(text, parent)
{
}

//--------------------------------------------------------------------------------------------------
bool TrackablePushButton::event(QEvent *event)
{
    if (isEnabled())
    {
        switch (event->type())
        {
        case QEvent::Enter: emit HoverEnter(); break;
        case QEvent::Leave: emit HoverLeave(); break;
        default: break;
        }
    }

    return QWidget::event(event);
}
