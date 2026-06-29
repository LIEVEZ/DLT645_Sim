#include "Utils/UiTheme.h"

#include <QHash>

namespace ui
{

ThemePalette darkPalette()
{
    ThemePalette p;
    // 背景层级：中性灰朝青微偏
    p.windowBg = "#262B2E";
    p.panelBg = "#353A3D";
    p.panelHover = "#3F4549";
    p.panelPressed = "#2A2F32";
    p.readonlyBg = "#2E3336";
    // 边框
    p.border = "#4C5256";
    p.borderStrong = "#565D61";
    p.gridLine = "#343A3D";
    // 文字：微着色白
    p.text = "#E6EBED";
    p.textMuted = "rgba(230, 235, 237, 0.42)";
    p.textReadonly = "#C9D1D9";
    p.onAccent = "#FFFFFF";
    // 强调：钢青蓝
    p.accent = "#2D8FB3";
    p.accentHover = "#34A0C4";
    p.accentPressed = "#247A9A";
    p.accentTitle = "#5FB8D4";
    // 语义色
    p.success = "#22C55E";
    p.danger = "#F87171";
    p.warning = "#FCD34D";
    // 日志级别色（暗底，接入钢青蓝体系：Info/通信取品牌色亮调，其余复用语义色）
    p.logInfo = "#7FC4DE";    // 信息：钢青蓝亮调
    p.logSuccess = "#22C55E"; // 成功：语义绿
    p.logComm = "#5FB8D4";    // 通信：强调钢青蓝亮调
    p.logWarning = "#FCD34D"; // 警告：语义黄
    p.logError = "#F87171";   // 错误：语义红
    // Banner
    p.noticeBg = "#31434A";
    p.noticeBorder = "#46646D";
    p.noticeText = "#D9F7F4";
    p.errorBg = "#4A2F2F";
    p.errorBorder = "#7A4C4C";
    p.errorText = "#FFDADA";
    // 报文解析卡片
    p.cardTitle = "#8ED9FF";
    p.cardDetail = "#B7C0C4";
    // 工具栏
    p.toolbarTop = "#525A5E";
    p.toolbarBottom = "#464E51";
    p.toolbarEdge = "#353B3D";
    p.tbSeparator = "rgba(255, 255, 255, 0.18)";
    p.tbHover = "rgba(255, 255, 255, 0.08)";
    p.tbHoverEdge = "rgba(255, 255, 255, 0.18)";
    p.tbPressed = "rgba(45, 143, 179, 0.72)";
    p.tbPressedEdge = "rgba(95, 184, 212, 0.55)";
    p.barBg = "#41484B";
    return p;
}

ThemePalette lightPalette()
{
    ThemePalette p;
    // 背景层级：中性白朝青微偏
    p.windowBg = "#FAFBFC";
    p.panelBg = "#FFFFFF";
    p.panelHover = "#EDF0F2";
    p.panelPressed = "#DDE3E6";
    p.readonlyBg = "#F2F5F6";
    // 边框
    p.border = "#C7CFD3";
    p.borderStrong = "#C7CFD3";
    p.gridLine = "#E2E7EA";
    // 文字：微着色黑
    p.text = "#1B2226";
    p.textMuted = "rgba(27, 34, 38, 0.40)";
    p.textReadonly = "#5A6469";
    p.onAccent = "#FFFFFF";
    // 强调：钢青蓝（更深保证白底对比）
    p.accent = "#0F7AA0";
    p.accentHover = "#0C6A8C";
    p.accentPressed = "#0A5871";
    p.accentTitle = "#0F7AA0";
    // 语义色
    p.success = "#15803D";
    p.danger = "#DC2626";
    p.warning = "#B45309";
    // 日志级别色（白底，取更深的同色相以保证对比度）
    p.logInfo = "#0F7AA0";    // 信息：钢青蓝
    p.logSuccess = "#15803D"; // 成功：语义绿（深）
    p.logComm = "#0C6A8C";    // 通信：钢青蓝（更深）
    p.logWarning = "#B45309"; // 警告：语义黄（深）
    p.logError = "#DC2626";   // 错误：语义红（深）
    // Banner
    p.noticeBg = "#EAF6F3";
    p.noticeBorder = "#B5D5CD";
    p.noticeText = "#1D5B50";
    p.errorBg = "#FBECEC";
    p.errorBorder = "#E3B4B4";
    p.errorText = "#9F2F2F";
    // 报文解析卡片
    p.cardTitle = "#006D9C";
    p.cardDetail = "#666666";
    // 工具栏
    p.toolbarTop = "#FAFCFD";
    p.toolbarBottom = "#EEF2F4";
    p.toolbarEdge = "#D6DDE1";
    p.tbSeparator = "rgba(0, 0, 0, 0.14)";
    p.tbHover = "rgba(0, 0, 0, 0.05)";
    p.tbHoverEdge = "rgba(0, 0, 0, 0.12)";
    p.tbPressed = "rgba(15, 122, 160, 0.12)";
    p.tbPressedEdge = "rgba(15, 122, 160, 0.32)";
    p.barBg = "#EDF1F3";
    return p;
}

namespace
{
// 全应用统一的按钮样式片段。模板中用 {{buttonBlock}} 占位，applyThemeTokens
// 会先把它展开为下面这段 QSS，再统一替换其中的 {{accent}} 等颜色 token。
//
// 设计：
//   - 次要按钮（默认外观）：面板底色 + 细边框，hover 时边框转强、底色提亮；
//   - 主按钮（QPushButton:default，即对话框「确定 / 连接 / 发送」等接受动作）：
//     钢青蓝实心 + 顶部微高光的竖向渐变，营造工业金属质感的层次；
//   - 统一 6px 圆角、min-width / min-height 保证按钮尺寸协调；
//   - 新增 :focus 强调边框，补齐键盘可达性（原样式缺失）。
QString buttonBlockTemplate()
{
    return QStringLiteral(R"(
        /* —— 次要按钮（默认外观） —— */
        QPushButton {
            background-color: {{panelBg}};
            color: {{text}};
            border: 1px solid {{border}};
            border-radius: 6px;
            padding: 6px 16px;
            min-width: 72px;
            min-height: 16px;
            font-weight: 500;
        }

        QPushButton:hover {
            background-color: {{panelHover}};
            border-color: {{borderStrong}};
        }

        QPushButton:pressed {
            background-color: {{panelPressed}};
        }

        QPushButton:focus {
            border: 1px solid {{accent}};
            outline: none;
        }

        QPushButton:disabled {
            color: {{textMuted}};
            border-color: {{border}};
        }

        /* —— 主按钮（对话框默认动作）：钢青蓝实心 + 顶部微高光渐变 —— */
        QPushButton:default {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                              stop:0 {{accentHover}}, stop:1 {{accent}});
            color: {{onAccent}};
            border: 1px solid {{accentPressed}};
        }

        QPushButton:default:hover {
            background-color: {{accentHover}};
            border-color: {{accentHover}};
        }

        QPushButton:default:pressed {
            background-color: {{accentPressed}};
            border-color: {{accentPressed}};
        }

        QPushButton:default:focus {
            border: 1px solid {{onAccent}};
        }

        QPushButton:default:disabled {
            background-color: {{panelBg}};
            color: {{textMuted}};
            border: 1px solid {{border}};
        }
    )");
}
} // namespace

QString applyThemeTokens(QString styleTemplate, const ThemePalette &p)
{
    // 先把 {{buttonBlock}} 展开为按钮样式片段（其中仍含颜色 token），
    // 再交给下面的颜色 token 循环统一替换，确保按钮与其余控件取同一调色板。
    styleTemplate.replace(QStringLiteral("{{buttonBlock}}"), buttonBlockTemplate());

    const QHash<QString, QString> tokens = {
        {"windowBg", p.windowBg},
        {"panelBg", p.panelBg},
        {"panelHover", p.panelHover},
        {"panelPressed", p.panelPressed},
        {"readonlyBg", p.readonlyBg},
        {"border", p.border},
        {"borderStrong", p.borderStrong},
        {"gridLine", p.gridLine},
        {"text", p.text},
        {"textMuted", p.textMuted},
        {"textReadonly", p.textReadonly},
        {"onAccent", p.onAccent},
        {"accent", p.accent},
        {"accentHover", p.accentHover},
        {"accentPressed", p.accentPressed},
        {"accentTitle", p.accentTitle},
        {"success", p.success},
        {"danger", p.danger},
        {"warning", p.warning},
        {"noticeBg", p.noticeBg},
        {"noticeBorder", p.noticeBorder},
        {"noticeText", p.noticeText},
        {"errorBg", p.errorBg},
        {"errorBorder", p.errorBorder},
        {"errorText", p.errorText},
        {"cardTitle", p.cardTitle},
        {"cardDetail", p.cardDetail},
        {"toolbarTop", p.toolbarTop},
        {"toolbarBottom", p.toolbarBottom},
        {"toolbarEdge", p.toolbarEdge},
        {"tbSeparator", p.tbSeparator},
        {"tbHover", p.tbHover},
        {"tbHoverEdge", p.tbHoverEdge},
        {"tbPressed", p.tbPressed},
        {"tbPressedEdge", p.tbPressedEdge},
        {"barBg", p.barBg},
    };

    for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it)
        styleTemplate.replace("{{" + it.key() + "}}", it.value());

    return styleTemplate;
}

} // namespace ui
