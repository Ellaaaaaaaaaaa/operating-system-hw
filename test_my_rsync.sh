#!/bin/bash

# ==============================================================================
# APUE 作业自动化测试脚本 for my_rsync
# ==============================================================================

# --- 配置 ---
# 定义颜色，用于输出
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 定义你的C程序的可执行文件路径
SYNC_PROGRAM="./my_rsync"

# --- 检查环境 ---
if [ ! -f "$SYNC_PROGRAM" ]; then
    echo -e "${RED}错误: 编译好的程序 '$SYNC_PROGRAM' 不存在!${NC}"
    echo -e "${YELLOW}请先执行 'gcc -o my_rsync my_rsync.c' 来编译代码。${NC}"
    exit 1
fi

# --- 测试准备 ---
echo "--- 1. 准备测试环境 ---"
# 清理旧的测试目录，确保每次测试都在干净的环境中进行
rm -rf test_src test_dest
mkdir test_src test_dest
echo "创建了 'test_src' 和 'test_dest' 目录。"
echo

# 使用 trap 命令确保脚本退出时总是执行清理工作
trap "echo; echo '--- 6. 清理测试环境 ---'; rm -rf test_src test_dest; echo '测试目录已删除。'" EXIT

# ==============================================================================
# --- 开始测试 ---
# ==============================================================================

# --- 测试用例 1: 目标文件不存在 ---
echo "--- 2. 测试用例 1: 目标文件不存在 ---"
echo "Hello APUE" > test_src/file1.txt
chmod 644 test_src/file1.txt
echo "创建源文件 test_src/file1.txt"

# 执行同步
"$SYNC_PROGRAM" test_src/file1.txt test_dest/file1.txt

# 验证结果
if [ -f "test_dest/file1.txt" ] && diff -q test_src/file1.txt test_dest/file1.txt; then
    echo -e "结果: ${GREEN}[ 通过 ]${NC} - 目标文件已成功创建且内容一致。"
else
    echo -e "结果: ${RED}[ 失败 ]${NC} - 目标文件未创建或内容不一致。"
    exit 1
fi
echo

# --- 测试用例 2: 目标文件存在，但内容不同 ---
echo "--- 3. 测试用例 2: 内容不同 ---"
echo "Old Content" > test_dest/file1.txt
# 关键步骤：等待1秒，确保文件的修改时间戳(mtime)一定不同
# 这是为了防止因脚本执行过快导致时间戳相同，从而让程序误判
sleep 1


# 执行同步
"$SYNC_PROGRAM" test_src/file1.txt test_dest/file1.txt

# 验证结果
if diff -q test_src/file1.txt test_dest/file1.txt; then
    echo -e "结果: ${GREEN}[ 通过 ]${NC} - 目标文件内容已成功更新。"
else
    echo -e "结果: ${RED}[ 失败 ]${NC} - 目标文件内容同步失败。"
    exit 1
fi
echo

# --- 测试用例 3: 内容相同，但权限不同 ---
echo "--- 4. 测试用例 3: 权限不同 ---"
# 先确保文件内容一致
cp test_src/file1.txt test_dest/file1.txt
# 修改目标文件的权限
chmod 777 test_dest/file1.txt
echo "将目标文件权限修改为 777。"

# 执行同步
"$SYNC_PROGRAM" test_src/file1.txt test_dest/file1.txt

# 验证结果
src_perms=$(stat -c "%a" test_src/file1.txt)
dest_perms=$(stat -c "%a" test_dest/file1.txt)
if [ "$src_perms" == "$dest_perms" ]; then
    echo -e "结果: ${GREEN}[ 通过 ]${NC} - 目标文件权限已成功同步为 $dest_perms。"
else
    echo -e "结果: ${RED}[ 失败 ]${NC} - 期望权限 '$src_perms'，实际权限是 '$dest_perms'。"
    exit 1
fi
echo

# --- 测试用例 4: 文件已完全同步 ---
echo "--- 5. 测试用例 4: 文件已同步 ---"
echo "此时源和目标文件应完全一致，程序不应执行复制操作。"

# 执行同步并捕获输出
output=$("$SYNC_PROGRAM" test_src/file1.txt test_dest/file1.txt)

# 验证结果
if echo "$output" | grep -q "already in sync"; then
    echo -e "结果: ${GREEN}[ 通过 ]${NC} - 程序正确识别出文件已同步。"
else
    echo -e "结果: ${RED}[ 失败 ]${NC} - 程序没有打印 'already in sync'，输出为: $output"
    exit 1
fi
echo

# --- 测试用例 5: 源文件是只读的 ---
echo "--- 6. 测试用例 5: 源文件只读 ---"
echo "Read Only Test" > test_src/readonly.txt
chmod 400 test_src/readonly.txt
echo "创建只读源文件 test_src/readonly.txt (权限 400)。"

# 执行同步
"$SYNC_PROGRAM" test_src/readonly.txt test_dest/readonly.txt

# 验证结果
src_perms_ro=$(stat -c "%a" test_src/readonly.txt)
dest_perms_ro=$(stat -c "%a" test_dest/readonly.txt 2>/dev/null) # 2>/dev/null 避免文件不存在时报错
if [ -f "test_dest/readonly.txt" ] && diff -q test_src/readonly.txt test_dest/readonly.txt && [ "$src_perms_ro" == "$dest_perms_ro" ]; then
    echo -e "结果: ${GREEN}[ 通过 ]${NC} - 只读文件的内容和权限 (400) 均已成功同步。"
else
    echo -e "结果: ${RED}[ 失败 ]${NC} - 只读文件同步失败。"
    echo "  - 目标文件是否存在? $([ -f test_dest/readonly.txt ] && echo Yes || echo No)"
    echo "  - 内容是否一致? $(diff -q test_src/readonly.txt test_dest/readonly.txt && echo Yes || echo No)"
    echo "  - 权限是否一致? (期望 $src_perms_ro, 实际 $dest_perms_ro)"
    exit 1
fi
echo