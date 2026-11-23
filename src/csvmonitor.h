#pragma once
#include <QObject>
#include <QTimer>

class CsvMonitor final : public QObject {
    Q_OBJECT
public:
    explicit CsvMonitor(QObject *parent = nullptr);
    void start(const QString &file, const QString &sensorE, const QString &sensorR);
    void stop();
signals:
    void newData(double e, double r);
    void errorOccurred(const QString &message);
private:
    void read();
    QTimer timer_;
    QString path_, sensorE_, sensorR_, lastError_;
    QByteArray lastRow_;
};
