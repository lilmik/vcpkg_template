// sqlite3handler.h
#ifndef SQLITE3HANDLER_H
#define SQLITE3HANDLER_H

#include "sqlite3statemachine.h"
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <map>
#include <string>
#include <vector>

class SQLite3Handler : public QObject {
    Q_OBJECT

public:
    explicit SQLite3Handler(const QString& dbFile, QObject* parent = nullptr);
    ~SQLite3Handler();

    // 初始化状态机和数据库连接
    bool initialize();

    // 用户管理操作 - 异步（使用队列）
    QString addUser(const QString& name, const QString& email, int age = 0);
    QString updateUser(int userId, const QVariantMap& updates);
    QString deleteUser(int userId);
    QString getUserById(int userId);
    QString getAllUsers();
    QString findUsersByName(const QString& name);
    QString findUsersByEmail(const QString& email);

    // 产品管理操作 - 异步（使用队列）
    QString addProduct(const QString& name, double price, int stock = 0);
    QString updateProduct(int productId, const QVariantMap& updates);
    QString deleteProduct(int productId);
    QString getProductById(int productId);
    QString getAllProducts();
    QString findProductsByPriceRange(double minPrice, double maxPrice);
    QString findProductsByName(const QString& name);

    // 库存管理操作
    QString updateProductStock(int productId, int newStock);
    QString increaseProductStock(int productId, int quantity);
    QString decreaseProductStock(int productId, int quantity);

    // 通用查询操作 - 异步（使用队列）
    QString executeCustomQuery(const QString& query, const QVariantMap& params = QVariantMap());

    // 立即执行操作（绕过队列）
    bool executeCustomCommand(const QString& command, const QVariantMap& params = QVariantMap());

    // 批量操作
    QString batchInsertUsers(const QVariantList& users);
    QString batchInsertProducts(const QVariantList& products);

    // 状态查询
    bool isConnected() const;
    QString currentState() const;
    int queueSize() const;

    // 事务支持（立即执行）
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    // 线程安全的控制方法
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    // 操作类型映射
    QString getOperationType(const QString& operationId) const;
    // void setOperationType(const QString& operationId, const QString& type);
    void clearOperationType(const QString& operationId);

public slots:
    void shutdown();

signals:
    // 通用操作完成信号
    void operationCompleted(const QString& operationId, bool success, const QVariant& result);

    // 特定操作完成信号
    void userAdded(const QString& operationId, bool success, const QVariant& result);
    void userUpdated(const QString& operationId, bool success, const QVariant& result);
    void userDeleted(const QString& operationId, bool success, const QVariant& result);
    void userRetrieved(const QString& operationId, bool success, const QVariant& result);

    void productAdded(const QString& operationId, bool success, const QVariant& result);
    void productUpdated(const QString& operationId, bool success, const QVariant& result);
    void productDeleted(const QString& operationId, bool success, const QVariant& result);
    void productRetrieved(const QString& operationId, bool success, const QVariant& result);

    void stockUpdated(const QString& operationId, bool success, const QVariant& result);

    // 批量操作信号
    void batchUsersCompleted(const QString& operationId, bool success, const QVariant& result);
    void batchProductsCompleted(const QString& operationId, bool success, const QVariant& result);

    // 状态信号
    void connected();
    void disconnected();
    void errorOccurred(const QString& error);

private slots:
    void onOperationCompleted(const QString& operationId, bool success, const QString& result);
    void onConnectionEstablished();
    void onConnectionLost();
    void onErrorOccurred(const QString& error);

private:
    // 类型转换辅助函数
    std::map<std::string, std::string> qvariantMapToStringMap(const QVariantMap& qmap) const;
    QVariant parseJsonResult(const QString& jsonResult) const;

    // 构建查询语句和参数
    std::string buildInsertUserQuery(const QVariantMap& data, std::map<std::string, std::string>& params) const;
    std::string buildUpdateUserQuery(int userId, const QVariantMap& updates, std::map<std::string, std::string>& params) const;
    std::string buildInsertProductQuery(const QVariantMap& data, std::map<std::string, std::string>& params) const;
    std::string buildUpdateProductQuery(int productId, const QVariantMap& updates, std::map<std::string, std::string>& params) const;

    // 操作类型映射
    // QString getOperationType(const QString& operationId) const;
    void setOperationType(const QString& operationId, const QString& type);
    // void clearOperationType(const QString& operationId);

    SQLite3StateMachine* m_stateMachine;
    QString m_dbFile;
    bool m_initialized;

    // 操作类型跟踪
    QMap<QString, QString> m_operationTypes;
};

#endif // SQLITE3HANDLER_H
