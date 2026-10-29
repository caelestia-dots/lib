#pragma once

#include <qobject.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qstring.h>
#include <qurl.h>

#include "caelestia/utils/cache.hpp"

namespace caelestia::utils {

class CacheWorker : public QObject {
public:
    explicit CacheWorker(QString file, qint64 maxSize, int version, QObject* parent = nullptr);
    ~CacheWorker() override;

    [[nodiscard]] const QString& file() const;
    [[nodiscard]] qint64 maxSize() const;
    [[nodiscard]] int version() const;

    void open();
    void touch(const QString& key);
    bool store(const QString& key, const QByteArray& data);
    bool store(const QString& key, const Cache::Producer& producer);
    void remove(const QString& key);
    void clear();

protected:
    [[nodiscard]] QSqlQuery query() const;
    void index(const QString& key, qint64 bytes);
    void forget(const QString& key);

    virtual void prepare() {}

    virtual void reconcile() {}

    [[nodiscard]] virtual bool contains(const QString& key) const = 0;
    virtual bool write(const QString& key, const QByteArray& data) = 0;
    virtual void erase(const QString& key) = 0;

private:
    const QString m_file;
    const qint64 m_maxSize;
    const int m_version;
    const QString m_connection;
    QSqlDatabase m_db;

    void evict();
};

class FileCacheWorker : public CacheWorker {
public:
    explicit FileCacheWorker(QString dir, qint64 maxSize, QString suffix, int version, QObject* parent = nullptr);

    [[nodiscard]] const QString& dir() const;
    [[nodiscard]] const QString& suffix() const;
    [[nodiscard]] QString pathFor(const QString& key) const;
    [[nodiscard]] QUrl read(const QString& key);

protected:
    void reconcile() override;
    [[nodiscard]] bool contains(const QString& key) const override;
    bool write(const QString& key, const QByteArray& data) override;
    void erase(const QString& key) override;

private:
    const QString m_dir;
    const QString m_suffix;
};

class BlobCacheWorker : public CacheWorker {
public:
    using CacheWorker::CacheWorker;

    [[nodiscard]] QByteArray read(const QString& key);

protected:
    void prepare() override;
    void reconcile() override;
    [[nodiscard]] bool contains(const QString& key) const override;
    bool write(const QString& key, const QByteArray& data) override;
    void erase(const QString& key) override;
};

} // namespace caelestia::utils
