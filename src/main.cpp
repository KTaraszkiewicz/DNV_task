#include "mainwindow.h"
#include <QApplication>
#include <iostream>
#include <exception>

int main(int argc, char *argv[])
{
    try {
        // Create Qt application instance
        QApplication a(argc, argv);
        
        // Create and show main window
        MainWindow w;
        w.show();
        
        // Start the main event loop (handles user input, window updates, etc.)
        return a.exec();
    } 
    catch (const std::exception& ex) {
        // Handle any standard C++ exceptions
        std::cerr << "Application error: " << ex.what() << std::endl;
        return 1;
    } 
    catch (...) {
        // Handle any other unexpected errors
        std::cerr << "Unknown error occurred." << std::endl;
        return 2;
    }
}