// dboperatethread.h
#ifndef DBOPERATETHREAD_H
#define DBOPERATETHREAD_H

#include "sqlite3handler.h"
#include <QObject>
#include <QString>
#include <QThread>

class DBOperateThread : public QObject {
    Q_OBJECT

public:
    explicit DBOperateThread(const QString& dbFile, QObject* parent = nullptr);
    ~DBOperateThread();

    // 初始化工作线程
    bool initialize();
    void shutdown();

    // 获取处理器（用于主线程调用）
    SQLite3Handler* handler() const { return m_handler; }

    // 线程状态
    bool isRunning() const;
    QString currentState() const;
    int queueSize() const;

public slots:
    void start();
    void stop();

signals:
    // 代理处理器信号到主线程
    void operationCompleted(const QString& operationId, bool success, const QVariant& result);
    void connected();
    void disconnected();
    void errorOccurred(const QString& error);

    // 内部线程控制信号
    void initializeRequested();
    void shutdownRequested();

private slots:
    void onOperationCompleted(const QString& operationId, bool success, const QVariant& result);
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(const QString& error);

private:
    void setupConnections();

    QThread* m_workerThread;
    SQLite3Handler* m_handler;
    QString m_dbFile;
    bool m_initialized;
};

#endif // DBOPERATETHREAD_H
