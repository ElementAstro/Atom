# Atom xmake构建系统

这个文件夹包含了使用xmake构建Atom库的配置文件。xmake是一个轻量级的跨平台构建系统，可以更简单地构建C/C++项目。

## 安装xmake

在使用本构建系统之前，请先安装xmake：

- 官方网站：<https://xmake.io/>
- GitHub：<https://github.com/xmake-io/xmake>

### Windows安装

```powershell
# 使用PowerShell安装
Invoke-Expression (Invoke-Webrequest 'https://xmake.io/psget.ps1' -UseBasicParsing).Content
```

### Linux/macOS安装

```bash
# 使用bash安装
curl -fsSL https://xmake.io/shget.text | bash
```

## 快速构建

我们提供了简单的构建脚本来简化构建过程：

### Windows

```cmd
# 默认构建（Release模式，静态库）
build.bat

# 构建Debug版本
build.bat --debug

# 构建共享库
build.bat --shared

# 构建Python绑定
build.bat --python

# 构建示例
build.bat --examples

# 构建测试
build.bat --tests

# 查看所有选项
build.bat --help
```

### Linux/macOS

```bash
# 默认构建（Release模式，静态库）
./build.sh

# 构建Debug版本
./build.sh --debug

# 构建共享库
./build.sh --shared

# 构建Python绑定
./build.sh --python

# 构建示例
./build.sh --examples

# 构建测试
./build.sh --tests

# 查看所有选项
./build.sh --help
```

## 手动构建

如果你想手动配置构建选项，可以使用以下命令：

```bash
# 配置项目
xmake config [选项]

# 构建项目
xmake build

# 安装项目
xmake install
```

### 可用的配置选项

- `--build_python=y/n`: 启用/禁用Python绑定构建
- `--shared_libs=y/n`: 构建共享库或静态库
- `--build_examples=y/n`: 启用/禁用示例构建
- `--build_tests=y/n`: 启用/禁用测试构建
- `--enable_ssh=y/n`: 启用/禁用SSH支持
- `-m debug/release`: 设置构建模式

例如：

```bash
xmake config -m debug --build_python=y --shared_libs=y
```

## 项目结构

这个构建系统使用了模块化的设计，每个子目录都有自己的`xmake.lua`文件：

- `xmake.lua`：根配置文件
- `atom/xmake.lua`：主库配置
- `atom/*/xmake.lua`：各模块配置
- `example/xmake.lua`：示例配置
- `tests/xmake.lua`：测试配置

## 自定义安装位置

你可以通过以下方式指定安装位置：

```bash
xmake install -o /path/to/install
```

## 打包

你可以使用xmake的打包功能创建发布包：

```bash
xmake package
```

## 清理构建文件

```bash
xmake clean
```

## 故障排除

如果遇到构建问题，可以尝试以下命令：

```bash
# 清理所有构建文件并重新构建
xmake clean -a
xmake

# 查看详细构建信息
xmake -v

# 更新xmake并重试
xmake update
xmake
```
