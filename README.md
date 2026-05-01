# 1. 确保在源码目录下，创建一个 build 文件夹用于存放编译生成的文件
mkdir build
cd build

# 2. 让 CMake 生成构建系统文件
cmake ..

# 3. 开始编译
cmake --build .

# 4. 运行编译好的程序
# Windows 下运行：
Debug\AutochessTest.exe  # 如果是 MSVC
# 或者
.\AutochessTest.exe      # 如果是 MinGW

# Mac/Linux 下运行：
./AutochessTest
