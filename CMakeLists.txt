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

# --- 新增：在 Windows (MinGW) 下强制静态链接 C++ 标准库 ---
if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
    add_compile_definitions(NOMINMAX) # <--- 全局禁用 Windows 的 min/max 宏
endif()

# --- 新增：专门解决 MSVC (Visual Studio) 乱码和编译错误 ---
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 2. 生成可执行文件
add_executable(AutochessTest ${SOURCES})

# 3. 为 Debug 版本自动添加 _d 后缀
set_target_properties(AutochessTest PROPERTIES DEBUG_POSTFIX "_d")

