#include "ProtocolParser.h"
#include "Models/DataItem.h"
#include "Utils/BCDHelper.h"
#include "Utils/DLT645BlockHandler.h"
#include "Utils/DLT645DataHelper.h"
#include <algorithm>

namespace
{
QString digitsOnly(QString value)
{
    value.remove(QRegExp("[^0-9]"));
    return value;
}

QByteArray bcdBytesFromDigits(QString digits, int byteCount, bool leastSignificantPairFirst)
{
    digits = digitsOnly(digits).rightJustified(byteCount * 2, QChar('0')).right(byteCount * 2);

    QByteArray bytes;
    bytes.reserve(byteCount);
    for (int i = 0; i < byteCount; ++i)
    {
        const int digitIndex = leastSignificantPairFirst
                                   ? digits.length() - (i + 1) * 2
                                   : i * 2;
        bool ok = false;
        const quint8 byte = static_cast<quint8>(digits.mid(digitIndex, 2).toUInt(&ok, 16));
        bytes.append(static_cast<char>(ok ? byte : 0x00));
    }
    return bytes;
}

void add33(QByteArray &data)
{
    for (int i = 0; i < data.size(); ++i)
    {
        data[i] = static_cast<char>(static_cast<quint8>(data[i]) + 0x33);
    }
}

QByteArray encodeValueBytes(const QVariant &value, const QString &format)
{
    QString formattedValue = BCDHelper::formatValueToString(value, format);
    formattedValue.remove(".");
    QByteArray bytes = BCDHelper::stringToByteArray(formattedValue);
    std::reverse(bytes.begin(), bytes.end());
    add33(bytes);
    return bytes;
}
}

ProtocolParser::ProtocolParser(MeterModel *meterModel, QObject *parent)
    : QObject(parent), m_meterModel(meterModel)
{
}

ProtocolParser::~ProtocolParser()
{
}

void ProtocolParser::setMeterModel(MeterModel *meterModel)
{
    m_meterModel = meterModel;
}

ProtocolFrame ProtocolParser::parseFrame(const QByteArray &data)
{
    ProtocolFrame frame;
    frame.parsedSize = 0; // 初始化为0

    if (data.isEmpty())
    {
        emit parseError("Empty data received");
        return frame;
    }

    int index = 0;

    // 检查前导FE（DLT645-2007协议中，前导码是四个0xFE）
    while (index < data.size() && static_cast<quint8>(data[index]) == 0xFE)
    {
        index++;
    }

    // 检查起始符
    if (index >= data.size() || static_cast<quint8>(data[index]) != 0x68)
    {
        emit parseError("Invalid start flag");
        return frame;
    }
    frame.startFlag = 0x68;
    index++;

    // 读取地址域（6字节）
    if (data.size() < index + 6)
    {
        emit parseError("Incomplete address field");
        return frame;
    }
    frame.address = data.mid(index, 6);
    index += 6;

    // 检查第二个起始符
    if (static_cast<quint8>(data[index]) != 0x68)
    {
        emit parseError("Invalid second start flag");
        return frame;
    }
    frame.startFlag2 = 0x68;
    index++;

    // 读取控制码
    if (data.size() < index + 1)
    {
        emit parseError("Incomplete control code");
        return frame;
    }
    frame.controlCode = static_cast<quint8>(data[index]);
    index++;

    // 读取数据长度
    if (data.size() < index + 1)
    {
        emit parseError("Incomplete data length");
        return frame;
    }
    frame.dataLength = static_cast<quint8>(data[index]);
    index++;

    // 读取数据域
    if (data.size() < index + frame.dataLength)
    {
        emit parseError("Incomplete data field");
        return frame;
    }
    frame.dataField = data.mid(index, frame.dataLength);
    index += frame.dataLength;

    // 读取校验和
    if (data.size() < index + 1)
    {
        emit parseError("Incomplete checksum");
        return frame;
    }
    frame.checksum = static_cast<quint8>(data[index]);
    int checksumIndex = index; // 保存校验和的位置
    index++;

    // 读取结束符
    if (data.size() < index + 1)
    {
        emit parseError("Incomplete end flag");
        return frame;
    }
    frame.endFlag = static_cast<quint8>(data[index]);
    index++; // 移动到结束符之后

    // 设置实际解析的字节数（不包括前导码）
    frame.parsedSize = index;

    // 验证校验和（从起始符开始计算，不包括前导码）
    // 找到第一个0x68的位置（跳过前导码）
    int startIndex = 0;
    while (startIndex < data.size() && static_cast<quint8>(data[startIndex]) == 0xFE)
    {
        startIndex++;
    }
    // startIndex 现在指向第一个 0x68

    // 校验和计算的数据范围：从第一个0x68到校验和之前
    // checksumIndex 指向校验和字节，所以数据域结束位置是 checksumIndex - 1
    QByteArray frameWithoutChecksum = data.mid(startIndex, checksumIndex - startIndex);
    quint8 calculatedChecksum = ChecksumHelper::calculateChecksum(frameWithoutChecksum);

    frame.checksumValid = (calculatedChecksum == frame.checksum);
    if (!frame.checksumValid)
    {
        emit parseError(QString("Checksum mismatch: expected 0x%1, got 0x%2")
                            .arg(calculatedChecksum, 2, 16, QChar('0'))
                            .arg(frame.checksum, 2, 16, QChar('0')));
        return frame;
    }

    return frame;
}

ProtocolFrame ProtocolParser::buildReadRequest(const QByteArray &address, const QString &diCode, bool includePrefixFE)
{
    Q_UNUSED(includePrefixFE);
    ProtocolFrame frame;

    frame.startFlag = 0x68;
    frame.address = address;
    frame.startFlag2 = 0x68;
    frame.controlCode = static_cast<quint8>(ControlCode::ReadData);

    // Data field: DI (4 bytes).
    QByteArray diBytes = diCodeToBytes(diCode);
    frame.dataField = diBytes;
    frame.dataLength = static_cast<quint8>(frame.dataField.size());

    frame.checksum = 0;
    QByteArray frameData = frame.toByteArray();
    frame.checksum = ChecksumHelper::calculateChecksum(frameData.left(frameData.size() - 2));
    frame.endFlag = 0x16;

    return frame;
}

ProtocolFrame ProtocolParser::buildReadAddressRequest(const QByteArray &address, bool includePrefixFE)
{
    Q_UNUSED(includePrefixFE);
    ProtocolFrame frame;

    frame.startFlag = 0x68;
    frame.address = address;
    frame.startFlag2 = 0x68;
    frame.controlCode = static_cast<quint8>(ControlCode::ReadAddress);
    frame.dataField.clear();
    frame.dataLength = 0x00;

    frame.checksum = 0;
    QByteArray frameData = frame.toByteArray();
    frame.checksum = ChecksumHelper::calculateChecksum(frameData.left(frameData.size() - 2));
    frame.endFlag = 0x16;

    return frame;
}

ProtocolFrame ProtocolParser::buildWriteRequest(const QByteArray &address, const QString &diCode, const QVariant &value, bool includePrefixFE)
{
    Q_UNUSED(includePrefixFE);
    ProtocolFrame frame;

    frame.startFlag = 0x68;
    frame.address = address;
    frame.startFlag2 = 0x68;
    frame.controlCode = static_cast<quint8>(ControlCode::WriteData);

    // 数据域：数据标识（4字节）+ 数据长度（1字节）+ 数据值
    QByteArray diBytes = diCodeToBytes(diCode);
    QByteArray valueBytes;

    // 根据值类型转换
    if (value.type() == QVariant::String)
    {
        QString strValue = value.toString();
        // 移除所有非数字字符
        strValue.remove(QRegExp("[^0-9]"));
        // 补齐到偶数长度
        if (strValue.length() % 2 != 0)
        {
            strValue.prepend('0');
        }
        // 转换为BCD码
        for (int i = 0; i < strValue.length(); i += 2)
        {
            QString byteStr = strValue.mid(i, 2);
            quint8 byte = static_cast<quint8>(byteStr.toInt(nullptr, 16));
            valueBytes.append(byte);
        }
    }
    else if (value.type() == QVariant::Double || value.type() == QVariant::Int)
    {
        // 数值类型转换为BCD码
        double numValue = value.toDouble();
        // 转换为整数（假设小数位数为2）
        qint64 intValue = static_cast<qint64>(numValue * 100);
        QString strValue = QString::number(intValue).rightJustified(12, '0');
        // 转换为BCD码
        for (int i = 0; i < strValue.length(); i += 2)
        {
            QString byteStr = strValue.mid(i, 2);
            quint8 byte = static_cast<quint8>(byteStr.toInt(nullptr, 16));
            valueBytes.append(byte);
        }
    }

    // DLT645协议：数据域需要倒序处理
    // 对每个字节加0x33处理
    for (int i = 0; i < valueBytes.size(); ++i)
    {
        valueBytes[i] = static_cast<char>(valueBytes[i] + 0x33);
    }

    std::reverse(valueBytes.begin(), valueBytes.end());

    frame.dataField = diBytes + QByteArray(1, static_cast<quint8>(valueBytes.size())) + valueBytes;
    frame.dataLength = static_cast<quint8>(frame.dataField.size());

    frame.checksum = 0;
    QByteArray frameData = frame.toByteArray();
    frame.checksum = ChecksumHelper::calculateChecksum(frameData.left(frameData.size() - 2));
    frame.endFlag = 0x16;

    return frame;
}

ProtocolFrame ProtocolParser::buildWriteRequest(const QByteArray &address,
                                                const QString &diCode,
                                                const QVariant &value,
                                                const QString &permission,
                                                const QString &password,
                                                const QString &operatorCode,
                                                bool includePrefixFE)
{
    Q_UNUSED(includePrefixFE);
    ProtocolFrame frame;

    frame.startFlag = 0x68;
    frame.address = address;
    frame.startFlag2 = 0x68;
    frame.controlCode = static_cast<quint8>(ControlCode::WriteData);

    QByteArray diBytes = diCodeToBytes(diCode);

    QByteArray authorityBytes;
    authorityBytes.append(bcdBytesFromDigits(permission, 1, false));
    authorityBytes.append(bcdBytesFromDigits(password, 3, false));
    authorityBytes.append(bcdBytesFromDigits(operatorCode, 4, false));
    add33(authorityBytes);

    QByteArray valueBytes;

    QString format;
    if (m_meterModel)
    {
        const DataItem dataItem = m_meterModel->getDataItem(diCode);
        format = dataItem.format;
    }

    if (!format.isEmpty())
    {
        valueBytes = processDataValue(value, format);
        std::reverse(valueBytes.begin(), valueBytes.end());
    }
    else if (value.type() == QVariant::String)
    {
        QString strValue = value.toString();
        strValue.remove(QRegExp("[^0-9]"));
        if (strValue.length() % 2 != 0)
        {
            strValue.prepend('0');
        }

        for (int i = 0; i < strValue.length(); i += 2)
        {
            const QString byteStr = strValue.mid(i, 2);
            valueBytes.append(static_cast<char>(byteStr.toInt(nullptr, 16)));
        }
    }
    else if (value.type() == QVariant::Double || value.type() == QVariant::Int)
    {
        const double numValue = value.toDouble();
        const qint64 intValue = static_cast<qint64>(numValue * 100);
        const QString strValue = QString::number(intValue).rightJustified(12, QChar('0'));
        for (int i = 0; i < strValue.length(); i += 2)
        {
            const QString byteStr = strValue.mid(i, 2);
            valueBytes.append(static_cast<char>(byteStr.toInt(nullptr, 16)));
        }
    }

    if (format.isEmpty())
    {
        std::reverse(valueBytes.begin(), valueBytes.end());
    }
    add33(valueBytes);

    frame.dataField = diBytes + authorityBytes + valueBytes;
    frame.dataLength = static_cast<quint8>(frame.dataField.size());

    frame.checksum = 0;
    QByteArray frameData = frame.toByteArray();
    frame.checksum = ChecksumHelper::calculateChecksum(frameData.left(frameData.size() - 2));
    frame.endFlag = 0x16;

    return frame;
}

QByteArray ProtocolParser::processDataValue(const QVariant &value, const QString &format)
{
    QByteArray result;

    // 使用BCDHelper的格式化方法获取字符串
    QString formattedString = BCDHelper::formatValueToString(value, format);

    // 去除字符串的小数点
    formattedString.remove(".");

    // 将字符串转换为BCD
    result = BCDHelper::stringToByteArray(formattedString);
    return result;
}
ProtocolFrame ProtocolParser::buildReadResponse(const ProtocolFrame &requestFrame, const QVariant &dataValue, const QString &diCode)
{
    ProtocolFrame frame;

    frame.startFlag = 0x68;
    frame.address = requestFrame.address;
    frame.startFlag2 = 0x68;
    frame.controlCode = requestFrame.controlCode | 0x80; // 响应帧控制码最高位置1

    // 数据域：数据标识（4字节）+ 数据长度（1字节）+ 数据值
    QByteArray diBytes = requestFrame.dataField.left(4);
    QByteArray valueBytes;

    // 从MeterModel中获取DataItem的format字段
    if ((requestFrame.controlCode & 0x7F) == static_cast<quint8>(ControlCode::ReadAddress) &&
        dataValue.canConvert<QByteArray>())
    {
        valueBytes = dataValue.toByteArray();
        add33(valueBytes);

        frame.dataField = valueBytes;
        frame.dataLength = static_cast<quint8>(frame.dataField.size());

        frame.checksum = 0;
        QByteArray frameData = frame.toByteArray();
        frame.checksum = ChecksumHelper::calculateChecksum(frameData.left(frameData.size() - 2));
        frame.endFlag = 0x16;

        return frame;
    }

    QString format = "";
    if (m_meterModel && !diCode.isEmpty())
    {
        DataItem dataItem = m_meterModel->getDataItem(diCode);
        format = dataItem.format;
    }

    // 处理多个值的情况，使用冒号分割
    QVariantList valuesList;
    if (dataValue.type() == QVariant::List)
    {
        valuesList = dataValue.toList();
    }
    else
    {
        valuesList.append(dataValue);
    }

    // 解析format字符串，按冒号分割成多个格式部分
    QStringList formatParts = format.split(":");
    // 确保format部分的数量与数据值的数量匹配
    if (formatParts.size() != valuesList.size())
    {
        return frame;
    }

    const QStringList payloadCodes = DLT645BlockHandler::payloadOrderCodesForBlock(m_meterModel, diCode);
    if (!payloadCodes.isEmpty() &&
        payloadCodes.size() == formatParts.size() &&
        payloadCodes.size() == valuesList.size())
    {
        for (int i = 0; i < valuesList.size(); ++i)
        {
            const QString currentFormat = formatParts.at(i);
            if (m_meterModel && !m_meterModel->isDataItemSupported(payloadCodes.at(i)))
            {
                const int byteCount = DLT645DataHelper::bytesRequiredForFormat(currentFormat);
                if (byteCount <= 0)
                {
                    return frame;
                }
                // 不支持的数据项：填充 0xFF 后执行 +0x33 偏移
                // 0xFF + 0x33 = 0x132 -> 取低字节 0x32
                QByteArray unsupportedBytes(byteCount, static_cast<char>(0xFF));
                add33(unsupportedBytes);
                valueBytes.append(unsupportedBytes);
            }
            else
            {
                valueBytes.append(encodeValueBytes(valuesList.at(i), currentFormat));
            }
        }

        frame.dataField = diBytes + valueBytes;
        frame.dataLength = static_cast<quint8>(frame.dataField.size());

        frame.checksum = 0;
        QByteArray frameData = frame.toByteArray();
        frame.checksum = ChecksumHelper::calculateChecksum(frameData.left(frameData.size() - 2));
        frame.endFlag = 0x16;

        return frame;
    }

    // 处理所有值，根据对应格式进行处理
    for (int i = 0; i < valuesList.size(); ++i)
    {
        const QVariant &value = valuesList[i];

        // 获取当前值对应的格式部分
        QString currentFormat = formatParts.size() > i ? formatParts[i] : (formatParts.isEmpty() ? "" : formatParts.last());

        // 使用新的processDataValue方法处理数据值
        QByteArray processedBytes = processDataValue(value, currentFormat);
        std::reverse(processedBytes.begin(), processedBytes.end());
        valueBytes.append(processedBytes);
    }
    // DLT645协议：数据域需要倒序处理
    // 对每个字节加0x33处理
    for (int i = 0; i < valueBytes.size(); ++i)
    {
        valueBytes[i] = static_cast<char>(valueBytes[i] + 0x33);
    }

    frame.dataField = diBytes + valueBytes;
    frame.dataLength = static_cast<quint8>(frame.dataField.size());

    frame.checksum = 0;
    QByteArray frameData = frame.toByteArray();
    frame.checksum = ChecksumHelper::calculateChecksum(frameData.left(frameData.size() - 2));
    frame.endFlag = 0x16;

    return frame;
}

ProtocolFrame ProtocolParser::buildWriteResponse(const ProtocolFrame &requestFrame, bool success)
{
    ProtocolFrame frame;

    frame.startFlag = 0x68;
    frame.address = requestFrame.address;
    frame.startFlag2 = 0x68;
    frame.controlCode = requestFrame.controlCode | 0x80;

    // 数据域：数据标识（4字节）+ 数据长度（1字节）+ 状态字（1字节）
    QByteArray diBytes = requestFrame.dataField.left(4);
    quint8 status = success ? 0x00 : 0xFF;
    frame.dataField = diBytes + QByteArray(1, 0x01) + QByteArray(1, status);
    frame.dataLength = static_cast<quint8>(frame.dataField.size());

    frame.checksum = 0;
    QByteArray frameData = frame.toByteArray();
    frame.checksum = ChecksumHelper::calculateChecksum(frameData.left(frameData.size() - 2));
    frame.endFlag = 0x16;

    return frame;
}

ProtocolFrame ProtocolParser::buildErrorResponse(const ProtocolFrame &requestFrame, quint8 errorCode)
{
    ProtocolFrame frame;

    frame.startFlag = 0x68;
    frame.address = requestFrame.address;
    frame.startFlag2 = 0x68;
    frame.controlCode = requestFrame.controlCode | 0x80;

    // 数据域：错误信息（1字节）
    frame.dataField = QByteArray(1, errorCode);
    frame.dataLength = 0x01;

    frame.checksum = 0;
    QByteArray frameData = frame.toByteArray();
    frame.checksum = ChecksumHelper::calculateChecksum(frameData.left(frameData.size() - 2));
    frame.endFlag = 0x16;

    return frame;
}

QString ProtocolParser::extractDIFromDataField(const QByteArray &dataField)
{
    if (dataField.size() >= 4)
    {
        QByteArray diBytes = dataField.left(4);
        // DLT645协议：DI需要倒序处理
        std::reverse(diBytes.begin(), diBytes.end());
        return bytesToDI(diBytes);
    }
    return QString();
}

QByteArray ProtocolParser::diCodeToBytes(const QString &diCode)
{
    QString code = diCode;
    code.remove("0x");

    // 补齐到8位
    while (code.length() < 8)
    {
        code.prepend('0');
    }

    QByteArray bytes;
    for (int i = 0; i < code.length(); i += 2)
    {
        QString byteStr = code.mid(i, 2);
        bool ok;
        quint8 byte = static_cast<quint8>(byteStr.toInt(&ok, 16));
        if (ok)
        {
            // DLT645协议：DI需要减去0x33，所以转换时需要加上0x33
            byte += 0x33;
            bytes.append(byte);
        }
    }

    // DLT645协议：DI需要倒序处理
    std::reverse(bytes.begin(), bytes.end());

    return bytes;
}

QString ProtocolParser::bytesToDI(const QByteArray &bytes)
{
    QString hexString;
    for (int i = 0; i < bytes.size(); ++i)
    {
        quint8 byte = static_cast<quint8>(bytes[i]);
        // DLT645协议：DI需要减去0x33
        byte -= 0x33;
        hexString += QString("%1").arg(byte, 2, 16, QChar('0')).toUpper();
    }
    return "0x" + hexString;
}

bool ProtocolParser::isValidAddress(const QString &address)
{
    if (address.length() != 12)
    {
        return false;
    }

    for (int i = 0; i < address.length(); ++i)
    {
        if (!address[i].isDigit())
        {
            return false;
        }
    }

    return true;
}

QByteArray ProtocolParser::addressToBytes(const QString &address)
{
    QByteArray bytes;

    // DLT645协议：地址需要倒序存储
    // 例如：地址"123456789012" -> 字节顺序为 [12, 90, 78, 56, 34, 12]
    for (int i = address.length() - 2; i >= 0; i -= 2)
    {
        QString byteStr = address.mid(i, 2);
        quint8 high = byteStr[0].digitValue();
        quint8 low = byteStr[1].digitValue();
        quint8 byte = (high << 4) | low;
        bytes.append(byte);
    }

    return bytes;
}

QString ProtocolParser::bytesToAddress(const QByteArray &bytes)
{
    QString address;

    // DLT645协议：地址需要倒序解析
    // 例如：字节顺序为 [12, 90, 78, 56, 34, 12] -> 地址"123456789012"
    for (int i = bytes.size() - 1; i >= 0; --i)
    {
        quint8 byte = static_cast<quint8>(bytes[i]);
        quint8 high = (byte >> 4) & 0x0F;
        quint8 low = byte & 0x0F;
        address += QString::number(high) + QString::number(low);
    }

    return address;
}

QByteArray ProtocolParser::frameToRawBytes(const ProtocolFrame &frame, bool includePrefixFE)
{
    QByteArray rawBytes = frame.toByteArray();
    if (includePrefixFE)
    {
        rawBytes.prepend(static_cast<char>(0xFE));
        rawBytes.prepend(static_cast<char>(0xFE));
        rawBytes.prepend(static_cast<char>(0xFE));
        rawBytes.prepend(static_cast<char>(0xFE));
    }

    return rawBytes;
}

bool ProtocolFrame::isValid() const
{
    return startFlag == 0x68 &&
           startFlag2 == 0x68 &&
           endFlag == 0x16 &&
           address.size() == 6;
}

QByteArray ProtocolFrame::toByteArray() const
{
    QByteArray data;

    data.append(startFlag);
    data.append(address);
    data.append(startFlag2);
    data.append(controlCode);
    data.append(dataLength);
    data.append(dataField);
    data.append(checksum);
    data.append(endFlag);

    return data;
}

QString ProtocolFrame::toString() const
{
    QString str = QString("Start=0x%1, Addr=%2, Ctrl=0x%3, Len=%4, Data=%5, CS=0x%6, End=0x%7")
                      .arg(startFlag, 2, 16, QChar('0'))
                      .arg(ProtocolParser::bytesToAddress(address))
                      .arg(controlCode, 2, 16, QChar('0'))
                      .arg(dataLength)
                      .arg(BCDHelper::bytesToHexString(dataField, ""))
                      .arg(checksum, 2, 16, QChar('0'))
                      .arg(endFlag, 2, 16, QChar('0'));

    return str;
}
