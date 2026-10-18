#pragma once
#include <string>
#include <cstring>

#include "wake_word.h"
#include "cloud_xfei_proc.h"

class codec;

// 起两个任务：feed（咪头→AFE）/ fetch（判断唤醒）
// xfei 识别完成后把整句交给 cloud_xfei_proc 的文本队列
void wakeup_board_start(wake_word &wk,
                        codec &mic,
                        const std::string &appid,
                        const std::string &apikey,
                        const std::string &apisecret,
                        const cloud_llm_config &llm);

// 串口诊断：由 xfei_task 自己建 WS；可发 1 秒静音测上传，结果不进入 LLM。
void wakeup_asr_probe(bool send_silence = false);
