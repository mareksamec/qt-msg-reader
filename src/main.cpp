#include <QApplication>
#include <QFileInfo>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Qt MSG Reader");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("QtMSGReader");
    
    MainWindow window;
    window.show();
    
    if (argc > 1) {
        QString filePath = QString::fromLocal8Bit(argv[1]);
        if (QFileInfo::exists(filePath)) {
            window.loadFile(filePath);
        }
    }
    
    return app.exec();
}
