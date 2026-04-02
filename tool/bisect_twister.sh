#!/bin/bash
#
# bisect_twister.sh - 自动 bisect + twister 定位问题 commit
#
# 用法:
#   ./bisect_twister.sh <good_commit> <bad_commit> [options]
#   示例: ./bisect_twister.sh HEAD 760a74145ed -t tests/boards/realtek/bee_osif -s /dev/ttyUSB0 -b 115200 -w jlink
#
# 参数说明:
#   <good_commit> : 已知正常的 commit (可以是 HEAD 或具体 commit hash)
#   <bad_commit>  : 引入 bug 的 commit (需要定位的目标)
#
# 选项:
#   -t, --testcase    测试用例路径 (必需)
#   -s, --serial      串口设备 (必需)
#   -p, --board       开发板 (必需)
#   -b, --baud        波特率 (默认: 115200)
#   -w, --west-runner west runner (默认: jlink)
#
# 环境要求:
#   - 已激活 zephyr venv: source ~/zephyrproject/.venv/bin/activate
#   - 硬件: 开发板已连接
#   - west 已经配置好
#

set -e

# 默认值
SERIAL_BAUD="115200"
WEST_RUNNER="jlink"
ZEPHYR_BASE="/home/zhiyuan/upstream/zephyr"

# 解析参数
GOOD_COMMIT=""
BAD_COMMIT=""
TEST_PATH=""
SERIAL_PORT=""
BOARD=""

show_usage() {
    echo "用法: $0 <good_commit> <bad_commit> [options]"
    echo ""
    echo "参数:"
    echo "  <good_commit>  已知正常的 commit (如: HEAD)"
    echo "  <bad_commit>   引入 bug 的 commit (如: 760a74145ed)"
    echo ""
    echo "选项:"
    echo "  -t, --testcase    测试用例路径 (必需)"
    echo "  -s, --serial      串口设备 (必需)"
    echo "  -p, --board       开发板 (必需)"
    echo "  -b, --baud        波特率 (默认: 115200)"
    echo "  -w, --west-runner west runner (默认: jlink)"
    echo "  -h, --help        显示帮助"
    echo ""
    echo "示例:"
    echo "  $0 HEAD 760a74145ed -t tests/boards/realtek/bee_osif -s /dev/ttyUSB0 -p rtl8752h_evb/rtl8752hjl"
    echo "  $0 HEAD abc123def -t tests/kernel/lifo -s /dev/ttyUSB1 -p rtl8752h_evb/rtl8752hjl -b 921600 -w openocd"
    exit 1
}

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--testcase)
            TEST_PATH="$2"
            shift 2
            ;;
        -s|--serial)
            SERIAL_PORT="$2"
            shift 2
            ;;
        -b|--baud)
            SERIAL_BAUD="$2"
            shift 2
            ;;
        -w|--west-runner)
            WEST_RUNNER="$2"
            shift 2
            ;;
        -p|--board)
            BOARD="$2"
            shift 2
            ;;
        -h|--help)
            show_usage
            ;;
        *)
            if [ -z "$GOOD_COMMIT" ]; then
                GOOD_COMMIT="$1"
            elif [ -z "$BAD_COMMIT" ]; then
                BAD_COMMIT="$1"
            else
                echo "错误: 未知参数 '$1'"
                show_usage
            fi
            shift
            ;;
    esac
done

# 检查必需参数
if [ -z "$GOOD_COMMIT" ] || [ -z "$BAD_COMMIT" ]; then
    echo "错误: 缺少必需参数"
    echo ""
    echo "提示: 第一个是 GOOD commit (正常的), 第二个是 BAD commit (引入 bug 的)"
    echo ""
    show_usage
fi

if [ -z "$TEST_PATH" ]; then
    echo "错误: 请指定测试用例路径 (-t, --testcase)"
    show_usage
fi

if [ -z "$SERIAL_PORT" ]; then
    echo "错误: 请指定串口设备 (-s, --serial)"
    show_usage
fi

if [ -z "$BOARD" ]; then
    echo "错误: 请指定开发板 (-p, --board)"
    show_usage
fi

echo "=============================================="
echo "Git Bisect + Twister 自动定位 Bug Commit"
echo "=============================================="
echo "  [GOOD] Commit: $GOOD_COMMIT  ← 正常工作的版本"
echo "  [BAD]  Commit: $BAD_COMMIT   ← 引入 bug 的版本"
echo ""
echo "  Board:       $BOARD"
echo "  Test Path:   $TEST_PATH"
echo "  Serial:      $SERIAL_PORT @ ${SERIAL_BAUD}bps"
echo "  West Runner: $WEST_RUNNER"
echo "=============================================="

# 创建 bisect 测试脚本
BISECT_SCRIPT="/tmp/bisect_twister_run.sh"
cat > "$BISECT_SCRIPT" << SCRIPT_EOF
#!/bin/bash
set -e

BOARD="$BOARD"
TEST_PATH="$TEST_PATH"
SERIAL_PORT="$SERIAL_PORT"
SERIAL_BAUD="$SERIAL_BAUD"
WEST_RUNNER="$WEST_RUNNER"
ZEPHYR_BASE="$ZEPHYR_BASE"

cd "$ZEPHYR_BASE"

# 清理之前的构建
rm -rf build/ || true

# 编译并测试
echo "=== Building and testing at commit \$(git rev-parse HEAD) ==="

source ~/zephyrproject/.venv/bin/activate

# 使用 twister 测试
# --device-testing: 硬件测试模式
# --device-serial: 串口号
# --device-serial-baud: 波特率
# --west-runner: 烧录工具
# -v: verbose
# --no-clean: 不清理构建（加速）
west twister -p "$BOARD" -T "$TEST_PATH" \
    --device-testing \
    --device-serial "$SERIAL_PORT" \
    --device-serial-baud "$SERIAL_BAUD" \
    --west-runner "$WEST_RUNNER" \
    -v --no-clean

# twister 成功返回 0，失败返回非 0
exit $?
SCRIPT_EOF

chmod +x "$BISECT_SCRIPT"

# 启动 bisect
cd "$ZEPHYR_BASE"
echo ""
echo ">>> 开始 Bisect <<<"
echo ""

git bisect start "$BAD_COMMIT" "$GOOD_COMMIT"

echo ""
echo ">>> 运行 Bisect <<<"
echo ""

# 执行 bisect
git bisect run "$BISECT_SCRIPT"

echo ""
echo ">>> Bisect 完成 <<<"
echo ""

# 显示结果
git bisect log
git bisect visualize --oneline

echo ""
echo "=============================================="
echo "结果: 找到导致测试失败的 commit"
echo "=============================================="

# 清理
git bisect reset

echo "Done!"