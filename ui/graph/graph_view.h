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

#pragma once
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGraphicsTextItem>
#include <QPushButton>
#include "dive_core/data_core.h"
#include "perfetto_trace/gpu_slice_data.h"

// Forward declaration
class GraphGraphicsView;
class GraphGraphicsScene;
class RulerGraphicsItem;
class GraphGraphicsItem;
class QFrame;
class QGraphicsScene;
class LabelGraphicsItem;
class HoverHelp;
class TrackablePushButton;
class TrackableCheckBox;
class TrackableComboBox;
class QGraphicsView;

//--------------------------------------------------------------------------------------------------
class GraphView : public QFrame
{
    Q_OBJECT

public:
    GraphView(const Dive::PerfettoData &perfetto_data);
    void OnLoadFile();
    void Reset(bool preserve_viewport = false);

public slots:
    void OnZoomFullClick();

private slots:
    void Update();
    void OnMouseWheel(QPoint mouse_pos, int angle_delta);
    void OnSelectionChange(const QString &);
    void OnZoomToFitClick();

protected:
    virtual void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;

private:
    const int kMargin = 20;

    QGraphicsScene                  *m_scene_ptr;
    GraphGraphicsView               *m_view_ptr;
    QGraphicsView                   *m_slot_view_ptr;
    RulerGraphicsItem               *m_ruler_item_ptr;
    GraphGraphicsItem               *m_item_ptr;
    TrackablePushButton             *m_zoom_to_fit_button_ptr;
    TrackablePushButton             *m_zoom_full_button_ptr;
    std::vector<LabelGraphicsItem *> m_navigator_text_label_ptrs;
    LabelGraphicsItem               *m_summary_text_label_ptr;
    HoverHelp                       *m_hover_help;

    const Dive::PerfettoData &m_perfetto_data;

signals:
    void SendSelectionInfo(const QString &);
};

//--------------------------------------------------------------------------------------------------
class TrackablePushButton : public QPushButton
{
    Q_OBJECT

public:
    TrackablePushButton(const QString &text, QWidget *parent = nullptr);

protected:
    bool event(QEvent *e);

signals:
    void HoverEnter();
    void HoverLeave();
};
