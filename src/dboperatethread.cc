// dboperatethread.cc
#include "dboperatethread.h"
#include <QDebug>

DBOperateThread::DBOperateThread(const QString& dbFile, QObject* parent)
    : QObject(parent)
    , m_workerThread(new QThread(this))
    , m_handler(nullptr)
    , m_dbFile(dbFile)
    , m_initialized(false)
{
    // 创建工作线程对象
    m_handler = new SQLite3Handler(m_dbFile);

    // 将处理器移动到工作线程
    m_handler->moveToThread(m_workerThread);

    // 设置连接
    setupConnections();
}

DBOperateThread::~DBOperateThread()
{
    shutdown();
}

bool DBOperateThread::initialize()
{
    if (m_initialized) {
        return true;
    }

    connect(m_workerThread, &QThread::started, [this]() { emit initializeRequested(); });

    m_workerThread->start();
    m_initialized = true;
    return true;
}

void DBOperateThread::shutdown()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        // 发送关闭请求到工作线程
        emit shutdownRequested();

        // 等待线程结束（超时5秒）
        m_workerThread->quit();
        if (!m_workerThread->wait(5000)) {
            qWarning() << "数据库操作线程关闭超时，强制终止";
            m_workerThread->terminate();
            m_workerThread->wait();
        }
    }

    if (m_handler) {
        m_handler->deleteLater();
        m_handler = nullptr;
    }
}

bool DBOperateThread::isRunning() const
{
    return m_workerThread && m_workerThread->isRunning();
}

QString DBOperateThread::currentState() const
{
    if (!m_handler) {
        qWarning() << "currentState() 调用时 m_handler 为空";
        return "uninitialized";
    }
    return m_handler->currentState();
}

int DBOperateThread::queueSize() const
{
    return m_handler ? m_handler->queueSize() : 0;
}

void DBOperateThread::start()
{
    if (m_handler) {
        // 通过信号槽调用，确保在工作线程中执行
        QMetaObject::invokeMethod(m_handler, "start", Qt::QueuedConnection);
    }
}

void DBOperateThread::stop()
{
    if (m_handler) {
        // 通过信号槽调用，确保在工作线程中执行
        QMetaObject::invokeMethod(m_handler, "stop", Qt::QueuedConnection);
    }
}

void DBOperateThread::setupConnections()
{
    // 连接工作线程信号
    connect(m_workerThread, &QThread::started, this, []() {
        qDebug() << "数据库操作线程启动";
    });

    connect(m_workerThread, &QThread::finished, this, []() {
        qDebug() << "数据库操作线程结束";
    });

    // 连接处理器信号（跨线程）
    if (m_handler) {
        connect(m_handler, &SQLite3Handler::operationCompleted, this, &DBOperateThread::onOperationCompleted, Qt::QueuedConnection);
        connect(m_handler, &SQLite3Handler::connected, this, &DBOperateThread::onConnected, Qt::QueuedConnection);
        connect(m_handler, &SQLite3Handler::disconnected, this, &DBOperateThread::onDisconnected, Qt::QueuedConnection);
        connect(m_handler, &SQLite3Handler::errorOccurred, this, &DBOperateThread::onErrorOccurred, Qt::QueuedConnection);
    }

    // 连接内部控制信号（跨线程）
    connect(this, &DBOperateThread::initializeRequested, m_handler, &SQLite3Handler::initialize);
    connect(this, &DBOperateThread::shutdownRequested, m_handler, &SQLite3Handler::stop);
}

void DBOperateThread::onOperationCompleted(const QString& operationId, bool success, const QVariant& result)
{
    // 将信号转发到主线程
    if (operationId.isEmpty()) {
        qWarning() << "忽略空 operationId 的操作完成信号";
        return;
    }
    emit operationCompleted(operationId, success, result);
}

void DBOperateThread::onConnected()
{
    emit connected();
}

void DBOperateThread::onDisconnected()
{
    emit disconnected();
}

void DBOperateThread::onErrorOccurred(const QString& error)
{
    emit errorOccurred(error);
}
