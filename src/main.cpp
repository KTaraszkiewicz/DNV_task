#include "mainwindow.h"
#include <QApplication>
#include <iostream>
#include <exception>

int main(int argc, char *argv[])
{
    try {
        QApplication a(argc, argv);
        MainWindow w;
        w.show();
        return a.exec();
    } catch (const std::exception& ex) {
        std::cerr << "Application error: " << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred." << std::endl;
        return 2;
    }
}
