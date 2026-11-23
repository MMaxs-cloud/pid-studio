#pragma once

#include <QPen>
#include <QPointF>
#include <QVector>
#include <QWidget>

// A small Qt painter widget for bounded, streaming time series.
class TrendPlot final : public QWidget {
    Q_OBJECT
public:
    explicit TrendPlot(QWidget *parent = nullptr);
    void setValueLabel(const QString &label);
    void addSeries(const QString &name, const QPen &pen);
    void appendSample(double seconds, const QVector<double> &values);
    void clear();
    void follow(double seconds);
    double valueSpan() const { return valueUpper_ - valueLower_; }
    int sampleCount(int series) const;
    QPair<double, double> timeRange() const { return {timeLower_, timeUpper_}; }
signals:
    void navigationStarted();
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
private:
    struct Series { QString name; QPen pen; QVector<QPointF> points; };
    QRectF chartRect() const;
    void fitValues();
    void constrainTimeRange();
    QVector<Series> series_;
    QString valueLabel_;
    double timeLower_ = 0.0, timeUpper_ = 120.0;
    double valueLower_ = -0.5, valueUpper_ = 0.5;
    double latest_ = 0.0;
    QPointF dragPosition_;
    bool dragging_ = false;
};
