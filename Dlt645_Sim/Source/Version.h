#ifndef VERSION_H
#define VERSION_H

/**
 * @brief 版本信息定义
 *
 * 集中管理 DLT645 电表模拟器的版本与标识信息，
 * 修改版本号时只需在此文件统一调整。
 */

// 软件版本号
#define APP_VERSION_SW                "1.0.0"

// 硬件版本号
#define APP_VERSION_HW                "1.0"

// 固件版本号
#define APP_VERSION_FW                "1.0.0"

// 产品标识
#define APP_PRODUCT_ID                "dlt645-meter-sim"

// 产品名称
#define APP_PRODUCT_NAME              "DLT645-2007 电表模拟器"

// 构建日期（编译时自动填充）
#define APP_BUILD_DATE                __DATE__

// 构建时间（编译时自动填充）
#define APP_BUILD_TIME                __TIME__

#endif // VERSION_H
