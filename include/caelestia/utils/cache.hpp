#pragma once

#include <qbytearray.h>
#include <qfuture.h>
#include <qstring.h>
#include <qthread.h>
#include <qurl.h>

#include <functional>

namespace caelestia::utils {

class CacheWorker;
class FileCacheWorker;
class BlobCacheWorker;

class Cache {
public:
    Q_DISABLE_COPY_MOVE(Cache)

    using Producer = std::function<QByteArray()>;

    virtual ~Cache();

    [[nodiscard]] static QString cacheDir();
    [[nodiscard]] QString file() const;
    [[nodiscard]] qint64 maxSize() const;
    [[nodiscard]] int version() const;

    [[nodiscard]] static QString keyFor(QStringView key);
    [[nodiscard]] static QString keyForPath(const QString& path, QStringView extraInfo = {});

    QFuture<bool> store(const QString& key, const QByteArray& data);
    QFuture<bool> store(const QString& key, Producer producer);
    void remove(const QString& key);
    void clear();

protected:
    explicit Cache(CacheWorker* worker);

    [[nodiscard]] CacheWorker* worker() const;

private:
    QThread m_thread;
    CacheWorker* m_worker;
};

class FileCache : public Cache {
public:
    explicit FileCache(const QString& dir, qint64 maxSize, QString suffix = {}, int version = 1);

    [[nodiscard]] QString dir() const;
    [[nodiscard]] QString suffix() const;
    [[nodiscard]] QString pathFor(const QString& key) const;

    [[nodiscard]] QFuture<QUrl> lookup(const QString& key) const;
    [[nodiscard]] QUrl lookupSync(const QString& key) const;

private:
    [[nodiscard]] FileCacheWorker* fileWorker() const;
};

class BlobCache : public Cache {
public:
    explicit BlobCache(const QString& name, qint64 maxSize, int version = 1);

    [[nodiscard]] QFuture<QByteArray> lookup(const QString& key) const;

private:
    [[nodiscard]] BlobCacheWorker* blobWorker() const;
};

} // namespace caelestia::utils
