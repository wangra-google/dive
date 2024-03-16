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

// =====================================================================================================================
// Handles the rendering of all wavefronts
// =====================================================================================================================

#pragma once
#include <QGraphicsItem>
#include <iomanip>
#include <set>
#include <sstream>
#include "gui_constants.h"
#include "perfetto_trace/gpu_slice_data.h"

// Forward declarations
class HoverHelp;

class GraphGraphicsItem : public QGraphicsObject
{
    Q_OBJECT

public:
    GraphGraphicsItem(const Dive::PerfettoData &perfetto_data);

    void ResetSelectionRegion();

    // Set width of item
    void SetWidth(uint64_t width);

    enum class SelectionType
    {
        kDrag,   // Drag-selection
        kEvent,  // Event-selection
        kNone
    };

    // Set total event duration
    void SetTotalCycles(uint64_t total_cycles);

    // Getters
    uint64_t GetTotalCycles();
    uint64_t GetSelectionStartCycle() const;
    uint64_t GetSelectionEndCycle() const;

    // Whether to serialize bar chart (kEventInfo-mode only)
    void EnableSerialChart(bool enable);

    // Whether serialize bar chart is enabled (kEventInfo-mode only)
    bool IsSerialChartEnabled();

    // Set the leftmost QGraphicsScene coordinate of item that is visible
    // This controls what is rendered in the viewport
    void SetVisibleRange(int64_t scene_x, int64_t width);

    // Select Navigator stream
    void SelectNavigatorStream(uint32_t stream_id);

    // Set Selection Info
    void SetSelectionInfo(SelectionType selection_type);

    // Wavefront rect spacing and location
    // These are used in inner-loop, so prefer to be inline
    uint32_t GetRectHeight() const { return 20; }
    uint32_t GetSummaryRowHeight() const { return 40; }
    uint32_t GetRectPadding() const { return 3; }
    double   GetRowY(uint32_t row) const;  // In local item coordinates

    // Event BarChart height
    uint32_t GetBarChartHeight() const { return 350; }

    // Occupancy % height
    uint32_t GetOccupancyPcHeight() const { return 210; }

    // Navigator spacing and location
    uint32_t GetNavigatorStartY(uint32_t row) const;
    uint32_t GetNavigatorLabelStartY(uint32_t row) const;
    uint32_t GetNavigatorEventStartY(uint32_t row) const;
    uint32_t GetNavigatorBarrierStartY(uint32_t row) const;
    uint32_t GetNavigatorHeight() const;
    uint32_t GetNavigatorLabelHeight() const { return m_show_labels ? GetRectHeight() * 1.5 : 0; }
    uint32_t GetNavigatorEventHeight() const { return GetRectHeight() * 1.2; }
    uint32_t GetNavigatorBarrierHeight() const { return GetRectHeight() * 1.2; }
    uint32_t GetNumNavigatorRows() const { return m_num_navigator_rows; }

    // Shader stage colors
    QColor GetShaderStageColor(Dive::ShaderStage shader_stage) const
    {
        return Dive::kShaderStageColor[(uint32_t)shader_stage];
    }

    QColor GetShaderStageBorderColor(Dive::ShaderStage shader_stage) const
    {
        return Dive::kShaderStageBorderColor[(uint32_t)shader_stage];
    }

    // QGraphicsItem overrides
    virtual QRectF boundingRect() const Q_DECL_OVERRIDE;
    virtual void   paint(QPainter                       *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget                        *widget) Q_DECL_OVERRIDE;

protected:
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event) Q_DECL_OVERRIDE;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) Q_DECL_OVERRIDE;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;

private:
    void DrawHGridLines(QPainter *painter);
    void DrawNavigator(QPainter *painter);
    void DrawSelectionRegion(QPainter *painter);
    void DrawNavigatorHighlighted(QPainter *painter);
    void DrawNavigatorHighlightedEvents(QPainter *painter, bool draw_shader_stages);

    void DrawEventRect(QPainter   *painter,
                       QColor      pen_color,
                       QColor      brush_color,
                       std::string slice_name,
                       double      x,
                       double      y,
                       double      width,
                       double      height,
                       bool        darken,
                       bool        draw_shader_stages);

    void DrawLabelRect(QPainter *painter,
                       QColor    pen_color,
                       QColor    brush_color,
                       double    x,
                       double    y,
                       double    width,
                       double    height);

    void HandleNavigatorHover(double item_x, double item_y);
    void HandleSelection(double item_x, double item_y);
    void HandleEventSelection(double item_x, double item_y);
    void CalcRectCoord(uint64_t  start_cycle,
                       uint64_t  end_cycle,
                       uint64_t *start_x,
                       uint64_t *end_x);
    bool IsAnyEventHighlighted() const;
    void PrintDragSelectionInfo(std::ostream &ostr) const;
    void PrintEventSelectionInfo(std::ostream &ostr) const;

    QColor GetSliceColor(const std::string &slice_name) const;

    inline void PrintPropertyPanelHead(std::ostream &ostr, std::string header) const
    {
        ostr << "<h3 style=\"background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 "
                "#e7effd, stop: 1 #cbdaf1);color:#000;\">";
        ostr << header;
        ostr << "</h3>";
    };

    inline void PrintPropertyPanelInfo(std::ostream &ostr,
                                       std::string   param,
                                       std::string   value) const
    {
        ostr << "<tr>";
        ostr << "<td width=\"120\">";
        ostr << "<b>" << param << "</b>:";
        ostr << "</td>";
        ostr << "<td style=\"padding-left:5px;\">" << value << "</td></tr>";
    };

    inline std::string double_to_string(double num) const
    {
        std::ostringstream stm;
        stm << std::fixed << std::setprecision(2) << num;
        return stm.str();
    };

    const uint32_t kInvalidSlice = UINT32_MAX;
    struct NavigatorHit
    {
        uint32_t m_slice;
        uint32_t m_prev_slice;  // last slice ending before hover cycle (on any stream)
        uint32_t m_next_slice;  // first slice starting after hover cycle (on any stream)
    };
    NavigatorHit FindNavigatorHit(double x, double y) const;

    const uint32_t kNavStartPadding = 8;
    const uint32_t kSelectedRectExtraPadding = 5;
    const uint32_t kNavBarrierRectHeight = 12;

    int64_t  m_visible_start;
    int64_t  m_visible_width;
    uint64_t m_width;

    uint32_t m_nav_first_selected_event = 0;
    uint32_t m_nav_last_selected_event = 0;

    bool m_enable_serial_chart = false;
    bool m_show_barriers = false;
    bool m_show_labels = false;

    std::vector<bool> m_select_nav_stream;
    bool              m_hover_msg_sent = false;
    HoverHelp        *m_hover_help;

    // Selection region paramters
    struct IndexRange
    {
        uint32_t min;
        uint32_t max;
    };

    bool                    m_is_selected;
    QPointF                 m_selection_start_pos;
    uint64_t                m_selection_region_prev_cycle;
    uint64_t                m_selection_region_start_cycle;
    uint64_t                m_selection_region_end_cycle;
    bool                    m_ignore_mouse_move;
    std::vector<IndexRange> m_wavefront_row_index_range;

    // Event info view parameters
    uint64_t              m_total_cycles;
    uint64_t              m_bar_chart_min_value;
    uint64_t              m_bar_chart_max_value;
    std::vector<uint64_t> m_bar_chart_values;
    std::vector<double>   m_bar_chart_barheight;

    uint32_t m_num_navigator_rows = 1;
    uint32_t m_navigator_stream_mask = 0x1;

    const Dive::PerfettoData &m_perfetto_data;

signals:
    void SendSelectionInfo(const QString &);
    void SelectedEventChanged(uint32_t sqtt_event);
};

//--------------------------------------------------------------------------------------------------
inline double GraphGraphicsItem::GetRowY(uint32_t row) const
{
    uint32_t top_margin = GetRectPadding() + GetSummaryRowHeight();
    return top_margin + (row * (GetRectHeight() + GetRectPadding()));
}
