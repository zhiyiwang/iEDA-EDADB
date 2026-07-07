# Ubuntu / VS Code 安装 Codex 与 Understand-Anything Plugin

本文记录如何在 Ubuntu/WSL 环境中安装 Codex、配置 VS Code，并安装
`Understand-Anything` plugin，使其他服务器上的 agent 可以复现当前环境。

当前本机验证过的目标：

- Ubuntu 22.04 / WSL2
- VS Code + Codex IDE extension
- Codex CLI
- Understand-Anything plugin
- Node.js 22 + pnpm 10

## 1. 推荐部署方式

如果服务器是纯 Ubuntu/headless：

1. 在服务器上安装 Codex CLI 和 plugin 运行依赖。
2. 在本地 Windows / macOS / Linux 的 VS Code 中安装 Codex IDE extension。
3. 使用 Remote SSH 或 WSL 打开远端工程。
4. 在 Codex 里让 agent 使用远端 workspace。

如果是在 Windows + WSL2 上开发：

1. 代码放在 WSL 的 `/home/<user>/...` 下，不建议放在 `/mnt/c/...`。
2. 从 WSL shell 进入工程目录后执行 `code .`。
3. 在 VS Code 设置中开启 Codex 使用 WSL。

## 2. 安装 Codex CLI

在 Ubuntu/WSL shell 中执行：

```bash
curl -fsSL https://chatgpt.com/codex/install.sh | sh
```

然后启动：

```bash
codex
```

首次启动会要求登录 ChatGPT 账号或配置认证。Codex IDE extension 与 CLI 共享
`~/.codex/config.toml` 等配置。

验证：

```bash
which codex
codex --version
```

## 3. VS Code 中安装 Codex

在 VS Code 中：

1. 打开 Extensions。
2. 搜索 `Codex` / `OpenAI Codex`。
3. 安装 OpenAI 发布的 Codex extension。
4. 登录 ChatGPT 账号或按提示配置 API key。

常用命令：

- `Ctrl+Shift+P` 打开 Command Palette。
- 搜索 `Codex`。
- 常用入口包括：
  - `Codex: Open Sidebar`
  - `Codex: New Chat`
  - `Codex: Add File to Thread`

如果在 Windows 上用 WSL 工程，打开 VS Code settings，搜索：

```text
chatgpt.runCodexInWindowsSubsystemForLinux
```

将其设为 `true`，让 Codex 在 WSL 中运行命令和访问 Linux 路径。

## 4. 安装 Node.js 22 与 pnpm

Understand-Anything 需要 Node.js >= 22 和 pnpm。为了避免修改系统自带
`/usr/bin/node`，推荐安装到用户目录：

```bash
set -euo pipefail

INSTALL_DIR="$HOME/.local/node-v22"
TMP_DIR="$(mktemp -d)"
cd "$TMP_DIR"

TARBALL="$(curl -fsSL https://nodejs.org/dist/latest-v22.x/ \
  | grep -o 'node-v22[^"]*linux-x64.tar.xz' \
  | head -n 1)"

curl -fsSLO "https://nodejs.org/dist/latest-v22.x/${TARBALL}"
mkdir -p "$HOME/.local"
rm -rf "$INSTALL_DIR"
tar -xJf "$TARBALL"
mv "${TARBALL%.tar.xz}" "$INSTALL_DIR"

if ! grep -q 'node-v22/bin' "$HOME/.bashrc" 2>/dev/null; then
  printf '\n# Node.js 22 for local development\nexport PATH="$HOME/.local/node-v22/bin:$PATH"\n' >> "$HOME/.bashrc"
fi

if ! grep -q 'node-v22/bin' "$HOME/.profile" 2>/dev/null; then
  printf '\n# Node.js 22 for local development\nexport PATH="$HOME/.local/node-v22/bin:$PATH"\n' >> "$HOME/.profile"
fi

export PATH="$HOME/.local/node-v22/bin:$PATH"
corepack enable
corepack prepare pnpm@10 --activate

node --version
pnpm --version
```

期望：

```text
node v22.x
pnpm 10.x
```

如果新终端里仍然看到旧 Node：

```bash
source ~/.bashrc
export PATH="$HOME/.local/node-v22/bin:$PATH"
```

## 5. 准备 Understand-Anything 源码

如果服务器可以联网，可以 clone：

```bash
mkdir -p ~/proj/agent/plugins
cd ~/proj/agent/plugins
git clone https://github.com/Egonex-AI/Understand-Anything.git
```

如果服务器不能联网，从已有机器复制整个目录：

```bash
scp -r /home/zhiyiwang/proj/agent/plugins/Understand-Anything \
  <user>@<server>:/home/<user>/proj/agent/plugins/
```

本文后续假设路径为：

```bash
UA_REPO="$HOME/proj/agent/plugins/Understand-Anything"
UA_PLUGIN="$UA_REPO/understand-anything-plugin"
```

如果你使用其他路径，替换这两个变量即可。

## 6. 构建 Understand-Anything

```bash
set -euo pipefail

export PATH="$HOME/.local/node-v22/bin:$PATH"
UA_REPO="$HOME/proj/agent/plugins/Understand-Anything"
UA_PLUGIN="$UA_REPO/understand-anything-plugin"

cd "$UA_PLUGIN"
pnpm install --frozen-lockfile
pnpm --filter @understand-anything/core build
pnpm --filter @understand-anything/dashboard build
```

验证：

```bash
test -f "$UA_PLUGIN/packages/core/dist/index.js"
test -f "$UA_PLUGIN/packages/dashboard/dist/index.html"

node -e "import('@understand-anything/core').then(() => console.log('core import ok'))"
```

如果 pnpm 提示 build scripts 被忽略，可以执行：

```bash
pnpm rebuild --pending
pnpm ignored-builds
```

`pnpm ignored-builds` 期望输出没有剩余 ignored builds。

## 7. 将 plugin skills 挂载给 Codex

Codex 官方 skill 搜索位置包括：

- repo scope：`$REPO_ROOT/.agents/skills`
- user scope：`$HOME/.agents/skills`
- admin scope：`/etc/codex/skills`

当前本机 Codex 会话也会读取 `~/.codex/skills`。为了兼容 CLI、IDE 和当前
本地环境，建议两个位置都挂 symlink。

```bash
set -euo pipefail

UA_PLUGIN="$HOME/proj/agent/plugins/Understand-Anything/understand-anything-plugin"

mkdir -p "$HOME/.agents/skills" "$HOME/.codex/skills"

for d in "$UA_PLUGIN"/skills/*; do
  name="$(basename "$d")"
  ln -sfn "$d" "$HOME/.agents/skills/$name"
  ln -sfn "$d" "$HOME/.codex/skills/$name"
done

ln -sfn "$UA_PLUGIN" "$HOME/.understand-anything-plugin"
```

验证：

```bash
ls -ld ~/.agents/skills/understand
ls -ld ~/.codex/skills/understand
ls -ld ~/.understand-anything-plugin
```

如果 Codex 已经打开，建议重启 VS Code/Codex 会话，让新 skills 被重新扫描。

## 8. 使用 Understand-Anything

在 Codex 中可以显式调用：

```text
use $understand-anything:understand at `/home/<user>/proj/agent/plugins/Understand-Anything/understand-anything-plugin/skills/understand/SKILL.md` to analyze `/path/to/project` with `--language zh`
```

如果 slash command 已被识别，也可以直接：

```text
/understand /path/to/project --language zh
```

第一次运行会生成：

```text
<project>/.understand-anything/.understandignore
```

如果你希望测试代码也进入知识图谱，不要在 `.understandignore` 中启用：

```text
test/
**/*Test*
```

生成结果：

```text
<project>/.understand-anything/knowledge-graph.json
<project>/.understand-anything/meta.json
<project>/.understand-anything/fingerprints.json
```

后续常用命令：

```text
/understand-dashboard
/understand-chat 这个项目的核心架构是什么？
/understand-explain include/edadb/DbObjectTraverser.h
/understand-diff
```

## 9. EDADB 当前本机成功案例

本机已成功分析：

```text
/home/zhiyiwang/cs/arch/eda/edadb/edadb
```

输出：

```text
/home/zhiyiwang/cs/arch/eda/edadb/edadb/.understand-anything/knowledge-graph.json
```

验证结果：

```text
files analyzed: 81
test files: 18
nodes: 350
edges: 806
review issues: 0
```

注意：该项目的测试文件命名是 `DbTableOpSelect.cpp` 这类 CTest target 风格，
不是 `*_test.cpp`。Understand-Anything 自带的 `tested_by` linker 对 C/C++
测试文件名比较保守，因此可用 `validates` 边表达 `test -> implementation`
的覆盖关系。

## 10. Windows 记录

### 10.1 安装 WSL

以管理员身份打开 PowerShell：

```powershell
wsl --install
```

安装完成后重启，再进入 Ubuntu：

```powershell
wsl
```

建议把代码放在 WSL home 下：

```bash
mkdir -p ~/cs/arch/eda
cd ~/cs/arch/eda
git clone <repo-url>
```

不要长期在 `/mnt/c/...` 下编译大型 C++ 项目，I/O 和符号链接行为都更容易出问题。

### 10.2 安装 VS Code 与 WSL extension

Windows 上安装：

- Visual Studio Code
- VS Code WSL extension
- Codex IDE extension

从 WSL shell 打开工程：

```bash
cd ~/cs/arch/eda/edadb/edadb
code .
```

确认 VS Code 左下角显示：

```text
WSL: Ubuntu
```

如果没有，使用 Command Palette：

```text
WSL: Reopen Folder in WSL
```

### 10.3 Windows 下记录文档

你可以把本文同步到 Windows 知识库，例如：

```text
C:\Users\zhiyi\KnowVault\Supernova\40_Knowledge\10_CS\41_iEDA\
```

WSL 中访问该目录：

```bash
cd /mnt/c/Users/zhiyi/KnowVault/Supernova/40_Knowledge/10_CS/41_iEDA
```

Windows Explorer 访问 WSL 文件：

```text
\\wsl$\Ubuntu\home\<user>\
```

## 11. 常见问题

### `corepack: command not found`

通常说明当前 shell 仍然使用系统旧 Node。

```bash
export PATH="$HOME/.local/node-v22/bin:$PATH"
which node
node --version
which corepack
```

### `pnpm: command not found`

```bash
export PATH="$HOME/.local/node-v22/bin:$PATH"
corepack enable
corepack prepare pnpm@10 --activate
pnpm --version
```

### Codex 看不到 `/understand`

检查 symlink：

```bash
ls -ld ~/.agents/skills/understand
ls -ld ~/.codex/skills/understand
```

然后重启 VS Code/Codex 会话。

也可以用显式 skill 路径调用：

```text
use $understand-anything:understand at `/absolute/path/to/understand-anything-plugin/skills/understand/SKILL.md` to analyze `/path/to/project` with `--language zh`
```

### dashboard 无法启动

先确认前端已经构建：

```bash
UA_PLUGIN="$HOME/proj/agent/plugins/Understand-Anything/understand-anything-plugin"
test -f "$UA_PLUGIN/packages/dashboard/dist/index.html"
```

必要时重新构建：

```bash
cd "$UA_PLUGIN"
export PATH="$HOME/.local/node-v22/bin:$PATH"
pnpm --filter @understand-anything/dashboard build
```

## 12. 官方依据

本文中的 Codex 行为基于 OpenAI Codex manual 中以下结论：

- Codex IDE extension 可在 VS Code 中使用 Codex，并与 Codex CLI 共享配置。
- Codex IDE extension 支持 Command Palette 命令和 slash commands。
- Skills 是可复用 workflow，Codex 会按 `SKILL.md` 的 `name` / `description`
  自动或显式触发。
- Codex 会扫描 `$HOME/.agents/skills` 等 skill 目录，并支持 symlink。
- Plugins 是 skills、apps、MCP server 的可安装分发单元；本地 plugin 可以通过
  marketplace 或直接 skill symlink 方式使用。
- Windows 上可以原生运行 Codex，也可以通过 WSL2 运行；当工程和工具链在 Linux
  中时，推荐用 VS Code Remote WSL 打开工程。
