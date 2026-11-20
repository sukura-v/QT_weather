# QT_weather

基于 Qt Widgets、Qt Network 和 Qt SQL 的 Windows 天气数据采集程序。程序定时获取中国天气网的城市实况，将解析后的业务字段写入 MySQL，并提供查询、记录维护、统计、CSV 导出和托盘运行。

## 主要功能

- 定时采集与立即采集共用同一套异步请求逻辑，默认间隔 60 秒，可设置为 10～3600 秒。
- 网络请求设有 10 秒超时，失败后按 1、2、4 秒退避，最多重试 3 次。全部失败后停止采集。
- 采集入库由工作线程处理，使用预编译参数绑定和逐条事务，写入失败回滚并记录日志。
- 支持城市、日期范围和采集状态的组合查询，以及选中回填、修改、删除、补录和详情查看。
- 对当前查询结果计算记录数、温湿度统计及成功、解析异常、入库失败状态的条数。
- 将当前查询结果导出为带 BOM 的 UTF-8 CSV。
- 支持最小化到托盘、双击恢复、单实例唤醒，以及通过 QSettings 保存参数。
- 日志按天写入程序目录下的 `logs/collect-yyyyMMdd.log`，界面保留最近 2000 行。

## 开发环境

| 项目 | 当前源码要求 |
| --- | --- |
| 操作系统 | Windows |
| Qt | Qt 6.5 及以上 |
| Qt 模块 | Core、Widgets、Sql、Network |
| 构建系统 | CMake 3.19 及以上 |
| C++ | 支持 C++17 的编译器，配合对应的 Qt Kit |
| 数据库 | MySQL，使用 QMYSQL 驱动 |
| 数据库地址 | 当前源码使用 `127.0.0.1:3306` |

使用 Qt Creator 打开根目录的 `CMakeLists.txt`，选择本机已配置的 Qt Kit，再由 Qt Creator 构建和运行。MySQL 驱动及客户端库需要与所选 Qt Kit 的架构和编译器匹配。

## 数据库准备

程序会连接已有数据库并操作 `weather` 表。可以在 MySQL 或 Navicat 中创建数据库和表，再在程序设置中填写库名、用户名和密码。

下面的表结构对应当前项目使用的字段与索引。数据库名称可以按实际环境调整。

```sql
CREATE DATABASE IF NOT EXISTS weather
    DEFAULT CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

USE weather;

CREATE TABLE IF NOT EXISTS weather (
    id BIGINT NOT NULL AUTO_INCREMENT COMMENT '每条天气记录的唯一主键',
    source VARCHAR(100) DEFAULT NULL COMMENT '数据来源',
    city_name VARCHAR(60) DEFAULT NULL COMMENT '城市名称',
    city_code VARCHAR(32) DEFAULT NULL COMMENT '城市代码',
    temperature DECIMAL(5,2) DEFAULT NULL COMMENT '温度，单位：℃',
    humidity DECIMAL(5,2) DEFAULT NULL COMMENT '相对湿度数值，单位：%',
    wind_direction VARCHAR(30) DEFAULT NULL COMMENT '风向',
    wind_force VARCHAR(40) DEFAULT NULL COMMENT '风力描述',
    collected_at DATETIME DEFAULT NULL COMMENT '本条数据的采集时刻',
    status VARCHAR(20) DEFAULT NULL COMMENT '采集处理状态',
    PRIMARY KEY (id),
    KEY idx_collected_at_city (collected_at, city_code, city_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    COLLATE=utf8mb4_unicode_ci COMMENT='天气业务数据表';
```

`OK` 表示解析成功并写入的记录，`PARSE_ERR` 表示响应或字段解析失败后写入的记录。解析失败时，无法取得的测量值以 SQL `NULL` 保存，编辑区和详情页显示“未采集”。`DB_ERR` 用于记录和展示数据库错误，写库失败会回滚并写日志。

## 首次使用

1. 准备好 MySQL 数据库、`weather` 表和可读写该表的数据库账号。
2. 启动程序后打开“文件 → 设置”，填写数据库名称、用户名和密码。首次未配置数据库时会弹出连接失败提示，可关闭提示后进入设置。
3. 设置天气接口前缀、城市代码和采集间隔，保存后点击“立即采集”或“启动采集”。
4. 使用查询区域筛选记录。查看解析异常时可选择对应状态，并清空城市名称和日期限制。
5. 导出时选择 CSV 保存位置。在 Excel 中查看完整日期时间时，可以加宽采集时间列。
6. 最小化或关闭主窗口后，程序继续在托盘运行。托盘菜单可显示主界面、打开设置或退出程序。

## 天气数据源

默认接口前缀为 `https://d1.weather.com.cn/sk_2d/`。程序将前缀、城市代码和 `.html` 拼成完整请求地址，例如 `https://d1.weather.com.cn/sk_2d/101040100.html`。

| 城市 | 城市代码 |
| --- | --- |
| 北京 | `101010100` |
| 重庆 | `101040100` |
| 上海 | `101020100` |
| 成都 | `101270101` |

该实况响应形如 `var dataSK={...}`。程序移除赋值前缀后解析 JSON，并为请求附上中国天气网来源页的 Referer 和浏览器 User-Agent。

| 响应字段 | 数据库字段 |
| --- | --- |
| `cityname` | `city_name` |
| `city` | `city_code` |
| `temp` | `temperature` |
| `SD` | `humidity`，去除百分号后存数值 |
| `WD` | `wind_direction` |
| `WS` | `wind_force` |

采集时间使用本机时间。请求地址写入 `source`。源码同时保留了旧版 `weatherinfo` JSON 的解析分支，已有旧默认接口前缀会在启动时迁移至新默认前缀。

接口可直接在参数页修改。浏览器从地址栏直接访问实况地址时可能收到 HTTP 403，可以从中国天气网页面的 Network 面板查看实际请求和响应。

## 源码结构

| 文件 | 作用 |
| --- | --- |
| `main.cpp` | 应用入口、单实例与窗口唤醒 |
| `mainwindow.h/.cpp/.ui` | 主界面、采集、查询、编辑、导出、统计、日志与托盘 |
| `weatherworker.h/.cpp` | 工作线程内的数据库连接、事务写入和重连通知 |
| `adddialog.h/.cpp/.ui` | 手工补录对话框 |
| `detaildialog.h/.cpp/.ui` | 记录详情对话框 |
| `settingdialog.h/.cpp/.ui` | 参数设置与持久化 |
| `CMakeLists.txt` | CMake 项目入口 |

`settings.ini` 保存在可执行文件所在目录，包含本机数据库连接参数。构建目录、Qt Creator 本机配置、日志、CSV、数据库文件和运行时设置由 `.gitignore` 排除。
