#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName("PID Studio");
    QApplication::setOrganizationName("PIDStudioDemo");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setStyle("Fusion");
    QApplication::setFont(QFont("Segoe UI", 10));
    QCommandLineParser parser;
    parser.setApplicationDescription("Temperature regulation and experiment logging with PI control, feed-forward, CSV input and a demo mode.");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"output-dir", "Directory for session CSV logs.", "directory"});
    parser.process(application);
    MainWindow window(parser.value("output-dir"));
    window.show();
    return application.exec();
}
