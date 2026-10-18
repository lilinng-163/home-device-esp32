#pragma once
#include <string>

#include "xfei.h"

struct cloud_llm_config
{
    std::string url;
    std::string model;
    std::string api_key;
};

// 绑定 xfei 的整句回调，起 LLM 消费任务和串口测试任务
void cloud_xfei_proc_start(xfei_dictation &xfei, const cloud_llm_config &llm);
