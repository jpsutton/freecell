#include <QApplication>
#include <QIcon>

#include "mainwindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("freecell");
    QCoreApplication::setApplicationName("FreeCell");
    QCoreApplication::setApplicationVersion(FREECELL_VERSION);
    QGuiApplication::setDesktopFileName("io.github.jpsutton.freecell");
    QApplication::setWindowIcon(QIcon(":/art/icon.png"));
    MainWindow w;
    w.show();
    return app.exec();
}
