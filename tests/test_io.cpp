#include "csv.h"
#include "csvmonitor.h"
#include "experimentplan.h"
#include "sessionlogger.h"
#include <QFile>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class IoTest : public QObject {
    Q_OBJECT
private slots:
    void csvQuoting();
    void plans();
    void malformedCycleIsTransactional();
    void sparseCycle();
    void sensorTailAndReplacement();
    void sessionSchema();
};

namespace {
void write(const QString &path, const QByteArray &bytes)
{
    QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(bytes), bytes.size());
}
}

void IoTest::csvQuoting()
{
    QCOMPARE(csv::split("a;\"b;c\";\"d\"\"e\";"), QStringList({"a", "b;c", "d\"e", ""}));
    QVERIFY(csv::split("a;\"broken").isEmpty());
    bool ok = false; QCOMPARE(csv::number(" 2,75 ", &ok), 2.75); QVERIFY(ok);
    csv::number("nan", &ok); QVERIFY(!ok);
    QCOMPARE(csv::escape("a;\"b\n"), QString("\"a;\"\"b \""));
}
void IoTest::plans()
{
    QString error; ExperimentPlan plan;
    QVERIFY2(ExperimentPlan::load(":/examples/plan.csv", "DEMO-FF", plan, error), qPrintable(error));
    QCOMPARE(plan.durationMs(), 120000); QCOMPARE(plan.e.gain, 0.8);
    QCOMPARE(plan.kp, pid::Parameters{}.kp); QCOMPARE(plan.ki, pid::Parameters{}.ki);
    QCOMPARE(plan.ePowerAt(14999), 0.0); QCOMPARE(plan.ePowerAt(15000), 3.0);
    QCOMPARE(plan.ePowerAt(45000), 0.0); QCOMPARE(plan.ePowerAt(120001), 0.0);
    QCOMPARE(plan.rPowerAt(67500), 1.0); QCOMPARE(plan.rPowerAt(-1), 0.0);
    QVERIFY(!ExperimentPlan::load(":/examples/plan.csv", "unknown", plan, error));
    QCOMPARE(plan.tag, QString("DEMO-FF"));
}
void IoTest::malformedCycleIsTransactional()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    const auto path = dir.filePath("plan.csv");
    write(path, "X;cycle;;80;2;0;0;0;0;0;0\n");
    write(dir.filePath("cycle.csv"), "0;0;0;0\n100;2;100;1\n50;0;200;0\n");
    ExperimentPlan plan; plan.tag = "unchanged"; QString error;
    QVERIFY(!ExperimentPlan::load(path, "X", plan, error)); QCOMPARE(plan.tag, QString("unchanged"));
    QVERIFY(error.contains("row 3"));
    write(dir.filePath("cycle.csv"), "0;0;0;0\n100;2;100;1\n");
    QVERIFY2(ExperimentPlan::load(path, "X", plan, error), qPrintable(error));
    QCOMPARE(plan.tag, QString("X"));
    write(path, "X;cycle;;bad;1;0;0;0;0;0;0\n");
    QVERIFY(!ExperimentPlan::load(path, "X", plan, error));
}
void IoTest::sparseCycle()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkdir("cycles"));
    write(dir.filePath("plan.csv"), "Example plan title;;;;;;;;;;;;\nX;sparse;;80;2;0;0;0;0;0;0;E;R\n");
    write(dir.filePath("cycles/sparse.csv"), "0;3.5;0;0;0;1\n16000;3.5;4000;0;16000;1\n;;4000;1;;\n;;7000;1;;\n;;7000;0;;\n");
    QString error; ExperimentPlan plan;
    QVERIFY2(ExperimentPlan::load(dir.filePath("plan.csv"), "X", plan, error, dir.filePath("cycles")), qPrintable(error));
    QCOMPARE(plan.rCycle.size(), 2); QCOMPARE(plan.eCycle.size(), 3);
    QCOMPARE(plan.durationMs(), 16000);
    QCOMPARE(plan.ePowerAt(3999), 0.0); QCOMPARE(plan.ePowerAt(4000), 1.0);
    QCOMPARE(plan.ePowerAt(7000), 0.0); QCOMPARE(plan.rPowerAt(1000), 3.5);
}
void IoTest::sensorTailAndReplacement()
{
    QTemporaryDir dir; const auto path = dir.filePath("sensors.csv");
    write(path, "timestamp;E;R\n0;18.1;19.2\n1;20.");
    CsvMonitor monitor; QSignalSpy values(&monitor, &CsvMonitor::newData);
    QSignalSpy errors(&monitor, &CsvMonitor::errorOccurred);
    monitor.start(path, "E", "R"); QCOMPARE(values.count(), 1);
    QCOMPARE(values.last()[0].toDouble(), 18.1); QCOMPARE(values.last()[1].toDouble(), 19.2);
    QTest::qWait(600); QCOMPARE(values.count(), 1); // unchanged file is stale, not a heartbeat
    write(path, "timestamp;E;R\n2;20.3;21.4\n");
    QTRY_COMPARE_WITH_TIMEOUT(values.count(), 2, 2000);
    QCOMPARE(values.last()[0].toDouble(), 20.3);
    write(path, "timestamp;E;wrong\n3;22;23\n");
    QTRY_COMPARE_WITH_TIMEOUT(errors.count(), 1, 2000); QCOMPARE(values.count(), 2);
    monitor.stop();
    write(path, "timestamp;E;R\n4;22;23\n");
    monitor.start(path, "E", "R"); QCOMPARE(values.count(), 3); monitor.stop();
}
void IoTest::sessionSchema()
{
    QTemporaryDir dir; SessionLogger logger; QString error;
    QVERIFY(logger.open(dir.path(), error));
    pid::ControlSample sample; sample.temperature = 17.25;
    QVERIFY(logger.sample(1.2, "tag;with\"quote", 17, sample, 18, 19, 19, true));
    QVERIFY(logger.event("start", "line\nbreak"));
    QFile file(logger.directory() + "/measurements.csv"); QVERIFY(file.open(QIODevice::ReadOnly));
    const auto header = csv::split(QString::fromUtf8(file.readLine()));
    const auto row = csv::split(QString::fromUtf8(file.readLine()));
    QCOMPARE(header.size(), 16); QCOMPARE(row.size(), header.size());
    QCOMPARE(row[2], QString("tag;with\"quote")); QCOMPARE(row[4].toDouble(), 17.25);
}

QTEST_GUILESS_MAIN(IoTest)
#include "test_io.moc"
