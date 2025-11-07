# 贡献指南

感谢你考虑为本项目做出贡献！

## 如何贡献

### 报告 Bug

如果你发现了 bug，请创建一个 issue 并包含以下信息：

- **问题描述**：简要说明问题
- **复现步骤**：详细的复现步骤
- **预期行为**：你期望发生什么
- **实际行为**：实际发生了什么
- **环境信息**：
  - Windows 版本
  - 蓝牙设备型号
  - 程序版本

### 提出新功能

如果你有新功能建议：

1. 创建一个 issue 说明你的想法
2. 等待维护者反馈
3. 如果得到认可，可以开始实现

### 提交代码

1. **Fork 本仓库**

2. **创建功能分支**
   ```bash
   git checkout -b feature/your-feature-name
   ```

3. **编写代码**
   - 遵循现有的代码风格
   - 添加必要的注释
   - 确保代码可以编译

4. **测试**
   - 测试你的修改
   - 确保不会破坏现有功能

5. **提交代码**
   ```bash
   git add .
   git commit -m "feat: 添加新功能描述"
   ```

6. **Push 到你的仓库**
   ```bash
   git push origin feature/your-feature-name
   ```

7. **创建 Pull Request**

## 代码规范

### C++ 代码风格

- 使用 4 个空格缩进
- 变量名使用 camelCase
- 函数名使用 PascalCase
- 常量使用 UPPER_CASE
- 添加必要的注释

### Commit 信息规范

使用语义化的 commit 信息：

- `feat:` 新功能
- `fix:` 修复 bug
- `docs:` 文档更新
- `style:` 代码格式调整
- `refactor:` 代码重构
- `test:` 测试相关
- `chore:` 构建/配置更新

示例：
```
feat: 添加设备自动重连功能
fix: 修复设备连接状态检测错误
docs: 更新 README 使用说明
```

## 开发环境设置

1. 安装 Visual Studio 2022（或 2019）
2. Clone 仓库
3. 运行编译脚本测试

### 编译测试

控制台版本：
```cmd
build.bat
```

GUI 版本：
```cmd
build_gui.bat
```

## 问题讨论

有任何问题可以通过以下方式联系：

- 创建 GitHub Issue
- 在 Pull Request 中讨论

## 许可证

提交代码即表示你同意你的贡献使用 MIT 许可证。
