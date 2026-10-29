#include "cacheworker.hpp"

#include <qdatetime.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qloggingcategory.h>
#include <qsavefile.h>
#include <qset.h>
#include <qsqlerror.h>
#include <quuid.h>

#include <utility>

namespace {

Q_LOGGING_CATEGORY(lcCache, "caelestia.utils.cache", QtInfoMsg)

} // namespace

namespace caelestia::utils {

using Qt::StringLiterals::operator""_s;

namespace {

qint64 now() {
    return QDateTime::currentMSecsSinceEpoch();
}

} // namespace

CacheWorker::CacheWorker(QString file, qint64 maxSize, int version, QObject* parent)
    : QObject(parent)
    , m_file(std::move(file))
    , m_maxSize(maxSize)
    , m_version(version)
    , m_connection(QUuid::createUuid().toString()) {
    Q_ASSERT(m_version != 0);
}

CacheWorker::~CacheWorker() {
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connection);
}

const QString& CacheWorker::file() const {
    return m_file;
}

qint64 CacheWorker::maxSize() const {
    return m_maxSize;
}

int CacheWorker::version() const {
    return m_version;
}

void CacheWorker::open() {
    const auto dir = QFileInfo(m_file).path();
    if (!QDir().mkpath(dir)) {
        qCWarning(lcCache).noquote() << "Failed to create cache dir" << dir;
        return;
    }

    m_db = QSqlDatabase::addDatabase(u"QSQLITE"_s, m_connection);
    m_db.setDatabaseName(m_file);
    if (!m_db.open()) {
        qCWarning(lcCache).noquote() << "Failed to open cache index:" << m_db.lastError().text();
        return;
    }

    auto q = query();
    q.exec(u"CREATE TABLE IF NOT EXISTS entries (key TEXT PRIMARY KEY, bytes INTEGER NOT NULL, "
           "last_used INTEGER NOT NULL)"_s);
    prepare();

    q.exec(u"PRAGMA user_version"_s);
    const int stored = q.next() ? q.value(0).toInt() : 0;
    if (stored == 0) {
        q.exec(u"PRAGMA user_version = %1"_s.arg(m_version));
    } else if (stored != m_version) {
        qCInfo(lcCache).noquote() << "Cache version changed, clearing" << m_file;
        reconcile();
        clear();
        q.exec(u"PRAGMA user_version = %1"_s.arg(m_version));
        return;
    }

    reconcile();
    evict();
}

void CacheWorker::touch(const QString& key) {
    auto q = query();
    q.prepare(u"UPDATE entries SET last_used = :now WHERE key = :key"_s);
    q.bindValue(u":now"_s, now());
    q.bindValue(u":key"_s, key);
    q.exec();
}

bool CacheWorker::store(const QString& key, const Cache::Producer& producer) {
    if (contains(key))
        return true;

    const auto data = producer();
    if (data.isNull()) {
        qCWarning(lcCache).noquote() << "Producer returned no data for" << key;
        return false;
    }
    return store(key, data);
}

bool CacheWorker::store(const QString& key, const QByteArray& data) {
    if (data.size() > m_maxSize)
        return false;

    if (!write(key, data))
        return false;

    index(key, data.size());
    qCDebug(lcCache).noquote() << "Stored" << key << data.size() << "bytes";
    evict();
    return true;
}

void CacheWorker::remove(const QString& key) {
    erase(key);
    forget(key);
}

void CacheWorker::clear() {
    auto q = query();
    q.exec(u"SELECT key FROM entries"_s);
    while (q.next())
        erase(q.value(0).toString());
    q.exec(u"DELETE FROM entries"_s);
}

QSqlQuery CacheWorker::query() const {
    return QSqlQuery(m_db);
}

void CacheWorker::index(const QString& key, qint64 bytes) {
    auto q = query();
    q.prepare(u"INSERT OR REPLACE INTO entries (key, bytes, last_used) VALUES (:key, :bytes, :now)"_s);
    q.bindValue(u":key"_s, key);
    q.bindValue(u":bytes"_s, bytes);
    q.bindValue(u":now"_s, now());
    q.exec();
}

void CacheWorker::forget(const QString& key) {
    auto q = query();
    q.prepare(u"DELETE FROM entries WHERE key = :key"_s);
    q.bindValue(u":key"_s, key);
    q.exec();
}

void CacheWorker::evict() {
    auto q = query();
    q.exec(u"SELECT COALESCE(SUM(bytes), 0) FROM entries"_s);
    qint64 total = q.next() ? q.value(0).toLongLong() : 0;
    if (total <= m_maxSize)
        return;

    QStringList stale;
    q.exec(u"SELECT key, bytes FROM entries ORDER BY last_used ASC"_s);
    while (total > m_maxSize && q.next()) {
        stale << q.value(0).toString();
        total -= q.value(1).toLongLong();
    }
    q.finish();

    for (const auto& key : std::as_const(stale)) {
        remove(key);
        qCDebug(lcCache).noquote() << "Evicted" << key;
    }
}

FileCacheWorker::FileCacheWorker(QString dir, qint64 maxSize, QString suffix, int version, QObject* parent)
    : CacheWorker(dir + u"/index.db"_s, maxSize, version, parent)
    , m_dir(std::move(dir))
    , m_suffix(std::move(suffix)) {}

const QString& FileCacheWorker::dir() const {
    return m_dir;
}

const QString& FileCacheWorker::suffix() const {
    return m_suffix;
}

QString FileCacheWorker::pathFor(const QString& key) const {
    return m_dir + u'/' + key + m_suffix;
}

QUrl FileCacheWorker::read(const QString& key) {
    const auto path = pathFor(key);
    if (key.isEmpty() || !QFile::exists(path))
        return {};

    touch(key);
    return QUrl::fromLocalFile(path);
}

void FileCacheWorker::reconcile() {
    QSet<QString> indexed;
    auto q = query();
    q.exec(u"SELECT key FROM entries"_s);
    while (q.next())
        indexed.insert(q.value(0).toString());

    QSet<QString> onDisk;
    const auto files = QDir(m_dir).entryInfoList({ u"*"_s + m_suffix }, QDir::Files);
    for (const auto& info : files) {
        if (info.filePath() == file())
            continue;

        const auto key = info.completeBaseName();
        onDisk.insert(key);
        if (!indexed.contains(key))
            index(key, info.size());
    }

    for (const QString& key : indexed)
        if (!onDisk.contains(key))
            forget(key);
}

bool FileCacheWorker::contains(const QString& key) const {
    return QFile::exists(pathFor(key));
}

bool FileCacheWorker::write(const QString& key, const QByteArray& data) {
    QSaveFile file(pathFor(key));
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        qCWarning(lcCache).noquote() << "Failed to write" << file.fileName() << file.errorString();
        return false;
    }
    return true;
}

void FileCacheWorker::erase(const QString& key) {
    QFile::remove(pathFor(key));
}

QByteArray BlobCacheWorker::read(const QString& key) {
    auto q = query();
    q.prepare(u"SELECT data FROM blobs WHERE key = :key"_s);
    q.bindValue(u":key"_s, key);
    if (!q.exec() || !q.next())
        return {};

    touch(key);
    return q.value(0).toByteArray();
}

void BlobCacheWorker::prepare() {
    query().exec(u"CREATE TABLE IF NOT EXISTS blobs (key TEXT PRIMARY KEY, data BLOB NOT NULL)"_s);
}

void BlobCacheWorker::reconcile() {
    auto q = query();
    q.exec(u"DELETE FROM entries WHERE key NOT IN (SELECT key FROM blobs)"_s);
    q.exec(u"DELETE FROM blobs WHERE key NOT IN (SELECT key FROM entries)"_s);
}

bool BlobCacheWorker::contains(const QString& key) const {
    auto q = query();
    q.prepare(u"SELECT 1 FROM blobs WHERE key = :key"_s);
    q.bindValue(u":key"_s, key);
    return q.exec() && q.next();
}

bool BlobCacheWorker::write(const QString& key, const QByteArray& data) {
    auto q = query();
    q.prepare(u"INSERT OR REPLACE INTO blobs (key, data) VALUES (:key, :data)"_s);
    q.bindValue(u":key"_s, key);
    q.bindValue(u":data"_s, data);
    if (q.exec())
        return true;

    qCWarning(lcCache).noquote() << "Failed to write" << key << q.lastError().text();
    return false;
}

void BlobCacheWorker::erase(const QString& key) {
    auto q = query();
    q.prepare(u"DELETE FROM blobs WHERE key = :key"_s);
    q.bindValue(u":key"_s, key);
    q.exec();
}

} // namespace caelestia::utils
