#ifndef UITHEME_H
#define UITHEME_H

#include <QString>

// ---------------------------------------------------------------------------
// 全局 UI 调色板（设计 token）
//
// 全应用（主窗口 + 各对话框 / 控件）共用同一份调色板，消除原先散落在 8 个文件、
// 300+ 处的硬编码颜色。改色（如更换强调色）只需修改本文件的 darkPalette() /
// lightPalette()，所有界面自动同步。
//
// 设计原则：
//   - 强调色取工业「钢青蓝」（hue ~195），区别于 Windows / VS Code 默认蓝；
//   - 中性灰朝品牌色相微偏（青向），形成潜意识的整体统一感；
//   - 文字采用微着色而非纯黑 / 纯白，降低长时间阅读的对比硬度；
//   - 圆角统一为 4px（控件）/ 8px（容器型：GroupBox、工具栏按钮）两档；
//   - 语义色（成功 / 危险 / 警告）独立于品牌色，保持通用语义可读性。
// ---------------------------------------------------------------------------
namespace ui
{

struct ThemePalette
{
    // —— 背景层级 ——
    QString windowBg;     // 主窗口 / 对话框 / 三栏容器底色
    QString panelBg;      // 控件背景（输入框、按钮、表头、下拉、表格）
    QString panelHover;   // 控件 hover
    QString panelPressed; // 控件 pressed
    QString readonlyBg;   // 只读输入框背景

    // —— 边框 ——
    QString border;       // 常规边框
    QString borderStrong; // 分隔线 / QSplitter 手柄
    QString gridLine;     // 表格网格线 / 行分隔

    // —— 文字 ——
    QString text;         // 主文字（微着色，非纯黑 / 纯白）
    QString textMuted;    // 次级 / disabled 文字
    QString textReadonly; // 只读输入框文字
    QString onAccent;     // 强调色之上的文字

    // —— 强调 ——
    QString accent;        // 选中 / focus 强调色
    QString accentHover;   // 强调实心按钮 hover
    QString accentPressed; // 强调实心按钮 pressed
    QString accentTitle;   // GroupBox 标题 / 分区标题（强调色亮调）

    // —— 语义色（独立于品牌色）——
    QString success; // 成功 / 已连接
    QString danger;  // 危险 / 断开 / 错误
    QString warning; // 警告 / 提示

    // —— 操作日志级别色（随主题切换，接入品牌 / 语义体系）——
    QString logInfo;          // 信息（强调蓝调）
    QString logSuccess;       // 成功（绿）
    QString logComm;          // 通信（青）
    QString logWarning;       // 警告（橙）
    QString logError;         // 错误（红）

    // —— Banner（提示 / 错误条）——
    QString noticeBg;
    QString noticeBorder;
    QString noticeText;
    QString errorBg;
    QString errorBorder;
    QString errorText;

    // —— 报文解析卡片 ——
    QString cardTitle;  // 卡片标题（强调蓝亮调）
    QString cardDetail; // 卡片次级说明文字

    // —— 工具栏（仅主窗口）——
    QString toolbarTop;    // 工具栏渐变顶
    QString toolbarBottom; // 工具栏渐变底
    QString toolbarEdge;   // 工具栏底边
    QString tbSeparator;   // 工具栏分隔（rgba）
    QString tbHover;       // 工具栏按钮 hover（rgba）
    QString tbHoverEdge;   // 工具栏按钮 hover 边框（rgba）
    QString tbPressed;     // 工具栏按钮 pressed/checked（rgba）
    QString tbPressedEdge; // 工具栏按钮 pressed 边框（rgba）
    QString barBg;         // 通信条 / 状态栏底色
};

/// 暗色主题调色板：钢青蓝强调，中性灰朝青微偏，文字微着色白。
ThemePalette darkPalette();

/// 亮色主题调色板：同色相钢青蓝（更深以保证白底对比），中性白朝青微偏，文字微着色黑。
ThemePalette lightPalette();

/// 将 QSS 模板中形如 {{token}} 的占位符替换为调色板取值，返回最终样式表。
QString applyThemeTokens(QString styleTemplate, const ThemePalette &p);

} // namespace ui

#endif // UITHEME_H
