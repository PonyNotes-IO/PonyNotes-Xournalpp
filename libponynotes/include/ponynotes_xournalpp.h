#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// 导出宏定义
#ifdef _WIN32
  #ifdef PONYNOTES_XOURNALPP_EXPORTS
    #define PN_API __declspec(dllexport)
  #else
    #define PN_API __declspec(dllimport)
  #endif
#else
  #define PN_API __attribute__((visibility("default")))
#endif

// 错误码定义
#define PN_SUCCESS 0
#define PN_ERROR_INVALID_HANDLE -1
#define PN_ERROR_INVALID_PARAM -2
#define PN_ERROR_FILE_NOT_FOUND -3
#define PN_ERROR_IO_ERROR -4
#define PN_ERROR_UNKNOWN -99

// 文档句柄类型
typedef void* PN_DOC_HANDLE;

// 笔迹点结构
typedef struct {
  float x;              // X坐标
  float y;              // Y坐标
  float pressure;       // 压感值 (0.0 ~ 1.0)
  long long timestamp;  // 时间戳（毫秒）
  int tool;             // 工具类型：0=pen, 1=eraser, 2=highlighter, 3=pencil
  int phase;            // 事件阶段：0=down, 1=move, 2=up
} PN_STROKE_POINT;

// 初始化/反初始化
PN_API int pn_xournal_init(const char* config_json);
PN_API int pn_xournal_shutdown(void);

// 文档生命周期
PN_API int pn_xournal_doc_create(PN_DOC_HANDLE* out_doc, const char* options_json);
PN_API int pn_xournal_doc_open(PN_DOC_HANDLE* out_doc, const char* xopp_path);
PN_API int pn_xournal_doc_save(PN_DOC_HANDLE doc, const char* xopp_path);
PN_API int pn_xournal_doc_close(PN_DOC_HANDLE doc);

// 笔迹处理
PN_API int pn_xournal_doc_handle_stroke(PN_DOC_HANDLE doc, const PN_STROKE_POINT* points, int count);

// 渲染输出
PN_API int pn_xournal_doc_render_page_to_png(
  PN_DOC_HANDLE doc,
  int page_index,
  const char* png_path,
  int width,
  int height,
  const char* options_json
);

// 获取文档信息
PN_API int pn_xournal_doc_get_page_count(PN_DOC_HANDLE doc, int* out_count);
PN_API int pn_xournal_doc_get_page_size(PN_DOC_HANDLE doc, int page_index, double* out_width, double* out_height);

#ifdef __cplusplus
}
#endif

