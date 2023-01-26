#include <iostream>

#include <IXWebSocket.h>
#include <libimobiledevice/libimobiledevice.h>
#include "QApplication"
#include "ui/mainwindow.h"
#include "QFontDatabase"

int main(int argc, char *argv[]) {

    QCoreApplication::setOrganizationName("Reito");
    QCoreApplication::setOrganizationDomain("reito.fun");
    QCoreApplication::setApplicationName("FaceRelay");

    QApplication a(argc, argv);

    MainWindow w;
    w.show();

    auto ret = QApplication::exec();

    return ret;
}
