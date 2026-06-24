#ifndef TERMCATALOGSERVICE_H
#define TERMCATALOGSERVICE_H

#include <QObject>
#include <QList>
#include "Models/TermDefinition.h"

/**
 * @brief 术语目录服务
 *
 * 负责加载、管理和查询电表术语库。
 * 采用单例模式，全局共享一个术语库实例。
 */
class TermCatalogService : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static TermCatalogService* instance();

    /**
     * @brief 搜索匹配的术语
     * @param keyword 搜索关键词
     * @return 匹配的术语定义，未找到返回空对象（term为空）
     */
    TermDefinition searchTerm(const QString &keyword) const;

    /**
     * @brief 获取所有术语
     */
    QList<TermDefinition> allTerms() const;

    /**
     * @brief 检查术语库是否已加载
     */
    bool isLoaded() const;

private:
    explicit TermCatalogService(QObject *parent = nullptr);
    ~TermCatalogService();

    // 禁用拷贝
    TermCatalogService(const TermCatalogService&) = delete;
    TermCatalogService& operator=(const TermCatalogService&) = delete;

    /**
     * @brief 从 JSON 文件加载术语库
     * @return 加载成功返回 true
     */
    bool loadFromJson();

    QList<TermDefinition> m_terms;  // 术语列表
    bool m_loaded;                   // 是否已加载
};

#endif // TERMCATALOGSERVICE_H
