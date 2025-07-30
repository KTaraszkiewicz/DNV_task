#include "mainwindow.h"
#include <QApplication>
#include <iostream>
#include <exception>
#include <QDebug>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QDir>

int main(int argc, char *argv[])
{
    try {
        QApplication a(argc, argv);
        
        // Enable all Qt logging and redirect to console
        QLoggingCategory::setFilterRules("*=true");
        
        std::cout << "Application starting..." << std::endl;
        qDebug() << "Application starting...";
        
        // Add application about to quit handler
        QObject::connect(&a, &QApplication::aboutToQuit, []() {
            std::cout << "Application about to quit..." << std::endl;
            qDebug() << "Application about to quit...";
        });
        
        MainWindow w;
        w.show();
        
        std::cout << "Entering application event loop..." << std::endl;
        qDebug() << "Entering application event loop...";
        int result = a.exec();
        std::cout << "Application event loop exited with code: " << result << std::endl;
        qDebug() << "Application event loop exited with code:" << result;
        return result;
    } catch (const std::exception& ex) {
        std::cerr << "Application error: " << ex.what() << std::endl;
        qCritical() << "Application error:" << ex.what();
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred." << std::endl;
        qCritical() << "Unknown error occurred.";
        return 2;
    }
}
