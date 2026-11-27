#include "WindowBehavior.h"

#include <QEvent>
#include <QObject>
#include <QTimer>
#include <QWidget>

namespace
{
class HideOnMinimizeFilter : public QObject
{
public:
    explicit HideOnMinimizeFilter(QWidget *window)
        : QObject(window), m_window(window), m_hidePending(false)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched);

        if (event->type() == QEvent::WindowStateChange && !m_hidePending && m_window->isMinimized())
        {
            // 等到本轮窗口状态变更处理完再隐藏；在事件内同步 hide() 会被随后的
            // 原生最小化/恢复流程重新显示出来。
            m_hidePending = true;
            QTimer::singleShot(0, m_window, [this]() {
                m_hidePending = false;
                m_window->setWindowState(m_window->windowState() & ~Qt::WindowMinimized);
                m_window->hide();
            });
        }

        return QObject::eventFilter(watched, event);
    }

private:
    QWidget *m_window;
    bool m_hidePending;
};
} // namespace

namespace ui
{
void hideOnMinimize(QWidget *window)
{
    if (!window)
    {
        return;
    }

    window->setWindowFlags(window->windowFlags() | Qt::WindowMinimizeButtonHint);
    window->installEventFilter(new HideOnMinimizeFilter(window));
}
} // namespace ui
