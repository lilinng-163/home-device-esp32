#pragma once

#include <mutex>

class lcd;

// LVGL 显示对接：初始化 display/draw buffer/flush/io 回调/tick（把 lcd 绑到 LVGL）
void lvgl_port_init(lcd &screen);

// 启动 LVGL 处理任务（内部循环 lv_timer_handler）
void lvgl_task_start(void);

// LVGL 不是线程安全的：别的任务调 LVGL API 前后要加锁。推荐 RAII：
//   lvgl_lock_guard lk(lvgl_mutex());
std::mutex &lvgl_mutex();
using lvgl_lock_guard = std::lock_guard<std::mutex>;
