#include "ChecksumHelper.h"

quint8 ChecksumHelper::calculateChecksum(const QByteArray &data)
{
    quint8 checksum = 0;

    for (int i = 0; i < data.size(); ++i)
    {
        checksum += static_cast<quint8>(data[i]);
    }

    return checksum;
}

bool ChecksumHelper::verifyChecksum(const QByteArray &data)
{
    if (data.isEmpty())
    {
        return false;
    }

    // 假设最后一个字节是校验和
    quint8 receivedChecksum = static_cast<quint8>(data[data.size() - 1]);

    // 计算除校验和外的数据的校验和
    QByteArray dataWithoutChecksum = data.left(data.size() - 1);
    quint8 calculatedChecksum = calculateChecksum(dataWithoutChecksum);

    return receivedChecksum == calculatedChecksum;
}

quint16 ChecksumHelper::calculateCRC16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;

    for (int i = 0; i < data.size(); ++i)
    {
        crc ^= static_cast<quint8>(data[i]);

        for (int j = 0; j < 8; ++j)
        {
            if (crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}
