#ifndef METERDATASTORE_H
#define METERDATASTORE_H

#include <QObject>
#include <QSqlDatabase>
#include <QVariant>

class MeterModel;

class MeterDataStore : public QObject
{
    Q_OBJECT

public:
    explicit MeterDataStore(QObject *parent = nullptr);
    ~MeterDataStore();

    bool isReady() const;
    bool hasMeterData(const QString &meterAddress);
    bool loadMeterData(MeterModel *meterModel);
    bool saveMeterData(const MeterModel *meterModel);

private:
    bool initialize();
    bool createTables();
    bool tableExists(const QString &tableName) const;
    bool ensureMeterTable(const QString &meterAddress);
    bool loadSnapshotFromTable(const QString &tableName, QMap<QString, QVariant> *snapshot) const;
    QString tableNameForAddress(const QString &meterAddress) const;
    QString quotedIdentifier(const QString &identifier) const;

    QString serializeValue(const QVariant &value, int valueType) const;
    QVariant deserializeValue(const QString &value, int valueType) const;

    QSqlDatabase m_database;
    QString m_connectionName;
};

#endif // METERDATASTORE_H
