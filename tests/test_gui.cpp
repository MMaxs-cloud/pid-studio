#include "mainwindow.h"
#include "trendplot.h"
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDateTimeEdit>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>

class GuiTest : public QObject {
    Q_OBJECT
private slots:
    void demoWorkflow();
    void liveCsvWorkflow();
    void plotRetentionAndNavigation();
};

void GuiTest::demoWorkflow()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    MainWindow window(dir.path()); window.show();
    auto *start = window.findChild<QPushButton *>("startButton");
    auto *stop = window.findChild<QPushButton *>("stopButton");
    auto *mode = window.findChild<QComboBox *>("modeSelector");
    auto *target = window.findChild<QDoubleSpinBox *>("targetInput");
    auto *cycle = window.findChild<QPushButton *>("cycleButton");
    auto *status = window.findChild<QLabel *>("cycleStatus");
    QVERIFY(start && stop && mode && target && cycle && status);
    QVERIFY(start->isEnabled()); QVERIFY(!stop->isEnabled()); QVERIFY(!cycle->isEnabled());
    QTest::qWait(300);
    auto *powerPlot = window.findChild<TrendPlot *>("powerPlot"); QVERIFY(powerPlot);
    QVERIFY(powerPlot->valueSpan() <= 2.0); // constant zero output has a stable range
    QTest::mouseClick(start, Qt::LeftButton);
    QVERIFY(!start->isEnabled()); QVERIFY(stop->isEnabled()); QVERIFY(!mode->isEnabled()); QVERIFY(!target->isEnabled());
    auto *tabs = window.findChild<QTabWidget *>(); QVERIFY(tabs); tabs->setCurrentIndex(1);
    QTest::mouseClick(cycle, Qt::LeftButton);
    QTRY_VERIFY(status->text().contains("Running"));
    QVERIFY(window.findChild<QPushButton *>("cancelCycleButton")->isEnabled());
    QTest::qWait(2500);
    const auto screenshot = qEnvironmentVariable("PID_SCREENSHOT_PATH");
    if (!screenshot.isEmpty()) {
        const int delay = qEnvironmentVariableIntValue("PID_SCREENSHOT_DELAY_MS");
        if (delay > 0 && delay <= 60000) QTest::qWait(delay);
        tabs->setCurrentIndex(0); QTest::qWait(200);
        QVERIFY(window.grab().save(screenshot));
        tabs->setCurrentIndex(1);
    }
    QTest::keyClick(&window, Qt::Key_Escape);
    QVERIFY(start->isEnabled()); QVERIFY(!stop->isEnabled()); QVERIFY(mode->isEnabled());
    QCOMPARE(status->text(), QString("Cancelled"));
    // Failed loading leaves the previous valid plan intact and usable.
    window.findChild<QLineEdit *>("experimentTagInput")->setText("not-a-tag");
    QTest::mouseClick(window.findChild<QPushButton *>("loadPlanButton"), Qt::LeftButton);
    QTest::mouseClick(start, Qt::LeftButton); QVERIFY(cycle->isEnabled());
    QTest::mouseClick(stop, Qt::LeftButton);
    QFile plan(dir.filePath("plan.csv")); QVERIFY(plan.open(QIODevice::WriteOnly));
    plan.write("SHORT;cycle;;80;2;0;0;0;0;0;0\n"); plan.close();
    QFile shortCycle(dir.filePath("cycle.csv")); QVERIFY(shortCycle.open(QIODevice::WriteOnly));
    shortCycle.write("0;0;0;0\n700;0;700;0\n"); shortCycle.close();
    window.findChild<QLineEdit *>("planPathInput")->setText(plan.fileName());
    window.findChild<QLineEdit *>("experimentTagInput")->setText("SHORT");
    QTest::mouseClick(window.findChild<QPushButton *>("loadPlanButton"), Qt::LeftButton);
    QTest::mouseClick(start, Qt::LeftButton);
    window.findChild<QDateTimeEdit *>()->setDateTime(QDateTime::currentDateTime().addSecs(1));
    QTest::mouseClick(window.findChild<QPushButton *>("scheduleButton"), Qt::LeftButton);
    QVERIFY(status->text().contains("Scheduled"));
    QTRY_VERIFY_WITH_TIMEOUT(status->text().contains("Completed"), 4000);
    QVERIFY(stop->isEnabled()); // cycle completion does not stop feedback control
    QTest::mouseClick(stop, Qt::LeftButton);
    window.resize(1000, 680); QTest::qWait(250);
    QVERIFY(window.width() >= window.minimumWidth());
    if (!screenshot.isEmpty()) QVERIFY(window.grab().save(screenshot + ".compact.png"));
    // A missing CSV is rejected without starting control.
    mode->setCurrentIndex(1); tabs->setCurrentIndex(2);
    QTest::mouseClick(window.findChild<QPushButton *>("connectInputButton"), Qt::LeftButton);
    QTest::mouseClick(start, Qt::LeftButton); QVERIFY(start->isEnabled()); QVERIFY(!stop->isEnabled());
}

void GuiTest::liveCsvWorkflow()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    MainWindow window(dir.path()); window.show();
    QFile input(dir.filePath("input.csv"));
    const auto writeInput = [&](const QByteArray &record) {
        QVERIFY(input.open(QIODevice::WriteOnly | QIODevice::Truncate));
        input.write("timestamp;temperature_c\n"); input.write(record); input.close();
    };
    writeInput("0;16.8\n");
    auto *mode = window.findChild<QComboBox *>("modeSelector"); mode->setCurrentIndex(1);
    window.findChild<QLineEdit *>("measurementPathInput")->setText(input.fileName());
    auto *connectButton = window.findChild<QPushButton *>("connectInputButton");
    auto *start = window.findChild<QPushButton *>("startButton");
    auto *stop = window.findChild<QPushButton *>("stopButton");
    auto *temperature = window.findChild<QLabel *>("temperatureReading");
    QTest::mouseClick(connectButton, Qt::LeftButton);
    QCOMPARE(temperature->text(), QString("16.800"));
    QTest::mouseClick(start, Qt::LeftButton); QVERIFY(stop->isEnabled());
    writeInput("1;16.9\n");
    QTRY_COMPARE_WITH_TIMEOUT(temperature->text(), QString("16.900"), 2000);
    // An unchanged file must not keep control active indefinitely.
    QTRY_VERIFY_WITH_TIMEOUT(start->isEnabled(), 6500); QVERIFY(!stop->isEnabled());
    writeInput("2;17.0\n");
    QTRY_COMPARE_WITH_TIMEOUT(temperature->text(), QString("17.000"), 2000);
    QTest::mouseClick(start, Qt::LeftButton); QVERIFY(stop->isEnabled());
    QTest::mouseClick(connectButton, Qt::LeftButton);
    QVERIFY(start->isEnabled()); QVERIFY(!stop->isEnabled());
}

void GuiTest::plotRetentionAndNavigation()
{
    TrendPlot plot; plot.resize(800, 260); plot.show();
    plot.addSeries("Test", QPen(Qt::blue));
    for (int i = 0; i <= 800; ++i) plot.appendSample(i, {2.0});
    QCOMPARE(plot.sampleCount(0), 601);
    plot.follow(800); QVERIFY(plot.valueSpan() < 1.0);
    QCOMPARE(plot.timeRange().first, 680.0);
    QSignalSpy navigation(&plot, &TrendPlot::navigationStarted);
    QTest::mousePress(&plot, Qt::LeftButton, Qt::NoModifier, QPoint(300, 100));
    QTest::mouseMove(&plot, QPoint(400, 100));
    QTest::mouseRelease(&plot, Qt::LeftButton, Qt::NoModifier, QPoint(400, 100));
    QCOMPARE(navigation.count(), 1); QVERIFY(plot.timeRange().first < 680.0);
    const auto before = plot.sampleCount(0);
    plot.appendSample(801, {std::numeric_limits<double>::quiet_NaN()});
    QCOMPARE(plot.sampleCount(0), before);
    plot.clear(); QCOMPARE(plot.sampleCount(0), 0);
}

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    QApplication::setOrganizationName("PIDStudioTests"); QApplication::setApplicationName("DemoWorkflow");
    QApplication::setStyle("Fusion");
    GuiTest test; return QTest::qExec(&test, argc, argv);
}
#include "test_gui.moc"
