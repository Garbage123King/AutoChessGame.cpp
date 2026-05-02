#!/bin/bash

# 遇到错误立即停止脚本
set -e

echo "========================================"
echo "    🚀 启动 AutochessCore 一键构建流程"
echo "========================================"

# --- 1. 构建 Debug 版本 ---
echo -e "\n[1/2] 开始构建 Debug 版本 (带调试符号 _d.exe)..."
mkdir -p build-debug
cd build-debug
# 配置 CMake
cmake -DCMAKE_BUILD_TYPE=Debug ..
# 执行编译 (-j4 表示使用 4 个 CPU 核心并行编译，速度更快)
cmake --build . -j4
cd ..
echo "✅ Debug 版本构建完成！"

# --- 2. 构建 Release 版本 ---
echo -e "\n[2/2] 开始构建 Release 版本 (开启 -O3 极致优化)..."
mkdir -p build-release
cd build-release
# 配置 CMake
cmake -DCMAKE_BUILD_TYPE=Release ..
# 执行编译
cmake --build . -j4
cd ..
echo "✅ Release 版本构建完成！"

echo "========================================"
echo " 🎉 全部构建成功！输出文件位于 bin/ 目录："
echo "========================================"

# 列出 bin 目录下的文件详情，方便确认
ls -lh bin/

echo -e "\n运行建议:"
echo "调试找 Bug 请运行: ./bin/AutochessTest_d.exe"
echo "体验极致性能请运行: ./bin/AutochessTest.exe"
