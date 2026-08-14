#ifndef METERAUTHENTICATIONCOMMANDGENERATOR_H
#define METERAUTHENTICATIONCOMMANDGENERATOR_H

#include <QByteArray>
#include <QDateTime>
#include <QString>

/**
 * @brief 加密电表认证下行命令生成工具
 *
 * 按控制码 14H、数据标识 07030001、02 级密码和固定操作者代码
 * 00000000 生成认证命令。本类只生成报文，不执行通信发送。
 */
class MeterAuthenticationCommandGenerator
{
public:
    /**
     * @brief 生成电表认证下行命令
     * @param meterAddress 12 位数字电表地址。
     * @param password 02 级六位数字密码。
     * @param deadline 截止时间，精确到分钟。
     * @param baselineTime 基准时间，精确到分钟。
     * @param frame 成功时返回以空格分隔的十六进制报文。
     * @param errorMessage 失败原因，可为 nullptr。
     * @return 生成成功返回 true，否则返回 false。
     */
    static bool generate(const QString &meterAddress,
                         const QString &password,
                         const QDateTime &deadline,
                         const QDateTime &baselineTime,
                         QString *frame,
                         QString *errorMessage = nullptr);

private:
    static void add33(QByteArray *data);
    static char toBcd(int value);
    static QString formatFrame(const QByteArray &frame);
    static QByteArray dateTimeToBcd(const QDateTime &dateTime);
    static QByteArray meterAddressToBcd(const QString &meterAddress);
    static QByteArray passwordAndPermissionToBcd(const QString &password);
};

#endif // METERAUTHENTICATIONCOMMANDGENERATOR_H
