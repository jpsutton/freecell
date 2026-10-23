#include <QApplication>
#include <QIcon>

#include "mainwindow.h"
#ifdef FREECELL_TOUCH_UI
#include "touchui.h"
#endif

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("freecell");
    QCoreApplication::setApplicationName("FreeCell");
    QCoreApplication::setApplicationVersion(FREECELL_VERSION);
    QGuiApplication::setDesktopFileName("io.github.jpsutton.freecell");
    QApplication::setWindowIcon(QIcon(":/art/icon.png"));
#ifdef FREECELL_TOUCH_UI
    applyTouchStyle(app);
#endif
    MainWindow w;
    w.show();
    return app.exec();
}
