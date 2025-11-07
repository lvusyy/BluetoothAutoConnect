# Git 仓库管理说明

## 仓库状态

✅ Git 仓库已初始化完成！

## 当前配置

- **分支**: master
- **用户**: Administrator
- **邮箱**: admin@localhost

## 常用命令

### 查看状态
```bash
git status
```

### 查看提交历史
```bash
git log --oneline --graph
```

### 提交更改

1. 查看修改的文件
   ```bash
   git status
   ```

2. 添加文件到暂存区
   ```bash
   # 添加所有文件
   git add .
   
   # 或添加特定文件
   git add 文件名
   ```

3. 提交更改
   ```bash
   git commit -m "提交信息"
   ```

### 分支管理

创建新分支：
```bash
git checkout -b feature/新功能名称
```

切换分支：
```bash
git checkout master
```

合并分支：
```bash
git merge feature/新功能名称
```

删除分支：
```bash
git branch -d feature/新功能名称
```

### 远程仓库操作

添加远程仓库：
```bash
git remote add origin https://github.com/你的用户名/BluetoothAutoConnect.git
```

推送到远程：
```bash
# 首次推送
git push -u origin master

# 后续推送
git push
```

拉取更新：
```bash
git pull
```

## Commit 信息规范

使用语义化的提交信息：

- `feat:` 新功能
  ```
  feat: 添加蓝牙设备过滤功能
  ```

- `fix:` 修复 bug
  ```
  fix: 修复设备连接失败的问题
  ```

- `docs:` 文档更新
  ```
  docs: 更新 README 安装说明
  ```

- `style:` 代码格式
  ```
  style: 统一代码缩进格式
  ```

- `refactor:` 代码重构
  ```
  refactor: 重构设备连接逻辑
  ```

- `test:` 测试相关
  ```
  test: 添加设备扫描单元测试
  ```

- `chore:` 构建/配置
  ```
  chore: 更新编译脚本
  ```

## 推送到 GitHub

### 1. 在 GitHub 创建仓库

1. 访问 https://github.com/new
2. 仓库名称：`BluetoothAutoConnect`
3. 描述：`Windows蓝牙设备自动连接工具 - 支持GUI和系统托盘`
4. 选择 Public 或 Private
5. **不要**勾选 "Initialize this repository with a README"
6. 点击 "Create repository"

### 2. 关联本地仓库

```bash
git remote add origin https://github.com/你的用户名/BluetoothAutoConnect.git
```

### 3. 推送代码

```bash
# 推送到 GitHub
git push -u origin master
```

### 4. 后续更新

```bash
# 提交新的更改
git add .
git commit -m "更新说明"
git push
```

## .gitignore 说明

已配置忽略以下文件：

- 编译输出：`*.exe`, `*.obj`, `*.lib` 等
- Visual Studio 文件：`.vs/`, `*.vcxproj` 等
- 临时文件：`*.log`, `*.tmp` 等
- 构建目录：`Debug/`, `Release/`, `build/` 等

## .gitattributes 说明

已配置文件换行符规则：

- C++ 源文件：使用 CRLF（Windows 风格）
- Markdown 文件：使用 LF（Unix 风格）
- 批处理文件：使用 CRLF

## 版本标签

创建版本标签：
```bash
# 创建标签
git tag -a v1.2 -m "版本 1.2 - 添加 GUI 和系统托盘"

# 推送标签到远程
git push origin v1.2

# 推送所有标签
git push --tags
```

查看标签：
```bash
git tag
```

## 回退操作

### 撤销未提交的修改
```bash
# 撤销工作区的修改
git checkout -- 文件名

# 撤销暂存区的文件
git reset HEAD 文件名
```

### 回退到上一个提交
```bash
# 保留修改
git reset --soft HEAD^

# 不保留修改
git reset --hard HEAD^
```

## 常见问题

### Q: 如何修改最后一次提交？
```bash
# 修改文件后
git add .
git commit --amend
```

### Q: 如何忽略已跟踪的文件？
```bash
# 从 git 中删除但保留本地文件
git rm --cached 文件名
```

### Q: 如何查看某个文件的修改历史？
```bash
git log --follow -p 文件名
```

## 更多帮助

查看 Git 帮助文档：
```bash
git help
git help commit
```

在线文档：
- https://git-scm.com/doc
- https://github.com/git-guides
