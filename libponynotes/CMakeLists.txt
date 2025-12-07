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
    # 确保符号被正确导出
    target_link_options(ponynotes_xournalpp PRIVATE
        -Wl,-export_dynamic
    )
    # 修复 portaudio 的框架链接问题
    # pkg-config 返回的格式是 "-framework CoreAudio AudioToolbox ..."
    # 这是错误的，我们需要在链接选项的最后添加正确格式来覆盖
    # 注意：必须在链接选项的最后，这样会覆盖错误的格式
    # 使用 -Wl,-framework 格式，确保链接器正确识别
    target_link_options(ponynotes_xournalpp PRIVATE
        -Wl,-framework,CoreAudio
        -Wl,-framework,AudioToolbox
        -Wl,-framework,AudioUnit
        -Wl,-framework,CoreFoundation
        -Wl,-framework,CoreServices
    )
    # 注意：由于 portaudio 的 pkg-config 返回的框架链接格式错误
    # 需要在构建时禁用 audio 支持（ENABLE_AUDIO=OFF）
    # 或者修复 portaudio 的 pkg-config 文件
endif()

# Windows特定设置
if(WIN32)
    set_target_properties(ponynotes_xournalpp PROPERTIES
        PREFIX ""
    )
endif()
