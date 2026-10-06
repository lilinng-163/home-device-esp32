#pragma once

#include <string>

// 启动 SNTP 并等首次同步（联网/拿到 IP 之后调）。成功返回 true。
bool time_sync(void);

// 返回 RFC1123 格式的 UTC 时间，如 "Mon, 04 Oct 2026 08:16:22 GMT"
std::string time_rfc1123(void);

// 时钟是否已同步好（年份 >= 2023 视为有效）
bool time_is_ready(void);
