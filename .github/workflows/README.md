# GitHub Actions 工作流说明

本项目使用 GitHub Actions 实现自动编译和发布。

## 工作流文件

### 1. `ci.yml` - 持续集成
**触发条件：**
- 推送到 `master` 或 `main` 分支
- 创建 Pull Request

**功能：**
- 自动编译控制台版本和 GUI 版本
- 上传构建产物到 Actions Artifacts
- 验证代码可以正常编译

### 2. `release.yml` - 自动发布
**触发条件：**
- 推送以 `v` 开头的标签（如 `v1.3`, `v2.0`）

**功能：**
- 自动编译两个版本
- 打包所有文件为 ZIP
- 创建 GitHub Release
- 上传可执行文件和压缩包

## 如何发布新版本

### 步骤 1：更新版本号
在 `README.md` 的更新日志中添加新版本信息：

```markdown
### v1.3 (2025-11-07)
- 新增功能描述
- 修复的问题
```

### 步骤 2：提交更改
```bash
git add .
git commit -m "chore: 准备发布 v1.3"
```

### 步骤 3：创建并推送标签
```bash
# 创建标签
git tag -a v1.3 -m "Release v1.3"

# 推送标签到远程仓库
git push origin v1.3
```

### 步骤 4：等待自动构建
1. 推送标签后，GitHub Actions 会自动触发
2. 在 GitHub 仓库的 "Actions" 标签页查看构建进度
3. 构建完成后，会在 "Releases" 页面自动创建新版本

## 发布内容

每个 Release 包含：
- `BluetoothMonitor.exe` - 控制台版本
- `BluetoothMonitorGUI.exe` - GUI 版本
- `BluetoothAutoConnect-v*.zip` - 完整压缩包（包含可执行文件、配置文件、README、LICENSE）

## 版本号规范

遵循语义化版本 (Semantic Versioning)：
- `v1.0.0` - 主版本.次版本.修订版本
- **主版本**：不兼容的 API 修改
- **次版本**：向下兼容的功能性新增
- **修订版本**：向下兼容的问题修正

示例：
- `v1.3` - 新增功能
- `v1.3.1` - 修复 bug
- `v2.0` - 重大更新

## 手动触发构建

如果需要重新构建某个版本：

1. 删除远程标签：
```bash
git push --delete origin v1.3
```

2. 删除本地标签：
```bash
git tag -d v1.3
```

3. 重新创建并推送标签：
```bash
git tag -a v1.3 -m "Release v1.3"
git push origin v1.3
```

## 故障排查

### 构建失败
1. 检查 Actions 日志查看具体错误
2. 确保代码可以在本地编译成功
3. 验证所有依赖库链接正确

### Release 未创建
1. 确认标签格式正确（以 `v` 开头）
2. 检查 `GITHUB_TOKEN` 权限
3. 查看 Actions 日志中的错误信息

## 本地测试

在推送标签前，可以本地测试编译：

```cmd
# 控制台版本
build.bat

# GUI 版本
build_gui.bat
```

确保两个版本都能正常编译后再发布。
