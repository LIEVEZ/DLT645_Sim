#ifndef WINDOWBEHAVIOR_H
#define WINDOWBEHAVIOR_H

class QWidget;

namespace ui
{

/**
 * @brief 把窗口的“最小化”改成“隐藏”
 *
 * 工具类对话框以主窗口为父窗口，最小化后不会出现在任务栏，用户很难找回；
 * 安装本行为后，点击标题栏最小化按钮会直接隐藏窗口，再次从工具栏/菜单
 * 打开时 show() 即可立即恢复。
 * @param window 需要安装行为的顶层窗口
 */
void hideOnMinimize(QWidget *window);

} // namespace ui

#endif // WINDOWBEHAVIOR_H
