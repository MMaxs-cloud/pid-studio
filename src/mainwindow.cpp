#include "mainwindow.h"
#include "trendplot.h"
#include <QApplication>
#include <QToolBar>
#include <QStyle>
#include <QPalette>
#include <QMessageBox>
#include <QMenuBar>
#include <QGridLayout>
#include <QDockWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QUrl>
#include <cmath>
#include <limits>

namespace {
QLabel *label(const QString &text, const char *name = "")
{
    auto *widget = new QLabel(text);
    widget->setObjectName(name);
    if (QByteArray(name) == "plotTitle") widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return widget;
}
QDoubleSpinBox *number(const char *name, double value, double minimum, double maximum, int decimals = 3)
{
    auto *widget = new QDoubleSpinBox;
    widget->setObjectName(name);
    widget->setRange(minimum, maximum);
    widget->setDecimals(decimals);
    widget->setValue(value);
    widget->setLocale(QLocale::c());
    widget->setKeyboardTracking(false);
    widget->setMinimumWidth(115);
    return widget;
}
QPushButton *button(const QString &text, const char *name = "")
{
    auto *widget = new QPushButton(text);
    widget->setObjectName(name);
    widget->setCursor(Qt::PointingHandCursor);
    return widget;
}
QWidget *fileField(QLineEdit *edit, QWidget *owner, const QString &title)
{
    auto *widget = new QWidget;
    auto *row = new QHBoxLayout(widget);
    row->setContentsMargins(0, 0, 0, 0);
    auto *browse = button("…");
    browse->setFixedWidth(32);
    browse->setAccessibleName(title);
    row->addWidget(edit); row->addWidget(browse);
    QObject::connect(browse, &QPushButton::clicked, owner, [=] {
        const auto path = QFileDialog::getOpenFileName(owner, title, {}, "CSV files (*.csv)");
        if (!path.isEmpty()) edit->setText(path);
    });
    return widget;
}
void addReading(QGridLayout *layout, int row, const QString &title, QLabel *&value, const QString &unit)
{
    layout->addWidget(label(title), row, 0);
    value = label("—", "readingValue");
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value->setMinimumWidth(80);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(value, row, 1);
    layout->addWidget(label(unit, "muted"), row, 2);
}
}

MainWindow::MainWindow(const QString &outputDirectory, QWidget *parent) : QMainWindow(parent)
{
    buildUi();
    buildPlots();
    clock_.start();
    QString error;
    const auto root = outputDirectory.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/sessions" : outputDirectory;
    if (!logger_.open(root, error)) { logFailed_ = true; report(error, true); }
    connect(&measurementMonitor_, &CsvMonitor::newData, this, [this](double temperature, double) {
        if (connected_ && !demo()) onTemperature(temperature);
    });
    connect(&measurementMonitor_, &CsvMonitor::errorOccurred, this, [this](const QString &message) {
        lastMeasurement_ = -1;
        stopControl(); report(message, true);
    });
    connect(&csvMonitor_, &CsvMonitor::newData, this, [this](double e, double r) {
        sensorE_ = e; sensorR_ = r; sensorTimestamp_ = clock_.elapsed();
    });
    connect(&csvMonitor_, &CsvMonitor::errorOccurred, this, [this](const QString &message) {
        sensorTimestamp_ = -1;
        if (running_ && !demo() && plan_.r.gain != 0.0) stopControl();
        report(message, true);
    });
    schedule_.setSingleShot(true);
    connect(&schedule_, &QTimer::timeout, this, &MainWindow::startCycle);
    tick_.setInterval(250);
    tick_.setTimerType(Qt::PreciseTimer);
    connect(&tick_, &QTimer::timeout, this, &MainWindow::onTick);
    tick_.start();
    loadPlan();
    report("Demo ready. Start control to stabilize the simulated heater.");
    updateUiState();
}

MainWindow::~MainWindow()
{
    schedule_.stop(); tick_.stop();
    stopControl(); disconnectInput();
    logger_.event("session_end", "Application closed");
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowLayout", saveState(2));
    settings.setValue("target", target_->value());
    settings.setValue("voltage", voltage_->value());
    settings.setValue("current", current_->value());

}

void MainWindow::buildUi()
{
    setWindowTitle("PID Studio - Temperature control");
    resize(1280, 820);
    setMinimumSize(1000, 680);
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);

    QPalette colors = QApplication::style()->standardPalette();
    colors.setColor(QPalette::Window, QColor("#f0f0ed"));
    colors.setColor(QPalette::WindowText, QColor("#262b2f"));
    colors.setColor(QPalette::Base, Qt::white);
    colors.setColor(QPalette::AlternateBase, QColor("#f5f6f7"));
    colors.setColor(QPalette::Text, QColor("#262b2f"));
    colors.setColor(QPalette::Button, QColor("#f0f0ed"));
    colors.setColor(QPalette::ButtonText, QColor("#262b2f"));
    colors.setColor(QPalette::Highlight, QColor("#356f9d"));
    colors.setColor(QPalette::HighlightedText, Qt::white);
    colors.setColor(QPalette::Disabled, QPalette::Text, QColor("#7b8185"));
    colors.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#8b9094"));
    setPalette(colors);
    setFont(QFont("Segoe UI", 9));

    auto *toolbar = addToolBar("Control");
    toolbar->setObjectName("controlToolbar");
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->addWidget(label("Source: "));
    mode_ = new QComboBox;
    mode_->setObjectName("modeSelector");
    mode_->addItems({"Demo (simulation)", "CSV input"});
    mode_->setMinimumWidth(188);
    toolbar->addWidget(mode_);
    toolbar->addSeparator();
    start_ = button("Start", "startButton");
    start_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    start_->setToolTip("Start PI control (Ctrl+Enter)");
    stop_ = button("Stop", "stopButton");
    stop_->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    stop_->setToolTip("Stop control and cancel the cycle (Esc)");
    toolbar->addWidget(start_);
    toolbar->addWidget(stop_);
    toolbar->addSeparator();
    state_ = label("Stopped", "controlState");
    state_->setMinimumWidth(90);
    toolbar->addWidget(state_);

    auto *settingsDock = new QDockWidget("Settings and readings", this);
    settingsDock->setObjectName("settingsDock");
    settingsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    settingsDock->setMinimumWidth(290);
    auto *settingsPanel = new QWidget;
    auto *settingsLayout = new QVBoxLayout(settingsPanel);
    settingsLayout->setContentsMargins(8, 6, 8, 6);
    settingsLayout->setSpacing(8);
    auto *readings = new QGroupBox("Readings");
    auto *readingGrid = new QGridLayout(readings);
    readingGrid->setContentsMargins(10, 12, 10, 8);
    readingGrid->setHorizontalSpacing(8);
    readingGrid->setVerticalSpacing(3);
    readingGrid->setColumnStretch(1, 1);
    addReading(readingGrid, 0, "Temperature", temperatureValue_, "°C");
    addReading(readingGrid, 1, "Setpoint", targetValue_, "°C");
    addReading(readingGrid, 2, "Error", errorValue_, "°C");
    addReading(readingGrid, 3, "Heater power", powerValue_, "W");
    temperatureValue_->setObjectName("temperatureReading");
    settingsLayout->addWidget(readings);
    auto *tabs = new QTabWidget;
    tabs->setObjectName("settingsTabs");
    tabs->addTab(controlPanel(), "Control");
    tabs->addTab(experimentPanel(), "Experiment");
    tabs->addTab(inputPanel(), "Input");
    settingsLayout->addWidget(tabs, 1);
    settingsDock->setWidget(settingsPanel);
    addDockWidget(Qt::LeftDockWidgetArea, settingsDock);

    auto *central = new QWidget;
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(8, 6, 8, 4);
    root->setSpacing(6);
    auto *plotTools = new QHBoxLayout;
    source_ = label("Simulated measurements", "sourceLabel");
    plotTools->addWidget(source_);
    plotTools->addStretch();
    follow_ = new QCheckBox("Follow live");
    follow_->setChecked(true);
    plotTools->addWidget(follow_);
    auto *clear = button("Clear plots");
    plotTools->addWidget(clear);
    root->addLayout(plotTools);

    auto *plots = new QSplitter(Qt::Vertical);
    plots->setObjectName("plotSplitter");
    const auto addPlot = [plots](const QString &title, const char *objectName) {
        auto *panel = new QFrame;
        panel->setObjectName("plotPanel");
        panel->setFrameShape(QFrame::StyledPanel);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(6, 5, 6, 3);
        layout->setSpacing(0);
        layout->addWidget(label(title, "plotTitle"));
        auto *plot = new TrendPlot;
        plot->setObjectName(objectName);
        layout->addWidget(plot, 1);
        plots->addWidget(panel);
        return plot;
    };
    temperaturePlot_ = addPlot("Temperature", "temperaturePlot");
    powerPlot_ = addPlot("Control output", "powerPlot");
    plots->setChildrenCollapsible(false);
    plots->setSizes({340, 240});
    root->addWidget(plots, 1);
    detail_ = label("P 0.00 W    I 0.00 W    FF 0.00 W    Code 0 / 1000", "controlDetails");
    root->addWidget(detail_);

    auto *eventsDock = new QDockWidget("Event log", this);
    eventsDock->setObjectName("eventsDock");
    eventsDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    events_ = new QListWidget;
    events_->setObjectName("eventList");
    events_->setUniformItemSizes(true);
    events_->setAlternatingRowColors(true);
    events_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    events_->setMinimumHeight(54);
    eventsDock->setWidget(events_);
    addDockWidget(Qt::BottomDockWidgetArea, eventsDock);
    resizeDocks({settingsDock}, {310}, Qt::Horizontal);
    resizeDocks({eventsDock}, {100}, Qt::Vertical);

    auto *fileMenu = menuBar()->addMenu("&File");
    auto *logs = fileMenu->addAction("Open session &folder");
    fileMenu->addSeparator();
    auto *quit = fileMenu->addAction("E&xit");
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);
    auto *viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(settingsDock->toggleViewAction());
    viewMenu->addAction(eventsDock->toggleViewAction());
    viewMenu->addSeparator();
    auto *resetLayout = viewMenu->addAction("Reset layout");
    connect(resetLayout, &QAction::triggered, this, [=] {
        settingsDock->setFloating(false);
        eventsDock->setFloating(false);
        addDockWidget(Qt::LeftDockWidgetArea, settingsDock);
        addDockWidget(Qt::BottomDockWidgetArea, eventsDock);
        settingsDock->show();
        eventsDock->show();
        resizeDocks({settingsDock}, {310}, Qt::Horizontal);
        resizeDocks({eventsDock}, {100}, Qt::Vertical);
    });
    auto *helpMenu = menuBar()->addMenu("&Help");
    auto *about = helpMenu->addAction("About PID Studio");
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox::about(this, "PID Studio", "PID Studio\nThermal control and experiment logging\n\nPI controller with two feed-forward channels.\nC++17 / Qt 6");
    });
    auto *aboutQt = helpMenu->addAction("About Qt");
    connect(aboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);

    connect(start_, &QPushButton::clicked, this, &MainWindow::startControl);
    connect(stop_, &QPushButton::clicked, this, &MainWindow::stopControl);
    connect(new QShortcut(QKeySequence("Ctrl+Return"), this), &QShortcut::activated, this, &MainWindow::startControl);
    connect(new QShortcut(QKeySequence(Qt::Key_Escape), this), &QShortcut::activated, this, &MainWindow::stopControl);
    connect(logs, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(logger_.directory()));
    });
    connect(clear, &QPushButton::clicked, this, [this] {
        for (auto *plot : {temperaturePlot_, powerPlot_}) {
            plot->clear();
        }
        report("Plot history cleared. Session CSV is retained.");
    });
    connect(mode_, &QComboBox::currentIndexChanged, this, [this] {
        stopControl(); disconnectInput(); csvMonitor_.stop();
        lastMeasurement_ = -1; lastControlSeconds_ = -1.0;
        baselineValid_ = false; sensorTimestamp_ = -1; sample_ = {};
        for (auto *plot : {temperaturePlot_, powerPlot_}) plot->clear();
        temperatureValue_->setText("—"); powerValue_->setText("0.00"); errorValue_->setText("—");
        source_->setText(demo() ? "Simulated measurements" : "CSV measurements · calculated output only");
        report(demo() ? "Demo mode selected." : "CSV mode selected. Choose a live measurement file before starting.");
        updateUiState();
    });
    setStyleSheet(R"(
        QMainWindow { background: #f0f0ed; }
        QToolBar { spacing: 5px; padding: 4px; border-bottom: 1px solid #bfc3c6; }
        QDockWidget::title { padding: 4px 6px; background: #e2e4e4; border-bottom: 1px solid #c3c7ca; }
        QGroupBox { margin-top: 10px; }
        QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 8px; padding: 0 3px; }
        QLabel#muted { color: #62696f; }
        QLabel#readingValue, QLabel#temperatureReading { font-family: Consolas, monospace; font-size: 12pt; }
        QLabel#temperatureReading { color: #235b86; font-weight: 600; }
        QLabel#sourceLabel { color: #555d63; }
        QLabel#plotTitle { color: #30363b; font-weight: 600; padding: 2px 5px; }
        QLabel#controlDetails { color: #4d565e; font-family: Consolas, monospace; font-size: 9pt; }
        QFrame#plotPanel { background: white; border: 1px solid #bfc5c9; }
        QPushButton { min-height: 22px; padding: 2px 8px; }
        QAbstractSpinBox, QLineEdit, QComboBox { min-height: 22px; }
        QScrollArea { border: none; }
        QListWidget#eventList { border: none; }
        QListWidget#eventList::item { padding: 2px 5px; }
        QStatusBar { border-top: 1px solid #c7cbcd; }
    )");
    QSettings settings;
    if (settings.contains("geometry")) restoreGeometry(settings.value("geometry").toByteArray());
    if (settings.contains("windowLayout")) restoreState(settings.value("windowLayout").toByteArray(), 2);
}

QWidget *MainWindow::controlPanel()
{
    auto *scroll = new QScrollArea; scroll->setWidgetResizable(true);
    auto *panel = new QWidget; auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 8, 6, 6); layout->setSpacing(8);
    parameterForm_ = new QWidget; auto *parametersLayout = new QVBoxLayout(parameterForm_);
    parametersLayout->setContentsMargins(0, 0, 0, 0);
    auto *controller = new QGroupBox("PI controller"); auto *form = new QFormLayout(controller);
    QSettings settings;
    target_ = number("targetInput", settings.value("target", 17.0).toDouble(), -50, 150); target_->setSuffix(" °C");
    kp_ = number("kpInput", pid::Parameters{}.kp, 0, 10000);
    ki_ = number("kiInput", pid::Parameters{}.ki, 0, 1000);
    bias_ = number("biasInput", pid::Parameters{}.bias, 0, 1000); bias_->setSuffix(" W");
    form->addRow("Setpoint", target_); form->addRow("Kp · W/°C", kp_); form->addRow("Ki · W/(°C·s)", ki_); form->addRow("Bias power", bias_);
    kp_->setToolTip("Proportional gain"); ki_->setToolTip("Integral gain; zero disables integration");
    parametersLayout->addWidget(controller);
    auto *actuator = new QGroupBox("Actuator / resolution"); auto *actuatorForm = new QFormLayout(actuator);
    voltage_ = number("voltageInput", settings.value("voltage", pid::Parameters{}.voltage).toDouble(), 0.001, 1000); voltage_->setSuffix(" V");
    current_ = number("currentInput", settings.value("current", pid::Parameters{}.current).toDouble(), 0.001, 100); current_->setSuffix(" A");
    codeMax_ = new QSpinBox; codeMax_->setRange(1, 65535); codeMax_->setValue(pid::Parameters{}.codeMax);
    adcBits_ = new QSpinBox; adcBits_->setRange(1, 24); adcBits_->setValue(pid::Parameters{}.adcBits);
    quantize_ = new QCheckBox("Quantize temperature · 0–100 °C");
    quantize_->setToolTip("Generic quantization example over 0–100 °C");
    actuatorForm->addRow("Supply voltage", voltage_); actuatorForm->addRow("Supply current", current_);
    actuatorForm->addRow("Max output code", codeMax_); actuatorForm->addRow("ADC bits", adcBits_);
    actuatorForm->addRow(quantize_); parametersLayout->addWidget(actuator); layout->addWidget(parameterForm_);
    auto *note = label("Stop control to edit parameters.\nDemo mode uses a simulated thermal plant.", "muted");
    note->setWordWrap(true); layout->addWidget(note); layout->addStretch();
    scroll->setWidget(panel); return scroll;
}

QWidget *MainWindow::experimentPanel()
{
    auto *scroll = new QScrollArea; scroll->setWidgetResizable(true);
    auto *panel = new QWidget; auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(6, 8, 6, 6);
    layout->addWidget(label("Experiment plan · semicolon CSV", "muted"));
    planPath_ = new QLineEdit(":/examples/plan.csv"); planPath_->setObjectName("planPathInput");
    layout->addWidget(fileField(planPath_, this, "Choose experiment plan"));
    layout->addWidget(label("Cycle folder · optional", "muted"));
    cycleDirectory_ = new QLineEdit; cycleDirectory_->setPlaceholderText("Same folder as the plan");
    auto *directoryRow = new QHBoxLayout;
    auto *browseDirectory = button("…"); browseDirectory->setFixedWidth(32);
    browseDirectory->setAccessibleName("Choose cycle folder");
    directoryRow->addWidget(cycleDirectory_); directoryRow->addWidget(browseDirectory); layout->addLayout(directoryRow);
    connect(browseDirectory, &QPushButton::clicked, this, [this] {
        if (running_) return;
        const auto directory = QFileDialog::getExistingDirectory(this, "Choose cycle folder");
        if (!directory.isEmpty()) cycleDirectory_->setText(directory);
    });
    layout->addWidget(label("Experiment tag", "muted"));
    tag_ = new QLineEdit("DEMO-FF"); tag_->setObjectName("experimentTagInput"); layout->addWidget(tag_);
    load_ = button("Load experiment", "loadPlanButton"); layout->addWidget(load_);
    ffInfo_ = label("No experiment loaded", "muted"); ffInfo_->setWordWrap(true); layout->addWidget(ffInfo_);
    auto *group = new QGroupBox("Cycle timeline"); auto *groupLayout = new QVBoxLayout(group);
    cycleInfo_ = label("Ready", "cycleStatus"); cycleInfo_->setWordWrap(true); groupLayout->addWidget(cycleInfo_);
    cycle_ = button("Run cycle now", "cycleButton"); groupLayout->addWidget(cycle_);
    startTime_ = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(60));
    startTime_->setDisplayFormat("dd MMM yyyy  HH:mm:ss"); startTime_->setCalendarPopup(true); groupLayout->addWidget(startTime_);
    scheduleButton_ = button("Schedule cycle", "scheduleButton"); groupLayout->addWidget(scheduleButton_);
    cancel_ = button("Cancel cycle", "cancelCycleButton"); groupLayout->addWidget(cancel_); layout->addWidget(group);
    auto *baseline = new QGroupBox("Feed-forward reference"); auto *baselineLayout = new QVBoxLayout(baseline);
    sensorInfo_ = label("E —   /   R —", "muted"); sensorInfo_->setWordWrap(true); baselineLayout->addWidget(sensorInfo_);
    baselineInfo_ = label("T₀ not captured", "baselineStatus"); baselineInfo_->setWordWrap(true); baselineLayout->addWidget(baselineInfo_);
    auto *capture = button("Capture T₀", "captureBaselineButton"); baselineLayout->addWidget(capture);
    auto *reset = button("Reset T₀"); baselineLayout->addWidget(reset); layout->addWidget(baseline);
    layout->addStretch(); scroll->setWidget(panel);
    connect(load_, &QPushButton::clicked, this, &MainWindow::loadPlan);
    connect(cycle_, &QPushButton::clicked, this, &MainWindow::startCycle);
    connect(scheduleButton_, &QPushButton::clicked, this, &MainWindow::scheduleCycle);
    connect(cancel_, &QPushButton::clicked, this, &MainWindow::cancelCycle);
    connect(capture, &QPushButton::clicked, this, &MainWindow::captureBaseline);
    connect(reset, &QPushButton::clicked, this, [this] {
        if (running_) { report("Stop control before resetting the reference.", true); return; }
        baselineValid_ = false; baselineInfo_->setText("T₀ not captured"); engine_.reset(); report("Feed-forward reference reset.");
    });
    return scroll;
}

QWidget *MainWindow::inputPanel()
{
    auto *scroll = new QScrollArea; scroll->setWidgetResizable(true);
    auto *panel = new QWidget; auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(6, 8, 6, 6);
    auto *measurements = new QGroupBox("Live measurement CSV"); auto *form = new QVBoxLayout(measurements);
    measurementPath_ = new QLineEdit; measurementPath_->setObjectName("measurementPathInput");
    measurementPath_->setPlaceholderText("Choose a live CSV file…");
    form->addWidget(fileField(measurementPath_, this, "Choose measurement CSV"));
    form->addWidget(label("Temperature column", "muted"));
    measurementColumn_ = new QLineEdit("temperature_c"); measurementColumn_->setObjectName("measurementColumnInput");
    form->addWidget(measurementColumn_);
    connect_ = button("Connect CSV", "connectInputButton"); form->addWidget(connect_); layout->addWidget(measurements);
    auto *notice = label("CSV mode computes and logs the controller output. This public version does not send commands to physical equipment.", "muted");
    notice->setWordWrap(true); layout->addWidget(notice);
    auto *sensors = new QGroupBox("External temperature log"); auto *sensorLayout = new QVBoxLayout(sensors);
    csvPath_ = new QLineEdit; csvPath_->setPlaceholderText("Choose sensor CSV…");
    sensorLayout->addWidget(fileField(csvPath_, this, "Choose sensor CSV"));
    auto *monitor = button("Monitor sensor CSV", "monitorCsvButton"); sensorLayout->addWidget(monitor);
    auto *note = label("Load an experiment first. CSV headers must match its sensor names. Only complete records are read.", "muted");
    note->setWordWrap(true); sensorLayout->addWidget(note); layout->addWidget(sensors); layout->addStretch(); scroll->setWidget(panel);
    connect(connect_, &QPushButton::clicked, this, [this] {
        if (connected_) { stopControl(); disconnectInput(); updateUiState(); }
        else connectInput();
    });
    connect(monitor, &QPushButton::clicked, this, &MainWindow::connectSensorLog);
    return scroll;
}

void MainWindow::buildPlots()
{
    temperaturePlot_->setValueLabel("Temperature (°C)");
    temperaturePlot_->addSeries("Temperature", QPen(QColor("#2164a4"), 1.8));
    temperaturePlot_->addSeries("Setpoint", QPen(QColor("#b24c32"), 1.4, Qt::DashLine));
    powerPlot_->setValueLabel("Power (W)");
    powerPlot_->addSeries("Heater output", QPen(QColor("#2164a4"), 1.8));
    powerPlot_->addSeries("PI + bias", QPen(QColor("#6e7770"), 1.3, Qt::DashLine));
    powerPlot_->addSeries("Feed-forward", QPen(QColor("#ae612c"), 1.3));
    for (auto *plot : {temperaturePlot_, powerPlot_})
        connect(plot, &TrendPlot::navigationStarted, this, [this] { follow_->setChecked(false); });
}

bool MainWindow::demo() const { return mode_->currentIndex() == 0; }
pid::Parameters MainWindow::parameters() const
{
    pid::Parameters p;
    p.target = target_->value(); p.kp = kp_->value(); p.ki = ki_->value(); p.bias = bias_->value();
    p.voltage = voltage_->value(); p.current = current_->value();
    p.codeMax = codeMax_->value(); p.adcBits = adcBits_->value(); p.quantize = quantize_->isChecked(); return p;
}

void MainWindow::startControl()
{
    if (running_) return;
    if (!parameters().valid()) { report("Invalid control parameters.", true); return; }
    if (!demo() && (!connected_ || lastMeasurement_ < 0 || clock_.elapsed() - lastMeasurement_ > 5000)) {
        report("Connect a live measurement CSV and wait for a fresh reading.", true); return;
    }
    if (!demo() && plan_.r.gain != 0.0 && (!baselineValid_ || sensorTimestamp_ < 0
        || clock_.elapsed() - sensorTimestamp_ > 5000)) {
        report("R compensation requires a fresh sensor CSV and captured T₀.", true); return;
    }
    const double temperature = demo() ? demoTemperature_ : sample_.temperature;
    engine_.reset(); sample_ = {}; sample_.temperature = temperature;
    sample_.error = target_->value() - temperature;
    running_ = true; lastControlSeconds_ = -1.0;
    report(QString("Control started · target %1 °C · Kp %2 · Ki %3 · %4")
        .arg(target_->value()).arg(kp_->value()).arg(ki_->value()).arg(demo() ? "demo" : "CSV input"));
    updateUiState();
}
void MainWindow::stopControl()
{
    cancelCycle();
    const bool wasRunning = running_;
    const double temperature = sample_.temperature;
    running_ = false; engine_.reset(); sample_ = {};
    sample_.temperature = temperature; sample_.error = target_->value() - temperature;
    if (wasRunning) report("Control stopped · output code reset to zero.");
    if (powerValue_) powerValue_->setText("0.00");
    updateUiState();
}

void MainWindow::onTick()
{
    const qint64 now = clock_.elapsed();
    const double dt = std::clamp((now - lastTick_) / 1000.0, 0.0, 2.0); lastTick_ = now;
    if (cycleActive_) {
        const auto elapsed = cycleClock_.elapsed();
        cycleInfo_->setText(QString("Running · %1 / %2 s\nΔT range: %3 °C")
            .arg(elapsed / 1000).arg(plan_.durationMs() / 1000).arg(cycleMax_ - cycleMin_, 0, 'f', 4));
        if (elapsed >= plan_.durationMs()) {
            cycleActive_ = false; cycleInfo_->setText(QString("Completed · ΔT %1 °C").arg(cycleMax_ - cycleMin_, 0, 'f', 4));
            report("Cycle completed. Temperature control continues."); updateUiState();
        }
    }
    if (demo()) {
        const double e = cycleActive_ ? plan_.ePowerAt(cycleClock_.elapsed()) : 0.0;
        const double r = cycleActive_ ? plan_.rPowerAt(cycleClock_.elapsed()) : 0.0;
        // Synthetic first-order plant: 12 W holds 17 °C at a 16 °C ambient.
        demoTemperature_ += (sample_.power + e + r - 12.0 * (demoTemperature_ - 16.0)) / 240.0 * dt;
        sensorE_ = 18.0 + e * 0.02; sensorR_ = 18.0 + r * 0.05; sensorTimestamp_ = now;
        onTemperature(demoTemperature_ + 0.0008 * std::sin(now / 730.0));
    } else if (running_ && (lastMeasurement_ < 0 || now - lastMeasurement_ > 5000)) {
        stopControl(); report("No temperature readings for 5 seconds. Control stopped.", true);
    } else if (running_ && plan_.r.gain != 0.0
               && (sensorTimestamp_ < 0 || now - sensorTimestamp_ > 5000)) {
        stopControl(); report("External sensor data is unavailable. Control stopped.", true);
    }
    sensorInfo_->setText(sensorTimestamp_ >= 0
        ? QString("E %1 °C  /  R %2 °C").arg(sensorE_, 0, 'f', 3).arg(sensorR_, 0, 'f', 3) : "E —   /   R —");
    statusBar()->showMessage(QString("%1  ·  %2 samples  ·  %3  ·  Session %4")
        .arg(demo() ? "SIMULATED DATA" : (connected_ ? "CSV CONNECTED" : "CSV DISCONNECTED"))
        .arg(sampleCount_).arg(logFailed_ ? "LOGGING ERROR" : "CSV recording").arg(QFileInfo(logger_.directory()).fileName()));
}

void MainWindow::onTemperature(double value)
{
    if (!std::isfinite(value)) return;
    const double seconds = clock_.elapsed() / 1000.0;
    lastMeasurement_ = clock_.elapsed();
    const double dt = lastControlSeconds_ < 0.0 ? 0.0 : seconds - lastControlSeconds_;
    lastControlSeconds_ = seconds;
    const auto p = parameters();
    const double e = cycleActive_ ? -plan_.ePowerAt(cycleClock_.elapsed()) : 0.0;
    const bool freshR = sensorTimestamp_ >= 0 && clock_.elapsed() - sensorTimestamp_ <= 5000;
    const double r = baselineValid_ && freshR ? baselineR_ - sensorR_ : 0.0;
    if (running_ && dt > 5.0) { stopControl(); report("Measurement interval exceeded 5 seconds.", true); }
    if (running_ && !demo() && plan_.r.gain != 0.0 && !freshR) {
        stopControl(); report("External sensor data is stale. Control stopped.", true);
    }
    if (running_) {
        sample_ = engine_.step(value, dt, p, e, r, plan_.e, plan_.r);
    } else {
        sample_ = {}; sample_.temperature = value; sample_.error = p.target - value;
    }
    if (cycleActive_) { cycleMin_ = std::min(cycleMin_, value); cycleMax_ = std::max(cycleMax_, value); }
    temperatureValue_->setText(QString::number(value, 'f', 3));
    targetValue_->setText(QString::number(p.target, 'f', 2));
    powerValue_->setText(QString::number(sample_.power, 'f', 2));
    errorValue_->setText(QString::number(sample_.error, 'f', 3));
    detail_->setText(QString("P  %1 W     I  %2 W     FF  %3 W     Code  %4 / %5")
        .arg(sample_.proportional, 0, 'f', 2).arg(sample_.integral, 0, 'f', 2)
        .arg(sample_.feedForwardE + sample_.feedForwardR, 0, 'f', 2).arg(sample_.code).arg(p.codeMax));
    ++sampleCount_;
    if (!logFailed_ && !logger_.sample(seconds, plan_.tag, p.target, sample_, sensorE_, sensorR_, baselineR_, running_)) {
        logFailed_ = true; report("CSV write failed. Check disk space and output folder permissions.", true);
    }
    updatePlots(seconds);
}

void MainWindow::updatePlots(double seconds)
{
    temperaturePlot_->appendSample(seconds, {sample_.temperature, target_->value()});
    powerPlot_->appendSample(seconds, {sample_.power,
        running_ ? sample_.proportional + sample_.integral + bias_->value() : 0.0,
        sample_.feedForwardE + sample_.feedForwardR});
    if (follow_->isChecked())
        for (auto *plot : {temperaturePlot_, powerPlot_}) plot->follow(seconds);
}

void MainWindow::connectInput()
{
    if (demo()) { report("Select CSV input mode before connecting.", true); return; }
    const auto path = measurementPath_->text().trimmed();
    const auto column = measurementColumn_->text().trimmed();
    if (path.isEmpty() || column.isEmpty()) { report("Choose a measurement CSV and temperature column.", true); return; }
    lastMeasurement_ = -1; lastControlSeconds_ = -1.0;
    connected_ = true;
    measurementMonitor_.start(path, column, {});
    report("Measurement CSV connected. Output is calculated and logged only."); updateUiState();
}

void MainWindow::disconnectInput()
{
    measurementMonitor_.stop(); connected_ = false; lastMeasurement_ = -1;
}

void MainWindow::loadPlan()
{
    if (running_ || cycleActive_ || schedule_.isActive()) { report("Stop control before loading another experiment.", true); return; }
    QString error; ExperimentPlan next;
    if (!ExperimentPlan::load(planPath_->text().trimmed(), tag_->text().trimmed(), next, error, cycleDirectory_->text().trimmed())) { report(error, true); return; }
    if (next.kp > kp_->maximum() || next.ki > ki_->maximum()) { report("Plan gains exceed the supported editor range.", true); return; }
    plan_ = next; planLoaded_ = true; kp_->setValue(plan_.kp); ki_->setValue(plan_.ki);
    baselineValid_ = false; sensorTimestamp_ = -1; csvMonitor_.stop(); engine_.reset();
    baselineInfo_->setText("T₀ not captured");
    ffInfo_->setText(QString("%1 · %2 s\nE: K %3 · τ %4 / %5 s\nR: K %6 · τ %7 / %8 s\nSensors: %9 / %10")
        .arg(plan_.tag).arg(plan_.durationMs() / 1000).arg(plan_.e.gain).arg(plan_.e.numerator).arg(plan_.e.denominator)
        .arg(plan_.r.gain).arg(plan_.r.numerator).arg(plan_.r.denominator).arg(plan_.sensorE, plan_.sensorR));
    cycleInfo_->setText("Loaded · ready to run"); report("Experiment loaded: " + plan_.tag); updateUiState();
}
void MainWindow::startCycle()
{
    if (!running_ || !planLoaded_ || cycleActive_) { report("Start control and load an experiment before running a cycle.", true); return; }
    schedule_.stop(); cycleClock_.restart(); cycleActive_ = true;
    cycleMin_ = cycleMax_ = sample_.temperature;
    report("Cycle started: " + plan_.tag); updateUiState();
}
void MainWindow::cancelCycle()
{
    if (cycleActive_ || schedule_.isActive()) {
        cycleActive_ = false; schedule_.stop(); cycleInfo_->setText("Cancelled"); report("Cycle cancelled.");
    }
    updateUiState();
}
void MainWindow::scheduleCycle()
{
    if (!running_ || !planLoaded_ || cycleActive_) { report("Start control and load an experiment before scheduling.", true); return; }
    const auto delay = QDateTime::currentDateTime().msecsTo(startTime_->dateTime());
    if (delay <= 0 || delay > std::numeric_limits<int>::max()) {
        report("Choose a future time within 24 days.", true); return;
    }
    schedule_.start(static_cast<int>(delay));
    cycleInfo_->setText("Scheduled · " + startTime_->dateTime().toString("dd MMM HH:mm:ss"));
    report("Cycle scheduled: " + startTime_->dateTime().toString(Qt::ISODate)); updateUiState();
}
void MainWindow::connectSensorLog()
{
    if (demo()) { report("Demo mode generates external sensor values automatically."); return; }
    if (running_) { report("Stop control before changing the sensor log.", true); return; }
    if (!planLoaded_ || (plan_.sensorE.isEmpty() && plan_.sensorR.isEmpty()) || csvPath_->text().trimmed().isEmpty()) {
        report("Load a plan with sensor names and choose a sensor CSV.", true); return;
    }
    baselineValid_ = false; baselineInfo_->setText("T₀ not captured"); sensorTimestamp_ = -1;
    csvMonitor_.start(csvPath_->text().trimmed(), plan_.sensorE, plan_.sensorR); report("Sensor CSV monitoring started.");
}
void MainWindow::captureBaseline()
{
    if (running_) { report("Stop control before changing the reference.", true); return; }
    if (sensorTimestamp_ < 0 || clock_.elapsed() - sensorTimestamp_ > 5000) { report("Wait for fresh external sensor values first.", true); return; }
    baselineR_ = sensorR_; baselineValid_ = true; engine_.reset();
    baselineInfo_->setText(QString("T₀ E %1 °C / R %2 °C").arg(sensorE_, 0, 'f', 3).arg(sensorR_, 0, 'f', 3));
    report("Feed-forward reference captured.");
}
void MainWindow::updateUiState()
{
    if (!start_) return;
    const bool pending = cycleActive_ || schedule_.isActive();
    start_->setEnabled(!running_); stop_->setEnabled(running_ || pending);
    mode_->setEnabled(!running_ && !pending); parameterForm_->setEnabled(!running_);
    load_->setEnabled(!running_ && !pending); planPath_->setEnabled(!running_); tag_->setEnabled(!running_);
    cycleDirectory_->setEnabled(!running_);
    cycle_->setEnabled(running_ && planLoaded_ && !pending);
    scheduleButton_->setEnabled(running_ && planLoaded_ && !pending); cancel_->setEnabled(pending);
    startTime_->setEnabled(!pending);
    measurementPath_->setEnabled(!connected_); measurementColumn_->setEnabled(!connected_);
    connect_->setEnabled(!demo()); connect_->setText(connected_ ? "Disconnect CSV" : "Connect CSV");
    state_->setText(running_ ? "Regulating" : "Stopped");
    state_->setStyleSheet(running_ ? "color: #25633d; font-weight: 600;" : "color: #62676b;");
    targetValue_->setText(QString::number(target_->value(), 'f', 2));
}
void MainWindow::report(const QString &message, bool error)
{
    const auto text = QDateTime::currentDateTime().toString("HH:mm:ss") + "   " + message;
    if (events_) {
        auto *item = new QListWidgetItem(text);
        item->setToolTip(message); item->setForeground(QColor(error ? "#ad322b" : "#30363b"));
        events_->insertItem(0, item);
        while (events_->count() > 100) delete events_->takeItem(events_->count() - 1);
    }
    logger_.event(error ? "error" : "info", message);
}
