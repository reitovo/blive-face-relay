//
// Created by reito on 2023/1/21.
//

#ifndef FACERELAY_MAINWINDOW_H
#define FACERELAY_MAINWINDOW_H

#include "IXWebSocket.h"
#include "libimobiledevice/libimobiledevice.h"
#include "QTimer"
#include "QThread"
#include "QMutex"
#include <QMainWindow>
#include <streambuf>

struct IDevice {
public:
    QString udid;
    QString name;
    idevice_connection_type connType;
};

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
Q_OBJECT
    std::unique_ptr<ix::WebSocket> ws;
    QTimer statusRefresh;
    QMap<QString, std::shared_ptr<IDevice>> devices;
    QList<std::shared_ptr<IDevice>> connectedDevices;

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void onIDeviceEvent(const idevice_event_t *event);
    void onIDeviceAdd(const std::shared_ptr<IDevice>& dev);
    void onIDeviceRemove(const std::shared_ptr<IDevice>& dev);

signals:
    void onShowError(QString err);

private slots:

    void showError(const QString &err);
    void connectToVts();
    void connectToUsb();
    void refreshStatus();
    void selectDeviceChanged(int index);

private:
    Ui::MainWindow *ui;

    void refreshDevices();

    QMutex usbLock;
    std::unique_ptr<idevice_t> usb;
    std::unique_ptr<idevice_connection_t> conn;
    std::unique_ptr<QThread> usbReceive;
    std::vector<uint8_t> usbBuf;
    std::atomic_bool usbRun;
    void usbStop();
    void usbStart(const std::shared_ptr<IDevice> &dev);
};

#endif //FACERELAY_MAINWINDOW_H
