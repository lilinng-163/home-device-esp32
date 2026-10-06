#pragma once
#include <string>
#include <cstring>

#include "wake_word.h"

class codec;

// 起两个任务：feed（咪头→AFE）/ fetch（判断唤醒）
void wakeup_board_start(wake_word &wk, codec &mic,
                        const std::string &appid,
                        const std::string &apikey,
                        const std::string &apisecret);
