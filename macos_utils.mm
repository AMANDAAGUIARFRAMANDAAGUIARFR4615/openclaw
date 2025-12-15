#include "macos_utils.h"
#include <Cocoa/Cocoa.h>

void setMacWindowAspectRatio(QWidget *widget, double width, double height)
{
    // 获取 Qt 窗口对应的原生 NSView
    NSView *view = (NSView *)widget->winId();
    
    // 获取该 View 所在的 NSWindow
    NSWindow *window = [view window];
    
    // 设置宽高比 (核心 API)
    // NSSize 是 macOS 的结构体
    [window setContentAspectRatio:NSMakeSize(width, height)];
    
    // 可选：如果希望窗口大小调整更“跟手”，可以设置 resize 增量
    // [window setResizeIncrements:NSMakeSize(1.0, 1.0)];
}