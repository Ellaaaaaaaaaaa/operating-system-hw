#!/bin/bash

# 定义测试目录和可执行文件名
SRC_DIR="test_source"
DEST_DIR="test_destination"
EXEC_FILE="./my_rsync_thread" # <-- 修改为新的可执行文件名

# 检查可执行文件是否存在
if [ ! -f "$EXEC_FILE" ]; then
    echo "错误: 未找到可执行文件 '$EXEC_FILE'。"
    echo "请先使用 'gcc -o my_rsync_thread my_rsync_thread.c -pthread' 命令编译C代码。"
    exit 1
fi

# 函数：清理并准备测试环境
setup() {
    echo "--- 正在设置测试环境 ---"
    rm -rf "$SRC_DIR" "$DEST_DIR"
    mkdir -p "$SRC_DIR/subdir1"
    mkdir -p "$DEST_DIR"
}

# 函数：报告测试结果
report_result() {
    if [ $? -eq 0 ]; then
        echo "测试通过: $1"
    else
        echo "测试失败: $1"
        cleanup
        exit 1
    fi
}

# 函数：清理测试目录
cleanup() {
    echo "--- 正在清理测试环境 ---"
    rm -rf "$SRC_DIR" "$DEST_DIR"
}

# --- 开始测试 ---

# 1. 首次同步测试
setup
echo "--- 场景1: 首次同步 ---"
echo "Hello World" > "$SRC_DIR/file1.txt"
echo "Another file" > "$SRC_DIR/subdir1/file2.txt"
date > "$SRC_DIR/date.log"

echo "创建的文件结构:"
find "$SRC_DIR"

echo "运行 my_rsync_thread..."
$EXEC_FILE "$SRC_DIR" "$DEST_DIR"

diff -r "$SRC_DIR" "$DEST_DIR"
report_result "首次同步后目录内容应完全一致"

# 2. 增量更新测试
echo ""
echo "--- 场景2: 增量更新 ---"
sleep 2 # 确保修改时间有明显变化
echo "Modified content" > "$SRC_DIR/file1.txt"
echo "A new file is added" > "$SRC_DIR/new_file.txt"

echo "修改后的源目录:"
find "$SRC_DIR"

echo "再次运行 my_rsync_thread..."
$EXEC_FILE "$SRC_DIR" "$DEST_DIR"

diff -r "$SRC_DIR" "$DEST_DIR"
report_result "增量更新后目录内容应再次一致"

# --- 测试结束 ---
