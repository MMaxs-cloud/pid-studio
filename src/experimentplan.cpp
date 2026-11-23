#include "experimentplan.h"
#include "csv.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringDecoder>
#include <algorithm>

namespace {
QString readText(QFile &file)
{
    const auto bytes = file.readAll();
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder(bytes);
    return decoder.hasError() ? QString::fromLocal8Bit(bytes) : text;
}

double valueAt(const QVector<CyclePoint> &points, qint64 time, bool interpolate)
{
    if (points.isEmpty() || time < points.first().timeMs || time > points.last().timeMs) return 0.0;
    const auto next = std::upper_bound(points.cbegin(), points.cend(), time,
        [](qint64 t, const CyclePoint &p) { return t < p.timeMs; });
    const auto previous = next - 1;
    if (!interpolate || next == points.cend()) return previous->value;
    const double fraction = double(time - previous->timeMs) / (next->timeMs - previous->timeMs);
    return previous->value + fraction * (next->value - previous->value);
}
}

qint64 ExperimentPlan::durationMs() const
{
    return std::max(eCycle.isEmpty() ? 0 : eCycle.last().timeMs,
                    rCycle.isEmpty() ? 0 : rCycle.last().timeMs);
}
double ExperimentPlan::ePowerAt(qint64 time) const { return valueAt(eCycle, time, false); }
double ExperimentPlan::rPowerAt(qint64 time) const { return valueAt(rCycle, time, true); }

bool ExperimentPlan::load(const QString &path, const QString &tag, ExperimentPlan &destination,
                          QString &error, const QString &cycleDirectory)
{
    error.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) {
        error = QStringLiteral("Cannot read plan (maximum 2 MB): %1").arg(path); return false;
    }
    ExperimentPlan result;
    QString cycle;
    for (const auto &line : readText(file).split('\n')) {
        const auto fields = csv::split(line);
        if (fields.isEmpty() || fields[0] != tag.trimmed()) continue;
        if (fields.size() < 11 || !result.tag.isEmpty()) {
            error = QStringLiteral("Incomplete or duplicate experiment: %1").arg(tag); return false;
        }
        result.tag = tag.trimmed();
        cycle = fields[1];
        double *values[] = {&result.kp, &result.ki, &result.e.gain, &result.e.numerator,
                            &result.e.denominator, &result.r.gain, &result.r.numerator, &result.r.denominator};
        for (int i = 0; i < 8; ++i) {
            bool ok = false;
            *values[i] = csv::number(fields[i + 3], &ok);
            if (!ok || ((i != 2 && i != 5) && *values[i] < 0.0)) {
                error = QStringLiteral("Invalid number in plan column %1").arg(i + 4); return false;
            }
        }
        if (fields.size() > 11) result.sensorE = fields[11];
        if (fields.size() > 12) result.sensorR = fields[12];
    }
    if (result.tag.isEmpty() || cycle.isEmpty()) {
        error = QStringLiteral("Experiment '%1' was not found in this plan.").arg(tag); return false;
    }
    if (!cycle.endsWith(".csv", Qt::CaseInsensitive)) cycle += ".csv";
    const auto directory = cycleDirectory.isEmpty() ? QFileInfo(path).path() : cycleDirectory;
    QFile cycleFile(QFileInfo(cycle).isAbsolute() ? cycle : QDir(directory).filePath(cycle));
    if (!cycleFile.open(QIODevice::ReadOnly) || cycleFile.size() > 2 * 1024 * 1024) {
        error = QStringLiteral("Cannot read cycle: %1").arg(cycleFile.fileName()); return false;
    }
    int lineNumber = 0;
    for (const auto &line : readText(cycleFile).split('\n')) {
        ++lineNumber;
        if (line.trimmed().isEmpty()) continue;
        const auto fields = csv::split(line);
        if (lineNumber == 1 && !fields.isEmpty() && fields[0] == "r_time_ms") continue;
        if (fields.size() < 4) { error = QStringLiteral("Cycle row %1 requires at least four columns.").arg(lineNumber); return false; }
        for (int column : {0, 2}) {
            // Channels can have independent sample times.
            if (fields[column].isEmpty() && fields[column + 1].isEmpty()) continue;
            bool okTime = false, okValue = false;
            const qint64 time = fields[column].toLongLong(&okTime);
            const double value = csv::number(fields[column + 1], &okValue);
            auto &points = column == 0 ? result.rCycle : result.eCycle;
            if (!okTime || !okValue || time < 0 || value < 0.0
                || (!points.isEmpty() && time < points.last().timeMs)) {
                error = QStringLiteral("Invalid cycle row %1: require ordered times and nonnegative power values.").arg(lineNumber);
                return false;
            }
            // Consecutive transition endpoints at the same instant: last state wins.
            if (!points.isEmpty() && time == points.last().timeMs) points.last().value = value;
            else points.append({time, value});
        }
    }
    if (result.durationMs() <= 0) { error = "The cycle must contain a positive duration."; return false; }
    destination = result;
    return true;
}
