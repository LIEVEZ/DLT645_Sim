#include "TermCatalogService.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

TermCatalogService* TermCatalogService::instance()
{
    static TermCatalogService instance;
    return &instance;
}

TermCatalogService::TermCatalogService(QObject *parent)
    : QObject(parent), m_loaded(false)
{
    // 懒加载，首次访问时才加载 JSON
}

TermCatalogService::~TermCatalogService()
{
}

bool TermCatalogService::loadFromJson()
{
    if (m_loaded) {
        return true;  // 已加载，避免重复加载
    }

    QByteArray data;

    // 尝试从资源文件加载
    QFile file(":/resources/terms_glossary.json");
    if (file.open(QIODevice::ReadOnly)) {
        data = file.readAll();
        file.close();
    } else {
        qWarning() << "Failed to open terms_glossary.json from resources";

        // 备用方案：从外部文件加载
        QFile externalFile("terms_glossary.json");
        if (!externalFile.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open terms_glossary.json from external path";
            m_loaded = true;  // 标记为已加载，避免每次都尝试
            return false;
        }
        data = externalFile.readAll();
        externalFile.close();
    }

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse terms_glossary.json:" << parseError.errorString();
        m_loaded = true;
        return false;
    }

    if (!document.isObject()) {
        qWarning() << "terms_glossary.json root is not an object";
        m_loaded = true;
        return false;
    }

    QJsonObject root = document.object();

    // 检查版本
    int version = root["version"].toInt();
    if (version < 1) {
        qWarning() << "terms_glossary.json version is invalid";
    }

    // 解析术语列表
    QJsonArray termsArray = root["terms"].toArray();
    for (const QJsonValue &termValue : termsArray) {
        if (!termValue.isObject()) {
            continue;
        }

        QJsonObject termObject = termValue.toObject();
        TermDefinition term;

        term.term = termObject["term"].toString();
        term.englishTerm = termObject["englishTerm"].toString();
        term.definition = termObject["definition"].toString();
        term.formula = termObject["formula"].toString();
        term.unit = termObject["unit"].toString();
        term.range = termObject["range"].toString();
        term.note = termObject["note"].toString();

        // 解析关键词列表
        QJsonArray keywordsArray = termObject["keywords"].toArray();
        for (const QJsonValue &kw : keywordsArray) {
            term.keywords.append(kw.toString());
        }

        // 验证术语名不能为空
        if (term.term.isEmpty()) {
            qWarning() << "Skipping term with empty name";
            continue;
        }

        m_terms.append(term);
    }

    m_loaded = true;
    qDebug() << "Loaded" << m_terms.size() << "terms from glossary";
    return true;
}

TermDefinition TermCatalogService::searchTerm(const QString &keyword) const
{
    // 确保已加载
    const_cast<TermCatalogService*>(this)->loadFromJson();

    if (keyword.isEmpty()) {
        return TermDefinition();
    }

    // 遍历术语列表，查找匹配项
    for (const TermDefinition &term : m_terms) {
        if (term.matchesKeyword(keyword)) {
            return term;
        }
    }

    // 未找到匹配项
    return TermDefinition();
}

QList<TermDefinition> TermCatalogService::allTerms() const
{
    // 确保已加载
    const_cast<TermCatalogService*>(this)->loadFromJson();
    return m_terms;
}

bool TermCatalogService::isLoaded() const
{
    return m_loaded;
}
