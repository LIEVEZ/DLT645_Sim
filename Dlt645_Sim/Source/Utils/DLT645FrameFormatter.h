#ifndef DLT645FRAMEFORMATTER_H
#define DLT645FRAMEFORMATTER_H

#include <QByteArray>
#include <QList>
#include <QString>

class MeterModel;

struct FrameParseCard
{
    QString title;
    QString value;
    QString detail;
    bool valueMonospace = false;
    bool detailMonospace = false;
};

struct FrameParseSection
{
    QString title;
    QList<FrameParseCard> cards;
    QString placeholderText;
};

struct FrameParseDisplayResult
{
    QList<FrameParseSection> sections;
    bool checksumValid = false;
    QString errorText;
    QString noticeText;
};

class DLT645FrameFormatter
{
public:
    static QString formatHex(const QByteArray &data, bool withSpaces = true);
    static QString controlCodeName(quint8 code);
    static QString buildFrameStructureText(const QByteArray &frame);
    static FrameParseDisplayResult buildParseDisplayResult(const QByteArray &frame, const MeterModel *meterModel);
    static FrameParseDisplayResult buildParseDisplayResult(const QByteArray &frame, const MeterModel *meterModel, bool checksumValid);
    static QString buildParseText(const QByteArray &frame, const MeterModel *meterModel);
    static QString buildParseText(const QByteArray &frame, const MeterModel *meterModel, bool checksumValid);
};

#endif // DLT645FRAMEFORMATTER_H
