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

# 生成可执行文件
add_executable(AutochessTest ${SOURCES})