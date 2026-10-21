# 设备 MQTT 报文

ESP32 和 Orange Pi 的 `main` 共用这个约定。上报与控制命令的外层只包含三个字段：

```json
{
    "version": "0.0.1",
    "device_id": 1,
    "data": {
        "temperature": 25.6,
        "humidity": 60
    }
}
```

- `version`：固定为字符串 `"0.0.1"`，其他版本拒绝处理。
- `device_id`：正整数，范围 1～9007199254740991；兼容 1～64 字节、无控制字符的字符串 ID。数字 `1` 与字符串 `"1"` 对应同一设备。
- `data`：包含 1～64 个字段的对象；字段名为 1～64 字节，不包含控制字符。具体字段由业务处理。
- 不在外层添加温度、LED、时间等字段；需要时放进 `data`。

## 控制 LED

主题：`home/room1/device1/cmd`

```json
{"version":"0.0.1","device_id":1,"data":{"led":1}}
```

`led` 使用整数 `0` / `1`，也兼容布尔 `false` / `true`。ESP32 先校验版本与设备 ID，再执行命令；小数、字符串和其他值不执行。

## 上报传感器

主题：`home/room1/device1/data`

```json
{"version":"0.0.1","device_id":1,"data":{"temperature":25.6,"humidity":60}}
```

其他设备可在 `data` 中上报不同字段。Orange Pi 沿用 `data_handlers.cpp` 的字段处理表，有什么处理什么；新增字段在表里注册，不修改公共外层。

## 代码入口

两边的 `device_message.h/.cpp` 提供 `make_device_message()` 组包、`read_device_message()` 校验。公共版本号只在各自的头文件定义一次。

- ESP32：`make_sensor_report()` 只组装传感器 `data`；`mqtt_custom.cpp` 校验外层和目标设备，业务回调只借用 `data`，不能删除 JSON 树。
- Orange Pi：`parse_device_report()` 复用外层校验，再分发字段和入库。下发命令也用 `make_device_message()`，传入 `{{"led", 1}}` 作为 `data`，再调用现有 `publish()`。
- 原有只含 `{"led":1}` 的命令不再接受，必须带上完整外层。

当前 ESP32 的队列最多容纳 511 字节正文，超长消息直接拒绝，不截断 JSON；Orange Pi 保留 16 KiB 的接收上限。发送给 ESP32 的命令应保持在它的容量以内。
