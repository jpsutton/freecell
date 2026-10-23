#include <QApplication>

#include "mainwindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("freecell");
    QCoreApplication::setApplicationName("FreeCell");
    MainWindow w;
    w.show();
    return app.exec();
}
