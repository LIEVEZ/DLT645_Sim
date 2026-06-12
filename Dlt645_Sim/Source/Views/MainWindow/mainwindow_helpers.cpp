#include "mainwindow.h"

#include "Views/DeviceTree/DeviceTreeWidget.h"
DeviceTreeWidget *MainWindow::deviceTreeWidget() const
{
    return findChild<DeviceTreeWidget *>();
}
