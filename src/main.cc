// src/main.cpp
#include "main.h"
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

DatabaseTest::DatabaseTest(const QString& dbFile, QObject* parent)
    : QObject(parent)
    , m_dbThread(new DBOperateThread(dbFile, this))
    , m_totalOperations(0)
    , m_completedOperations(0)
    , m_basicTestsDone(false)
{
    connect(m_dbThread, &DBOperateThread::connected,
        this, &DatabaseTest::onConnected);
    connect(m_dbThread, &DBOperateThread::operationCompleted,
        this, &DatabaseTest::onOperationCompleted);
    connect(m_dbThread, &DBOperateThread::errorOccurred,
        this, &DatabaseTest::onErrorOccurred);
}

DatabaseTest::~DatabaseTest()
{
    if (m_dbThread) {
        m_dbThread->shutdown();
    }
}

void DatabaseTest::startTest()
{
    qDebug() << "=== SQLite3 数据库操作测试 ===";
    qDebug() << "数据库文件:" << (m_dbThread->handler() ? "已设置" : "未设置");
    qDebug() << "初始化数据库操作线程...";

    if (m_dbThread->initialize()) {
        m_dbThread->start();
    } else {
        qCritical() << "数据库操作线程初始化失败";
        QCoreApplication::quit();
    }
}

void DatabaseTest::onConnected()
{
    static bool firstConnection = true;

    if (firstConnection) {
        firstConnection = false;
        qDebug() << "✓ 数据库连接成功";
        qDebug() << "当前状态:" << m_dbThread->currentState();
        qDebug() << "队列大小:" << m_dbThread->queueSize();

        // 清理旧的测试数据
        cleanupTestData();

        // 开始基本测试
        performBasicTests();
    }
}

void DatabaseTest::cleanupTestData()
{
    qDebug() << "清理测试数据...";

    // 删除包含测试标识的数据
    m_dbThread->handler()->executeCustomCommand(
        "DELETE FROM users WHERE email LIKE '%@example.com'");
    m_dbThread->handler()->executeCustomCommand(
        "DELETE FROM products WHERE name LIKE '%测试%' OR name LIKE '%笔记本%' OR name LIKE '%手机%'");

    qDebug() << "测试数据清理完成";
}

void DatabaseTest::onOperationCompleted(const QString& operationId, bool success, const QVariant& result)
{
    m_completedOperations++;

    QString opType = "unknown";
    opType = m_dbThread->handler()->getOperationType(operationId);
    qDebug() << "operationId: " << operationId;
    qDebug() << "opType: " << opType;
    // if (m_dbThread && m_dbThread->handler()) {
    //     opType = m_dbThread->handler()->getOperationType(operationId);
    // }

    if (success) {
        qDebug() << "✓ 操作完成:" << operationId << "类型:" << opType;
        displayResults(operationId, result);
    } else {
        QString errorStr = result.toString();
        qDebug() << "✗ 操作失败:" << operationId << "类型:" << opType;

        if (errorStr.contains("UNIQUE constraint failed")) {
            qDebug() << "  错误原因: 数据重复（唯一约束冲突）";
        } else if (errorStr.contains("NOT NULL constraint failed")) {
            qDebug() << "  错误原因: 缺少必需数据（非空约束冲突）";
        } else if (errorStr.contains("no such table")) {
            qDebug() << "  错误原因: 表不存在";
        } else {
            qDebug() << "  错误信息:" << errorStr;
        }
    }

    // 检查是否所有操作都完成了
    if (m_completedOperations >= m_totalOperations) {
        if (!m_basicTestsDone) {
            m_basicTestsDone = true;
            qDebug() << "\n=== 基本测试完成 ===";
            qDebug() << "成功操作:" << m_completedOperations << "/" << m_totalOperations;
            qDebug() << "开始高级测试...";
            performAdvancedTests();
        } else {
            qDebug() << "\n=== 所有测试完成 ===";
            qDebug() << "总成功操作:" << m_completedOperations << "/" << m_totalOperations;
            qDebug() << "数据库状态:" << m_dbThread->currentState();
            qDebug() << "队列大小:" << m_dbThread->queueSize();
            qDebug() << "3秒后退出程序...";
            QTimer::singleShot(3000, QCoreApplication::instance(), &QCoreApplication::quit);
        }
    }
}

void DatabaseTest::onErrorOccurred(const QString& error)
{
    qCritical() << "数据库错误:" << error;
}

void DatabaseTest::performBasicTests()
{
    qDebug() << "\n--- 执行基本测试 ---";

    // 使用时间戳生成唯一邮箱
    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());

    // 1. 添加用户（使用唯一邮箱）
    qDebug() << "1. 添加测试用户...";
    QString user1 = m_dbThread->handler()->addUser("张三",
        "zhangsan_" + timestamp + "@example.com", 25);
    QString user2 = m_dbThread->handler()->addUser("李四",
        "lisi_" + timestamp + "@example.com", 30);
    m_totalOperations += 2;

    // 2. 添加产品（使用唯一名称）
    qDebug() << "2. 添加测试产品...";
    QString product1 = m_dbThread->handler()->addProduct("笔记本电脑_" + timestamp, 5999.99, 10);
    QString product2 = m_dbThread->handler()->addProduct("智能手机_" + timestamp, 2999.99, 20);
    m_totalOperations += 2;

    // 3. 查询所有用户
    qDebug() << "3. 查询所有用户...";
    QString allUsers = m_dbThread->handler()->getAllUsers();
    m_totalOperations += 1;

    // 4. 查询所有产品
    qDebug() << "4. 查询所有产品...";
    QString allProducts = m_dbThread->handler()->getAllProducts();
    m_totalOperations += 1;

    qDebug() << "等待基本测试操作完成...";
}

void DatabaseTest::performAdvancedTests()
{
    qDebug() << "\n--- 执行高级测试 ---";

    // 重置计数器
    m_totalOperations = 0;
    m_completedOperations = 0;

    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());

    // 5. 条件查询
    qDebug() << "5. 条件查询测试...";
    QString usersByName = m_dbThread->handler()->findUsersByName("张");
    QString productsByPrice = m_dbThread->handler()->findProductsByPriceRange(2000.0, 6000.0);
    m_totalOperations += 2;

    // 6. 获取特定用户和产品（使用实际存在的ID）
    qDebug() << "6. 获取特定用户和产品...";
    // 这里我们假设ID 1存在，因为基本测试已经插入了数据
    QString specificUser = m_dbThread->handler()->getUserById(1);
    QString specificProduct = m_dbThread->handler()->getProductById(1);
    m_totalOperations += 2;

    // 7. 更新操作（使用新邮箱避免唯一约束）
    qDebug() << "7. 更新操作测试...";
    QVariantMap userUpdates;
    userUpdates["age"] = "26";
    userUpdates["email"] = "updated_" + timestamp + "@example.com";
    QString updateUser = m_dbThread->handler()->updateUser(1, userUpdates);

    QVariantMap productUpdates;
    productUpdates["price"] = "6099.99";
    QString updateProduct = m_dbThread->handler()->updateProduct(1, productUpdates);
    m_totalOperations += 2;

    // 8. 库存操作
    qDebug() << "8. 库存操作测试...";
    QString stockUpdate = m_dbThread->handler()->increaseProductStock(1, 5);
    QString stockDecrease = m_dbThread->handler()->decreaseProductStock(1, 3);
    m_totalOperations += 2;

    // 9. 最终查询验证
    qDebug() << "9. 最终数据验证...";
    QString finalUsers = m_dbThread->handler()->getAllUsers();
    QString finalProducts = m_dbThread->handler()->getAllProducts();
    m_totalOperations += 2;

    qDebug() << "等待高级测试操作完成...";
}

void DatabaseTest::displayResults(const QString& operationId, const QVariant& result)
{
    // 先拿到操作类型
    QString opType = "unknown";
    if (m_dbThread && m_dbThread->handler()) {
        opType = m_dbThread->handler()->getOperationType(operationId);
    }

    qDebug() << "  operationId:" << operationId << "operationType:" << opType;

    if (result.isNull()) {
        qDebug() << "  操作结果: 空";
        return;
    }

    // === 1. 用户查询类 ===
    if (opType == "getAllUsers"
        || opType == "findUsersByName"
        || opType == "findUsersByEmail"
        || opType == "getUser") {

        if (result.type() == QVariant::List) {
            QVariantList users = result.toList();
            qDebug() << "  用户查询结果 - 数量:" << users.size();
            for (int i = 0; i < users.size(); ++i) {
                QVariantMap user = users[i].toMap();
                qDebug() << "    [" << i << "] ID:" << user["id"]
                         << "姓名:" << user["name"]
                         << "邮箱:" << user["email"]
                         << "年龄:" << user["age"]
                         << "创建时间:" << user.value("created_at", "N/A");
            }
        } else {
            qDebug() << "  用户结果:" << result;
        }
        return;
    }

    // === 2. 产品查询类 ===
    if (opType == "getAllProducts"
        || opType == "findProductsByName"
        || opType == "findProductsByPriceRange"
        || opType == "getProduct") {

        if (result.type() == QVariant::List) {
            QVariantList products = result.toList();
            qDebug() << "  产品查询结果 - 数量:" << products.size();
            for (int i = 0; i < products.size(); ++i) {
                QVariantMap product = products[i].toMap();
                qDebug() << "    [" << i << "] ID:" << product["id"]
                         << "名称:" << product["name"]
                         << "价格:" << product["price"]
                         << "库存:" << product["stock"]
                         << "创建时间:" << product.value("created_at", "N/A");
            }
        } else {
            qDebug() << "  产品结果:" << result;
        }
        return;
    }

    // === 3. 插入类操作 ===
    if (opType == "addUser"
        || opType == "addProduct"
        || opType == "customQuery"
        || opType == "batchUsers"
        || opType == "batchProducts") {

        if (result.type() == QVariant::Map) {
            QVariantMap resultMap = result.toMap();
            qDebug() << "  插入/批量操作结果 - 影响行数:" << resultMap["affected_rows"]
                     << "最后ID:" << resultMap["last_insert_id"];
        } else {
            qDebug() << "  插入/批量操作结果:" << result;
        }
        return;
    }

    // === 4. 更新类（包括库存） ===
    if (opType == "updateUser"
        || opType == "updateProduct"
        || opType == "updateStock"
        || opType == "increaseStock"
        || opType == "decreaseStock") {

        if (result.type() == QVariant::Map) {
            QVariantMap resultMap = result.toMap();
            qDebug() << "  更新操作结果 - 影响行数:" << resultMap["affected_rows"];
        } else {
            qDebug() << "  更新操作结果:" << result;
        }
        return;
    }

    // === 5. 删除类 ===
    if (opType == "deleteUser" || opType == "deleteProduct") {
        if (result.type() == QVariant::Map) {
            QVariantMap resultMap = result.toMap();
            qDebug() << "  删除操作结果 - 影响行数:" << resultMap["affected_rows"];
        } else {
            qDebug() << "  删除操作结果:" << result;
        }
        return;
    }

    // === 6. 兜底：未知类型 ===
    qDebug() << "  未知操作类型结果 - operationId:" << operationId
             << "operationType:" << opType
             << "result:" << result;
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
    qDebug() << "应用程序目录:" << QCoreApplication::applicationDirPath();

    // drogon gRPC Version
    QString drogonVersionQStr = QString::fromUtf8(drogon::getVersion());
    qDebug() << "drogon Version：" << drogonVersionQStr;

    QString grpcVersionQStr = QString::fromUtf8(grpc_version_string());
    qDebug() << "gRPC Version：" << grpcVersionQStr;

    // 使用当前目录的数据库文件
    QString dbFile = "test_database.db";

    DatabaseTest test(dbFile);

    QTimer::singleShot(0, &test, &DatabaseTest::startTest);

    return app.exec();
}
