#include "ponynotes_xournalpp.h"

#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

// Xournal++ 核心头文件
#include <cairo.h>
#include "model/Document.h"
#include "model/DocumentHandler.h"
#include "model/PageRef.h"
#include "model/XojPage.h"
#include "model/PageType.h"
#include "model/Stroke.h"
#include "model/Point.h"
#include "model/Layer.h"
#include "util/include/util/Color.h"
#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "view/DocumentView.h"
#include "view/background/BackgroundFlags.h"
#include "control/PdfCache.h"
#include <glib.h>  // for g_message, g_warning
#include <iostream>  // for std::cerr

// 内部文档包装结构
struct PonyNotesDocument {
    std::shared_ptr<Document> doc;
    std::shared_ptr<DocumentHandler> handler;
    std::unique_ptr<PdfCache> pdfCache;  // PDF缓存，用于渲染PDF背景
    std::mutex mutex;
    
    PonyNotesDocument() {
        // 创建DocumentHandler（简化版本，实际可能需要更多初始化）
        handler = std::make_shared<DocumentHandler>();
        doc = std::make_shared<Document>(handler.get());
    }
};

// 全局文档句柄映射（用于管理多个文档）
static std::unordered_map<PN_DOC_HANDLE, std::unique_ptr<PonyNotesDocument>> g_documents;
static std::mutex g_documents_mutex;
static bool g_initialized = false;

// 初始化
extern "C" int pn_xournal_init(const char* config_json) {
    // TODO: 解析config_json，初始化必要的全局资源
    // 目前先简单标记为已初始化
    g_initialized = true;
    return PN_SUCCESS;
}

// 反初始化
extern "C" int pn_xournal_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_documents_mutex);
    
    // 关闭所有打开的文档
    g_documents.clear();
    
    g_initialized = false;
    return PN_SUCCESS;
}

// 创建新文档
extern "C" int pn_xournal_doc_create(PN_DOC_HANDLE* out_doc, const char* options_json) {
    if (!g_initialized) {
        return PN_ERROR_UNKNOWN;
    }
    
    if (!out_doc) {
        return PN_ERROR_INVALID_PARAM;
    }
    
    try {
        auto pn_doc = std::make_unique<PonyNotesDocument>();
        
        // 创建默认页面（TODO: 根据options_json配置页面大小等）
        // A4 尺寸：595.275591 x 841.889764 points
        PageRef page = std::make_shared<XojPage>(595.275591, 841.889764);
        page->setBackgroundType(PageType(PageTypeFormat::Plain));
        
        pn_doc->doc->lock();
        pn_doc->doc->addPage(page);
        pn_doc->doc->unlock();
        
        PN_DOC_HANDLE handle = pn_doc.get();
        
        std::lock_guard<std::mutex> lock(g_documents_mutex);
        g_documents[handle] = std::move(pn_doc);
        
        *out_doc = handle;
        return PN_SUCCESS;
    } catch (...) {
        return PN_ERROR_UNKNOWN;
    }
}

// 打开文档
extern "C" int pn_xournal_doc_open(PN_DOC_HANDLE* out_doc, const char* xopp_path) {
    if (!g_initialized || !out_doc || !xopp_path) {
        return PN_ERROR_INVALID_PARAM;
    }
    
    try {
        // 使用LoadHandler加载xopp文件
        LoadHandler loader;
        auto loaded_doc = loader.loadDocument(xopp_path);
        
        if (!loaded_doc) {
            return PN_ERROR_FILE_NOT_FOUND;
        }
        
        if (loader.getLastError().length() > 0) {
            return PN_ERROR_FILE_NOT_FOUND;
        }
        
        auto pn_doc = std::make_unique<PonyNotesDocument>();
        pn_doc->doc = std::move(loaded_doc);
        
        PN_DOC_HANDLE handle = pn_doc.get();
        
        std::lock_guard<std::mutex> lock(g_documents_mutex);
        g_documents[handle] = std::move(pn_doc);
        
        *out_doc = handle;
        return PN_SUCCESS;
    } catch (...) {
        return PN_ERROR_UNKNOWN;
    }
}

// 打开PDF文档
extern "C" int pn_xournal_doc_open_pdf(PN_DOC_HANDLE* out_doc, const char* pdf_path, int attach_to_document) {
    if (!g_initialized || !out_doc || !pdf_path) {
        return PN_ERROR_INVALID_PARAM;
    }
    
    try {
        auto pn_doc = std::make_unique<PonyNotesDocument>();
        
        // 使用Document::readPdf加载PDF文件
        // attach_to_document: 0=替换当前文档, 1=附加到当前文档
        bool attach = (attach_to_document != 0);
        bool success = pn_doc->doc->readPdf(pdf_path, /*initPages=*/true, attach);
        
        if (!success) {
            // 获取错误信息
            std::string error = pn_doc->doc->getLastErrorMsg();
            if (error.empty()) {
                return PN_ERROR_FILE_NOT_FOUND;
            }
            return PN_ERROR_IO_ERROR;
        }
        
        // 如果文档包含PDF页面，创建PdfCache用于渲染PDF背景
        pn_doc->doc->lock();
        size_t pdfPageCount = pn_doc->doc->getPdfPageCount();
        size_t pageCount = pn_doc->doc->getPageCount();
        g_message("[ponynotes_xournalpp] PDF opened, pdfPageCount=%zu, pageCount=%zu", pdfPageCount, pageCount);
        
        // 检查页面背景类型
        if (pageCount > 0) {
            PageRef firstPage = pn_doc->doc->getPage(0);
            PageType bgType = firstPage->getBackgroundType();
            bool isPdfPage = bgType.isPdfPage();
            size_t pdfPageNr = firstPage->getPdfPageNr();
            g_message("[ponynotes_xournalpp] First page: isPdfPage=%d, pdfPageNr=%zu", isPdfPage, pdfPageNr);
        }
        
        if (pdfPageCount > 0) {
            try {
                // 创建PdfCache，Settings传nullptr使用默认设置
                pn_doc->pdfCache = std::make_unique<PdfCache>(pn_doc->doc->getPdfDocument(), nullptr);
                g_message("[ponynotes_xournalpp] PdfCache created successfully, pointer=%p", pn_doc->pdfCache.get());
            } catch (const std::exception& e) {
                g_warning("[ponynotes_xournalpp] Failed to create PdfCache: %s", e.what());
            } catch (...) {
                g_warning("[ponynotes_xournalpp] Failed to create PdfCache: unknown exception");
            }
        } else {
            g_warning("[ponynotes_xournalpp] PDF opened but pdfPageCount is 0");
        }
        pn_doc->doc->unlock();
        
        PN_DOC_HANDLE handle = pn_doc.get();
        
        std::lock_guard<std::mutex> lock(g_documents_mutex);
        g_documents[handle] = std::move(pn_doc);
        
        *out_doc = handle;
        return PN_SUCCESS;
    } catch (...) {
        return PN_ERROR_UNKNOWN;
    }
}

// 保存文档
extern "C" int pn_xournal_doc_save(PN_DOC_HANDLE doc, const char* xopp_path) {
    if (!doc || !xopp_path) {
        return PN_ERROR_INVALID_PARAM;
    }
    
    std::lock_guard<std::mutex> lock(g_documents_mutex);
    auto it = g_documents.find(doc);
    if (it == g_documents.end()) {
        return PN_ERROR_INVALID_HANDLE;
    }
    
    try {
        auto& pn_doc = it->second;
        
        pn_doc->doc->lock();
        pn_doc->doc->setFilepath(xopp_path);
        
        SaveHandler saver;
        saver.prepareSave(pn_doc->doc.get(), xopp_path);
        saver.saveTo(xopp_path);
        
        pn_doc->doc->unlock();
        
        return PN_SUCCESS;
    } catch (...) {
        return PN_ERROR_IO_ERROR;
    }
}

// 关闭文档
extern "C" int pn_xournal_doc_close(PN_DOC_HANDLE doc) {
    if (!doc) {
        return PN_ERROR_INVALID_PARAM;
    }
    
    std::lock_guard<std::mutex> lock(g_documents_mutex);
    auto it = g_documents.find(doc);
    if (it == g_documents.end()) {
        return PN_ERROR_INVALID_HANDLE;
    }
    
    g_documents.erase(it);
    return PN_SUCCESS;
}

// 处理笔迹
extern "C" int pn_xournal_doc_handle_stroke(PN_DOC_HANDLE doc, const PN_STROKE_POINT* points, int count) {
    if (!doc || !points || count <= 0) {
        return PN_ERROR_INVALID_PARAM;
    }
    
    std::lock_guard<std::mutex> lock(g_documents_mutex);
    auto it = g_documents.find(doc);
    if (it == g_documents.end()) {
        return PN_ERROR_INVALID_HANDLE;
    }
    
    try {
        auto& pn_doc = it->second;
        std::lock_guard<std::mutex> doc_lock(pn_doc->mutex);
        
        // 获取第一页（或当前页）
        if (pn_doc->doc->getPageCount() == 0) {
            return PN_ERROR_INVALID_PARAM;
        }
        
        PageRef page = pn_doc->doc->getPage(0);
        if (!page) {
            return PN_ERROR_INVALID_PARAM;
        }
        
        // 获取第一个可见图层
        auto& layers = page->getLayers();
        if (layers.empty()) {
            return PN_ERROR_INVALID_PARAM;
        }
        
        Layer* layer = layers[0];
        if (!layer) {
            return PN_ERROR_INVALID_PARAM;
        }
        
        // 创建Stroke对象
        auto stroke = std::make_unique<Stroke>();
        
        // 设置工具类型
        StrokeTool::Value toolType = StrokeTool::PEN;
        if (points[0].tool == 1) {
            toolType = StrokeTool::ERASER;
        } else if (points[0].tool == 2) {
            toolType = StrokeTool::HIGHLIGHTER;
        }
        stroke->setToolType(toolType);
        
        // 设置默认宽度（可以根据压感调整）
        double defaultWidth = 2.0;
        if (toolType == StrokeTool::HIGHLIGHTER) {
            defaultWidth = 10.0;
        }
        stroke->setWidth(defaultWidth);
        
        // 设置颜色（默认黑色，后续可以从options_json解析）
        stroke->setColor(Colors::black);
        
        // 添加点
        for (int i = 0; i < count; i++) {
            const PN_STROKE_POINT& pt = points[i];
            
            // 转换压力值：PN_STROKE_POINT的pressure是0.0~1.0，Point的z是压力值或NO_PRESSURE
            double pressure = Point::NO_PRESSURE;
            if (pt.pressure > 0.0 && pt.pressure <= 1.0) {
                pressure = pt.pressure;
            }
            
            Point xojPoint(pt.x, pt.y, pressure);
            stroke->addPoint(xojPoint);
        }
        
        // 将Stroke添加到图层
        layer->addElement(std::move(stroke));
        
        return PN_SUCCESS;
    } catch (...) {
        return PN_ERROR_UNKNOWN;
    }
}

// 渲染页面为PNG
extern "C" int pn_xournal_doc_render_page_to_png(
    PN_DOC_HANDLE doc,
    int page_index,
    const char* png_path,
    int width,
    int height,
    const char* options_json
) {
    if (!doc || !png_path || page_index < 0) {
        return PN_ERROR_INVALID_PARAM;
    }
    
    std::lock_guard<std::mutex> lock(g_documents_mutex);
    auto it = g_documents.find(doc);
    if (it == g_documents.end()) {
        return PN_ERROR_INVALID_HANDLE;
    }
    
    try {
        auto& pn_doc = it->second;
        std::lock_guard<std::mutex> doc_lock(pn_doc->mutex);
        
        if (page_index >= static_cast<int>(pn_doc->doc->getPageCount())) {
            return PN_ERROR_INVALID_PARAM;
        }
        
        PageRef page = pn_doc->doc->getPage(page_index);
        if (!page) {
            return PN_ERROR_INVALID_PARAM;
        }
        
        // 调试：检查页面背景类型
        PageType bgType = page->getBackgroundType();
        bool isPdfPage = bgType.isPdfPage();
        size_t pdfPageNr = page->getPdfPageNr();
        int bgFormat = static_cast<int>(bgType.format);
        g_message("[ponynotes_xournalpp] Rendering page %d: isPdfPage=%d, pdfPageNr=%zu, bgFormat=%d, pdfCache=%p", 
                  page_index, isPdfPage, pdfPageNr, bgFormat, pn_doc->pdfCache.get());
        
        // 计算缩放比例
        double pageWidth = page->getWidth();
        double pageHeight = page->getHeight();
        
        double scaleX = (width > 0) ? (width / pageWidth) : 1.0;
        double scaleY = (height > 0) ? (height / pageHeight) : 1.0;
        double scale = (scaleX < scaleY) ? scaleX : scaleY;  // 保持宽高比
        
        int renderWidth = static_cast<int>(pageWidth * scale);
        int renderHeight = static_cast<int>(pageHeight * scale);
        
        // 创建Cairo surface
        cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, renderWidth, renderHeight);
        if (!surface || cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
            g_warning("[ponynotes_xournalpp] Failed to create Cairo surface");
            return PN_ERROR_IO_ERROR;
        }
        
        // 设置设备缩放为1.0（确保PdfBackgroundView的断言通过）
        cairo_surface_set_device_scale(surface, 1.0, 1.0);
        
        cairo_t* cr = cairo_create(surface);
        if (!cr || cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
            cairo_surface_destroy(surface);
            g_warning("[ponynotes_xournalpp] Failed to create Cairo context");
            return PN_ERROR_IO_ERROR;
        }
        
        // 缩放画布（均匀缩放，确保PdfBackgroundView的断言通过）
        cairo_scale(cr, scale, scale);
        
        // 验证矩阵（用于调试）
        cairo_matrix_t matrix = {0};
        cairo_get_matrix(cr, &matrix);
        g_message("[ponynotes_xournalpp] Cairo matrix: xx=%.6f, yy=%.6f, xy=%.6f, yx=%.6f, scale=%.6f", 
                  matrix.xx, matrix.yy, matrix.xy, matrix.yx, scale);
        
        // 使用DocumentView渲染页面
        DocumentView view;
        
        // 设置PdfCache（如果存在），用于渲染PDF背景
        if (pn_doc->pdfCache) {
            view.setPdfCache(pn_doc->pdfCache.get());
            g_message("[ponynotes_xournalpp] PdfCache set to DocumentView");
        } else {
            g_warning("[ponynotes_xournalpp] PdfCache is null, PDF background may not render");
        }
        
        xoj::view::BackgroundFlags flags;
        flags.showPDF = xoj::view::SHOW_PDF_BACKGROUND;
        flags.showImage = xoj::view::SHOW_IMAGE_BACKGROUND;
        flags.showRuling = xoj::view::SHOW_RULING_BACKGROUND;
        
        g_message("[ponynotes_xournalpp] Calling drawPage with flags: showPDF=%d, showImage=%d, showRuling=%d",
                  flags.showPDF, flags.showImage, flags.showRuling);
        
        view.drawPage(page, cr, true /* dont render editing stroke */, flags);
        
        g_message("[ponynotes_xournalpp] drawPage completed");
        
        // 导出为PNG
        cairo_status_t status = cairo_surface_write_to_png(surface, png_path);
        
        // 清理资源
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        
        if (status != CAIRO_STATUS_SUCCESS) {
            return PN_ERROR_IO_ERROR;
        }
        
        return PN_SUCCESS;
    } catch (...) {
        return PN_ERROR_UNKNOWN;
    }
}

// 获取页面数量
extern "C" int pn_xournal_doc_get_page_count(PN_DOC_HANDLE doc, int* out_count) {
    if (!doc || !out_count) {
        return PN_ERROR_INVALID_PARAM;
    }
    
    std::lock_guard<std::mutex> lock(g_documents_mutex);
    auto it = g_documents.find(doc);
    if (it == g_documents.end()) {
        return PN_ERROR_INVALID_HANDLE;
    }
    
    try {
        auto& pn_doc = it->second;
        pn_doc->doc->lock();
        *out_count = static_cast<int>(pn_doc->doc->getPageCount());
        pn_doc->doc->unlock();
        return PN_SUCCESS;
    } catch (...) {
        return PN_ERROR_UNKNOWN;
    }
}

// 获取页面尺寸
extern "C" int pn_xournal_doc_get_page_size(PN_DOC_HANDLE doc, int page_index, double* out_width, double* out_height) {
    if (!doc || !out_width || !out_height || page_index < 0) {
        return PN_ERROR_INVALID_PARAM;
    }
    
    std::lock_guard<std::mutex> lock(g_documents_mutex);
    auto it = g_documents.find(doc);
    if (it == g_documents.end()) {
        return PN_ERROR_INVALID_HANDLE;
    }
    
    try {
        auto& pn_doc = it->second;
        pn_doc->doc->lock();
        
        if (page_index >= static_cast<int>(pn_doc->doc->getPageCount())) {
            pn_doc->doc->unlock();
            return PN_ERROR_INVALID_PARAM;
        }
        
        PageRef page = pn_doc->doc->getPage(page_index);
        *out_width = Document::getPageWidth(page);
        *out_height = Document::getPageHeight(page);
        
        pn_doc->doc->unlock();
        return PN_SUCCESS;
    } catch (...) {
        return PN_ERROR_UNKNOWN;
    }
}

