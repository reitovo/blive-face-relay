#include <iostream>

#include <IXWebSocket.h>
#include <libimobiledevice/libimobiledevice.h>
#include "QApplication"
#include "ui/mainwindow.h"
#include "QFontDatabase"
#include "QFile"
#include "QDateTime"

#if TARGET_OS_MAC

#include <unistd.h>
#include <sys/sysctl.h>

int IsDebuggerPresent() {
    int junk;
    int mib[4];
    kinfo_proc info{};
    size_t size;

    // Initialize the flags so that, if sysctl fails for some bizarre
    // reason, we get a predictable result.
    info.kp_proc.p_flag = 0;

    // Initialize mib, which tells sysctl the info we want, in this case
    // we're looking for information about a specific process ID.
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();

    // Call sysctl.
    size = sizeof(info);
    junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, nullptr, 0);
    assert(junk == 0);

    // We're being debugged if the P_TRACED flag is set.
    auto present = ((info.kp_proc.p_flag & P_TRACED) != 0);
    return present;
}

#endif

#if TARGET_OS_WINDOWS
#include <client/crash_report_database.h>
#include <client/settings.h>
#include <client/crashpad_client.h>

void initializeCrashpad();
void redirectDebugOutput();
#endif

void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

int main(int argc, char *argv[]) {
    QFile log("../relay.log");
    if (log.exists() && log.size() > 1024 * 1024 * 32)
        log.remove();

#if TARGET_OS_WINDOWS
    initializeCrashpad();
    if (!IsDebuggerPresent()) {
        qInstallMessageHandler(customMessageHandler);
        QThread::create(redirectDebugOutput)->start();
    }
#elif TARGET_OS_MAC
    if (!IsDebuggerPresent()) {
        qInstallMessageHandler(customMessageHandler);
    }
#endif

    QCoreApplication::setOrganizationName("Reito");
    QCoreApplication::setOrganizationDomain("reito.fun");
    QCoreApplication::setApplicationName("FaceRelay");

    QApplication a(argc, argv);

    MainWindow w;
    w.show();

    auto ret = QApplication::exec();

    return ret;
}

void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    Q_UNUSED(context);

    QString dt = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
    QString txt = QString("[%1] ").arg(dt);

    switch (type) {
        case QtInfoMsg:
            txt += QString("{Info}     %1").arg(msg);
            break;
        case QtDebugMsg:
            txt += QString("{Debug}    %1").arg(msg);
            break;
        case QtWarningMsg:
            txt += QString("{Warning}  %1").arg(msg);
            break;
        case QtCriticalMsg:
            txt += QString("{Critical} %1").arg(msg);
            break;
        case QtFatalMsg:
            txt += QString("{Fatal}    %1").arg(msg);
            abort();
            break;
    }

    QFile outFile("../relay.log");
    outFile.open(QIODevice::WriteOnly | QIODevice::Append);

    QTextStream textStream(&outFile);
    textStream << txt << "\r\n";
}

#if TARGET_OS_WINDOWS
void initializeCrashpad() {
    using namespace crashpad;
    std::map<std::string, std::string> annotations;
    std::vector<std::string> arguments;

    //改为自己的crashpad_handler.exe 路径
    QString exePath = "./crashpad_handler.exe";

    // Register your own token at backtrace.io!
    std::string url("https://submit.backtrace.io/reito/" BACKTRACE_SUBMIT_TOKEN "/minidump");
    annotations["token"] = BACKTRACE_SUBMIT_TOKEN;
    annotations["format"] = "minidump";

    arguments.emplace_back("--no-rate-limit");
    arguments.emplace_back("--attachment=../relay.log");

    //放dump的文件夹 按需改
    base::FilePath db(QString("../crash").toStdWString());
    //crashpad_handler.exe 按需改
    base::FilePath handler(exePath.toStdWString());

    std::unique_ptr<CrashReportDatabase> database =
            crashpad::CrashReportDatabase::Initialize(db);

    if (database == nullptr || database->GetSettings() == NULL) {
        qCritical() << "CrashReportDatabase Init Error";
        return;
    }

    database->GetSettings()->SetUploadsEnabled(true);

    CrashpadClient client;
    bool ret = client.StartHandler(handler,
                                   db,
                                   db,
                                   url,
                                   annotations,
                                   arguments,
                                   true,
                                   true);
    if (ret == false) {
        return;
    }

    ret = client.WaitForHandlerStart(INFINITE);
    if (ret == false) {
        qCritical() << "CrashpadClient Start Error";
        return;
    }
}

const int MAX_DebugBuffer = 4096;

typedef struct dbwin_buffer {
    DWORD dwProcessId;
    char data[4096 - sizeof(DWORD)];
} DEBUGBUFFER, *PDEBUGBUFFER;

void redirectDebugOutput() {
    HANDLE hMapping = NULL;
    HANDLE hAckEvent = NULL;
    HANDLE hReadyEvent = NULL;
    PDEBUGBUFFER pdbBuffer = NULL;

    auto thisProcId = QCoreApplication::applicationPid();

    // 打开事件句柄
    hAckEvent = CreateEvent(NULL, FALSE, FALSE, TEXT("DBWIN_BUFFER_READY"));
    if (hAckEvent == NULL) {
        CloseHandle(hAckEvent);
        return;
    }

    hReadyEvent = CreateEvent(NULL, FALSE, FALSE, TEXT("DBWIN_DATA_READY"));
    if (hReadyEvent == NULL) {
        CloseHandle(hReadyEvent);
        return;
    }
    // 创建文件映射
    hMapping = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, MAX_DebugBuffer, TEXT("DBWIN_BUFFER"));
    if (hMapping == NULL) {
        CloseHandle(hMapping);
        return;
    }

    // 映射调试缓冲区
    pdbBuffer = (PDEBUGBUFFER) MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);

    // 循环
    while (true) {
        // 激活事件
        SetEvent(hAckEvent);
        // 等待缓冲区数据
        if (WaitForSingleObject(hReadyEvent, INFINITE) == WAIT_OBJECT_0) {
            if (pdbBuffer->dwProcessId == thisProcId) {
                // 保存信息，这就是我们想要的，有了这个信息，想打log或是输出到控制台就都可以啦
                qDebug() << QString("%1").arg(QString::fromUtf8(pdbBuffer->data));
            }
        }
    }

    // 释放
    if (pdbBuffer) {
        UnmapViewOfFile(pdbBuffer);
    }
    CloseHandle(hMapping);
    CloseHandle(hReadyEvent);
    CloseHandle(hAckEvent);
}
#endif