#ifndef AUTHENTICATIONSTATEFRAMEGENERATOR_H
#define AUTHENTICATIONSTATEFRAMEGENERATOR_H

#include <QByteArray>
#include <QString>

/**
 * @brief 厂商扩展认证状态写报文生成工具
 *
 * 认证状态报文的长度域比实际数据域少一个字节，不能复用标准 DLT645
 * 协议帧构建逻辑。本类只生成展示报文，不执行通信发送。
 */
class AuthenticationStateFrameGenerator
{
public:
    /**
     * @brief 生成认证状态写请求报文
     * @param meterAddress 12 位数字电表地址。
     * @param authenticationEnabled true 为开启认证，false 为关闭认证。
     * @param frame 成功时返回以空格分隔的十六进制报文。
     * @param errorMessage 失败原因，可为 nullptr。
     * @return 生成成功返回 true，否则返回 false。
     */
    static bool generateWriteFrame(const QString &meterAddress,
                                   bool authenticationEnabled,
                                   QString *frame,
                                   QString *errorMessage = nullptr);

private:
    static QString formatFrame(const QByteArray &frame);
    static QByteArray meterAddressToBcd(const QString &meterAddress);
};

#endif // AUTHENTICATIONSTATEFRAMEGENERATOR_H
