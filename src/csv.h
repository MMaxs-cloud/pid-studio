#pragma once
#include <QStringList>

namespace csv {
// Semicolon CSV, escaped quotes, one record per line; decimal dot or comma.
QStringList split(const QString &line);
QString escape(QString value);
double number(QString value, bool *ok);
}
