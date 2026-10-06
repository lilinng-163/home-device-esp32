set(src src/wifi.cpp src/mqtt.cpp src/https_server.cpp src/web_config.cpp src/time_sync.cpp)
idf_component_register(SRCS ${src}
                    INCLUDE_DIRS inc
                    REQUIRES esp_event esp_wifi espressif__mqtt esp_https_server
                    PRIV_REQUIRES esp_netif esp_rom esp_stdio esp_system nvs_flash freertos cxx
                    EMBED_TXTFILES "servercert.pem" "prvtkey.pem" "cacert.pem")

# ---- 把 www/index.html 变成可链接的 ELF 对象（objcopy）----
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/index_html.o
    COMMAND ${CMAKE_OBJCOPY} -I binary -O elf32-xtensa-le index.html ${CMAKE_CURRENT_BINARY_DIR}/index_html.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/index.html
    VERBATIM)
target_sources(${COMPONENT_LIB} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/index_html.o)
