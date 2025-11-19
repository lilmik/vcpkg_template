#ifndef SQLITE3STATEMANAGER_H
#define SQLITE3STATEMANAGER_H

#include <QObject>
#include <QScxmlStateMachine>
#include <QString>
#include <memory>

namespace soci {
class session;
}

class SQLite3StateManager : public QObject {
    Q_OBJECT

public:
    explicit SQLite3StateManager(const QString& db_file = "qt_sqlite3_db.sqlite", QObject* parent = nullptr);
    ~SQLite3StateManager() override;

    bool start();
    void stop();

    QString currentState() const;
    bool isRunning() const;
    QScxmlStateMachine* stateMachine() const { return state_machine_; }

    // 公共方法来发送事件
    void startTask();
    void stopTask();
    bool checkDatabaseExists();

signals:
    void stateChanged(const QString& newState);
    void databaseInitialized(bool success);
    void errorOccurred(const QString& error);

private:
    bool initializeDatabase();
    void setupStateMachineConnections();

private:
    QString db_file_;
    std::unique_ptr<soci::session> db_session_;
    QScxmlStateMachine* state_machine_;
    bool database_initialized_;
};

#endif // SQLITE3STATEMANAGER_H
