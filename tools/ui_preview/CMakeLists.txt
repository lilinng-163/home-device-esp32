cmake_minimum_required(VERSION 3.20)
project(voice_ui_preview LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)
set(LV_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
set(LV_FETCH_DEPENDENCIES OFF CACHE BOOL "" FORCE)
add_subdirectory(../../managed_components/lvgl__lvgl lvgl)

add_executable(ui_preview
    main.cpp
    ../../components/ui/src/dashboard.cpp
    ../../components/ui/fonts/ui_font_chinese_16.c)
target_include_directories(ui_preview PRIVATE ../../components/ui/inc)
target_link_libraries(ui_preview PRIVATE lvgl::lvgl)
