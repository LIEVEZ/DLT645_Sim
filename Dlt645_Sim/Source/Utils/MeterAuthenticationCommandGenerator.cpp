#include "Utils/MeterAuthenticationCommandGenerator.h"

#include "Utils/ChecksumHelper.h"

#include <QRegularExpression>
#include <QStringList>

namespace
{
constexpr quint8 kPrefixByte = 0xFE;
constexpr int kPrefixLength = 4;
constexpr quint8 kFrameStart = 0x68;
constexpr quint8 kFrameEnd = 0x16;
constexpr quint8 kWriteDataControlCode = 0x14;
constexpr quint8 kAuthenticationDataLength = 0x18;
constexpr quint8 kLevelTwoPermission = 0x02;
constexpr int kMinimumYear = 2000;
constexpr int kMaximumYear = 2099;

const QByteArray kEncodedAuthenticationDi = QByteArray::fromHex("3433363A");
const QByteArray kEncodedOperatorCode = QByteArray(4, static_cast<char>(0x33));
} // namespace

bool MeterAuthenticationCommandGenerator::generate(const QString &meterAddress,
                                                    const QString &password,
                                                    const QDateTime &deadline,
                                                    const QDateTime &baselineTime,
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

    const QString normalizedPassword = password.trimmed();
    if (!QRegularExpression(QStringLiteral("^\\d{6}$")).match(normalizedPassword).hasMatch())
    {
        if (errorMessage)
        {
            *errorMessage = QString::fromUtf8("02 级密码必须为 6 位数字。");
        }
        return false;
    }

    if (!deadline.isValid() || !baselineTime.isValid()
        || deadline.date().year() < kMinimumYear || deadline.date().year() > kMaximumYear
        || baselineTime.date().year() < kMinimumYear || baselineTime.date().year() > kMaximumYear)
    {
        if (errorMessage)
        {
            *errorMessage = QString::fromUtf8("认证时间必须在 2000-2099 年范围内。");
        }
        return false;
    }

    if (deadline < baselineTime)
    {
        if (errorMessage)
        {
            *errorMessage = QString::fromUtf8("截止时间不能早于基准时间。");
        }
        return false;
    }

    QByteArray timeData = dateTimeToBcd(deadline);
    timeData.append(dateTimeToBcd(baselineTime));
    add33(&timeData);

    QByteArray rawFrame;
    rawFrame.reserve(40);
    rawFrame.append(QByteArray(kPrefixLength, static_cast<char>(kPrefixByte)));
    rawFrame.append(static_cast<char>(kFrameStart));
    rawFrame.append(meterAddressToBcd(normalizedAddress));
    rawFrame.append(static_cast<char>(kFrameStart));
    rawFrame.append(static_cast<char>(kWriteDataControlCode));
    rawFrame.append(static_cast<char>(kAuthenticationDataLength));
    rawFrame.append(kEncodedAuthenticationDi);
    rawFrame.append(passwordAndPermissionToBcd(normalizedPassword));
    rawFrame.append(kEncodedOperatorCode);
    rawFrame.append(timeData);
    const QByteArray checksumData = rawFrame.mid(kPrefixLength);
    rawFrame.append(static_cast<char>(ChecksumHelper::calculateChecksum(checksumData)));
    rawFrame.append(static_cast<char>(kFrameEnd));

    if (frame)
    {
        *frame = formatFrame(rawFrame);
    }
    return true;
}

void MeterAuthenticationCommandGenerator::add33(QByteArray *data)
{
    if (!data)
    {
        return;
    }

    for (int index = 0; index < data->size(); ++index)
    {
        (*data)[index] = static_cast<char>(static_cast<quint8>(data->at(index)) + 0x33);
    }
}

char MeterAuthenticationCommandGenerator::toBcd(int value)
{
    return static_cast<char>(((value / 10) << 4) | (value % 10));
}

QString MeterAuthenticationCommandGenerator::formatFrame(const QByteArray &frame)
{
    QStringList bytes;
    bytes.reserve(frame.size());
    for (char byte : frame)
    {
        bytes.append(QStringLiteral("%1").arg(static_cast<quint8>(byte), 2, 16, QLatin1Char('0')).toUpper());
    }
    return bytes.join(QLatin1Char(' '));
}

QByteArray MeterAuthenticationCommandGenerator::dateTimeToBcd(const QDateTime &dateTime)
{
    const QDate date = dateTime.date();
    const QTime time = dateTime.time();
    const int year = date.year();

    QByteArray bytes;
    bytes.reserve(6);
    // 时间属于数据域，按低字节在前发送：分、时、日、月、年、世纪。
    bytes.append(toBcd(time.minute()));
    bytes.append(toBcd(time.hour()));
    bytes.append(toBcd(date.day()));
    bytes.append(toBcd(date.month()));
    bytes.append(toBcd(year % 100));
    bytes.append(toBcd(year / 100));
    return bytes;
}

QByteArray MeterAuthenticationCommandGenerator::meterAddressToBcd(const QString &meterAddress)
{
    QByteArray bytes;
    bytes.reserve(6);
    for (int index = meterAddress.size() - 2; index >= 0; index -= 2)
    {
        const quint8 highNibble = static_cast<quint8>(meterAddress.at(index).digitValue());
        const quint8 lowNibble = static_cast<quint8>(meterAddress.at(index + 1).digitValue());
        bytes.append(static_cast<char>((highNibble << 4) | lowNibble));
    }
    return bytes;
}

QByteArray MeterAuthenticationCommandGenerator::passwordAndPermissionToBcd(const QString &password)
{
    QByteArray bytes;
    bytes.reserve(4);
    bytes.append(static_cast<char>(kLevelTwoPermission));
    for (int index = password.size() - 2; index >= 0; index -= 2)
    {
        const quint8 highNibble = static_cast<quint8>(password.at(index).digitValue());
        const quint8 lowNibble = static_cast<quint8>(password.at(index + 1).digitValue());
        bytes.append(static_cast<char>((highNibble << 4) | lowNibble));
    }
    add33(&bytes);
    return bytes;
}
