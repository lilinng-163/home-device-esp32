#include <cstdio>
#include <cstring>

#include "device_command.h"

static int failures = 0;

static void check(const char *name, const char *json, bool expect_ok, device_command expect_cmd)
{
    device_command cmd = device_command::none;
    bool ok = device_command_parse(json, strlen(json), cmd);
    bool pass = (ok == expect_ok) && (!ok || cmd == expect_cmd);
    std::printf("%s %s\n", pass ? "PASS" : "FAIL", name);
    if (!pass)
    {
        failures++;
    }
}

int main(void)
{
    check("led on", "{\"device\":\"led\",\"action\":\"on\"}", true, device_command::led_on);
    check("led off", "{\"device\":\"led\",\"action\":\"off\"}", true, device_command::led_off);
    check("none", "{\"device\":\"none\",\"action\":\"none\"}", true, device_command::none);
    check("whitespace", " {\"device\":\"led\",\"action\":\"on\"} \n", true, device_command::led_on);

    check("extra field",
          "{\"device\":\"led\",\"action\":\"on\",\"gpio\":2}",
          false,
          device_command::none);
    check("missing field", "{\"device\":\"led\"}", false, device_command::none);
    check("wrong type", "{\"device\":\"led\",\"action\":1}", false, device_command::none);
    check("illegal value", "{\"device\":\"fan\",\"action\":\"on\"}", false, device_command::none);
    check(
        "trailing garbage", "{\"device\":\"led\",\"action\":\"on\"}x", false, device_command::none);
    check("not object", "[\"led\",\"on\"]", false, device_command::none);
    check("empty", "", false, device_command::none);
    check("duplicate field",
          "{\"device\":\"led\",\"action\":\"on\",\"action\":\"off\"}",
          false,
          device_command::none);
    check("NUL in action",
          "{\"device\":\"led\",\"action\":\"on\\u0000off\"}",
          false,
          device_command::none);

    std::printf("%s (%d failure)\n", failures == 0 ? "OK" : "NG", failures);
    return failures == 0 ? 0 : 1;
}
