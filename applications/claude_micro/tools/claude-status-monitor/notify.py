#!/usr/bin/env python3
"""Claude Code hook -> Teams 通知(非阻塞)。

两个事件:
  waiting  -> 需要用户确认(PermissionRequest)
  done     -> 任务整体完成(Stop)

为了不阻塞 hook,前台进程只负责派生一个脱离的后台 worker 然后立刻退出;
真正的网络 POST(最长 10s 超时)在 worker 里完成,不落在 Claude Code 的
关键路径上。
"""
import os
import subprocess
import sys
from pathlib import Path

# Teams Workflow webhook URL 从环境变量读取,不要写进代码仓库
TEAMS_WORKFLOW_URL = os.environ.get('TEAMS_WORKFLOW_URL', '')

# event -> (标题模板, 正文, 级别)
EVENTS = {
    'waiting': ('[{project}] 等待操作', 'Claude Code 需要你的确认才能继续,请切回终端处理。', 'warning'),
    'done':    ('[{project}] 任务完成', 'Claude Code 已处理完当前任务。', 'success'),
}


def project_name() -> str:
    # Claude Code 会给 hook 注入 CLAUDE_PROJECT_DIR;退化到 cwd。
    project_dir = os.environ.get('CLAUDE_PROJECT_DIR') or os.getcwd()
    return Path(project_dir).name


def send(event: str, project: str) -> None:
    """worker 路径:真正发起网络请求(可能耗时,运行在后台)。"""
    if not TEAMS_WORKFLOW_URL:
        return
    from TeamsWorkflowNotifier import TeamsWorkflowNotifier

    title_tmpl, message, severity = EVENTS[event]
    title = title_tmpl.format(project=project)
    TeamsWorkflowNotifier(TEAMS_WORKFLOW_URL).send_alert(title, message, severity=severity)


def spawn_detached(event: str, project: str) -> None:
    """派生一个脱离父进程的后台 worker,然后立刻返回。"""
    creationflags = 0
    if os.name == 'nt':
        # DETACHED_PROCESS | CREATE_NO_WINDOW:不挂在父进程上,也不弹窗口
        creationflags = 0x00000008 | 0x08000000
    subprocess.Popen(
        [sys.executable, os.path.abspath(__file__), '--worker', event, project],
        creationflags=creationflags,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        close_fds=True,
    )


def main() -> None:
    args = sys.argv[1:]
    if not args:
        print('Usage: python notify.py <waiting|done>')
        sys.exit(1)

    # 后台 worker 路径
    if args[0] == '--worker':
        if len(args) >= 3 and args[1] in EVENTS:
            send(args[1], args[2])
        return

    # 前台 hook 路径:派生后台进程后立刻退出,不等待网络
    event = args[0].lower()
    if event not in EVENTS:
        print(f'Unknown event: {event}. Use: waiting, done', file=sys.stderr)
        sys.exit(1)
    if not TEAMS_WORKFLOW_URL:
        print('TEAMS_WORKFLOW_URL not set, skip Teams notification', file=sys.stderr)
        return
    spawn_detached(event, project_name())


if __name__ == '__main__':
    main()
