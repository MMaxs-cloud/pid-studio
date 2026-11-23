#pragma once
#include "controlengine.h"
#include "experimentplan.h"
#include "sessionlogger.h"
#include "csvmonitor.h"
#include <QElapsedTimer>
#include <QMainWindow>
#include <QTimer>

class QComboBox;
class QDateTimeEdit;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class TrendPlot;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &outputDirectory, QWidget *parent = nullptr);
    ~MainWindow() override;
private:
    void buildUi();
    void buildPlots();
    QWidget *controlPanel();
    QWidget *experimentPanel();
    QWidget *inputPanel();
    void startControl();
    void stopControl();
    void onTick();
    void onTemperature(double value);
    void updateUiState();
    void updatePlots(double seconds);
    void connectInput();
    void disconnectInput();
    void loadPlan();
    void startCycle();
    void cancelCycle();
    void scheduleCycle();
    void connectSensorLog();
    void captureBaseline();
    void report(const QString &message, bool error = false);
    pid::Parameters parameters() const;
    bool demo() const;

    pid::ControlEngine engine_;
    pid::ControlSample sample_;
    ExperimentPlan plan_;
    SessionLogger logger_;
    CsvMonitor csvMonitor_;
    CsvMonitor measurementMonitor_;
    QElapsedTimer clock_, cycleClock_;
    QTimer tick_, schedule_;
    qint64 lastTick_ = 0, lastMeasurement_ = -1;
    double lastControlSeconds_ = -1.0;
    double demoTemperature_ = 16.35;
    double sensorE_ = 18.0, sensorR_ = 18.0, baselineR_ = 0.0;
    qint64 sensorTimestamp_ = -1;
    bool running_ = false, connected_ = false, cycleActive_ = false;
    bool baselineValid_ = false, logFailed_ = false;
    bool planLoaded_ = false;
    double cycleMin_ = 0.0, cycleMax_ = 0.0;
    int sampleCount_ = 0;

    QComboBox *mode_ = nullptr;
    QDoubleSpinBox *target_ = nullptr, *kp_ = nullptr, *ki_ = nullptr, *bias_ = nullptr;
    QDoubleSpinBox *voltage_ = nullptr, *current_ = nullptr;
    QSpinBox *codeMax_ = nullptr, *adcBits_ = nullptr;
    QCheckBox *quantize_ = nullptr, *follow_ = nullptr;
    QLineEdit *planPath_ = nullptr, *cycleDirectory_ = nullptr, *tag_ = nullptr, *csvPath_ = nullptr, *measurementPath_ = nullptr, *measurementColumn_ = nullptr;
    QDateTimeEdit *startTime_ = nullptr;
    QPushButton *start_ = nullptr, *stop_ = nullptr, *connect_ = nullptr;
    QPushButton *load_ = nullptr, *cycle_ = nullptr, *scheduleButton_ = nullptr, *cancel_ = nullptr;
    QLabel *temperatureValue_ = nullptr, *targetValue_ = nullptr, *powerValue_ = nullptr, *errorValue_ = nullptr;
    QLabel *state_ = nullptr, *cycleInfo_ = nullptr, *ffInfo_ = nullptr, *baselineInfo_ = nullptr;
    QLabel *sensorInfo_ = nullptr, *detail_ = nullptr, *source_ = nullptr;
    QWidget *parameterForm_ = nullptr;
    QListWidget *events_ = nullptr;
    TrendPlot *temperaturePlot_ = nullptr, *powerPlot_ = nullptr;
};
