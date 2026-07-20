#include <QtTest>

#include "Utils/BCDHelper.h"

class PowerFactorFormatTest : public QObject
{
    Q_OBJECT

private slots:
    void encodeSignedValues();
    void decodeSignedValues();
    void preserveTwoByteBlockLength();
    void preserveOrdinaryXxFormat();
};

void PowerFactorFormatTest::encodeSignedValues()
{
    QCOMPARE(BCDHelper::formatValueToString(0.969, QStringLiteral("PF,xx,AA")), QStringLiteral("0969"));
    QCOMPARE(BCDHelper::formatValueToString(-0.969, QStringLiteral("PF,xx,AA")), QStringLiteral("8969"));
    QCOMPARE(BCDHelper::formatValueToString(-1.000, QStringLiteral("PF,xx,AA")), QStringLiteral("9000"));
    QCOMPARE(BCDHelper::formatValueToString(0.000, QStringLiteral("PF,xx,AA")), QStringLiteral("0000"));
    QCOMPARE(BCDHelper::formatValueToString(1.000, QStringLiteral("PF,xx,AA")), QStringLiteral("1000"));
}

void PowerFactorFormatTest::decodeSignedValues()
{
    QCOMPARE(BCDHelper::decodeValueFromFormat(QByteArray::fromHex("0969"), QStringLiteral("PF,xx,AA")),
             QStringLiteral("0.969"));
    QCOMPARE(BCDHelper::decodeValueFromFormat(QByteArray::fromHex("8969"), QStringLiteral("PF,xx,AA")),
             QStringLiteral("-0.969"));
    QCOMPARE(BCDHelper::decodeValueFromFormat(QByteArray::fromHex("9000"), QStringLiteral("PF,xx,AA")),
             QStringLiteral("-1.000"));
}

void PowerFactorFormatTest::preserveTwoByteBlockLength()
{
    const QStringList values = {QStringLiteral("0.969"),
                                QStringLiteral("-0.485"),
                                QStringLiteral("0.089"),
                                QStringLiteral("-1.000")};
    QByteArray payload;
    for (const QString &value : values)
    {
        payload.append(BCDHelper::stringToByteArray(
            BCDHelper::formatValueToString(value.toDouble(), QStringLiteral("PF,xx,AA"))));
    }
    QCOMPARE(payload.size(), 8);
}

void PowerFactorFormatTest::preserveOrdinaryXxFormat()
{
    QVERIFY(BCDHelper::decodeValueFromFormat(QByteArray::fromHex("0969"), QStringLiteral("xx,AA")).isEmpty());
}

QTEST_APPLESS_MAIN(PowerFactorFormatTest)

#include "PowerFactorFormatTest.moc"
