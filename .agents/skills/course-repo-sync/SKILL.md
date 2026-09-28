# 课程仓库同步

## 适用场景

本项目的课程源代码位于 NJU Git 服务，个人学习进度保存在 GitHub 私有仓库。需要拉取课程更新、提交学习记录或推送实验进度时使用本流程。

## 关键步骤

1. 操作前检查 `git status --short --branch` 和 `git remote -v`，确认当前分支及工作区内容。
2. 保留 `origin` 作为课程仓库，只从 `origin` 获取课程更新；`github` 是个人私有仓库，保存个人提交。
3. 修改前确认需要提交的文件。使用明确的路径暂存，例如 `git add -- README.md`，不要用 `git add -A` 把无关文件带入提交。
4. 拉取课程更新前先检查上游状态和本地改动，再按当前分支历史选择合并方式；不要盲目重置或强推。
5. 将当前分支推到个人仓库，例如 `git push github M1`。若设置 `branch.M1.pushRemote=github`，普通 `git push` 也会推到个人仓库，而拉取仍跟踪 `origin/M1`。
6. 推送后检查 `git ls-remote github refs/heads/M1`，并复核工作区和两个 remote。

## 常见陷阱

- 不要把个人提交推到课程 `origin`。
- `origin` 和 `github` 是不同用途的 remote；不要为方便而覆盖课程源地址。
- 推送成功只表示远端分支收到提交，不表示课程代码已实现、构建或验证通过。
- 不要把生成的可执行文件、地图临时文件或其他无关内容意外提交。

## 验证方式

确认 `git remote -v` 中两个 URL 符合预期，个人仓库可见性为私有，`git ls-remote github refs/heads/M1` 返回的提交与本地 `M1` 一致，并检查工作区没有意外改动。
