cmake_minimum_required(VERSION 3.12)

# 定义动态库
add_library(ponynotes_xournalpp SHARED
    src/ponynotes_xournalpp.cpp
)

# 设置导出宏
target_compile_definitions(ponynotes_xournalpp PRIVATE PONYNOTES_XOURNALPP_EXPORTS)

# 包含目录
target_include_directories(ponynotes_xournalpp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# 链接Xournal++核心库
target_link_libraries(ponynotes_xournalpp
    PRIVATE
        xoj::core
        xoj::util
        xoj::defaults
        xoj::external_modules
)

# 设置C++标准
target_compile_features(ponynotes_xournalpp PUBLIC cxx_std_20)

# 设置输出名称
set_target_properties(ponynotes_xournalpp PROPERTIES
    OUTPUT_NAME "ponynotes_xournalpp"
    VERSION 1.0.0
    SOVERSION 1
)

# macOS特定设置
if(APPLE)
    set_target_properties(ponynotes_xournalpp PROPERTIES
        INSTALL_NAME_DIR "@rpath"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

# Windows特定设置
if(WIN32)
    set_target_properties(ponynotes_xournalpp PROPERTIES
        PREFIX ""
    )
endif()

