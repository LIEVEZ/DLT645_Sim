#include "Views/MainWindow/mainwindow.h"
#include "Utils/Logger.h"

#include <QApplication>
#include <QStandardPaths>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 配置日志系统
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(logDir);
    QString logFilePath = logDir + "/dlt645_sim.log";

    Logger *logger = Logger::instance();
    logger->setLogFile(logFilePath);
    logger->setLogRotation(10 * 1024 * 1024, 5); // 10MB per file, keep 5 backups
    logger->setLogLevel(LogLevel::Info);          // 生产环境使用 Info 级别
    logger->setConsoleOutputEnabled(true);
    logger->setFileOutputEnabled(true);

    LOG_INFO(QString("Application started, log file: %1").arg(logFilePath));

    MainWindow w;
    w.show();
    return a.exec();
}
