#ifndef CONFIGURATIONSERIALIZER_H
#define CONFIGURATIONSERIALIZER_H

#include <QList>
#include <QString>
#include "Models/ConnectionConfig.h"
#include "Models/MeterPhaseType.h"

struct MeterConfigSnapshot
{
    QString alias;
    QString address;
    QString operatorCode;
    QString password;
    bool includeLeadingFE = false;
    bool eventRecordPresent = true;
    MeterPhaseType phaseType = MeterPhaseType::ThreePhase;
};

struct ConnectionConfigSnapshot
{
    ConnectionConfig config;
    QList<MeterConfigSnapshot> meters;
};

struct ConfigurationSnapshot
{
    QList<ConnectionConfigSnapshot> connections;
};

class ConfigurationSerializer
{
public:
    static bool readFromFile(const QString &fileName,
                             ConfigurationSnapshot *snapshot,
                             QStringList *validationErrors,
                             QString *errorMessage);
    static bool writeToFile(const QString &fileName,
                            const ConfigurationSnapshot &snapshot,
                            QString *errorMessage);
};

#endif // CONFIGURATIONSERIALIZER_H
