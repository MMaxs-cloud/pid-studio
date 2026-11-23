#include "trendplot.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>

TrendPlot::TrendPlot(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(240, 140);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::OpenHandCursor);
    setAccessibleName("Time series chart");
    setToolTip("Drag to pan. Scroll to zoom. Follow live returns to recent samples.");
}

void TrendPlot::setValueLabel(const QString &label) { valueLabel_ = label; update(); }
void TrendPlot::addSeries(const QString &name, const QPen &pen)
{
    series_.append({name, pen, {}});
    update();
}
int TrendPlot::sampleCount(int series) const
{
    return series >= 0 && series < series_.size() ? int(series_[series].points.size()) : 0;
}
void TrendPlot::appendSample(double seconds, const QVector<double> &values)
{
    if (!std::isfinite(seconds) || seconds < latest_ || values.size() != series_.size()) return;
    if (std::any_of(values.cbegin(), values.cend(), [](double v) { return !std::isfinite(v); })) return;
    latest_ = seconds;
    for (int i = 0; i < series_.size(); ++i) {
        auto &points = series_[i].points;
        points.append({seconds, values[i]});
        const auto first = std::lower_bound(points.cbegin(), points.cend(), seconds - 600.0,
            [](const QPointF &point, double time) { return point.x() < time; });
        points.remove(0, int(first - points.cbegin()));
        // Also bound memory when a producer sends many samples in a short interval.
        if (points.size() > 20000) points.remove(0, int(points.size() - 20000));
    }
    fitValues();
    update();
}
void TrendPlot::clear()
{
    for (auto &series : series_) series.points.clear();
    latest_ = 0.0;
    timeLower_ = 0.0; timeUpper_ = 120.0;
    valueLower_ = -0.5; valueUpper_ = 0.5;
    update();
}
void TrendPlot::follow(double seconds)
{
    timeLower_ = std::max(0.0, seconds - 120.0);
    timeUpper_ = std::max(120.0, seconds);
    fitValues(); update();
}
QRectF TrendPlot::chartRect() const
{
    return QRectF(64, 28, std::max(1, width() - 82), std::max(1, height() - 80));
}
void TrendPlot::fitValues()
{
    double low = std::numeric_limits<double>::infinity();
    double high = -low;
    for (const auto &series : series_)
        for (const auto &point : series.points)
            if (point.x() >= timeLower_ && point.x() <= timeUpper_) {
                low = std::min(low, point.y()); high = std::max(high, point.y());
            }
    if (!std::isfinite(low)) return;
    const double half = std::max(0.1, high - low) * 0.65;
    const double center = (low + high) * 0.5;
    valueLower_ = center - half; valueUpper_ = center + half;
}
void TrendPlot::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::white);
    auto font = painter.font(); font.setPointSize(9); painter.setFont(font);
    const auto chart = chartRect();
    painter.setPen(QColor("#505860"));
    painter.drawText(QRectF(8, 2, width() - 16, 22), Qt::AlignLeft | Qt::AlignVCenter, valueLabel_);
    const auto x = [&](double v) { return chart.left() + (v - timeLower_) / (timeUpper_ - timeLower_) * chart.width(); };
    const auto y = [&](double v) { return chart.bottom() - (v - valueLower_) / valueSpan() * chart.height(); };
    for (int i = 0; i <= 4; ++i) {
        const double fraction = i / 4.0;
        const double px = chart.left() + fraction * chart.width();
        const double py = chart.bottom() - fraction * chart.height();
        painter.setPen(QPen(QColor("#d9dde0"), 1, Qt::DotLine));
        painter.drawLine(QPointF(px, chart.top()), QPointF(px, chart.bottom()));
        painter.drawLine(QPointF(chart.left(), py), QPointF(chart.right(), py));
        painter.setPen(QColor("#505860"));
        painter.drawText(QRectF(0, py - 9, chart.left() - 8, 18), Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(valueLower_ + fraction * valueSpan(), 'g', 4));
        const int time = qRound(timeLower_ + fraction * (timeUpper_ - timeLower_));
        const auto text = QString("%1:%2").arg(time / 60, 2, 10, QLatin1Char('0')).arg(time % 60, 2, 10, QLatin1Char('0'));
        painter.drawText(QRectF(px - 30, chart.bottom() + 5, 60, 18), Qt::AlignCenter, text);
    }
    painter.setPen(QColor("#8b9196"));
    painter.drawLine(chart.bottomLeft(), chart.bottomRight());
    painter.drawLine(chart.topLeft(), chart.bottomLeft());
    painter.drawText(QRectF(chart.left(), height() - 22, chart.width(), 18), Qt::AlignCenter, "Time from start (min:sec)");
    painter.save(); painter.setClipRect(chart.adjusted(0, -1, 1, 0));
    for (const auto &series : series_) {
        QPainterPath path;
        bool first = true;
        for (const auto &point : series.points) {
            const QPointF pixel(x(point.x()), y(point.y()));
            if (first) { path.moveTo(pixel); first = false; } else path.lineTo(pixel);
        }
        painter.setPen(series.pen); painter.drawPath(path);
    }
    painter.restore();
    // Put the compact legend inside the chart, leaving the axes unobstructed.
    const auto metrics = painter.fontMetrics();
    int legendWidth = 0;
    for (const auto &series : series_) legendWidth = std::max(legendWidth, metrics.horizontalAdvance(series.name) + 48);
    const QRectF legend(chart.right() - legendWidth - 6, chart.top() + 5, legendWidth, series_.size() * 19 + 8);
    painter.fillRect(legend, QColor(255, 255, 255, 235));
    int row = 0;
    for (const auto &series : series_) {
        const double py = legend.top() + 13 + row++ * 19;
        painter.setPen(series.pen); painter.drawLine(QPointF(legend.left() + 5, py), QPointF(legend.left() + 28, py));
        painter.setPen(QColor("#30363b"));
        painter.drawText(QRectF(legend.left() + 34, py - 9, legend.width() - 34, 18), Qt::AlignLeft | Qt::AlignVCenter, series.name);
    }
}
void TrendPlot::constrainTimeRange()
{
    const double span = std::clamp(timeUpper_ - timeLower_, 1.0, 600.0);
    const double earliest = std::max(0.0, latest_ - 600.0);
    timeLower_ = std::clamp(timeLower_, earliest, std::max(earliest, latest_));
    timeUpper_ = timeLower_ + span;
    fitValues(); update();
}
void TrendPlot::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !chartRect().contains(event->position())) return;
    dragging_ = true; dragPosition_ = event->position(); setCursor(Qt::ClosedHandCursor);
    emit navigationStarted();
}
void TrendPlot::mouseMoveEvent(QMouseEvent *event)
{
    if (!dragging_) return;
    const double shift = (dragPosition_.x() - event->position().x()) / chartRect().width() * (timeUpper_ - timeLower_);
    timeLower_ += shift; timeUpper_ += shift; dragPosition_ = event->position();
    constrainTimeRange();
}
void TrendPlot::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) { dragging_ = false; setCursor(Qt::OpenHandCursor); }
}
void TrendPlot::wheelEvent(QWheelEvent *event)
{
    if (!chartRect().contains(event->position())) { event->ignore(); return; }
    emit navigationStarted();
    const double fraction = std::clamp((event->position().x() - chartRect().left()) / chartRect().width(), 0.0, 1.0);
    const double anchor = timeLower_ + fraction * (timeUpper_ - timeLower_);
    const double span = std::clamp((timeUpper_ - timeLower_) * std::pow(0.8, event->angleDelta().y() / 120.0), 1.0, 600.0);
    timeLower_ = anchor - fraction * span; timeUpper_ = timeLower_ + span;
    constrainTimeRange(); event->accept();
}
