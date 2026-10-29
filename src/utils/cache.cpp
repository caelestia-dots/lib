#include "caelestia/utils/cache.hpp"

#include <qcryptographichash.h>
#include <qdatetime.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qpromise.h>
#include <qstandardpaths.h>

#include <type_traits>
#include <utility>

#include "cacheworker.hpp"

namespace caelestia::utils {

using Qt::StringLiterals::operator""_s;

namespace {

template <typename Read> auto runOn(QObject* worker, Read read) {
    QPromise<std::invoke_result_t<Read>> promise;
    promise.start();
    auto future = promise.future();
    QMetaObject::invokeMethod(
        worker,
        [promise = std::move(promise), read = std::move(read)]() mutable {
            promise.addResult(read());
            promise.finish();
        },
        Qt::QueuedConnection);
    return future;
}

} // namespace

Cache::Cache(CacheWorker* worker)
    : m_worker(worker) {
    m_worker->moveToThread(&m_thread);
    QObject::connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.start(QThread::LowPriority);
    QMetaObject::invokeMethod(m_worker, &CacheWorker::open, Qt::QueuedConnection);
}

Cache::~Cache() {
    m_thread.quit();
    m_thread.wait();
}

QString Cache::cacheDir() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + u"/caelestia"_s;
}

QString Cache::file() const {
    return m_worker->file();
}

qint64 Cache::maxSize() const {
    return m_worker->maxSize();
}

int Cache::version() const {
    return m_worker->version();
}

QString Cache::keyFor(QStringView key) {
    return QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString Cache::keyForPath(const QString& path, QStringView extraInfo) {
    const QFileInfo info(path);
    if (!info.exists())
        return {};

    return keyFor(u"%1|%2|%3|%4"_s.arg(info.absoluteFilePath(), QString::number(info.lastModified().toSecsSinceEpoch()),
        QString::number(info.size()), extraInfo));
}

QFuture<bool> Cache::store(const QString& key, const QByteArray& data) {
    if (key.isEmpty())
        return QtFuture::makeReadyValueFuture(false);

    return runOn(m_worker, [worker = m_worker, key, data] {
        return worker->store(key, data);
    });
}

QFuture<bool> Cache::store(const QString& key, Producer producer) {
    if (key.isEmpty())
        return QtFuture::makeReadyValueFuture(false);

    return runOn(m_worker, [worker = m_worker, key, producer = std::move(producer)] {
        return worker->store(key, producer);
    });
}

void Cache::remove(const QString& key) {
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, key] {
            worker->remove(key);
        },
        Qt::QueuedConnection);
}

void Cache::clear() {
    QMetaObject::invokeMethod(m_worker, &CacheWorker::clear, Qt::QueuedConnection);
}

CacheWorker* Cache::worker() const {
    return m_worker;
}

FileCache::FileCache(const QString& dir, qint64 maxSize, QString suffix, int version)
    : Cache(new FileCacheWorker(cacheDir() + u'/' + dir, maxSize, std::move(suffix), version)) {}

QString FileCache::dir() const {
    return fileWorker()->dir();
}

QString FileCache::suffix() const {
    return fileWorker()->suffix();
}

QString FileCache::pathFor(const QString& key) const {
    return fileWorker()->pathFor(key);
}

QFuture<QUrl> FileCache::lookup(const QString& key) const {
    return runOn(worker(), [worker = fileWorker(), key] {
        return worker->read(key);
    });
}

QUrl FileCache::lookupSync(const QString& key) const {
    const auto path = pathFor(key);
    if (key.isEmpty() || !QFile::exists(path))
        return {};

    QMetaObject::invokeMethod(
        worker(),
        [worker = worker(), key] {
            worker->touch(key);
        },
        Qt::QueuedConnection);
    return QUrl::fromLocalFile(path);
}

FileCacheWorker* FileCache::fileWorker() const {
    return static_cast<FileCacheWorker*>(worker());
}

BlobCache::BlobCache(const QString& name, qint64 maxSize, int version)
    : Cache(new BlobCacheWorker(cacheDir() + u'/' + name + u".db"_s, maxSize, version)) {}

QFuture<QByteArray> BlobCache::lookup(const QString& key) const {
    return runOn(worker(), [worker = blobWorker(), key] {
        return worker->read(key);
    });
}

BlobCacheWorker* BlobCache::blobWorker() const {
    return static_cast<BlobCacheWorker*>(worker());
}

} // namespace caelestia::utils
