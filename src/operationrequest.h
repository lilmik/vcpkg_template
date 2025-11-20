// operationrequest.h
#ifndef OPERATIONREQUEST_H
#define OPERATIONREQUEST_H

#include <chrono>
#include <map>
#include <string>
#include <vector>

// 使用标准C++类型定义操作请求，不依赖任何数据库库
struct OperationRequest {
    std::string id;
    std::string type;
    std::map<std::string, std::string> string_params;
    std::map<std::string, int> int_params;
    std::map<std::string, double> double_params;
    std::map<std::string, bool> bool_params;
    std::map<std::string, std::vector<std::string>> string_array_params;
    std::map<std::string, std::vector<int>> int_array_params;

    std::chrono::system_clock::time_point timestamp;

    OperationRequest(const std::string& opType)
        : type(opType)
        , timestamp(std::chrono::system_clock::now())
    {
        id = generateUUID();
    }

    OperationRequest(const std::string& opType,
        const std::map<std::string, std::string>& params)
        : type(opType)
        , string_params(params)
        , timestamp(std::chrono::system_clock::now())
    {
        id = generateUUID();
    }

    // 设置参数的方法
    void setStringParam(const std::string& key, const std::string& value)
    {
        string_params[key] = value;
    }

    void setIntParam(const std::string& key, int value)
    {
        int_params[key] = value;
    }

    void setDoubleParam(const std::string& key, double value)
    {
        double_params[key] = value;
    }

    void setBoolParam(const std::string& key, bool value)
    {
        bool_params[key] = value;
    }

    void setStringArrayParam(const std::string& key, const std::vector<std::string>& value)
    {
        string_array_params[key] = value;
    }

    void setIntArrayParam(const std::string& key, const std::vector<int>& value)
    {
        int_array_params[key] = value;
    }

    // 获取参数的方法
    std::string getStringParam(const std::string& key, const std::string& defaultValue = "") const
    {
        auto it = string_params.find(key);
        return it != string_params.end() ? it->second : defaultValue;
    }

    int getIntParam(const std::string& key, int defaultValue = 0) const
    {
        auto it = int_params.find(key);
        return it != int_params.end() ? it->second : defaultValue;
    }

    double getDoubleParam(const std::string& key, double defaultValue = 0.0) const
    {
        auto it = double_params.find(key);
        return it != double_params.end() ? it->second : defaultValue;
    }

    bool getBoolParam(const std::string& key, bool defaultValue = false) const
    {
        auto it = bool_params.find(key);
        return it != bool_params.end() ? it->second : defaultValue;
    }

    std::vector<std::string> getStringArrayParam(const std::string& key) const
    {
        auto it = string_array_params.find(key);
        return it != string_array_params.end() ? it->second : std::vector<std::string>();
    }

    std::vector<int> getIntArrayParam(const std::string& key) const
    {
        auto it = int_array_params.find(key);
        return it != int_array_params.end() ? it->second : std::vector<int>();
    }

    // 检查操作类型
    bool isQueryType() const { return type == "query"; }
    bool isTransactionType() const { return type == "transaction"; }

private:
    std::string generateUUID()
    {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        return "op_" + std::to_string(millis) + "_" + std::to_string(rand() % 10000);
    }
};

// 操作结果结构体
struct OperationResult {
    std::string operation_id;
    bool success;
    std::string error_message;
    std::string result_data; // JSON格式的结果数据
    std::chrono::system_clock::time_point completion_time;

    OperationResult(const std::string& opId, bool succ = true)
        : operation_id(opId)
        , success(succ)
        , completion_time(std::chrono::system_clock::now())
    {
    }

    OperationResult(const std::string& opId, bool succ, const std::string& error)
        : operation_id(opId)
        , success(succ)
        , error_message(error)
        , completion_time(std::chrono::system_clock::now())
    {
    }
};

#endif // OPERATIONREQUEST_H
