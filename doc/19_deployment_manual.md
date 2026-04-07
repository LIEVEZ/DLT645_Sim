# 部署手册

## 1. 适用范围

适用于 Windows 环境下部署和运行 `DLT645_Sim`。

## 2. 依赖要求

- Qt 5.x 开发环境
- MinGW 或 MSVC 编译器
- Windows 10/11

## 3. 源码构建步骤

### 3.1 进入工程根目录

```powershell
cd e:\04_Git\01_github\DLT645_Sim
```

### 3.2 生成 Makefile

示例命令：

```powershell
& 'D:\Qt\Qt5.14.2\5.14.2\mingw73_64\bin\qmake.exe' -o Build\Makefile .\Dlt645_Sim\Dlt645_Sim.pro -spec win32-g++
```

### 3.3 编译

```powershell
$env:Path='D:\Qt\Qt5.14.2\Tools\mingw730_64\bin;' + $env:Path
& 'D:\Qt\Qt5.14.2\Tools\mingw730_64\bin\mingw32-make.exe' -C Build -f Makefile.Release -j4
```

## 4. 运行说明

- 启动可执行文件后自动初始化 SQLite
- 首次运行会在应用目录创建 `meter_data.db`

## 5. 部署产物

| 产物 | 说明 |
| --- | --- |
| `Dlt645_Sim.exe` | 主程序 |
| Qt 运行时 DLL | Qt 依赖 |
| 平台插件 | `platforms/qwindows.dll` |
| `meter_data.db` | 运行后自动生成 |

## 6. 常见问题

### 6.1 编译后链接失败

- 现象：`Dlt645_Sim.exe: Permission denied`
- 原因：可执行文件正在运行，被系统占用
- 处理：关闭正在运行的程序后重新编译

### 6.2 程序启动缺少平台插件

- 检查 `platforms/qwindows.dll` 是否随程序发布

### 6.3 数据库未生成

- 检查程序目录写权限
- 检查 Qt SQL 模块是否正常链接

