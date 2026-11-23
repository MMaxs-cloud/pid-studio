#include "csvmonitor.h"
#include "csv.h"
#include <QFile>

CsvMonitor::CsvMonitor(QObject *parent) : QObject(parent)
{
    timer_.setInterval(500);
    connect(&timer_, &QTimer::timeout, this, &CsvMonitor::read);
}
void CsvMonitor::start(const QString &path, const QString &e, const QString &r)
{
    stop(); path_ = path; sensorE_ = e; sensorR_ = r;
    lastError_.clear(); lastRow_.clear(); timer_.start(); read();
}
void CsvMonitor::stop() { timer_.stop(); }
void CsvMonitor::read()
{
    const auto fail = [this](const QString &message) {
        if (message != lastError_) { lastError_ = message; emit errorOccurred(message); }
    };
    QFile file(path_);
    if (!file.open(QIODevice::ReadOnly)) { fail("Cannot read sensor CSV: " + path_); return; }
    const auto header = csv::split(QString::fromUtf8(file.readLine(65536)));
    const auto find = [&header](const QString &name) {
        for (int i = 0; i < header.size(); ++i)
            if (header[i].compare(name, Qt::CaseInsensitive) == 0) return i;
        return -1;
    };
    const int e = sensorE_.isEmpty() ? -1 : find(sensorE_);
    const int r = sensorR_.isEmpty() ? -1 : find(sensorR_);
    if ((!sensorE_.isEmpty() && e < 0) || (!sensorR_.isEmpty() && r < 0)) {
        fail("Sensor name not found in CSV header. Check the experiment plan."); return;
    }
    // Bounded tail read, including files replaced or truncated by the producer.
    const auto bodyStart = file.pos();
    const bool tail = file.size() - bodyStart > 65536;
    if (tail) file.seek(file.size() - 65536);
    const auto bytes = file.read(65536);
    const auto lines = bytes.split('\n');
    QByteArray row;
    for (int i = tail ? 1 : 0; i < lines.size() - 1; ++i)
        if (!lines[i].trimmed().isEmpty()) row = lines[i];
    if (row.isEmpty()) return; // Never parse a record still being appended.
    const auto fields = csv::split(QString::fromUtf8(row));
    bool okE = true, okR = true;
    const double vE = e < 0 ? 0.0 : csv::number(fields.value(e), &okE);
    const double vR = r < 0 ? 0.0 : csv::number(fields.value(r), &okR);
    if (!okE || !okR) { fail("Invalid sensor CSV record; waiting for a complete numeric row."); return; }
    if (row == lastRow_) return; // An unchanged old file is not a fresh sensor reading.
    lastError_.clear(); lastRow_ = row; emit newData(vE, vR);
}
