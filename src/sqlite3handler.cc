// sqlite3handler.cc
#include "sqlite3handler.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

SQLite3Handler::SQLite3Handler(const QString& dbFile, QObject* parent)
    : QObject(parent)
    , m_dbFile(dbFile)
    , m_initialized(false)
{
    m_stateMachine = new SQLite3StateMachine(dbFile, this);

    // 连接状态机信号
    connect(m_stateMachine, &SQLite3StateMachine::operationCompleted,
        this, &SQLite3Handler::onOperationCompleted);
    connect(m_stateMachine, &SQLite3StateMachine::connectionEstablished,
        this, &SQLite3Handler::onConnectionEstablished);
    connect(m_stateMachine, &SQLite3StateMachine::connectionLost,
        this, &SQLite3Handler::onConnectionLost);
    connect(m_stateMachine, &SQLite3StateMachine::errorOccurred,
        this, &SQLite3Handler::onErrorOccurred);
}

SQLite3Handler::~SQLite3Handler()
{
    shutdown();
}

bool SQLite3Handler::initialize()
{
    if (m_initialized) {
        return true;
    }

    if (!m_stateMachine->initialize()) {
        qCritical() << "Failed to initialize state machine";
        return false;
    }

    m_initialized = true;
    return true;
}

// 用户管理操作 - 异步
QString SQLite3Handler::addUser(const QString& name, const QString& email, int age)
{
    QVariantMap data;
    data["name"] = name;
    data["email"] = email;
    data["age"] = age;

    std::map<std::string, std::string> params;
    std::string query = buildInsertUserQuery(data, params);

    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query), params);
    setOperationType(operationId, "addUser");
    return operationId;
}

QString SQLite3Handler::updateUser(int userId, const QVariantMap& updates)
{
    if (updates.isEmpty()) {
        return QString();
    }

    std::map<std::string, std::string> params;
    std::string query = buildUpdateUserQuery(userId, updates, params);

    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query), params);
    setOperationType(operationId, "updateUser");
    return operationId;
}

QString SQLite3Handler::deleteUser(int userId)
{
    std::string query = "DELETE FROM users WHERE id = :id";
    std::map<std::string, std::string> params;
    params["id"] = std::to_string(userId);

    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query), params);
    setOperationType(operationId, "deleteUser");
    return operationId;
}

QString SQLite3Handler::getUserById(int userId)
{
    std::string query = "SELECT * FROM users WHERE id = :id";
    std::map<std::string, std::string> params;
    params["id"] = std::to_string(userId);

    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query), params);
    setOperationType(operationId, "getUser");
    return operationId;
}

QString SQLite3Handler::getAllUsers()
{
    std::string query = "SELECT * FROM users ORDER BY id";
    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query));
    setOperationType(operationId, "getAllUsers");
    return operationId;
}

QString SQLite3Handler::findUsersByName(const QString& name)
{
    std::string query = "SELECT * FROM users WHERE name LIKE :name ORDER BY id";
    std::map<std::string, std::string> params;
    params["name"] = "%" + name.toStdString() + "%";

    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query), params);
    setOperationType(operationId, "findUsersByName");
    return operationId;
}

QString SQLite3Handler::findUsersByEmail(const QString& email)
{
    std::string query = "SELECT * FROM users WHERE email LIKE :email ORDER BY id";
    std::map<std::string, std::string> params;
    params["email"] = "%" + email.toStdString() + "%";

    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query), params);
    setOperationType(operationId, "findUsersByEmail");
    return operationId;
}

// 产品管理操作 - 异步
QString SQLite3Handler::addProduct(const QString& name, double price, int stock)
{
    QVariantMap data;
    data["name"] = name;
    data["price"] = price;
    data["stock"] = stock;

    std::map<std::string, std::string> params;
    std::string query = buildInsertProductQuery(data, params);

    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query), params);
    setOperationType(operationId, "addProduct");
    return operationId;
}

QString SQLite3Handler::updateProduct(int productId, const QVariantMap& updates)
{
    if (updates.isEmpty()) {
        return QString();
    }

    std::map<std::string, std::string> params;
    std::string query = buildUpdateProductQuery(productId, updates, params);

    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query), params);
    setOperationType(operationId, "updateProduct");
    return operationId;
}

QString SQLite3Handler::deleteProduct(int productId)
{
    std::string query = "DELETE FROM products WHERE id = :id";
    std::map<std::string, std::string> params;
    params["id"] = std::to_string(productId);

    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query), params);
    setOperationType(operationId, "deleteProduct");
    return operationId;
}

QString SQLite3Handler::getProductById(int productId)
{
    std::string query = "SELECT * FROM products WHERE id = :id";
    std::map<std::string, std::string> params;
    params["id"] = std::to_string(productId);

    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query), params);
    setOperationType(operationId, "getProduct");
    return operationId;
}

QString SQLite3Handler::getAllProducts()
{
    std::string query = "SELECT * FROM products ORDER BY id";
    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query));
    setOperationType(operationId, "getAllProducts");
    return operationId;
}

QString SQLite3Handler::findProductsByPriceRange(double minPrice, double maxPrice)
{
    std::string query = "SELECT * FROM products WHERE price BETWEEN :minPrice AND :maxPrice ORDER BY price";

    std::map<std::string, std::string> params;
    params["minPrice"] = std::to_string(minPrice);
    params["maxPrice"] = std::to_string(maxPrice);

    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query), params);
    setOperationType(operationId, "findProductsByPriceRange");
    return operationId;
}

QString SQLite3Handler::findProductsByName(const QString& name)
{
    std::string query = "SELECT * FROM products WHERE name LIKE :name ORDER BY id";
    std::map<std::string, std::string> params;
    params["name"] = "%" + name.toStdString() + "%";

    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query), params);
    setOperationType(operationId, "findProductsByName");
    return operationId;
}

// 库存管理操作
QString SQLite3Handler::updateProductStock(int productId, int newStock)
{
    std::string query = "UPDATE products SET stock = :stock WHERE id = :id";
    std::map<std::string, std::string> params;
    params["id"] = std::to_string(productId);
    params["stock"] = std::to_string(newStock);

    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query), params);
    setOperationType(operationId, "updateStock");
    return operationId;
}

QString SQLite3Handler::increaseProductStock(int productId, int quantity)
{
    std::string query = "UPDATE products SET stock = stock + :quantity WHERE id = :id";
    std::map<std::string, std::string> params;
    params["id"] = std::to_string(productId);
    params["quantity"] = std::to_string(quantity);

    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query), params);
    setOperationType(operationId, "increaseStock");
    return operationId;
}

QString SQLite3Handler::decreaseProductStock(int productId, int quantity)
{
    std::string query = "UPDATE products SET stock = stock - :quantity WHERE id = :id AND stock >= :quantity";
    std::map<std::string, std::string> params;
    params["id"] = std::to_string(productId);
    params["quantity"] = std::to_string(quantity);

    QString operationId = m_stateMachine->executeQuery(QString::fromStdString(query), params);
    setOperationType(operationId, "decreaseStock");
    return operationId;
}

// 通用查询操作
QString SQLite3Handler::executeCustomQuery(const QString& query, const QVariantMap& params)
{
    QString operationId = m_stateMachine->executeQuery(query, qvariantMapToStringMap(params));
    setOperationType(operationId, "customQuery");
    return operationId;
}

bool SQLite3Handler::executeCustomCommand(const QString& command, const QVariantMap& params)
{
    return m_stateMachine->executeImmediateQuery(command, qvariantMapToStringMap(params));
}

// 批量操作
QString SQLite3Handler::batchInsertUsers(const QVariantList& users)
{
    // 这里只是一个示例实现：开启事务，循环调用 addUser
    QString operationId = "batch_users_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    bool allOk = beginTransaction();

    if (allOk) {
        for (const QVariant& userVar : users) {
            QVariantMap user = userVar.toMap();
            QString singleOpId = addUser(
                user["name"].toString(),
                user["email"].toString(),
                user["age"].toInt());
            Q_UNUSED(singleOpId);
        }
        allOk = commitTransaction();
    } else {
        rollbackTransaction();
    }

    // 这里只是占位结果，真正的业务可以根据需要改造
    setOperationType(operationId, "batchUsers");
    return operationId;
}

QString SQLite3Handler::batchInsertProducts(const QVariantList& products)
{
    QString operationId = "batch_products_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    bool allOk = beginTransaction();

    if (allOk) {
        for (const QVariant& productVar : products) {
            QVariantMap product = productVar.toMap();
            QString singleOpId = addProduct(
                product["name"].toString(),
                product["price"].toDouble(),
                product["stock"].toInt());
            Q_UNUSED(singleOpId);
        }
        allOk = commitTransaction();
    } else {
        rollbackTransaction();
    }

    setOperationType(operationId, "batchProducts");
    return operationId;
}

// 状态查询
bool SQLite3Handler::isConnected() const
{
    return m_stateMachine->isConnected();
}

QString SQLite3Handler::currentState() const
{
    return m_stateMachine->currentState();
}

int SQLite3Handler::queueSize() const
{
    return m_stateMachine->queueSize();
}

// 事务支持
bool SQLite3Handler::beginTransaction()
{
    return executeCustomCommand("BEGIN TRANSACTION");
}

bool SQLite3Handler::commitTransaction()
{
    return executeCustomCommand("COMMIT");
}

bool SQLite3Handler::rollbackTransaction()
{
    return executeCustomCommand("ROLLBACK");
}

void SQLite3Handler::start()
{
    if (m_initialized) {
        m_stateMachine->startConnection();
    }
}

void SQLite3Handler::stop()
{
    if (m_stateMachine) {
        m_stateMachine->stopConnection();
    }
}

void SQLite3Handler::shutdown()
{
    stop();
    if (m_stateMachine) {
        m_stateMachine->shutdown();
    }
}

// 私有槽函数
void SQLite3Handler::onOperationCompleted(const QString& operationId, bool success, const QString& result)
{
    QString operationType = getOperationType(operationId);
    QVariant parsedResult = parseJsonResult(result);

    // 发出通用操作完成信号
    emit operationCompleted(operationId, success, parsedResult);

    // 发出特定操作信号
    if (operationType == "addUser") {
        emit userAdded(operationId, success, parsedResult);
    } else if (operationType == "updateUser") {
        emit userUpdated(operationId, success, parsedResult);
    } else if (operationType == "deleteUser") {
        emit userDeleted(operationId, success, parsedResult);
    } else if (operationType.startsWith("getUser") || operationType.startsWith("findUser")) {
        emit userRetrieved(operationId, success, parsedResult);

    } else if (operationType == "addProduct") {
        emit productAdded(operationId, success, parsedResult);
    } else if (operationType == "updateProduct") {
        emit productUpdated(operationId, success, parsedResult);
    } else if (operationType == "deleteProduct") {
        emit productDeleted(operationId, success, parsedResult);
    } else if (operationType.startsWith("getProduct") || operationType.startsWith("findProduct")) {
        emit productRetrieved(operationId, success, parsedResult);

    } else if (operationType.contains("Stock")) {
        emit stockUpdated(operationId, success, parsedResult);

    } else if (operationType == "batchUsers") {
        emit batchUsersCompleted(operationId, success, parsedResult);
    } else if (operationType == "batchProducts") {
        emit batchProductsCompleted(operationId, success, parsedResult);
    }

    // 清理操作类型映射
    clearOperationType(operationId);
}

void SQLite3Handler::onConnectionEstablished()
{
    emit connected();
}

void SQLite3Handler::onConnectionLost()
{
    emit disconnected();
}

void SQLite3Handler::onErrorOccurred(const QString& error)
{
    emit errorOccurred(error);
}

// 私有辅助函数
std::map<std::string, std::string> SQLite3Handler::qvariantMapToStringMap(const QVariantMap& qmap) const
{
    std::map<std::string, std::string> result;
    for (auto it = qmap.begin(); it != qmap.end(); ++it) {
        result[it.key().toStdString()] = it.value().toString().toStdString();
    }
    return result;
}

QVariant SQLite3Handler::parseJsonResult(const QString& jsonResult) const
{
    if (jsonResult.isEmpty() || jsonResult == "{}") {
        return QVariant();
    }

    QJsonDocument doc = QJsonDocument::fromJson(jsonResult.toUtf8());

    if (doc.isArray()) {
        return doc.array().toVariantList();
    } else if (doc.isObject()) {
        return doc.object().toVariantMap();
    }

    return QVariant(jsonResult);
}

std::string SQLite3Handler::buildInsertUserQuery(const QVariantMap& data, std::map<std::string, std::string>& params) const
{
    std::string query = "INSERT INTO users (name, email, age) VALUES (:name, :email, :age)";

    // 确保参数名称与SQL语句中的占位符完全匹配
    params["name"] = data["name"].toString().toStdString();
    params["email"] = data["email"].toString().toStdString();
    params["age"] = std::to_string(data["age"].toInt());

    return query;
}

std::string SQLite3Handler::buildUpdateUserQuery(int userId, const QVariantMap& updates, std::map<std::string, std::string>& params) const
{
    std::string query = "UPDATE users SET ";
    QStringList setClauses;

    for (auto it = updates.begin(); it != updates.end(); ++it) {
        std::string paramName = it.key().toStdString();
        setClauses << QString::fromStdString(paramName + " = :" + paramName);
        params[paramName] = it.value().toString().toStdString();
    }

    query += setClauses.join(", ").toStdString();
    query += " WHERE id = :id";
    params["id"] = std::to_string(userId);

    return query;
}

std::string SQLite3Handler::buildInsertProductQuery(const QVariantMap& data, std::map<std::string, std::string>& params) const
{
    std::string query = "INSERT INTO products (name, price, stock) VALUES (:name, :price, :stock)";

    params["name"] = data["name"].toString().toStdString();
    params["price"] = std::to_string(data["price"].toDouble());
    params["stock"] = std::to_string(data["stock"].toInt());

    return query;
}

std::string SQLite3Handler::buildUpdateProductQuery(int productId, const QVariantMap& updates, std::map<std::string, std::string>& params) const
{
    std::string query = "UPDATE products SET ";
    QStringList setClauses;

    for (auto it = updates.begin(); it != updates.end(); ++it) {
        std::string paramName = it.key().toStdString();
        setClauses << QString::fromStdString(paramName + " = :" + paramName);
        params[paramName] = it.value().toString().toStdString();
    }

    query += setClauses.join(", ").toStdString();
    query += " WHERE id = :id";
    params["id"] = std::to_string(productId);

    return query;
}

QString SQLite3Handler::getOperationType(const QString& operationId) const
{

    return m_operationTypes.value(operationId, "unknown");
}

void SQLite3Handler::setOperationType(const QString& operationId, const QString& type)
{
    m_operationTypes[operationId] = type;
}

void SQLite3Handler::clearOperationType(const QString& operationId)
{
    m_operationTypes.remove(operationId);
}
