#pragma once
#include "controlengine.h"
#include <QFile>
#include <QString>

class SessionLogger {
public:
    bool open(const QString &directory, QString &error);
    bool sample(double elapsed, const QString &tag, double target, const pid::ControlSample &sample,
                double sensorE, double sensorR, double baselineR, bool running);
    bool event(const QString &type, const QString &details);
    QString directory() const { return directory_; }
private:
    QFile samples_, events_;
    QString directory_;
};
