#include "MeterDataStore.h"
#include "Models/MeterModel.h"
#include "Utils/Logger.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSqlError>
#include <QSqlQuery>

namespace
{
    bool isPersistedDataItem(const DataItem &item)
    {
        return !item.diCode.endsWith(QStringLiteral("FF00"));
    }
}

MeterDataStore::MeterDataStore(QObject *parent)
    : QObject(parent), m_connectionName(QStringLiteral("MeterDataStoreConnection"))
{
    initialize();
}

MeterDataStore::~MeterDataStore()
{
    if (m_database.isValid())
    {
        m_database.close();
    }

    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool MeterDataStore::isReady() const
{
    return m_database.isValid() && m_database.isOpen();
}

bool MeterDataStore::hasMeterData(const QString &meterAddress)
{
    if (!isReady() || meterAddress.isEmpty())
    {
        return false;
    }

    const QString tableName = tableNameForAddress(meterAddress);
    if (tableExists(tableName))
    {
        QSqlQuery query(m_database);
        if (!query.exec(QStringLiteral("SELECT 1 FROM %1 LIMIT 1").arg(quotedIdentifier(tableName))))
        {
            LOG_ERROR(QStringLiteral("SQLite check meter table failed: %1").arg(query.lastError().text()));
            return false;
        }

        return query.next();
    }
    
    return false;
}

bool MeterDataStore::loadMeterData(MeterModel *meterModel)
{
    if (!isReady() || !meterModel)
    {
        return false;
    }

    const QString meterAddress = meterModel->getMeterAddress();
    if (meterAddress.isEmpty())
    {
        return false;
    }

    const QString tableName = tableNameForAddress(meterAddress);
    QMap<QString, QVariant> snapshot;
    if (!tableExists(tableName) || !loadSnapshotFromTable(tableName, &snapshot) || snapshot.isEmpty())
    {
        return false;
    }

    meterModel->applyPersistedData(snapshot);
    return true;
}

bool MeterDataStore::saveMeterData(const MeterModel *meterModel)
{
    if (!isReady() || !meterModel)
    {
        return false;
    }

    const QString meterAddress = meterModel->getMeterAddress();
    if (meterAddress.isEmpty())
    {
        return false;
    }

    if (!ensureMeterTable(meterAddress))
    {
        return false;
    }

    const QString tableName = tableNameForAddress(meterAddress);
    const QString quotedTableName = quotedIdentifier(tableName);

    if (!m_database.transaction())
    {
        LOG_ERROR(QStringLiteral("SQLite begin transaction failed: %1").arg(m_database.lastError().text()));
        return false;
    }

    QSqlQuery deleteQuery(m_database);
    deleteQuery.prepare(QStringLiteral("DELETE FROM %1").arg(quotedTableName));
    if (!deleteQuery.exec())
    {
        LOG_ERROR(QStringLiteral("SQLite delete meter snapshot failed: %1").arg(deleteQuery.lastError().text()));
        m_database.rollback();
        return false;
    }

    QSqlQuery insertQuery(m_database);
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO %1 (di_code, value_text, value_type, updated_at) "
        "VALUES (?, ?, ?, ?)").arg(quotedTableName));

    const QString updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QList<DataItem> items = meterModel->getAllDataItems();
    for (const DataItem &item : items)
    {
        if (!isPersistedDataItem(item))
        {
            continue;
        }

        const int valueType = static_cast<int>(item.value.type());
        insertQuery.bindValue(0, item.diCode);
        insertQuery.bindValue(1, serializeValue(item.value, valueType));
        insertQuery.bindValue(2, valueType);
        insertQuery.bindValue(3, updatedAt);
        if (!insertQuery.exec())
        {
            LOG_ERROR(QStringLiteral("SQLite insert meter item failed: %1").arg(insertQuery.lastError().text()));
            m_database.rollback();
            return false;
        }
        insertQuery.finish();
    }

    if (!m_database.commit())
    {
        LOG_ERROR(QStringLiteral("SQLite commit failed: %1").arg(m_database.lastError().text()));
        m_database.rollback();
        return false;
    }

    return true;
}

bool MeterDataStore::initialize()
{
    if (QSqlDatabase::contains(m_connectionName))
    {
        m_database = QSqlDatabase::database(m_connectionName);
    }
    else
    {
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    }

    const QString databasePath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("meter_data.db"));
    m_database.setDatabaseName(databasePath);
    if (!m_database.open())
    {
        LOG_ERROR(QStringLiteral("SQLite open failed: %1").arg(m_database.lastError().text()));
        return false;
    }

    QSqlQuery pragmaQuery(m_database);
    pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

    if (!createTables())
    {
        return false;
    }

    LOG_INFO(QStringLiteral("SQLite meter data store ready: %1").arg(databasePath));
    return true;
}

bool MeterDataStore::createTables()
{
    if (!isReady())
    {
        return false;
    }

    QSqlQuery query(m_database);
    const QString sql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS meter_store_meta ("
        "meta_key TEXT PRIMARY KEY,"
        "meta_value TEXT NOT NULL"
        ")");

    if (!query.exec(sql))
    {
        LOG_ERROR(QStringLiteral("SQLite create table failed: %1").arg(query.lastError().text()));
        return false;
    }

    if (tableExists(QStringLiteral("meter_data")))
    {
        QSqlQuery dropQuery(m_database);
        if (!dropQuery.exec(QStringLiteral("DROP TABLE meter_data")))
        {
            LOG_ERROR(QStringLiteral("SQLite drop legacy table failed: %1").arg(dropQuery.lastError().text()));
            return false;
        }
    }

    QSqlQuery initQuery(m_database);
    if (!initQuery.exec(QStringLiteral(
            "INSERT OR REPLACE INTO meter_store_meta (meta_key, meta_value) "
            "VALUES ('schema_mode', 'per_meter_table')")))
    {
        LOG_ERROR(QStringLiteral("SQLite init meta failed: %1").arg(initQuery.lastError().text()));
        return false;
    }

    return true;
}

bool MeterDataStore::tableExists(const QString &tableName) const
{
    if (!isReady() || tableName.isEmpty())
    {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT 1 FROM sqlite_master "
        "WHERE type = 'table' AND name = ? LIMIT 1"));
    query.addBindValue(tableName);
    if (!query.exec())
    {
        LOG_ERROR(QStringLiteral("SQLite check table exists failed: %1").arg(query.lastError().text()));
        return false;
    }

    return query.next();
}

bool MeterDataStore::ensureMeterTable(const QString &meterAddress)
{
    if (!isReady() || meterAddress.isEmpty())
    {
        return false;
    }

    const QString tableName = tableNameForAddress(meterAddress);
    if (tableExists(tableName))
    {
        return true;
    }

    QSqlQuery query(m_database);
    const QString sql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "di_code TEXT PRIMARY KEY,"
        "value_text TEXT,"
        "value_type INTEGER NOT NULL,"
        "updated_at TEXT"
        ")").arg(quotedIdentifier(tableName));

    if (!query.exec(sql))
    {
        LOG_ERROR(QStringLiteral("SQLite create meter table failed: %1").arg(query.lastError().text()));
        return false;
    }

    return true;
}

bool MeterDataStore::loadSnapshotFromTable(const QString &tableName, QMap<QString, QVariant> *snapshot) const
{
    if (!isReady() || tableName.isEmpty() || !snapshot)
    {
        return false;
    }

    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
            "SELECT di_code, value_text, value_type "
            "FROM %1").arg(quotedIdentifier(tableName))))
    {
        LOG_ERROR(QStringLiteral("SQLite load meter table failed: %1").arg(query.lastError().text()));
        return false;
    }

    snapshot->clear();
    while (query.next())
    {
        const QString diCode = query.value(0).toString();
        const QString valueText = query.value(1).toString();
        const int valueType = query.value(2).toInt();
        snapshot->insert(diCode, deserializeValue(valueText, valueType));
    }

    return !snapshot->isEmpty();
}

QString MeterDataStore::tableNameForAddress(const QString &meterAddress) const
{
    QString tableName = QStringLiteral("meter_");
    tableName.reserve(meterAddress.size() + 6);

    for (const QChar &ch : meterAddress)
    {
        tableName.append(ch.isLetterOrNumber() ? ch : QChar('_'));
    }

    if (tableName == QStringLiteral("meter_"))
    {
        tableName.append(QStringLiteral("default"));
    }

    return tableName;
}

QString MeterDataStore::quotedIdentifier(const QString &identifier) const
{
    QString escaped = identifier;
    escaped.replace(QChar('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QString MeterDataStore::serializeValue(const QVariant &value, int valueType) const
{
    switch (static_cast<QVariant::Type>(valueType))
    {
    case QVariant::List:
    {
        QJsonArray values;
        const QVariantList valueList = value.toList();
        for (const QVariant &entry : valueList)
        {
            const int entryType = static_cast<int>(entry.type());
            QJsonObject entryObject;
            entryObject.insert(QStringLiteral("type"), entryType);
            entryObject.insert(QStringLiteral("value"), serializeValue(entry, entryType));
            values.append(entryObject);
        }
        return QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Compact));
    }
    case QVariant::Bool:
        return value.toBool() ? QStringLiteral("1") : QStringLiteral("0");
    case QVariant::ByteArray:
        return QString::fromLatin1(value.toByteArray().toBase64());
    default:
        return value.toString();
    }
}

QVariant MeterDataStore::deserializeValue(const QString &value, int valueType) const
{
    switch (static_cast<QVariant::Type>(valueType))
    {
    case QVariant::List:
    {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(value.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isArray())
        {
            return value.split(QStringLiteral(":"), Qt::KeepEmptyParts);
        }

        QVariantList values;
        const QJsonArray valueArray = document.array();
        values.reserve(valueArray.size());
        for (const QJsonValue &entryValue : valueArray)
        {
            const QJsonObject entryObject = entryValue.toObject();
            const int entryType = entryObject.value(QStringLiteral("type")).toInt(static_cast<int>(QVariant::String));
            const QString entryText = entryObject.value(QStringLiteral("value")).toString();
            values.append(deserializeValue(entryText, entryType));
        }
        return values;
    }
    case QVariant::Int:
        return value.toInt();
    case QVariant::UInt:
        return value.toUInt();
    case QVariant::LongLong:
        return value.toLongLong();
    case QVariant::ULongLong:
        return value.toULongLong();
    case QVariant::Double:
        return value.toDouble();
    case QVariant::Bool:
        return value == QStringLiteral("1") || value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
    case QVariant::ByteArray:
        return QByteArray::fromBase64(value.toLatin1());
    default:
        return value;
    }
}
