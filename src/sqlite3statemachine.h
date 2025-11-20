// sqlite3statemachine.h
#ifndef SQLITE3STATEMACHINE_H
#define SQLITE3STATEMACHINE_H

#include "operationrequest.h"
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QScxmlStateMachine>
#include <QTimer>
#include <map>
#include <memory>
#include <soci/soci.h>
#include <string>

class SQLite3StateMachine : public QObject {
    Q_OBJECT

public:
    explicit SQLite3StateMachine(const QString& dbFile, QObject* parent = nullptr);
    ~SQLite3StateMachine();

    // 状态机控制
    bool initialize();
    void shutdown();

    // 状态查询
    QString currentState() const;
    bool isRunning() const;
    bool isConnected() const;

    // 数据库操作接口
    soci::session* getSession() const;

    // 队列管理
    int queueSize() const;
    void clearQueue();
    QString currentOperationId() const;

public slots:
    // 状态机控制
    void startConnection();
    void stopConnection();

    // 异步业务操作 - 添加到队列
    QString executeQuery(const QString& query, const std::map<std::string, std::string>& params = {});

    // 直接操作（绕过队列）
    bool executeImmediateQuery(const QString& query, const std::map<std::string, std::string>& params = {});

signals:
    // 状态变化
    void stateChanged(const QString& state);
    void connectionEstablished();
    void connectionLost();

    // 队列状态
    void operationQueued(const QString& operationId, const QString& type);
    void operationStarted(const QString& operationId);
    void operationCompleted(const QString& operationId, bool success, const QString& result);
    void queueSizeChanged(int size);

    // 错误通知
    void errorOccurred(const QString& error);

private slots:
    void handleStateMachineEvent(const QString& event, const QVariant& data);
    void processNextOperation();

private:
    void setupConnections();
    bool connectToDatabase();
    void disconnectDatabase();
    void handleQueryExecution(const OperationRequest& request);
    void addToQueue(const OperationRequest& request);
    OperationRequest dequeue();
    // 添加错误处理函数声明
    void handleError(const QString& errorMsg);

    // 类型转换辅助函数
    std::string qstringToString(const QString& qstr) const;
    QString stringToQString(const std::string& str) const;
    std::map<std::string, std::string> qvariantMapToStringMap(const QVariantMap& qmap) const;

    QString m_dbFile;
    std::unique_ptr<soci::session> m_dbSession;
    QScxmlStateMachine* m_stateMachine = nullptr;

    // 队列相关
    mutable QMutex m_queueMutex;
    QQueue<OperationRequest> m_operationQueue;
    bool m_processingOperation = false;
    QString m_currentOperationId;

    // 当前操作
    OperationRequest m_currentRequest;

    // 如果需要，添加重试计数
    int m_retryCount = 0;
};

#endif // SQLITE3STATEMACHINE_H
