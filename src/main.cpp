#include <QApplication>
#include <QFileInfo>
#include "MainWindow.h"

/**
 * Application entry point.
 * Creates the main window and optionally loads a file passed as command-line argument.
 */
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Qt MSG Reader");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("QtMSGReader");
    
    MainWindow window;
    window.show();
    
    // Load file if provided as command-line argument
    if (argc > 1) {
        QString filePath = QString::fromLocal8Bit(argv[1]);
        if (QFileInfo::exists(filePath)) {
            window.loadFile(filePath);
        }
    }
    
    return app.exec();
}
