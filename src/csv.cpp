#include "csv.h"
#include <cmath>

QStringList csv::split(const QString &line)
{
    QStringList fields;
    QString field;
    bool quoted = false;
    for (qsizetype i = 0; i < line.size(); ++i) {
        const auto c = line[i];
        if (c == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                field += '"'; ++i;
            } else quoted = !quoted;
        } else if (c == ';' && !quoted) {
            fields << field.trimmed(); field.clear();
        } else if (c != QChar::ByteOrderMark) field += c;
    }
    if (quoted) return {};
    fields << field.trimmed();
    return fields;
}

QString csv::escape(QString value)
{
    value.replace('\r', ' ').replace('\n', ' ').replace('"', "\"\"");
    return '"' + value + '"';
}

double csv::number(QString value, bool *ok)
{
    const double result = value.trimmed().replace(',', '.').toDouble(ok);
    *ok = *ok && std::isfinite(result);
    return result;
}
