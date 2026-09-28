# 课程仓库同步

## 适用场景

本项目的课程源代码位于 NJU Git 服务，个人学习进度保存在 GitHub 仓库。需要拉取课程更新、提交学习记录或推送实验进度时使用本流程。

## 关键步骤

1. 操作前检查 `git status --short --branch` 和 `git remote -v`，确认当前分支及工作区内容。
2. `origin` 是个人 GitHub 仓库，`upstream` 是课程仓库。保留两个 remote，用 `upstream` 获取课程更新，用 `origin` 保存个人提交。
3. 修改前确认需要提交的文件。使用明确的路径暂存，例如 `git add -- README.md`，不要用 `git add -A` 把无关文件带入提交。
4. 拉取课程更新前先检查 `upstream` 状态和本地改动，再按当前分支历史选择合并方式；不要盲目重置或强推。
5. 将当前分支推到个人仓库，例如 `git push origin M1`。设置 `branch.M1.pushRemote=origin` 后，普通 `git push` 也会推到个人仓库；`branch.M1.remote=upstream` 让拉取继续跟踪 `upstream/M1`。
6. 推送后检查 `git ls-remote origin refs/heads/M1`，并复核工作区和两个 remote。

## 常见陷阱

- 不要把个人提交推到课程 `upstream`。
- 命令中显式写出的 remote 会决定推送目标：`git push upstream` 会发往课程仓库，即使默认推送目标是 `origin`。
- 推送成功只表示远端分支收到提交，不表示课程代码已实现、构建或验证通过。
- 不要把生成的可执行文件、地图临时文件或其他无关内容意外提交。

## 验证方式

确认 `git remote -v` 中两个 URL 符合预期，个人仓库可见性符合用户要求，`git ls-remote origin refs/heads/M1` 返回的提交与本地 `M1` 一致，并检查工作区没有意外改动。
