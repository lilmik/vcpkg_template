# 项目引入vcpkg包管理工具安装的依赖的静态库,下面的内容需要做一些调整

# 新版本的CMakeLists.txt配置文件
cmake_minimum_required(VERSION 3.20)

# 引入vcpkg工具链
# 使用构建好的带vcpkg的alpine镜像来编译为完全静态链接的可执行文件
# 镜像链接:https://hub.docker.com/r/ignislee/alpine319-static-compiler
# 编译时发现了一些库提示缺失ICU和i18n,通过vcpkg补足

# set(CMAKE_TOOLCHAIN_FILE "/opt/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "vcpkg toolchain file" FORCE)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE
        "/opt/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file"
    )
endif()

set(VCPKG_TARGET_TRIPLET "microear-x64-linux-dynamic" CACHE STRING "" FORCE)
set(VCPKG_HOST_TRIPLET   "microear-x64-linux-dynamic" CACHE STRING "" FORCE)



project(qt_app VERSION 0.0.1 LANGUAGES C CXX)

# 根据 BUILD_SHARED_LIBS 判断是否使用静态链接
# if(BUILD_SHARED_LIBS)
#     message(STATUS "Configuring for dynamic linking")
# else()
#     message(STATUS "Configuring for static linking")
#     
#     # 设置静态链接标志
#     set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libstdc++ -static-libgcc")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")
#     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static")
# endif()


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 使用 project() 的版本号
set(APP_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(APP_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(APP_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(APP_VERSION "${PROJECT_VERSION}")

# 获取构建时间（简化格式）
string(TIMESTAMP BUILD_DATE "%Y%m%d")
string(TIMESTAMP BUILD_TIME "%H%M%S")
set(BUILD_TIMESTAMP "${BUILD_DATE}-${BUILD_TIME}")

# 获取 Git 信息
find_package(Git)
if(GIT_FOUND)
    message(STATUS "Found Git: ${GIT_EXECUTABLE}")
    
    # Git 提交哈希（短）
    execute_process(
        # COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_RESULT
    )
    
    if(NOT GIT_RESULT EQUAL 0)
        set(GIT_COMMIT_HASH "nogit")
        message(WARNING "Failed to get Git commit hash")
    endif()
    
    # 检查是否有未提交的更改
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_STATUS_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_RESULT
    )
    
    if(GIT_RESULT EQUAL 0 AND GIT_STATUS_OUTPUT STREQUAL "")
        set(GIT_DIRTY "")
    else()
        set(GIT_DIRTY "-Dirty")
    endif()
    
    # 组合 Git 信息
    set(GIT_INFO "${GIT_COMMIT_HASH}${GIT_DIRTY}")
    
else()
    message(WARNING "Git not found")
    set(GIT_INFO "nogit")
endif()

# 根据构建类型添加后缀
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_TYPE_SUFFIX "-Debug")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(BUILD_TYPE_SUFFIX "-Release")
else()
    set(BUILD_TYPE_SUFFIX "")
endif()

# 构建完整版本字符串：版本号-git信息-构建时间-构建类型
set(FULL_VERSION_STRING "${APP_VERSION}-${GIT_INFO}-${BUILD_TIMESTAMP}${BUILD_TYPE_SUFFIX}")

# 打印调试信息
message(STATUS "App Version: ${APP_VERSION}")
message(STATUS "Git Info: ${GIT_INFO}")
message(STATUS "Build Timestamp: ${BUILD_TIMESTAMP}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Full Version: ${FULL_VERSION_STRING}")

# 配置版本头文件
configure_file(
    ${CMAKE_SOURCE_DIR}/version.h.in
    ${CMAKE_BINARY_DIR}/version.h
    @ONLY
)

# 包含生成的头文件目录
include_directories(${CMAKE_BINARY_DIR})


# 检测架构和编译器
message(STATUS "Target architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# 在 project() 之后强制设置编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Configuring Debug build")
    
    # 核心调试选项（最小化，避免干扰线程检测）
    add_compile_options(
        -g           # 生成完整调试信息
        -O0          # 无优化
        -DDEBUG      # 启用调试宏
        -Wall -Wextra    # 添加基本警告
        -fno-omit-frame-pointer  # 堆栈跟踪
        -fno-inline              # 禁用内联
    )
    
    # 基本警告
    add_compile_options(
        -Wall        # 基本警告
        -Wextra      # 额外警告
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Release 版本：优化 + 保护 + 无调试信息 + 符号剥离
    add_compile_options(
        -O3          # 最高级别优化（性能+代码结构破坏）
        -DNDEBUG     # 禁用断言和调试代码
        -flto        # 链接时优化（跨模块代码结构破坏）
        -g0          # 完全禁用调试信息生成
        -fPIE        # 位置无关代码（ASLR支持）
        -Wformat -Wformat-security -Werror=format-security  # 格式化字符串安全检查
    )

    # 链接器选项（按功能分组，每项都有详细注释）

    # 1. 代码优化和保护
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  -flto=auto")           # 链接时优化（破坏代码结构）,-flto=auto消除编译时的警告
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")            # 位置无关执行（ASLR支持")

    # 2. 逆向工程防护
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--strip-all")     # 剥离所有符号
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--build-id=none") # 移除构建标识符
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--discard-all")   # 丢弃本地符号

    # 3. 运行时安全保护
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,now")          # 立即绑定
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,relro")        # 只读重定位
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,noexecstack")  # 栈不可执行

    # 架构特定的保护选项
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|i?86")
        message(STATUS "Enabling x86/x64 specific protections")
        add_compile_options(-fcf-protection=full)  # 控制流完整性保护
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        message(STATUS "Enabling ARM64 specific protections")
        add_compile_options(-mbranch-protection=standard)  # ARM分支目标保护
    endif()

    # 编译器特定的栈保护选项
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
            add_compile_options(-fstack-protector-strong -fstack-clash-protection)
        else()
            add_compile_options(-fstack-protector-strong)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(-fstack-protector-strong)
    endif()
    
endif()

# 自定义输出目录
set(DEBUG_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/output/debug" CACHE PATH "Debug output directory")
set(RELEASE_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/output/release" CACHE PATH "Release output directory")

# 查找依赖
find_package(Qt6 REQUIRED COMPONENTS Core Scxml)


# 添加可执行文件
add_executable(qt_app
    src/main.cpp 
)


# 只有在静态构建时才添加静态链接选项
# if(NOT BUILD_SHARED_LIBS)
#     target_link_options(qt_app PRIVATE 
#         "-static"
#         "-static-libstdc++" 
#         "-static-libgcc"
#     )
# endif()

# 设置目标属性
set_target_properties(${PROJECT_NAME} PROPERTIES
    # 不设置 VERSION 和 SOVERSION，避免自动添加版本后缀
    OUTPUT_NAME "${PROJECT_NAME}-${FULL_VERSION_STRING}"
    # 明确设置可执行文件后缀为空
    SUFFIX ""
)


target_link_libraries(qt_app
    PRIVATE 
    Qt6::Core
    Qt6::Scxml
)

# === 把 XML 状态机文件拷贝到构建目录，运行时能直接读到 ===
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/statemachine.scxml
    ${CMAKE_CURRENT_BINARY_DIR}/statemachine.scxml
    COPYONLY
)
