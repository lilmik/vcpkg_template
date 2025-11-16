// src/main.cpp
#include <iostream>
#include "version.h" // 增加版本信息

#include <QCoreApplication>
#include <QScxmlStateMachine>
#include <QTimer>
#include <QDebug>


int main(int argc, char *argv[])
{
    // 增加版本信息
    if (argc == 2)
    {
        std::string arg = argv[1];
        if (arg == "--version" || arg == "-v")
        {
            VersionInfo::print();
            return 0;
        }
        else if (arg == "--version-detailed" || arg == "-V")
        {
            VersionInfo::printDetailed();
            return 0;
        }
        else if (arg == "--version-short")
        {
            std::cout << VersionInfo::version() << std::endl;
            return 0;
        }
        else if (arg == "--git-info")
        {
            std::cout << VersionInfo::gitInfo() << std::endl;
            return 0;
        }
        else if (arg == "--build-time")
        {
            std::cout << VersionInfo::buildTime() << std::endl;
            return 0;
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "Usage: " << argv[0] << " [OPTION]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -v, --version           Print version (compact format)" << std::endl;
            std::cout << "  -V, --version-detailed  Print detailed version information" << std::endl;
            std::cout << "  --version-short         Print version number only" << std::endl;
            std::cout << "  --git-info              Print Git information only" << std::endl;
            std::cout << "  --build-time            Print build timestamp only" << std::endl;
            std::cout << "  -h, --help              Print this help message" << std::endl;
            return 0;
        }
    }

    // 正常程序逻辑
    std::cout << "Starting " << APP_NAME << " " << VersionInfo::fullVersion() << std::endl;

    // 在日志中记录版本信息
    // std::clog << "Application: " << VersionInfo::fullVersion() << std::endl;
    
    
    
    QCoreApplication app(argc, argv);

    // 从当前工作目录读取 statemachine.scxml
    // 因为 CMake 已经在构建目录复制了一份
    const QString scxmlPath = QStringLiteral("statemachine.scxml");

    QScopedPointer<QScxmlStateMachine> machine(
        QScxmlStateMachine::fromFile(scxmlPath));

    if (!machine || !machine->parseErrors().isEmpty()) {
        qCritical() << "Failed to load SCXML file:" << scxmlPath;
        const auto errors = machine ? machine->parseErrors() : QList<QScxmlError>{};
        for (const auto &err : errors) {
            qCritical() << err.toString();
        }
        return 1;
    }

    // 每次状态机达到稳定状态时，打印当前激活状态
    QObject::connect(machine.data(), &QScxmlStateMachine::reachedStableState,
                     [&]() {
        qDebug() << "Active states:" << machine->activeStateNames();
    });

    machine->start();

    // 1 秒后发出 "start" 事件，切到 running
    QTimer::singleShot(1000, [&]() {
        qDebug() << "Submit event: start";
        machine->submitEvent("start");
    });

    // 2 秒后发出 "stop" 事件，切回 idle
    QTimer::singleShot(2000, [&]() {
        qDebug() << "Submit event: stop";
        machine->submitEvent("stop");
    });

    // 3 秒后退出程序
    QTimer::singleShot(3000, &app, &QCoreApplication::quit);

    return app.exec();
}
