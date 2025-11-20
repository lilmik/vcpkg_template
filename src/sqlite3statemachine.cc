// sqlite3statemachine.cpp
#include "sqlite3statemachine.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <qfileinfo.h>
#include <qjsonarray.h>
#include <soci/sqlite3/soci-sqlite3.h>

SQLite3StateMachine::SQLite3StateMachine(const QString& dbFile, QObject* parent)
    : QObject(parent)
    , m_dbFile(dbFile)
    , m_currentRequest("")
{
}

SQLite3StateMachine::~SQLite3StateMachine()
{
    shutdown();
}

// 类型转换实现
std::string SQLite3StateMachine::qstringToString(const QString& qstr) const
{
    return qstr.toStdString();
}

QString SQLite3StateMachine::stringToQString(const std::string& str) const
{
    return QString::fromStdString(str);
}

std::map<std::string, std::string> SQLite3StateMachine::qvariantMapToStringMap(const QVariantMap& qmap) const
{
    std::map<std::string, std::string> result;
    for (auto it = qmap.begin(); it != qmap.end(); ++it) {
        result[it.key().toStdString()] = it.value().toString().toStdString();
    }
    return result;
}

bool SQLite3StateMachine::initialize()
{
    if (m_stateMachine) {
        return true;
    }

    QString scxmlPath = QCoreApplication::applicationDirPath() + "/statemachine/sqlite3_init_statemachine.scxml";
    if (!QFile::exists(scxmlPath)) {
        qCritical() << "状态机文件不存在:" << scxmlPath;
        return false;
    }

    qDebug() << "加载状态机文件:" << scxmlPath;

    // 读取文件内容进行诊断
    QFile file(scxmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "无法打开状态机文件:" << file.errorString();
        return false;
    }

    QString scxmlContent = file.readAll();
    file.close();

    qDebug() << "状态机文件大小:" << scxmlContent.size() << "字节";

    // 检查文件内容
    if (scxmlContent.isEmpty()) {
        qCritical() << "状态机文件为空";
        return false;
    }

    // 尝试加载状态机
    m_stateMachine = QScxmlStateMachine::fromFile(scxmlPath);
    if (!m_stateMachine) {
        qCritical() << "无法加载状态机文件";
        return false;
    }

    // 检查状态机是否真的有错误
    if (!m_stateMachine->parseErrors().isEmpty()) {
        qCritical() << "状态机解析错误:";
        for (const QScxmlError& error : m_stateMachine->parseErrors()) {
            qCritical() << "  - 行" << error.line() << ", 列" << error.column() << ":" << error.description();
        }
        return false;
    }

    setupConnections();
    qDebug() << "状态机加载成功，状态机名称:" << m_stateMachine->name();
    return true;
}

void SQLite3StateMachine::shutdown()
{
    if (m_stateMachine && m_stateMachine->isRunning()) {
        m_stateMachine->stop();
    }

    clearQueue();
    disconnectDatabase();
}

QString SQLite3StateMachine::currentState() const
{
    if (!m_stateMachine) {
        return "uninitialized";
    }

    auto activeStates = m_stateMachine->activeStateNames();
    if (!activeStates.isEmpty()) {
        return activeStates.first();
    }

    return "unknown";
}

bool SQLite3StateMachine::isRunning() const
{
    return m_stateMachine && m_stateMachine->isRunning();
}

bool SQLite3StateMachine::isConnected() const
{
    QString state = currentState();
    return state == "idle" || state == "running";
}

soci::session* SQLite3StateMachine::getSession() const
{
    return m_dbSession.get();
}

int SQLite3StateMachine::queueSize() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_operationQueue.size();
}

void SQLite3StateMachine::clearQueue()
{
    QMutexLocker locker(&m_queueMutex);
    m_operationQueue.clear();
    emit queueSizeChanged(0);
}

QString SQLite3StateMachine::currentOperationId() const
{
    return m_currentOperationId;
}

void SQLite3StateMachine::startConnection()
{
    if (m_stateMachine && !isRunning()) {
        m_stateMachine->start();
    }
}

void SQLite3StateMachine::stopConnection()
{
    if (m_stateMachine && isRunning()) {
        m_stateMachine->submitEvent("shutdown");
    }
}

QString SQLite3StateMachine::executeQuery(const QString& query, const std::map<std::string, std::string>& params)
{
    OperationRequest request("query");
    request.setStringParam("query", qstringToString(query));

    // 添加其他参数
    for (const auto& param : params) {
        request.setStringParam(param.first, param.second);
    }

    addToQueue(request);
    return QString::fromStdString(request.id);
}

bool SQLite3StateMachine::executeImmediateQuery(const QString& query, const std::map<std::string, std::string>& params)
{
    if (!isConnected()) {
        return false;
    }

    try {
        // 直接执行查询逻辑
        // 这里需要根据query和params执行具体的SQL
        Q_UNUSED(query)
        Q_UNUSED(params)
        return true;
    } catch (const std::exception& e) {
        qCritical() << "立即查询执行失败:" << e.what();
        return false;
    }
}

void SQLite3StateMachine::handleStateMachineEvent(const QString& event, const QVariant& data)
{
    qDebug() << "处理状态机事件:" << event;

    if (event == "check.database") {
        bool success = connectToDatabase();
        if (success) {
            m_stateMachine->submitEvent("db.exists");
        } else {
            m_stateMachine->submitEvent("db.create.fail");
        }
    } else if (event == "start.actual.task") {
        // 开始处理队列中的操作
        processNextOperation();
    } else if (event == "stop.actual.task") {
        // 停止当前操作
        m_processingOperation = false;
        m_currentOperationId.clear();
    } else if (event == "record.state") {
        // data 里传的是当前状态，比如 "idle" / "running" / "error"
        QString state = data.toString();
        if (m_dbSession) {
            try {
                std::string state_escaped = qstringToString(state);

                // 只写入 app_state 表，记录状态变化
                std::string insertSQL = "INSERT INTO app_state (state_name) VALUES (?)";
                *m_dbSession << insertSQL, soci::use(state_escaped);

                qDebug() << "状态记录成功:" << state;
            } catch (const std::exception& e) {
                qWarning() << "记录状态失败:" << e.what();
            }
        }
    } else if (event == "reset.retry.count") {
        qDebug() << "重置重试计数";
        // 这里可以添加重置重试计数的具体逻辑
        // 例如：m_retryCount = 0;
    } else if (event == "handle.error") {
        QString errorMsg = data.toString();
        qCritical() << "处理错误:" << errorMsg;
        handleError(errorMsg); // 传递错误信息
    } else if (event == "db.create.success" || event == "db.exists") {
        // 数据库连接成功的事件处理
        qDebug() << "数据库连接成功，状态:" << event;
        emit connectionEstablished();
    } else if (event == "db.create.fail") {
        // 数据库连接失败的事件处理
        qCritical() << "数据库连接失败";
        emit errorOccurred("数据库连接失败");
    } else {
        // 处理其他未明确处理的事件
        qDebug() << "未处理的状态机事件:" << event;
        if (data.isValid()) {
            qDebug() << "事件数据:" << data;
        }
    }
}

void SQLite3StateMachine::processNextOperation()
{
    if (m_processingOperation) {
        return; // 已经在处理操作
    }

    OperationRequest request = dequeue();
    if (request.id.empty()) {
        // 队列为空，停止任务
        m_stateMachine->submitEvent("stop");
        return;
    }

    m_processingOperation = true;
    m_currentOperationId = QString::fromStdString(request.id);
    m_currentRequest = request;

    emit operationStarted(m_currentOperationId);

    if (request.isQueryType()) {
        handleQueryExecution(request);
    }
    // 可以添加其他操作类型的处理
}

void SQLite3StateMachine::setupConnections()
{
    if (!m_stateMachine)
        return;

    // 连接状态变化信号
    connect(m_stateMachine, &QScxmlStateMachine::reachedStableState, this, [this]() {
        QString state = this->currentState();
        qDebug() << "状态机进入稳定状态:" << state;
        emit stateChanged(state);

        if (state == "idle" || state == "running") {
            emit connectionEstablished();
        } else if (state == "error" || state == "final") {
            emit connectionLost();
        }

        // 如果在running状态且没有在处理操作，尝试处理下一个
        if (state == "running" && !m_processingOperation) {
            QTimer::singleShot(0, this, &SQLite3StateMachine::processNextOperation);
        }
    });

    // 连接状态机运行状态变化信号
    connect(m_stateMachine, &QScxmlStateMachine::runningChanged, this, [](bool running) {
        if (running) {
            qDebug() << "数据库连接状态机已启动";
        } else {
            qDebug() << "数据库连接状态机已停止";
        }
    });

    // 连接具体的SCXML事件
    m_stateMachine->connectToEvent("check.database", this, [this](const QScxmlEvent&) {
        handleStateMachineEvent("check.database", QVariant());
    });

    m_stateMachine->connectToEvent("start.actual.task", this, [this](const QScxmlEvent&) {
        handleStateMachineEvent("start.actual.task", QVariant());
    });

    m_stateMachine->connectToEvent("stop.actual.task", this, [this](const QScxmlEvent&) {
        handleStateMachineEvent("stop.actual.task", QVariant());
    });

    m_stateMachine->connectToEvent("record.state", this, [this](const QScxmlEvent& event) {
        handleStateMachineEvent("record.state", event.data());
    });

    m_stateMachine->connectToEvent("reset.retry.count", this, [this](const QScxmlEvent&) {
        handleStateMachineEvent("reset.retry.count", QVariant());
    });

    m_stateMachine->connectToEvent("handle.error", this, [this](const QScxmlEvent& event) {
        handleStateMachineEvent("handle.error", event.data());
    });

    // 连接错误事件
    m_stateMachine->connectToEvent("error.occurred", this, [this](const QScxmlEvent& event) {
        QString errorMsg = event.data().toString();
        qCritical() << "发生错误:" << errorMsg;
        emit errorOccurred(errorMsg);
    });

    m_stateMachine->connectToEvent("task.error", this, [this](const QScxmlEvent& event) {
        QString errorMsg = event.data().toString();
        qCritical() << "任务错误:" << errorMsg;
        emit errorOccurred(errorMsg);

        // 任务错误时，标记当前操作完成
        if (!m_currentOperationId.isEmpty()) {
            emit operationCompleted(m_currentOperationId, false, errorMsg);
            m_processingOperation = false;
            m_currentOperationId.clear();

            // 继续处理下一个操作
            QTimer::singleShot(0, this, &SQLite3StateMachine::processNextOperation);
        }
    });
}

bool SQLite3StateMachine::connectToDatabase()
{
    try {
        if (m_dbSession) {
            return true; // 已经连接
        }

        bool dbExists = QFile::exists(m_dbFile);
        qDebug() << "数据库文件路径:" << QFileInfo(m_dbFile).absoluteFilePath();

        m_dbSession = std::make_unique<soci::session>(soci::sqlite3, qstringToString(m_dbFile));

        // 无论数据库是否存在，都执行创建表语句
        std::vector<std::string> createTableStatements = {
            R"(CREATE TABLE IF NOT EXISTS app_state (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                state_name TEXT NOT NULL,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
            ))",

            R"(CREATE TABLE IF NOT EXISTS operation_queue (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                operation_id TEXT UNIQUE NOT NULL,
                operation_type TEXT NOT NULL,
                parameters TEXT,
                status TEXT DEFAULT 'pending',
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                started_at DATETIME,
                completed_at DATETIME
            ))",

            // 为 operation_queue 添加索引以提高查询性能
            R"(CREATE INDEX IF NOT EXISTS idx_operation_queue_status ON operation_queue(status))",
            R"(CREATE INDEX IF NOT EXISTS idx_operation_queue_created ON operation_queue(created_at))",

            R"(CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                email TEXT UNIQUE NOT NULL,
                age INTEGER CHECK (age >= 0 AND age <= 150),
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            ))",

            // 为 users 表添加索引
            R"(CREATE INDEX IF NOT EXISTS idx_users_email ON users(email))",

            R"(CREATE TABLE IF NOT EXISTS products (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                price REAL NOT NULL CHECK (price >= 0),
                stock INTEGER DEFAULT 0 CHECK (stock >= 0),
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            ))",

            // 为 products 表添加索引
            R"(CREATE INDEX IF NOT EXISTS idx_products_price ON products(price))",
            R"(CREATE INDEX IF NOT EXISTS idx_products_stock ON products(stock))"
        };

        for (const auto& stmt : createTableStatements) {
            try {
                *m_dbSession << stmt;
            } catch (const std::exception& e) {
                qCritical() << "创建表/索引失败:" << e.what() << "，语句:" << QString::fromStdString(stmt);
                // 继续创建其他表
            }
        }

        if (!dbExists) {
            qDebug() << "新数据库已创建并连接:" << m_dbFile;
        } else {
            qDebug() << "数据库已连接:" << m_dbFile;
        }
        return true;

    } catch (const std::exception& e) {
        qCritical() << "数据库连接失败:" << e.what();
        emit errorOccurred(QString("数据库连接失败: %1").arg(e.what()));
        return false;
    }
}

void SQLite3StateMachine::disconnectDatabase()
{
    if (m_dbSession) {
        m_dbSession.reset();
        qDebug() << "数据库连接已关闭";
    }
}

void SQLite3StateMachine::addToQueue(const OperationRequest& request)
{
    QMutexLocker locker(&m_queueMutex);
    m_operationQueue.enqueue(request);

    // 保存到数据库（可选）
    if (m_dbSession) {
        try {
            std::string operationId = request.id;
            std::string operationType = request.type;

            // 将参数转换为JSON字符串
            QJsonObject jsonObj;
            for (const auto& param : request.string_params) {
                jsonObj[QString::fromStdString(param.first)] = QString::fromStdString(param.second);
            }

            QJsonDocument doc(jsonObj);
            std::string parameters = doc.toJson(QJsonDocument::Compact).toStdString();

            std::string insertSQL = "INSERT INTO operation_queue (operation_id, operation_type, parameters) VALUES (?, ?, ?)";
            *m_dbSession << insertSQL,
                soci::use(operationId),
                soci::use(operationType),
                soci::use(parameters);
        } catch (const std::exception& e) {
            qWarning() << "保存操作到队列失败:" << e.what();
        }
    }

    emit operationQueued(QString::fromStdString(request.id), QString::fromStdString(request.type));
    emit queueSizeChanged(m_operationQueue.size());

    // 如果状态机在idle状态，启动任务处理
    if (currentState() == "idle") {
        m_stateMachine->submitEvent("start");
    }
}

OperationRequest SQLite3StateMachine::dequeue()
{
    QMutexLocker locker(&m_queueMutex);
    if (m_operationQueue.isEmpty()) {
        return OperationRequest("");
    }

    OperationRequest request = m_operationQueue.dequeue();
    emit queueSizeChanged(m_operationQueue.size());

    // 更新数据库状态（可选）
    if (m_dbSession) {
        try {
            std::string operationId = request.id;
            std::string updateSQL = "UPDATE operation_queue SET status = 'processing', started_at = CURRENT_TIMESTAMP WHERE operation_id = ?";
            *m_dbSession << updateSQL, soci::use(operationId);
        } catch (const std::exception& e) {
            qWarning() << "更新操作状态失败:" << e.what();
        }
    }

    return request;
}

void SQLite3StateMachine::handleError(const QString& errorMsg)
{
    static int retryCount = 0;
    const int maxRetries = 3;

    qDebug() << "开始处理错误:" << errorMsg;

    if (retryCount < maxRetries) {
        retryCount++;
        qDebug() << "第" << retryCount << "次重试连接数据库，错误原因:" << errorMsg;

        // 使用指数退避策略，延迟时间逐渐增加
        int delayMs = 1000 * (1 << (retryCount - 1)); // 1s, 2s, 4s
        QTimer::singleShot(delayMs, this, [this, errorMsg]() {
            qDebug() << "执行第" << retryCount << "次重试";
            bool success = connectToDatabase();
            if (success) {
                qDebug() << "重试成功";
                m_stateMachine->submitEvent("db.exists");
                retryCount = 0; // 重置重试计数
            } else if (retryCount >= maxRetries) {
                qCritical() << "达到最大重试次数(" << maxRetries << ")，连接失败，最后错误:" << errorMsg;
                emit errorOccurred(QString("达到最大重试次数(%1)，连接失败: %2").arg(maxRetries).arg(errorMsg));
            }
        });
    } else {
        qCritical() << "已达到最大重试次数(" << maxRetries << ")，停止重试，错误:" << errorMsg;
        emit errorOccurred(QString("已达到最大重试次数(%1): %2").arg(maxRetries).arg(errorMsg));
    }
}

void SQLite3StateMachine::handleQueryExecution(const OperationRequest& request)
{
    if (!m_dbSession) {
        emit operationCompleted(QString::fromStdString(request.id), false, "数据库连接已断开");
        m_processingOperation = false;
        QTimer::singleShot(0, this, &SQLite3StateMachine::processNextOperation);
        return;
    }

    try {
        // 取出 SQL 文本
        const std::string query = request.getStringParam("query");
        const QString qQuery = QString::fromStdString(query);
        qDebug() << "执行查询:" << qQuery;

        const QString trimmed = qQuery.trimmed();
        const QString lower = trimmed.toLower();

        // 小工具：统一按“参数名”绑定
        auto bindParamsByName = [&](soci::statement& st) {
            // 所有从上层传下来的参数，string/int/double 都可以
            // 注意跳过 "query" 这个 key
            for (const auto& p : request.string_params) {
                if (p.first == "query") {
                    continue;
                }
                st.exchange(soci::use(p.second, p.first)); // 按名字绑定
            }
            for (const auto& p : request.int_params) {
                st.exchange(soci::use(p.second, p.first));
            }
            for (const auto& p : request.double_params) {
                st.exchange(soci::use(p.second, p.first));
            }
        };

        const bool isSelect = lower.startsWith(QStringLiteral("select"));

        if (isSelect) {
            //
            // ========= SELECT 查询 =========
            //
            soci::statement st = (m_dbSession->prepare << query);
            bindParamsByName(st);

            // 用 statement + row 循环，而不是 rowset(prepare<<) 那种无参方式
            soci::row row;
            st.exchange(soci::into(row));
            st.define_and_bind();
            st.execute();

            QJsonArray results;

            while (st.fetch()) {
                QJsonObject rowData;

                for (std::size_t i = 0; i < row.size(); ++i) {
                    const soci::column_properties& props = row.get_properties(i);
                    const std::string columnName = props.get_name();

                    if (row.get_indicator(i) == soci::i_ok) {
                        switch (props.get_data_type()) {
                        case soci::dt_string:
                            rowData[QString::fromStdString(columnName)] = QString::fromStdString(row.get<std::string>(i));
                            break;
                        case soci::dt_integer:
                            rowData[QString::fromStdString(columnName)] = row.get<int>(i);
                            break;
                        case soci::dt_double:
                            rowData[QString::fromStdString(columnName)] = row.get<double>(i);
                            break;
                        case soci::dt_long_long:
                            rowData[QString::fromStdString(columnName)] = static_cast<qint64>(row.get<long long>(i));
                            break;
                        default:
                            // 其他类型尽量按字符串取
                            try {
                                rowData[QString::fromStdString(columnName)] = QString::fromStdString(row.get<std::string>(i));
                            } catch (...) {
                                rowData[QString::fromStdString(columnName)] = "N/A";
                            }
                            break;
                        }
                    } else {
                        // NULL 值
                        rowData[QString::fromStdString(columnName)] = QJsonValue::Null;
                    }
                }

                results.append(rowData);
            }

            QJsonDocument doc(results);
            emit operationCompleted(
                QString::fromStdString(request.id),
                true,
                QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));

        } else {
            //
            // INSERT/UPDATE/DELETE 查询 - 使用 SOCI 的 prepared statement
            soci::statement st = (m_dbSession->prepare << query);

            // 小工具：去掉参数名开头的冒号（如果有的话）
            auto normalizeName = [](const std::string& raw) -> std::string {
                if (!raw.empty() && raw[0] == ':') {
                    return raw.substr(1); // ":age" -> "age"
                }
                return raw;
            };

            // 绑定参数 - 按“名字”绑定，避免顺序问题
            // 字符串参数
            for (const auto& param : request.string_params) {
                if (param.first == "query") {
                    continue; // 跳过保存 SQL 文本的那个 key
                }
                const std::string name = normalizeName(param.first);
                st.exchange(soci::use(param.second, name));
            }

            // int 参数
            for (const auto& param : request.int_params) {
                const std::string name = normalizeName(param.first);
                st.exchange(soci::use(param.second, name));
            }

            // double 参数
            for (const auto& param : request.double_params) {
                const std::string name = normalizeName(param.first);
                st.exchange(soci::use(param.second, name));
            }

            // bool 参数 -> int
            for (const auto& param : request.bool_params) {
                const std::string name = normalizeName(param.first);
                int boolAsInt = param.second ? 1 : 0;
                st.exchange(soci::use(boolAsInt, name));
            }

            st.define_and_bind();
            st.execute(true);

            // 受影响行数
            const int affectedRows = st.get_affected_rows();

            QJsonObject resultObj;
            resultObj["affected_rows"] = affectedRows;

            // 如果是 INSERT，顺便查 last_insert_rowid()
            if (lower.startsWith(QStringLiteral("insert"))) {
                long long lastId = 0;
                try {
                    *m_dbSession << "SELECT last_insert_rowid()", soci::into(lastId);
                    resultObj["last_insert_id"] = static_cast<qint64>(lastId);
                } catch (...) {
                    resultObj["last_insert_id"] = 0;
                }
            }

            QJsonDocument doc(resultObj);
            emit operationCompleted(
                QString::fromStdString(request.id),
                true,
                QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        }

    } catch (const std::exception& e) {
        const QString error = QStringLiteral("查询执行失败: ") + QString::fromUtf8(e.what());
        qCritical() << error;
        emit operationCompleted(QString::fromStdString(request.id), false, error);
        m_stateMachine->submitEvent("task.error", error);
    }

    m_processingOperation = false;
    QTimer::singleShot(0, this, &SQLite3StateMachine::processNextOperation);
}
