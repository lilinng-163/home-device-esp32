# ESP32-S3 语音控制 LED

链路：唤醒词 → 讯飞 iat 实时识别 → 完整一句话 → DeepSeek JSON → 校验 → LED。

## 配置

1. 复制 `main/secrets.h.example` 为 `main/secrets.h`，填讯飞和 DeepSeek 凭据。
2. `idf.py menuconfig` → `App`：
   - `APP_LED_GPIO`：默认 `1`（板载红灯，`-1` 关闭）
   - `APP_LED_ACTIVE_LOW`：默认 `y`（低电平亮）
3. 当前串口控制台是 UART0，直接用它测试，不要再起第二个读串口的任务。

主机上运行的大模型和设备固件用的 `DEEPSEEK_MODEL` 是两回事，互不影响；固件只认
`main/secrets.h` 里的模型名。

本次使用 DeepSeek 官方接口和 `deepseek-flash`；按官方 2026-10-17 更新日志，该名称对应
DeepSeek-V4.1-Flash-1017：[官方更新日志](https://api-docs.deepseek.com/zh-cn/updates/)。
请求关闭思考模式、只输出 JSON，最多 128 个输出 token，每句话只请求一次，不自动重试。

## 允许的 JSON

只接受下面三种，必须恰好两个字段，不能有额外字段或尾部垃圾：

```json
{"device":"led","action":"on"}
{"device":"led","action":"off"}
{"device":"none","action":"none"}
```

不符合上述结构或取值的内容都会拒绝，LED 不变；无关和多个意图由系统提示要求返回 `none`。

## 串口测试（115200）

```text
voice 开灯
voice 关灯
json {"device":"led","action":"on"}
json {"device":"led","action":"off"}
status
```

- `voice ...` 走真实文本队列和 LLM 请求，日志会标 `injected voice`。
- `json ...` 只走同一套严格解析/执行路径，不请求云端，日志会标 `injected`。
- `status` 打印 LED 和文本队列状态。
- 开机不会自动发 LLM 请求；只有唤醒识别或串口注入才会触发。

连接诊断可用 `asr_probe`；上传诊断可用 `asr_tx_probe`，它发送一秒静音并打印发送耗时与栈余量。
这两个诊断命令都不把结果送入 LLM，也不执行设备命令。静音诊断出现 `empty final text` 是预期结果。

本板启用 PSRAM，`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096` 让较大的普通分配优先使用外部 RAM。
MQTT 应用处理任务和音频发送任务的栈放在 PSRAM，WS 库的 12KB 动态任务栈仍使用内部 RAM。

## 离线验证 JSON 解析

纯解析函数在 `components/app/src/device_command.cpp`，不依赖 ESP-IDF，可主机编译：

```powershell
New-Item -ItemType Directory -Force build | Out-Null
gcc -c managed_components/espressif__cjson/cJSON/cJSON.c -o build/cJSON.o
g++ -std=c++17 -I components/app/inc -I managed_components/espressif__cjson/cJSON `
  tools/device_command_check.cpp components/app/src/device_command.cpp build/cJSON.o `
  -o build/device_command_check.exe
.\build\device_command_check.exe
```

它只检查 schema 和取值，不测网络、不测 GPIO。

## 本次板上验证（2026-10-18）

ESP-IDF 6.1 虚拟环境编译、烧录通过，离线 JSON 解析检查 13 项通过。
真实语音“开灯”“关灯”均完成讯飞识别、DeepSeek HTTP 200 和 LED 执行，用户已确认实际开关正常。
本次日志中，从本地 VAD 结束到 LED 动作分别约 1.9 秒、1.5 秒；WS 任务最低余栈 8288 字节，静音上传诊断中发送任务最低余栈 4732 字节。
