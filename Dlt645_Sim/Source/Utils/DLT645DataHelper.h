#ifndef DLT645DATAHELPER_H
#define DLT645DATAHELPER_H

#include <QByteArray>
#include <QString>

class MeterModel;

class DLT645DataHelper
{
public:
    static QByteArray stripLeadingFE(const QByteArray &frame, int *leadingCount = nullptr);
    static QByteArray subtract33(const QByteArray &data);
    static QByteArray decodeReversedPayload(const QByteArray &data);

    static int bytesRequiredForFormat(const QString &format);
    static QString formatDecimalValue(const QByteArray &payload, int decimalPlaces);
    static QString decodePayloadByFormat(const QByteArray &payload, const QString &format, int decimalPlaces);
    static QString decodePayloadForItem(const MeterModel *meterModel,
                                        const QString &diCode,
                                        const QByteArray &payload,
                                        QString *descriptionOut = nullptr,
                                        QString *unitOut = nullptr);
};

#endif // DLT645DATAHELPER_H
