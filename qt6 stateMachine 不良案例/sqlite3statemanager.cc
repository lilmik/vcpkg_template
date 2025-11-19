#include "sqlite3statemanager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTimer>

// SOCI 头文件
#include <soci/soci.h>
#include <soci/sqlite3/soci-sqlite3.h>

SQLite3StateManager::SQLite3StateManager(const QString& db_file, QObject* parent)
    : QObject(parent)
    , db_file_(db_file)
    , state_machine_(nullptr)
    , database_initialized_(false)
{
    qDebug() << "SQLite3StateManager 创建 | 打开 数据库文件:" << db_file_;
}

SQLite3StateManager::~SQLite3StateManager()
{
    stop();
    if (state_machine_) {
        state_machine_->deleteLater();
    }
}

bool SQLite3StateManager::start()
{
    // 加载状态机
    if (!state_machine_) {
        QString scxmlPath = QCoreApplication::applicationDirPath() + "/statemachine/sqlite3_init_statemachine.scxml";
        qDebug() << "scxmlPath: " << scxmlPath;

        if (!QFile::exists(scxmlPath)) {
            qCritical() << "状态机文件不存在:" << scxmlPath;
            emit errorOccurred("状态机文件不存在");
            return false;
        }

        state_machine_ = QScxmlStateMachine::fromFile(scxmlPath);
        if (!state_machine_) {
            qCritical() << "无法加载状态机文件:" << scxmlPath;
            emit errorOccurred("无法加载状态机文件");
            return false;
        }

        setupStateMachineConnections();
    }

    // 启动状态机（数据库初始化将在状态机内部触发）
    state_machine_->start();
    return true;
}

void SQLite3StateManager::stop()
{
    if (state_machine_ && isRunning()) {
        state_machine_->stop();
    }

    // 关闭数据库连接
    db_session_.reset();
    database_initialized_ = false;
}

QString SQLite3StateManager::currentState() const
{
    if (!state_machine_) {
        return "未初始化";
    }

    auto activeStates = state_machine_->activeStateNames();
    if (!activeStates.isEmpty()) {
        return activeStates.first();
    }

    return "未知状态";
}

bool SQLite3StateManager::isRunning() const
{
    return state_machine_ && state_machine_->isRunning();
}

void SQLite3StateManager::startTask()
{
    if (state_machine_ && currentState() == "idle") {
        state_machine_->submitEvent("start");
    }
}

void SQLite3StateManager::stopTask()
{
    if (state_machine_ && currentState() == "running") {
        state_machine_->submitEvent("stop");
    }
}

bool SQLite3StateManager::checkDatabaseExists()
{
    return QFile::exists(db_file_);
}

bool SQLite3StateManager::initializeDatabase()
{
    if (database_initialized_) {
        qDebug() << "数据库已经连接过了";
        return true;
    }

    try {
        qDebug() << "正在连接数据库:" << db_file_;

        // 创建数据库会话
        db_session_ = std::make_unique<soci::session>(soci::sqlite3, db_file_.toStdString());

        // 创建必要的表
        std::string createTableSQL = "CREATE TABLE IF NOT EXISTS app_state ("
                                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                     "state_name TEXT NOT NULL, "
                                     "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
                                     ");";

        *db_session_ << createTableSQL;

        database_initialized_ = true;
        qDebug() << "数据库连接成功";
        return true;

    } catch (const std::exception& e) {
        qCritical() << "数据库连接失败:" << e.what();
        emit errorOccurred(QString("数据库连接失败: %1").arg(e.what()));
        return false;
    }
}

void SQLite3StateManager::setupStateMachineConnections()
{
    if (!state_machine_)
        return;

    // 连接状态机的信号来调试
    connect(state_machine_, &QScxmlStateMachine::runningChanged, [this](bool running) {
        qDebug() << "[状态机] 运行状态改变:" << running;
        if (running) {
            qDebug() << "[状态机] 状态机已启动，当前状态:" << this->currentState();
        } else {
            qDebug() << "[状态机] 状态机已停止";
        }
    });

    connect(state_machine_, &QScxmlStateMachine::finished, []() {
        qDebug() << "[状态机] 状态机已结束";
    });

    // 连接状态变化信号
    connect(state_machine_, &QScxmlStateMachine::reachedStableState,
        this, [this]() {
            QString state = this->currentState();
            qDebug() << "[状态机] 状态稳定:" << state;
            emit stateChanged(state);

            // 记录状态到数据库
            if (database_initialized_ && db_session_) {
                try {
                    std::string state_escaped = state.toStdString();
                    std::replace(state_escaped.begin(), state_escaped.end(), '\'', ' ');
                    std::string insertSQL = "INSERT INTO app_state (state_name) VALUES ('" + state_escaped + "')";
                    *db_session_ << insertSQL;
                } catch (const std::exception& e) {
                    qWarning() << "记录状态失败:" << e.what();
                }
            }

            // 如果进入 init 状态，检查数据库并相应处理
            if (state == "init") {
                qDebug() << "[状态机] 在 init 状态，检查数据库...";

                // 先检查数据库是否已经存在
                if (checkDatabaseExists()) {
                    qDebug() << "[状态机] 数据库已存在，直接打开...";
                    bool success = initializeDatabase();
                    if (success) {
                        qDebug() << "[状态机] 数据库打开成功，发送 db.exists 事件";
                        // 使用单次定时器确保状态机稳定后再发送事件
                        QTimer::singleShot(10, this, [this]() {
                            qDebug() << "[状态机] 正在提交 db.exists 事件";
                            state_machine_->submitEvent("db.exists");
                            qDebug() << "[状态机] 事件已提交，当前状态:" << this->currentState();

                            // 再次检查状态，确认转换是否发生
                            QTimer::singleShot(100, [this]() {
                                qDebug() << "[状态机] 100ms后状态:" << this->currentState();
                            });
                        });
                        emit databaseInitialized(true);
                    } else {
                        qDebug() << "[状态机] 数据库打开失败，发送 db.create.fail 事件";
                        QTimer::singleShot(10, this, [this]() {
                            state_machine_->submitEvent("db.create.fail");
                        });
                        emit databaseInitialized(false);
                    }
                } else {
                    qDebug() << "[状态机] 数据库不存在，开始创建和初始化...";
                    bool success = initializeDatabase();
                    if (success) {
                        qDebug() << "[状态机] 数据库创建成功，发送 db.create.success 事件";
                        // 使用单次定时器确保状态机稳定后再发送事件
                        QTimer::singleShot(10, this, [this]() {
                            qDebug() << "[状态机] 正在提交 db.create.success 事件";
                            state_machine_->submitEvent("db.create.success");
                            qDebug() << "[状态机] 事件已提交，当前状态:" << this->currentState();

                            // 再次检查状态，确认转换是否发生
                            QTimer::singleShot(100, [this]() {
                                qDebug() << "[状态机] 100ms后状态:" << this->currentState();
                            });
                        });
                        emit databaseInitialized(true);
                    } else {
                        qDebug() << "[状态机] 数据库创建失败，发送 db.create.fail 事件";
                        QTimer::singleShot(10, this, [this]() {
                            state_machine_->submitEvent("db.create.fail");
                        });
                        emit databaseInitialized(false);
                    }
                }
            }
        });
}
