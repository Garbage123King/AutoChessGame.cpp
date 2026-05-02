cmake_minimum_required(VERSION 3.10)

# 项目名称
project(AutochessCore)

# 强制使用 C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 收集所有的源码文件
set(SOURCES
    GameConfig.cpp
    Deck.cpp
    Player.cpp
    BattleEngine.cpp
    GameManager.cpp
    main.cpp
)
# 1. 设置全局输出目录（必须在 add_executable 之前声明！）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# 2. 生成可执行文件
add_executable(AutochessTest ${SOURCES})

# 3. 为 Debug 版本自动添加 _d 后缀
set_target_properties(AutochessTest PROPERTIES DEBUG_POSTFIX "_d")

