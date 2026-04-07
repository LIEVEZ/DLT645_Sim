# 业务流程说明

## 1. 总体业务流程

```text
启动应用
  -> 初始化主窗口、控制器、日志、数据库
  -> 创建/恢复连接配置上下文
  -> 用户新建连接
  -> 用户打开连接
  -> 用户新建电表
  -> 系统按电表地址读取数据库
     -> 若存在则恢复历史数据
     -> 若不存在则创建默认数据并落库
  -> 用户在设备树选择分类
  -> 中部数据表展示分类数据
  -> 用户编辑/随机化/重置/导出
  -> 数据模型更新
  -> SQLite 保存
  -> 操作日志显示
```

## 2. 电表数据读取流程

```text
选择电表
  -> createMeterModel()
  -> 检查 m_meterModelsByAddress 是否已有实例
  -> 无实例则创建 MeterModel
  -> 尝试从 MeterDataStore 加载
  -> 加载失败则保存初始化数据
  -> 注册到 ProtocolController
  -> 应用为当前电表
```

## 3. 数据修改流程

```text
用户修改数据
  -> DataTableWidget 提交写入
  -> MeterModel.setDataValue()
  -> 发出 dataChanged / dataBatchChanged
  -> MainWindow::attachMeterPersistence() 响应
  -> MeterDataStore.saveMeterData()
  -> Logger 输出成功信息
  -> OperationLogWidget 显示日志
```

## 4. 随机化流程

### 4.1 全部随机化

```text
点击“随机化所有数据”
  -> MeterModel.randomizeData()
  -> 同步总电能、总需量、总功率等聚合值
  -> 发出 dataBatchChanged
  -> 持久化保存
  -> 日志显示
```

### 4.2 当前列表随机化

```text
点击“随机化当前数据”
  -> DataTableWidget 收集当前表格 DI 列表
  -> MeterModel.randomizeData(diCodes)
  -> 仅随机化当前列表相关项
  -> 持久化保存
  -> 日志显示
```

## 5. 通信观察流程

```text
打开连接
  -> 打开通信交互窗口
  -> 接收/发送原始数据
  -> ProtocolController 与 ProtocolParser 处理
  -> UI 显示日志和帧信息
```

