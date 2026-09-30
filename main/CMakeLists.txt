idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_netif esp_rom esp_stdio esp_system esp_wifi nvs_flash cxx net device app)

# ---- 方式二：ld -r -b binary 把 www/index.html 变成可链接对象 ----
# cd 到 www 目录执行，符号名才是干净的 _binary_index_html_start/_end
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/index_html.o
    COMMAND ${CMAKE_LINKER} -r --no-relax -b binary index.html -o ${CMAKE_CURRENT_BINARY_DIR}/index_html.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/index.html
    COMMENT "Embedding index.html via ld -r -b binary")
add_custom_target(embed_html DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/index_html.o)
add_dependencies(${COMPONENT_LIB} embed_html)
target_sources(${COMPONENT_LIB} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/index_html.o)
