# home-device-esp32

基于 ESP32-S3 的智能家居无线设备端。通过 MQTT 接收设备命令、上报数据，通过讯飞和 DeepSeek 完成中文语音控制，LCD 显示连接和语音状态。

配套家庭网关：[home-gateway-orangepi](https://github.com/lilinng-163/home-gateway-orangepi)。两端使用相同的 `version`、`device_id`、`data` 报文格式。

## 当前功能

- ESP-SR 唤醒词检测，ES8388 采集语音，讯飞实时语音识别。
- 识别文本交给 DeepSeek，校验返回的 JSON 后控制 LED。
- MQTT 接收 LED 命令，每 5 秒上报一次模拟温湿度。
- 240 × 320 LCD 状态面板，显示 Wi-Fi、MQTT、语音过程和 LED 状态。
- HTTPS 配网，Wi-Fi 和 MQTT 配置保存在 NVS。
- MQTT 收发日志统一打印 `RX/TX topic=... payload=...`。

温湿度目前固定为 `25.6` 和 `60`，用于演示报文；接入真实传感器时替换采样值。

## 硬件与开发环境

- 正点原子 DNESP32S3 开发板，ESP32-S3、16 MB Flash、PSRAM。
- ES8388 音频芯片、ST7789 LCD、XL9555 IO 扩展器。
- ESP-IDF **6.1**，使用该版本安装时创建的 Python 虚拟环境。

板级引脚在 `components/board/Kconfig` 中配置；板载红灯默认 GPIO1，低电平点亮。

## 编译和烧录

先运行本机 ESP-IDF 6.1 的 `export.ps1`（Windows）或 `export.sh`（Linux），确认 `idf.py --version`。后续命令均在这个环境中执行。

```sh
git clone https://github.com/lilinng-163/home-device-esp32.git
cd home-device-esp32

cp config/secrets.h.example main/secrets.h
python tools/generate_local_tls.py
idf.py set-target esp32s3
idf.py build
```

编译前编辑 `main/secrets.h`，填写自己的讯飞、DeepSeek 凭据。证书生成工具依赖 `cryptography`，缺失时在同一虚拟环境运行 `python -m pip install cryptography`。已有的本地证书不会被覆盖。

Windows 烧录示例，串口按实际连接修改：

```sh
idf.py -p COM8 flash monitor
```

生成的固件为 `build/home-device-esp32.bin`。首次烧录需要同时写入模型分区，使用完整的 `flash` 命令。构建参数由 `sdkconfig.defaults` 提供，分区表见 `partitions.csv`。

## 配网

没有 Wi-Fi 配置时自动进入 AP 模式；已有配置时，开机按住板载按键约两秒进入配网。

1. 连接热点 `esp32`，开发默认密码为 `esp32-lilinng`。
2. 打开 `https://192.168.4.1`。本地自签名证书可能需要在浏览器中手动信任。
3. 填写 Wi-Fi、MQTT 地址、端口、客户端 ID 和认证信息。
4. 保存后重新启动设备，进入 STA 模式。

填写 MQTT 主机时使用 IP 或域名，不带 `http://`。配置结构见 [provisioning.example.json](config/provisioning.example.json)。默认示例域名为 `mqtt.example.com`，需要替换成自己的 broker；已有 NVS 配置继续生效。

## MQTT 报文

温湿度上报主题：`home/room1/device1/data`。

```json
{"version":"0.0.1","device_id":1,"data":{"temperature":25.6,"humidity":60}}
```

LED 控制主题：`home/room1/device1/cmd`。

```json
{"version":"0.0.1","device_id":1,"data":{"led":1}}
```

`led` 为 `1` / `0`，也支持 `true` / `false`。版本、目标设备或字段不合法时不执行。设备 ID 在 `main/main.cpp` 定义，收命令和发上报共用。

公共组包和校验在 `device_message.h/.cpp`；添加传感器时只组装 `data`，不重复拼接外层。完整规则见 [设备报文说明](docs/device_message.md)。

## 代码结构

```text
main/               启动流程、应用接线、本地 secrets.h
components/
  app/              MQTT 业务、报文校验、传感器上报、语音任务
  board/            开发板引脚与硬件初始化
  cloud/            讯飞、DeepSeek 客户端
  device/           音频、LCD、LED、按键等驱动
  net/              Wi-Fi、MQTT、HTTPS 配网、时间同步
  ui/               LVGL 显示适配、面板和中文字库
config/             可公开的配置模板
docs/               协议、语音控制、界面说明
tools/              本地证书生成、JSON 验证、字库生成、UI 预览
```

## 扩展和调试

- 新增上报字段：在 `sensor_report.cpp` 中采样并组装 `data`，网关注册对应字段处理函数。
- 新增设备动作：在 `main/main.cpp` 的业务回调中处理 `data`，公共版本和设备 ID 校验继续复用。
- 本地语音链路和串口调试：[语音控制 LED](docs/voice-led.md)。LLM 的内部 `device/action` JSON 与 MQTT 的三字段报文用途不同。
- LCD 页面与主机预览：[语音控制面板](docs/voice-ui.md)。

`main/secrets.h`、本地 TLS 私钥、生成的 `sdkconfig` 和构建产物不提交。API 密钥仍会编译进入设备固件，这个项目中的本地文件隔离不等于成品防逆向保护。
