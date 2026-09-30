idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_netif esp_rom esp_stdio esp_system esp_wifi nvs_flash cxx net device app)

# ---- 方式二：把 www/index.html 变成可链接的 ELF 对象 ----
# 注意：xtensa 的 `ld -r -b binary` 输出 raw/合并失败，不能用；用 objcopy（等价手段）
# 符号：_binary_index_html_start / _end / _size（WORKING_DIRECTORY 设为 www，名字才干净）
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/index_html.o
    COMMAND ${CMAKE_OBJCOPY} -I binary -O elf32-xtensa-le index.html ${CMAKE_CURRENT_BINARY_DIR}/index_html.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/index.html
    COMMENT "Embedding index.html via objcopy")
add_custom_target(embed_html DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/index_html.o)
add_dependencies(${COMPONENT_LIB} embed_html)
target_sources(${COMPONENT_LIB} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/index_html.o)
