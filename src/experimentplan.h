#pragma once
#include "controlengine.h"
#include <QString>
#include <QVector>

struct CyclePoint { qint64 timeMs; double value; };

struct ExperimentPlan {
    QString tag, sensorE, sensorR;
    double kp = 80.0, ki = 2.0;
    pid::FeedForward e, r;
    QVector<CyclePoint> eCycle, rCycle;
    qint64 durationMs() const;
    double ePowerAt(qint64 timeMs) const;
    double rPowerAt(qint64 timeMs) const;
    // Transactional: destination remains unchanged on failure.
    static bool load(const QString &file, const QString &tag, ExperimentPlan &destination,
                     QString &error, const QString &cycleDirectory = {});
};
