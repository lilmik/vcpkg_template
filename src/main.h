// main.h
#ifndef MAIN_H
#define MAIN_H

#include "dboperatethread.h"
#include <QCoreApplication>
#include <QObject>
#include <QVariant>

class DatabaseTest : public QObject {
    Q_OBJECT

public:
    explicit DatabaseTest(const QString& dbFile, QObject* parent = nullptr);
    ~DatabaseTest();

    void startTest();

private slots:
    void onConnected();
    void cleanupTestData();
    void onOperationCompleted(const QString& operationId, bool success, const QVariant& result);
    void onErrorOccurred(const QString& error);

private:
    void performBasicTests();
    void performAdvancedTests();
    void displayResults(const QString& operationId, const QVariant& result);

    DBOperateThread* m_dbThread;
    int m_totalOperations;
    int m_completedOperations;
    bool m_basicTestsDone;
};

#endif // MAIN_H
