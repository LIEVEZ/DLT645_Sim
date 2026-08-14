#include "Utils/AuthenticationStateFrameGenerator.h"

#include "Utils/ChecksumHelper.h"

#include <QRegularExpression>
#include <QStringList>

namespace
{
constexpr quint8 kPrefixByte = 0xFE;
constexpr int kPrefixLength = 4;
constexpr quint8 kFrameStart = 0x68;
constexpr quint8 kFrameEnd = 0x16;
constexpr quint8 kWriteAuthenticationState = 0x2F;
constexpr quint8 kExtendedDataLength = 0x04;
constexpr quint8 kEncodedAuthenticationEnabled = 0xDD;
constexpr quint8 kEncodedAuthenticationDisabled = 0x33;

const QByteArray kEncodedAuthenticationStateDi = QByteArray::fromHex("0037344F");
} // namespace

bool AuthenticationStateFrameGenerator::generateWriteFrame(const QString &meterAddress,
                                                            bool authenticationEnabled,
                                                            QString *frame,
                                                            QString *errorMessage)
{
    if (frame)
    {
        frame->clear();
    }
    if (errorMessage)
    {
        errorMessage->clear();
    }

    const QString normalizedAddress = meterAddress.trimmed();
    if (!QRegularExpression(QStringLiteral("^\\d{12}$")).match(normalizedAddress).hasMatch())
    {
        if (errorMessage)
        {
            *errorMessage = QString::fromUtf8("电表地址必须为 12 位数字。");
        }
        return false;
    }

    QByteArray rawFrame;
    rawFrame.reserve(21);
    rawFrame.append(QByteArray(kPrefixLength, static_cast<char>(kPrefixByte)));
    rawFrame.append(static_cast<char>(kFrameStart));
    rawFrame.append(meterAddressToBcd(normalizedAddress));
    rawFrame.append(static_cast<char>(kFrameStart));
    rawFrame.append(static_cast<char>(kWriteAuthenticationState));
    // 厂商扩展约定：实际 DI + 状态为 5 字节，长度域固定写为 04。
    rawFrame.append(static_cast<char>(kExtendedDataLength));
    rawFrame.append(kEncodedAuthenticationStateDi);
    // 状态值属于数据域，开启 AA、关闭 00 均需加 33H 后发送。
    rawFrame.append(static_cast<char>(authenticationEnabled ? kEncodedAuthenticationEnabled
                                                            : kEncodedAuthenticationDisabled));
    const QByteArray checksumData = rawFrame.mid(kPrefixLength);
    rawFrame.append(static_cast<char>(ChecksumHelper::calculateChecksum(checksumData)));
    rawFrame.append(static_cast<char>(kFrameEnd));

    if (frame)
    {
        *frame = formatFrame(rawFrame);
    }
    return true;
}

QString AuthenticationStateFrameGenerator::formatFrame(const QByteArray &frame)
{
    QStringList bytes;
    bytes.reserve(frame.size());
    for (char byte : frame)
    {
        bytes.append(QStringLiteral("%1").arg(static_cast<quint8>(byte), 2, 16, QLatin1Char('0')).toUpper());
    }
    return bytes.join(QLatin1Char(' '));
}

QByteArray AuthenticationStateFrameGenerator::meterAddressToBcd(const QString &meterAddress)
{
    QByteArray bcdBytes;
    bcdBytes.reserve(6);
    for (int index = meterAddress.size() - 2; index >= 0; index -= 2)
    {
        const quint8 highNibble = static_cast<quint8>(meterAddress.at(index).digitValue());
        const quint8 lowNibble = static_cast<quint8>(meterAddress.at(index + 1).digitValue());
        bcdBytes.append(static_cast<char>((highNibble << 4) | lowNibble));
    }
    return bcdBytes;
}
