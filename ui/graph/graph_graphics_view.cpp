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
#include "graph_graphics_view.h"
#include <QApplication>
#include <QEasingCurve>
#include <QMouseEvent>
#include <QScroller>
#include <QWheelEvent>

//--------------------------------------------------------------------------------------------------
GraphGraphicsView::GraphGraphicsView(QWidget *parent) :
    QGraphicsView(parent)
{
    m_scroller_ptr = QScroller::scroller(this);

    QScrollerProperties prop = m_scroller_ptr->scrollerProperties();
    prop.setScrollMetric(QScrollerProperties::ScrollingCurve, QEasingCurve(QEasingCurve::OutExpo));
    prop.setScrollMetric(QScrollerProperties::DecelerationFactor, 1.0);
    prop.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
                         QScrollerProperties::OvershootAlwaysOff);
    prop.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
                         QScrollerProperties::OvershootAlwaysOff);
    m_scroller_ptr->setScrollerProperties(prop);
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control)
    {
        QApplication::setOverrideCursor(Qt::DragMoveCursor);
        m_scroller_ptr->grabGesture(this, QScroller::LeftMouseButtonGesture);
    }
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control)
    {
        QApplication::restoreOverrideCursor();
        m_scroller_ptr->ungrabGesture(this);
    }
}

//--------------------------------------------------------------------------------------------------
void GraphGraphicsView::wheelEvent(QWheelEvent *event)
{
    // It's possible to get a x-axis-only wheel event on trackpads!
    // Only deal with y-axis ones (ie. mouse wheels)
    if (event->angleDelta().y() == 0)
        return;
    emit OnMouseWheel(event->pos(), event->angleDelta().y());
}