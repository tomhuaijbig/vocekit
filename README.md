# VoceKit 工作区

当前唯一继续开发和发布的项目是 [`vocekit-qt6`](vocekit-qt6)。它使用 Qt 6.11.1、MinGW 13.1 64 位和 C++17，构建、测试、部署与打包均以该目录中的脚本为准。GitHub Actions 另用 Qt 6.10.3 + MinGW 13.1 做向后兼容构建，因为当前 `aqtinstall` 尚不能解析 Qt 6.11 的新仓库布局；这不改变本机 Qt 6.11.1 开发基线。

`vocekit` 是 Qt 5.9 遗留源码区，只用于迁移核对和历史追溯，不再接受功能开发，也不能作为发布输入。仓库中已有的 Qt 5 本地构建目录可能包含未提交的测试产物，因此未经单独备份和确认不得删除、移动或覆盖。

常用命令：

```powershell
& .\vocekit-qt6\scripts\build.ps1 -Configuration debug
& .\vocekit-qt6\scripts\run-all-tests.ps1 -Configuration debug
& .\vocekit-qt6\scripts\build.ps1 -Configuration release
& .\vocekit-qt6\scripts\deploy.ps1
& .\vocekit-qt6\scripts\package-test.ps1 -PackageName vocekit-qt6-portable
```

正式对外发布前还必须满足 [`vocekit-qt6/docs/UPDATES.md`](vocekit-qt6/docs/UPDATES.md) 和 [`vocekit-qt6/docs/ACCEPTANCE_MATRIX.md`](vocekit-qt6/docs/ACCEPTANCE_MATRIX.md) 的门槛。
