#include <QtTest>

#include <memory>

#include "Controllers/ProtocolController.h"
#include "Models/MeterModel.h"
#include "Services/ProtocolParser.h"
#include "Utils/ChecksumHelper.h"

class ProtocolControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void rejectsInvalidWriteCredentials();
    void acceptsConfiguredWriteCredentials();
    void rejectsInvalidRemoteControlCredentials();
    void acceptsConfiguredRemoteControlCredentials();
    void writesCommunicationAddressDataItem();
    void routesRequestsUsingNewAddressAfterAddressWrite();
};

namespace
{
constexpr const char *kMeterAddress = "202101190552";
constexpr const char *kPassword = "02012345";
constexpr const char *kOperatorCode = "87654321";

std::unique_ptr<ProtocolController> createController(MeterModel **meterModel)
{
    auto controller = std::make_unique<ProtocolController>();
    auto *model = new MeterModel(controller.get());
    model->setMeterAddress(QString::fromLatin1(kMeterAddress));
    model->setPassword(QString::fromLatin1(kPassword));
    model->setOperatorCode(QString::fromLatin1(kOperatorCode));
    controller->setMeterModel(model);
    controller->registerMeterModel(QString::fromLatin1(kMeterAddress), model);
    controller->setKnownMeterAddresses({QString::fromLatin1(kMeterAddress)});
    controller->setHasMeter(true);
    *meterModel = model;
    return controller;
}

QByteArray rawWriteRequest(MeterModel *model, const QString &permission, const QString &password,
                           const QString &operatorCode,
                           const QString &address = QString::fromLatin1(kMeterAddress))
{
    ProtocolParser parser(model);
    const ProtocolFrame frame = parser.buildWriteRequest(
        ProtocolParser::addressToBytes(address),
        QStringLiteral("0x04000302"),
        9,
        permission,
        password,
        operatorCode,
        true);
    return ProtocolParser::frameToRawBytes(frame, true);
}

QByteArray rawWriteAddressRequest(const QString &newAddress)
{
    ProtocolFrame frame;
    frame.startFlag = 0x68;
    frame.address = ProtocolParser::addressToBytes(QString::fromLatin1(kMeterAddress));
    frame.startFlag2 = 0x68;
    frame.controlCode = static_cast<quint8>(ControlCode::WriteAddress);
    frame.dataField = ProtocolParser::addressToBytes(newAddress);
    for (int i = 0; i < frame.dataField.size(); ++i)
    {
        frame.dataField[i] = static_cast<char>(static_cast<quint8>(frame.dataField.at(i)) + 0x33);
    }
    frame.dataLength = static_cast<quint8>(frame.dataField.size());
    frame.checksum = 0;
    frame.endFlag = 0x16;
    const QByteArray frameData = frame.toByteArray();
    frame.checksum = ChecksumHelper::calculateChecksum(frameData.left(frameData.size() - 2));
    return ProtocolParser::frameToRawBytes(frame, true);
}

QByteArray rawCommunicationAddressDataWriteRequest(MeterModel *model, const QString &newAddress)
{
    ProtocolParser parser(model);
    const ProtocolFrame frame = parser.buildWriteRequest(
        ProtocolParser::addressToBytes(QString::fromLatin1(kMeterAddress)),
        QStringLiteral("0x04000401"),
        newAddress,
        QStringLiteral("02"),
        QStringLiteral("012345"),
        QString::fromLatin1(kOperatorCode),
        true);
    return ProtocolParser::frameToRawBytes(frame, true);
}

QByteArray rawRemoteControlRequest(MeterModel *model, const QString &permission, const QString &password,
                                   const QString &operatorCode)
{
    ProtocolParser parser(model);
    const ProtocolFrame frame = parser.buildRemoteControlRequest(
        ProtocolParser::addressToBytes(QString::fromLatin1(kMeterAddress)),
        0x1A,
        QByteArray(7, '\0'),
        permission,
        password,
        operatorCode,
        true);
    return ProtocolParser::frameToRawBytes(frame, true);
}
}

void ProtocolControllerTest::rejectsInvalidWriteCredentials()
{
    MeterModel *model = nullptr;
    const std::unique_ptr<ProtocolController> controller = createController(&model);
    const QVariant before = model->getDataValue(QStringLiteral("0x04000302"));

    controller->processReceivedData(rawWriteRequest(model, QStringLiteral("00"), QStringLiteral("000000"),
                                                    QStringLiteral("00000000")));

    QCOMPARE(model->getDataValue(QStringLiteral("0x04000302")), before);
}

void ProtocolControllerTest::acceptsConfiguredWriteCredentials()
{
    MeterModel *model = nullptr;
    const std::unique_ptr<ProtocolController> controller = createController(&model);

    controller->processReceivedData(rawWriteRequest(model, QStringLiteral("02"), QStringLiteral("012345"),
                                                    QString::fromLatin1(kOperatorCode)));

    QCOMPARE(model->getDataValue(QStringLiteral("0x04000302")).toInt(), 9);
}

void ProtocolControllerTest::rejectsInvalidRemoteControlCredentials()
{
    MeterModel *model = nullptr;
    const std::unique_ptr<ProtocolController> controller = createController(&model);
    const QVariant before = model->getDataValue(QStringLiteral("0x04000503"));

    controller->processReceivedData(rawRemoteControlRequest(model,
                                                            QStringLiteral("00"),
                                                            QStringLiteral("000000"),
                                                            QStringLiteral("00000000")));

    QCOMPARE(model->getDataValue(QStringLiteral("0x04000503")), before);
}

void ProtocolControllerTest::acceptsConfiguredRemoteControlCredentials()
{
    MeterModel *model = nullptr;
    const std::unique_ptr<ProtocolController> controller = createController(&model);

    controller->processReceivedData(rawRemoteControlRequest(model,
                                                            QStringLiteral("02"),
                                                            QStringLiteral("012345"),
                                                            QString::fromLatin1(kOperatorCode)));

    const quint16 statusWord3 = static_cast<quint16>(model->getDataValue(QStringLiteral("0x04000503")).toUInt());
    QVERIFY(statusWord3 & (1U << 4));
    QVERIFY(statusWord3 & (1U << 6));
}

void ProtocolControllerTest::writesCommunicationAddressDataItem()
{
    constexpr const char *kNewAddress = "202101190553";
    MeterModel *model = nullptr;
    const std::unique_ptr<ProtocolController> controller = createController(&model);

    controller->processReceivedData(
        rawCommunicationAddressDataWriteRequest(model, QString::fromLatin1(kNewAddress)));

    QCOMPARE(model->getMeterAddress(), QString::fromLatin1(kNewAddress));
    QCOMPARE(model->getDataValue(QStringLiteral("0x04000401")).toString(),
             QString::fromLatin1(kNewAddress));
}

void ProtocolControllerTest::routesRequestsUsingNewAddressAfterAddressWrite()
{
    constexpr const char *kNewAddress = "202101190553";
    MeterModel *model = nullptr;
    const std::unique_ptr<ProtocolController> controller = createController(&model);
    const QVariant before = model->getDataValue(QStringLiteral("0x04000302"));

    controller->processReceivedData(rawWriteAddressRequest(QString::fromLatin1(kNewAddress)));
    QCOMPARE(model->getMeterAddress(), QString::fromLatin1(kNewAddress));
    QCOMPARE(model->getDataValue(QStringLiteral("0x04000401")).toString(), QString::fromLatin1(kNewAddress));

    controller->processReceivedData(rawWriteRequest(model,
                                                    QStringLiteral("02"),
                                                    QStringLiteral("012345"),
                                                    QString::fromLatin1(kOperatorCode)));
    QCOMPARE(model->getDataValue(QStringLiteral("0x04000302")), before);

    controller->processReceivedData(rawWriteRequest(model,
                                                    QStringLiteral("02"),
                                                    QStringLiteral("012345"),
                                                    QString::fromLatin1(kOperatorCode),
                                                    QString::fromLatin1(kNewAddress)));
    QCOMPARE(model->getDataValue(QStringLiteral("0x04000302")).toInt(), 9);
}

QTEST_APPLESS_MAIN(ProtocolControllerTest)

#include "ProtocolControllerTest.moc"
