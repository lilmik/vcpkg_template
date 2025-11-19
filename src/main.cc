// src/main.cpp
#include "version.h" // 增加版本信息
#include <iostream>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QScxmlStateMachine>
#include <QTextStream>
#include <QTimer>

#include <drogon/drogon.h>
#include <grpc/grpc.h>

#include "sqlite3statemanager.h"

// 模拟真实的数据库操作
void performDatabaseOperations(SQLite3StateManager& stateManager)
{
    qDebug() << "[数据库] 执行查询操作...";
    // 这里应该是真实的数据库查询

    qDebug() << "[数据库] 执行更新操作...";
    // 这里应该是真实的数据库更新

    qDebug() << "[数据库] 执行插入操作...";
    // 这里应该是真实的数据库插入

    // 操作完成后，根据业务逻辑决定下一步
    // 可能是自动停止，也可能是等待下一个操作

    // 示例：5秒后完成操作（模拟实际的操作时间）
    QTimer::singleShot(5000, [&stateManager]() {
        qDebug() << "[数据库] 操作完成";
        stateManager.stopTask();
    });
}

int main(int argc, char* argv[])
{
    // 增加版本信息
    if (argc == 2) {
        std::string arg = argv[1];
        if (arg == "--version" || arg == "-v") {
            VersionInfo::print();
            return 0;
        } else if (arg == "--version-detailed" || arg == "-V") {
            VersionInfo::printDetailed();
            return 0;
        } else if (arg == "--version-short") {
            std::cout << VersionInfo::version() << std::endl;
            return 0;
        } else if (arg == "--git-info") {
            std::cout << VersionInfo::gitInfo() << std::endl;
            return 0;
        } else if (arg == "--build-time") {
            std::cout << VersionInfo::buildTime() << std::endl;
            return 0;
        } else if (arg == "--help" || arg == "-h") {
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

    qDebug() << "应用程序启动...";
    qDebug() << "当前工作目录:" << QDir::currentPath();
    qDebug() << "应用程序目录:" << QCoreApplication::applicationDirPath();

    // drogon gRPC Version
    QString drogonVersionQStr = QString::fromUtf8(drogon::getVersion());
    qDebug() << "drogon Version：" << drogonVersionQStr;

    QString grpcVersionQStr = QString::fromUtf8(grpc_version_string());
    qDebug() << "gRPC Version：" << grpcVersionQStr;

    // // 从当前工作目录读取 statemachine.scxml
    // const QString scxmlPath = QStringLiteral("statemachine.scxml");

    // QScopedPointer<QScxmlStateMachine> machine(
    //     QScxmlStateMachine::fromFile(scxmlPath));

    // if (!machine || !machine->parseErrors().isEmpty()) {
    //     qCritical() << "Failed to load SCXML file:" << scxmlPath;
    //     const auto errors = machine ? machine->parseErrors() : QList<QScxmlError> {};
    //     for (const auto& err : errors) {
    //         qCritical() << err.toString();
    //     }
    //     return 1;
    // }

    // // 每次状态机达到稳定状态时，打印当前激活状态
    // QObject::connect(machine.data(), &QScxmlStateMachine::reachedStableState,
    //     [&]() {
    //         qDebug() << "Active states:" << machine->activeStateNames();
    //     });

    // machine->start();

    // // 1 秒后发出 "start" 事件，切到 running
    // QTimer::singleShot(1000, [&]() {
    //     qDebug() << "Submit event: start";
    //     machine->submitEvent("start");
    // });

    // // 2 秒后发出 "stop" 事件，切回 idle
    // QTimer::singleShot(2000, [&]() {
    //     qDebug() << "Submit event: stop";
    //     machine->submitEvent("stop");
    // });

    SQLite3StateManager stateManager("qt_app_database.sqlite");

    // 连接状态变化信号
    QObject::connect(&stateManager, &SQLite3StateManager::stateChanged,
        [](const QString& newState) {
            qDebug() << "[应用] 状态改变 ->" << newState;
        });

    // 连接数据库初始化信号
    QObject::connect(&stateManager, &SQLite3StateManager::databaseInitialized,
        [](bool success) {
            qDebug() << "[应用] 数据库初始化:" << (success ? "成功" : "失败");
        });

    // 启动状态机
    qDebug() << "[应用] 正在启动状态机...";
    if (!stateManager.start()) {
        qCritical() << "[应用] 启动状态机失败";
        return -1;
    }

    // 模拟真实的任务序列
    QObject::connect(&stateManager, &SQLite3StateManager::stateChanged,
        [&stateManager](const QString& state) {
            if (state == "idle") {
                qDebug() << "[系统] 系统就绪，等待用户操作";
                // 在实际应用中，这里会等待用户输入或其他触发条件
                // 而不是自动开始任务
            } else if (state == "running") {
                qDebug() << "[任务] 开始执行数据库操作...";

                // 模拟实际的数据库操作（CRUD）
                // 这里应该是真实的业务逻辑，而不是定时器
                performDatabaseOperations(stateManager);
            }
        });

    // 示例：模拟用户操作序列
    QTimer::singleShot(2000, [&stateManager]() {
        qDebug() << "[用户] 用户点击开始任务";
        stateManager.startTask();
    });

    QTimer::singleShot(8000, [&stateManager]() {
        qDebug() << "[用户] 用户手动停止任务";
        stateManager.stopTask();
    });

    QTimer::singleShot(12000, [&stateManager]() {
        qDebug() << "[用户] 用户再次开始任务";
        stateManager.startTask();
    });

    // 优雅关闭
    auto gracefulShutdown = [&stateManager]() {
        QString currentState = stateManager.currentState();
        qDebug() << "[关闭] 开始关闭流程，当前状态:" << currentState;

        if (currentState == "idle") {
            qDebug() << "[关闭] 系统空闲，立即关闭";
            stateManager.stateMachine()->submitEvent("shutdown");
        } else if (currentState == "running") {
            qDebug() << "[关闭] 等待任务完成后再关闭";
            // 在实际应用中，这里应该通知任务尽快完成
            // 然后等待任务完成信号
            stateManager.stopTask();
        }
    };

    // 20秒后优雅关闭
    QTimer::singleShot(20000, gracefulShutdown);

    // 安全退出计时器
    QTimer::singleShot(30000, [&app]() {
        qDebug() << "[超时] 安全退出计时器触发";
        app.quit();
    });

    return app.exec();
}
