#include "sessionlogger.h"
#include "csv.h"
#include <QDateTime>
#include <QDir>
#include <QTextStream>

bool SessionLogger::open(const QString &root, QString &error)
{
    directory_ = QDir(root).filePath(QDateTime::currentDateTime().toString("yyyy-MM-dd/HHmmss_zzz"));
    if (!QDir().mkpath(directory_)) { error = "Cannot create log directory: " + directory_; return false; }
    samples_.setFileName(QDir(directory_).filePath("measurements.csv"));
    events_.setFileName(QDir(directory_).filePath("events.csv"));
    if (!samples_.open(QIODevice::WriteOnly | QIODevice::Text)
        || !events_.open(QIODevice::WriteOnly | QIODevice::Text)) {
        error = "Cannot create session logs: " + directory_; return false;
    }
    samples_.write("timestamp;elapsed_s;tag;target_c;temperature_c;error_c;p_w;i_w;ff_e_w;ff_r_w;power_w;code;sensor_e_c;sensor_r_c;baseline_r_c;running\n");
    events_.write("timestamp;event;details\n");
    return samples_.flush() && events_.flush();
}
bool SessionLogger::sample(double elapsed, const QString &tag, double target, const pid::ControlSample &s,
                           double e, double r, double baseline, bool running)
{
    if (!samples_.isOpen()) return false;
    QTextStream out(&samples_);
    out.setRealNumberPrecision(10);
    out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << ';' << elapsed << ';'
        << csv::escape(tag) << ';' << target << ';' << s.temperature << ';' << s.error << ';'
        << s.proportional << ';' << s.integral << ';' << s.feedForwardE << ';' << s.feedForwardR << ';'
        << s.power << ';' << s.code << ';' << e << ';' << r << ';' << baseline << ';'
        << int(running) << '\n';
    out.flush();
    return out.status() == QTextStream::Ok && samples_.flush();
}
bool SessionLogger::event(const QString &type, const QString &details)
{
    if (!events_.isOpen()) return false;
    QTextStream out(&events_);
    out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << ';'
        << csv::escape(type) << ';' << csv::escape(details) << '\n';
    out.flush();
    return out.status() == QTextStream::Ok && events_.flush();
}
