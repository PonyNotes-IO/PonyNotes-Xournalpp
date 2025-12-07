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

# macOS特定编译选项：确保符号可见性
if(APPLE)
    # 设置默认符号可见性为 default，确保符号被导出到动态符号表
    # 这必须在使用 PN_API 宏之前设置
    target_compile_options(ponynotes_xournalpp PRIVATE
        -fvisibility=default
    )
endif()

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
    # 确保符号被正确导出到导出表
    # 在 macOS 上，dlsym 查找的是导出表中的符号
    # 使用 -Wl,-exported_symbol 显式导出每个符号
    # 这是最可靠的方法，确保符号被正确导出
    target_link_options(ponynotes_xournalpp PRIVATE
        -Wl,-exported_symbol,_pn_xournal_init
        -Wl,-exported_symbol,_pn_xournal_shutdown
        -Wl,-exported_symbol,_pn_xournal_doc_create
        -Wl,-exported_symbol,_pn_xournal_doc_open
        -Wl,-exported_symbol,_pn_xournal_doc_open_pdf
        -Wl,-exported_symbol,_pn_xournal_doc_save
        -Wl,-exported_symbol,_pn_xournal_doc_close
        -Wl,-exported_symbol,_pn_xournal_doc_handle_stroke
        -Wl,-exported_symbol,_pn_xournal_doc_render_page_to_png
        -Wl,-exported_symbol,_pn_xournal_doc_get_page_count
        -Wl,-exported_symbol,_pn_xournal_doc_get_page_size
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
