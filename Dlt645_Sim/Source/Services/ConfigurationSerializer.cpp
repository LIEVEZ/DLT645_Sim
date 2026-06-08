#include "ConfigurationSerializer.h"
#include "Services/ProtocolParser.h"
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

namespace
{
void appendValidationError(QStringList *validationErrors, const QString &message)
{
    if (validationErrors && validationErrors->size() < 10)
    {
        validationErrors->append(message);
    }
}

QJsonObject meterToJson(const MeterConfigSnapshot &meter)
{
    QJsonObject meterObject;
    meterObject.insert(QStringLiteral("alias"), meter.alias);
    meterObject.insert(QStringLiteral("address"), meter.address);
    meterObject.insert(QStringLiteral("operatorCode"), meter.operatorCode);
    meterObject.insert(QStringLiteral("password"), meter.password);
    meterObject.insert(QStringLiteral("includeLeadingFE"), meter.includeLeadingFE);
    meterObject.insert(QStringLiteral("phaseType"), static_cast<int>(meter.phaseType));
    return meterObject;
}

QJsonObject connectionToJson(const ConnectionConfigSnapshot &connection)
{
    const ConnectionConfig &config = connection.config;

    QJsonObject connectionObject;
    connectionObject.insert(QStringLiteral("type"),
                            config.type == ConnectionType::Serial ? QStringLiteral("serial") : QStringLiteral("tcp"));
    connectionObject.insert(QStringLiteral("alias"), config.alias);
    connectionObject.insert(QStringLiteral("serialPort"), config.serialPort);
    connectionObject.insert(QStringLiteral("baudRate"), config.baudRate);
    connectionObject.insert(QStringLiteral("dataBits"), config.dataBits);
    connectionObject.insert(QStringLiteral("stopBits"), config.stopBits);
    connectionObject.insert(QStringLiteral("parity"), config.parity);
    connectionObject.insert(QStringLiteral("flowControl"), config.flowControl);
    connectionObject.insert(QStringLiteral("tcpAddress"), config.tcpAddress);
    connectionObject.insert(QStringLiteral("tcpPort"), config.tcpPort);

    QJsonArray meterArray;
    for (const MeterConfigSnapshot &meter : connection.meters)
    {
        meterArray.append(meterToJson(meter));
    }

    connectionObject.insert(QStringLiteral("meters"), meterArray);
    return connectionObject;
}
}

bool ConfigurationSerializer::readFromFile(const QString &fileName,
                                           ConfigurationSnapshot *snapshot,
                                           QStringList *validationErrors,
                                           QString *errorMessage)
{
    if (snapshot)
    {
        snapshot->connections.clear();
    }
    if (validationErrors)
    {
        validationErrors->clear();
    }
    if (errorMessage)
    {
        errorMessage->clear();
    }

    if (!snapshot)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Configuration snapshot output is null");
        }
        return false;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Cannot read configuration file: %1").arg(file.errorString());
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Configuration file format error: %1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonArray connectionArray = document.object().value(QStringLiteral("connections")).toArray();
    if (connectionArray.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Configuration file contains no connection configs");
        }
        return false;
    }

    QSet<QString> importedConnectionAliases;
    QSet<QString> importedMeterAddresses;
    ConfigurationSnapshot parsedSnapshot;

    for (int connectionIndex = 0; connectionIndex < connectionArray.size(); ++connectionIndex)
    {
        const QJsonValue connectionValue = connectionArray.at(connectionIndex);
        if (!connectionValue.isObject())
        {
            appendValidationError(validationErrors,
                                  QStringLiteral("Connection %1 is not a valid object").arg(connectionIndex + 1));
            continue;
        }

        const QJsonObject connectionObject = connectionValue.toObject();
        ConnectionConfig config;
        config.type = connectionObject.value(QStringLiteral("type")).toString() == QStringLiteral("tcp")
                          ? ConnectionType::TCP
                          : ConnectionType::Serial;
        config.alias = connectionObject.value(QStringLiteral("alias")).toString().trimmed();
        config.serialPort = connectionObject.value(QStringLiteral("serialPort")).toString();
        config.baudRate = connectionObject.value(QStringLiteral("baudRate")).toInt(config.baudRate);
        config.dataBits = connectionObject.value(QStringLiteral("dataBits")).toInt(config.dataBits);
        config.stopBits = connectionObject.value(QStringLiteral("stopBits")).toInt(config.stopBits);
        config.parity = connectionObject.value(QStringLiteral("parity")).toInt(config.parity);
        config.flowControl = connectionObject.value(QStringLiteral("flowControl")).toInt(config.flowControl);
        config.tcpAddress = connectionObject.value(QStringLiteral("tcpAddress")).toString();
        config.tcpPort = connectionObject.value(QStringLiteral("tcpPort")).toInt(config.tcpPort);

        if (config.alias.isEmpty())
        {
            appendValidationError(validationErrors,
                                  QStringLiteral("Connection %1 alias is empty").arg(connectionIndex + 1));
            continue;
        }

        if (importedConnectionAliases.contains(config.alias))
        {
            appendValidationError(validationErrors,
                                  QStringLiteral("Duplicate connection alias: %1").arg(config.alias));
            continue;
        }
        importedConnectionAliases.insert(config.alias);

        ConnectionConfigSnapshot connectionSnapshot;
        connectionSnapshot.config = config;

        const QJsonArray meterArray = connectionObject.value(QStringLiteral("meters")).toArray();
        for (int meterIndex = 0; meterIndex < meterArray.size(); ++meterIndex)
        {
            const QJsonValue meterValue = meterArray.at(meterIndex);
            if (!meterValue.isObject())
            {
                appendValidationError(validationErrors,
                                      QStringLiteral("Connection %1 meter %2 is not a valid object")
                                          .arg(config.alias)
                                          .arg(meterIndex + 1));
                continue;
            }

            const QJsonObject meterObject = meterValue.toObject();
            MeterConfigSnapshot meter;
            meter.alias = meterObject.value(QStringLiteral("alias")).toString().trimmed();
            meter.address = meterObject.value(QStringLiteral("address")).toString().trimmed();
            meter.operatorCode = meterObject.value(QStringLiteral("operatorCode")).toString();
            meter.password = meterObject.value(QStringLiteral("password")).toString();
            meter.includeLeadingFE = meterObject.value(QStringLiteral("includeLeadingFE")).toBool();
            meter.phaseType = static_cast<MeterPhaseType>(
                meterObject.value(QStringLiteral("phaseType")).toInt(static_cast<int>(MeterPhaseType::ThreePhase)));

            if (meter.alias.isEmpty())
            {
                appendValidationError(validationErrors,
                                      QStringLiteral("Connection %1 meter %2 alias is empty")
                                          .arg(config.alias)
                                          .arg(meterIndex + 1));
                continue;
            }

            if (!ProtocolParser::isValidAddress(meter.address))
            {
                appendValidationError(validationErrors,
                                      QStringLiteral("Meter %1 address is invalid: %2").arg(meter.alias, meter.address));
                continue;
            }

            if (importedMeterAddresses.contains(meter.address))
            {
                appendValidationError(validationErrors,
                                      QStringLiteral("Duplicate meter address: %1").arg(meter.address));
                continue;
            }

            importedMeterAddresses.insert(meter.address);
            connectionSnapshot.meters.append(meter);
        }

        parsedSnapshot.connections.append(connectionSnapshot);
    }

    if (validationErrors && !validationErrors->isEmpty())
    {
        return false;
    }

    if (parsedSnapshot.connections.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Configuration file contains no importable connection configs");
        }
        return false;
    }

    *snapshot = parsedSnapshot;
    return true;
}

bool ConfigurationSerializer::writeToFile(const QString &fileName,
                                          const ConfigurationSnapshot &snapshot,
                                          QString *errorMessage)
{
    if (errorMessage)
    {
        errorMessage->clear();
    }

    QJsonArray connectionArray;
    for (const ConnectionConfigSnapshot &connection : snapshot.connections)
    {
        connectionArray.append(connectionToJson(connection));
    }

    QJsonObject rootObject;
    rootObject.insert(QStringLiteral("version"), 1);
    rootObject.insert(QStringLiteral("exportedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
    rootObject.insert(QStringLiteral("connections"), connectionArray);

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Cannot write configuration file: %1").arg(file.errorString());
        }
        return false;
    }

    const QByteArray configData = QJsonDocument(rootObject).toJson(QJsonDocument::Indented);
    if (file.write(configData) != configData.size())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Configuration file write was incomplete: %1").arg(file.errorString());
        }
        return false;
    }

    return true;
}
