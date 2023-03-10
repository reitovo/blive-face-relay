//
// Created by reito on 2023/1/21.
//

// You may need to build the project (run Qt uic code generator) to get "ui_MainWindow.h" resolved

#include "mainwindow.h"
#include "ui_MainWindow.h"
#include "QDesktopServices"
#include "IXNetSystem.h"
#include "libimobiledevice/lockdown.h"
#include "QMessageBox"
#include <QSettings>
#include <stdexcept>

#define IDEVICE_THROWONERROR(x) { \
    auto _err = x;                    \
    if (_err != 0) {  \
        qDebug() << "idevice api error" << _err << __FILE__ << __LINE__ ; \
        emit onShowError(QString(QCoreApplication::translate("MainWindow", "USB连接设备失败 %1\n请检查iOS弹幕姬是否启动").arg(_err)));    \
        throw std::logic_error("idevice api error");                 \
    }                                 \
}

static void idevice_event_cb(const idevice_event_t *event, void *user_data) {
    auto ins = static_cast<MainWindow *>(user_data);
    ins->onIDeviceEvent(event);
}

MainWindow::MainWindow(QWidget *parent) :
        QMainWindow(parent), ui(new Ui::MainWindow) {
    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);

    ui->setupUi(this);

    idevice_event_subscribe(idevice_event_cb, this);

    connect(&statusRefresh, &QTimer::timeout, this, &MainWindow::refreshStatus);
    statusRefresh.setInterval(100);
    statusRefresh.start();

    ix::initNetSystem();
    ws = std::make_unique<ix::WebSocket>();
    ws->setOnMessageCallback([=](const ix::WebSocketMessagePtr &msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            //qDebug() << "received message: " << QString::fromLocal8Bit(msg->str);
            if (this->usbRun && conn != nullptr && !closing) {
                uint32_t sent;
                auto s = msg->str;
                usbLock.lock();
                char zero = 0;
                s.append(&zero, 1);
                if (conn != nullptr)
                    idevice_connection_send(*conn, s.data(), s.size(), &sent);
                usbLock.unlock();
                //qDebug() << "idevice sent" << sent << s.size();
                if (sent != s.size()) {
                    qWarning() << "idevice sent != s.size()";
                }
            }
        } else if (msg->type == ix::WebSocketMessageType::Open) {
            qDebug() << "Connection established";
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            // Maybe SSL is not configured properly
            qDebug() << "Connection error: " << QString::fromLocal8Bit(msg->errorInfo.reason);
        }
    });
    QSettings s;
    auto port = s.value("port", 8001).toInt();
    auto url = QString("ws://127.0.0.1:%1").arg(port);
    qDebug() << "restore url" << url;

    ui->port->setText(QString("%1").arg(port));

    ws->setUrl(url.toStdString());
    ws->enableAutomaticReconnection();
    ws->start();

    connect(ui->connectVts, &QPushButton::clicked, this, &MainWindow::connectToVts);
    connect(ui->connectUsb, &QPushButton::clicked, this, &MainWindow::connectToUsb);
    connect(ui->showHelp, &QPushButton::clicked, this, [=]() {
        QDesktopServices::openUrl(QUrl("https://www.wolai.com/reito/rcFXnosdGZNULYZV9TyTdj"));
    });
    connect(this, &MainWindow::onShowError, this, &MainWindow::showError);
    connect(ui->idevices, &QComboBox::currentIndexChanged, this, &MainWindow::selectDeviceChanged);
}

MainWindow::~MainWindow() {
    closing = true;

    statusRefresh.stop();
    usbStop();

    ws->close();
    ws.reset();

    idevice_event_unsubscribe();
    devices.clear();
    connectedDevices.clear();

    delete ui;
}

void MainWindow::connectToVts() {
    auto port = ui->port->text().toInt();
    ws->stop();
    ws->setUrl(QString("ws://127.0.0.1:%1").arg(port).toStdString());
    auto url = QString::fromLocal8Bit(ws->getUrl());
    qDebug() << "set url" << url;
    ws->start();

    QSettings s;
    s.setValue("port", port);
    s.sync();
}

void MainWindow::connectToUsb() {
    auto udid = ui->idevices->currentData().toString();
    qDebug() << "device selected" << udid;

    auto dev = devices.value(udid, nullptr);
    if (dev == nullptr) {
        qDebug() << "unknown device";
    } else {
        qDebug() << "current device" << dev->name;
        usbStart(dev);
    }
}

void MainWindow::refreshStatus() {
    ui->vtsStatus->setText(ws->getReadyState() == ix::ReadyState::Open ? tr("已连接") : tr("未连接"));
    ui->usbStatus->setText(usbRun ? tr("已连接") : tr("未连接"));
}

void MainWindow::onIDeviceEvent(const idevice_event_t *event) {
    if (closing)
        return;

    switch (event->event) {
        case IDEVICE_DEVICE_ADD: {
            qDebug() << "idevice event add" << event->udid;

            auto dev = devices.value(event->udid, nullptr);
            if (dev == nullptr) {
                dev = std::make_shared<IDevice>();

                idevice_t device;
                lockdownd_client_t lock;
                char *name;
                IDEVICE_THROWONERROR(idevice_new(&device, event->udid))
                IDEVICE_THROWONERROR(lockdownd_client_new_with_handshake(device, &lock, "BliveAssist.FaceRelay"))
                IDEVICE_THROWONERROR(lockdownd_get_device_name(lock, &name))

                qDebug() << "idevice get name" << event->udid << name;

                dev->udid = event->udid;
                dev->name = name;
                dev->connType = event->conn_type;

                idevice_free(device);
                lockdownd_client_free(lock);
                free(name);

                devices.insert(event->udid, dev);
            }

            onIDeviceAdd(dev);
            break;
        }
        case IDEVICE_DEVICE_REMOVE: {
            qDebug() << "idevice event remove" << event->udid;
            auto dev = devices.value(event->udid, nullptr);
            if (dev == nullptr) {
                qDebug() << "unknown device";
            } else {
                onIDeviceRemove(dev);
            }
            break;
        }
        case IDEVICE_DEVICE_PAIRED: {
            qDebug() << "idevice event paired" << event->udid;
            break;
        }
    }
}

void MainWindow::showError(const QString &err) {
    QMessageBox::critical(this, tr("错误"), err);
}

void MainWindow::onIDeviceAdd(const std::shared_ptr<IDevice> &dev) {
    connectedDevices.push_back(dev);
    refreshDevices();
}

void MainWindow::onIDeviceRemove(const std::shared_ptr<IDevice> &dev) {
    connectedDevices.removeAll(dev);
    refreshDevices();
}

void MainWindow::selectDeviceChanged(int index) {
    usbStop();
}

void MainWindow::refreshDevices() {
    ui->idevices->clear();
    for (auto &dev: connectedDevices) {
        ui->idevices->addItem(dev->name, dev->udid);
    }
}

void MainWindow::usbStop() {
    if (usbRun) {
        usbLock.lock();
        usbRun = false;
        if (usbReceive != nullptr && !usbReceive->isFinished() && !usbReceive->wait(500)) {
            qWarning() << "uneasy to exit usb receive";
            usbReceive->terminate();
            usbReceive->wait(500);
            usbReceive = nullptr;
        }

        if (conn != nullptr) {
            idevice_disconnect(*conn);
            conn.reset();
        }

        if (usb != nullptr) {
            idevice_free(*usb);
            usb.reset();
        }
        usbLock.unlock();
    }
}

void MainWindow::usbStart(const std::shared_ptr<IDevice> &dev) {
    usbStop();

    usbRun = true;
    try {
        usbLock.lock();
        usb = std::make_unique<idevice_t>();
        conn = std::make_unique<idevice_connection_t>();

        IDEVICE_THROWONERROR(idevice_new(usb.get(), dev->udid.toStdString().c_str()))
        IDEVICE_THROWONERROR(idevice_connect(*usb, 14524, conn.get()));
        usbReceive = std::unique_ptr<QThread>(QThread::create([=]() {
            char buf[4096];
            uint32_t bufLen;

            while (this->usbRun && !this->closing) {
                idevice_connection_receive_timeout(*conn, buf, 4096, &bufLen, 50);
                if (bufLen != 0) {
                    //qDebug() << "idevice recv" << bufLen;
                    auto start = usbBuf.size();
                    usbBuf.insert(usbBuf.end(), buf, buf + bufLen);
                    for (auto i = start; i < usbBuf.size(); ++i) {
                        if (usbBuf[i] == 0) {
                            std::string s((const char *) usbBuf.data(), i);
                            usbBuf.erase(usbBuf.begin(), usbBuf.begin() + i + 1);
                            if (!closing)
                                ws->sendUtf8Text(s);
                            i = 0;
                        }
                    }
                }
            }
        }));
        usbReceive->start();

        usbLock.unlock();
    } catch (std::exception &ex) {
        usbLock.unlock();
        usbStop();
    }
}
