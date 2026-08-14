#include "Utils/MeterPasswordGenerator.h"

#include <QByteArray>
#include <QRegularExpression>

bool MeterPasswordGenerator::generate(const QString &meterAddress,
                                      const QString &key,
                                      QString *password,
                                      QString *errorMessage)
{
    if (password)
    {
        password->clear();
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

    const QByteArray keyBytes = key.toUtf8();
    if (keyBytes.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QString::fromUtf8("密钥不能为空。");
        }
        return false;
    }

    const QByteArray addressBytes = meterAddressToBcd(normalizedAddress);
    QByteArray xorBytes(addressBytes.size(), '\0');
    for (int index = 0; index < addressBytes.size(); ++index)
    {
        xorBytes[index] = static_cast<char>(static_cast<quint8>(addressBytes.at(index))
                                            ^ static_cast<quint8>(keyBytes.at(index % keyBytes.size())));
    }

    QByteArray oddBytes;
    QByteArray evenBytes;
    oddBytes.reserve((xorBytes.size() + 1) / 2);
    evenBytes.reserve((xorBytes.size() + 1) / 2);
    for (int index = 0; index < xorBytes.size(); ++index)
    {
        if ((index % 2) == 0)
        {
            evenBytes.append(xorBytes.at(index));
        }
        else
        {
            oddBytes.append(xorBytes.at(index));
        }
    }

    const quint32 combinedCrc = (static_cast<quint32>(calculateCrc16X25(oddBytes)) << 16)
                                | calculateCrc16X25(evenBytes);
    QString generatedPassword = QStringLiteral("%1").arg(combinedCrc, 8, 16, QLatin1Char('0')).toUpper();
    for (int index = 0; index < generatedPassword.size(); ++index)
    {
        const QChar character = generatedPassword.at(index);
        if (character >= QLatin1Char('A') && character <= QLatin1Char('F'))
        {
            generatedPassword[index] = QChar(QLatin1Char('1').unicode()
                                              + character.unicode()
                                              - QLatin1Char('A').unicode());
        }
    }

    if (password)
    {
        *password = generatedPassword.left(6);
    }
    return true;
}

quint16 MeterPasswordGenerator::calculateCrc16X25(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (char byte : data)
    {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc & 0x0001) != 0 ? static_cast<quint16>((crc >> 1) ^ 0x8408)
                                      : static_cast<quint16>(crc >> 1);
        }
    }

    return static_cast<quint16>(~crc);
}

QByteArray MeterPasswordGenerator::meterAddressToBcd(const QString &meterAddress)
{
    QByteArray bcdBytes;
    bcdBytes.reserve(meterAddress.size() / 2);
    for (int index = 0; index < meterAddress.size(); index += 2)
    {
        const quint8 highNibble = static_cast<quint8>(meterAddress.at(index).digitValue());
        const quint8 lowNibble = static_cast<quint8>(meterAddress.at(index + 1).digitValue());
        bcdBytes.append(static_cast<char>((highNibble << 4) | lowNibble));
    }
    return bcdBytes;
}
