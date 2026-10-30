#include "package.h"

#include "unpack.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>

namespace {

QString jsonString(const QJsonObject& obj, const char* key)
{
    return obj.value(QLatin1String(key)).toString();
}

qint64 jsonInt(const QJsonValue& value)
{
    if (value.isDouble())
        return qint64(value.toDouble());
    return value.toString().toLongLong();
}

CipFile fileFromJson(const QJsonObject& obj)
{
    CipFile file;
    file.url = jsonString(obj, "url");
    file.localfile = jsonString(obj, "localfile");
    file.packedhash = jsonString(obj, "packedhash");
    file.packedsize = jsonInt(obj.value(QLatin1String("packedsize")));
    file.unpackedhash = jsonString(obj, "unpackedhash");
    file.unpackedsize = jsonInt(obj.value(QLatin1String("unpackedsize")));
    return file;
}

CipBuild buildFromJson(const QJsonObject& obj)
{
    CipBuild build;
    build.os = jsonString(obj, "os");
    build.architecture = jsonString(obj, "architecture");
    build.packageUrl = QUrl(jsonString(obj, "url"));
    build.versionUrl = QUrl(jsonString(obj, "versionurl"));
    build.assetsUrl = QUrl(jsonString(obj, "assetsurl"));
    build.assetsVersionUrl = QUrl(jsonString(obj, "assetsversionurl"));
    return build;
}

QVector<CipFile> filesFromArray(const QJsonArray& array)
{
    QVector<CipFile> files;
    files.reserve(array.size());
    for (const QJsonValue& value : array) {
        if (value.isObject())
            files.push_back(fileFromJson(value.toObject()));
    }
    return files;
}

} // namespace

PackageClient::PackageClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void PackageClient::setCdnBase(const QString& base)
{
    m_cdn = base;
    while (m_cdn.endsWith(QLatin1Char('/')))
        m_cdn.chop(1);
}

void PackageClient::setTipUrl(const QUrl& url)
{
    m_tipUrl = url;
}

QUrl PackageClient::tipUrl() const
{
    if (m_tipUrl.isValid())
        return m_tipUrl;
    if (!m_cdn.isEmpty())
        return QUrl(m_cdn + QStringLiteral("/launcher/tibiametadata.json"));
    return QUrl(QStringLiteral("https://static.tibia.com/launcher/tibiametadata.json"));
}

bool PackageClient::useCdnContentStore() const
{
    // Explicit opt-in for content-addressed /content/ store + /launcher/ rewrite.
    if (qEnvironmentVariableIntValue("QTCLIENT_CDN_CONTENT_STORE") == 1)
        return true;
    if (qEnvironmentVariableIntValue("QTCLIENT_CDN_CONTENT_STORE") == 0)
        return false;
    if (m_cdn.isEmpty())
        return false;
    const QString host = QUrl(m_cdn).host();
    return host.contains(QLatin1String("example.invalid")) || host.contains(QLatin1String("qtclient.local"));
}

void PackageClient::setDataDir(const QString& dir)
{
    m_dataDir = dir;
}

QString PackageClient::hostOs() const
{
#if defined(__EMSCRIPTEN__)
    return QStringLiteral("web");
#elif defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    return QStringLiteral("linux");
#endif
}

QString PackageClient::hostArch() const
{
#if defined(__EMSCRIPTEN__)
    return QStringLiteral("wasm");
#elif defined(__aarch64__) || defined(__arm64__)
    return QStringLiteral("arm64");
#else
    return QStringLiteral("x86_64");
#endif
}

QString PackageClient::sha256Hex(const QByteArray& data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString PackageClient::sha256File(QFile& file)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1 << 20);
        if (chunk.isEmpty())
            break;
        hash.addData(chunk);
    }
    return QString::fromLatin1(hash.result().toHex());
}

const CipBuild* PackageClient::matchBuild(const QVector<CipBuild>& builds, const QString& os, const QString& arch)
{
    const CipBuild* fallback = nullptr;
    for (const CipBuild& build : builds) {
        if (build.os != os)
            continue;
        const QStringList parts = build.architecture.split(QLatin1Char(';'));
        if (parts.contains(arch) || build.architecture == arch)
            return &build;
        if (!fallback)
            fallback = &build;
    }
    return fallback;
}

QUrl PackageClient::resolve(const QUrl& base, const QString& url) const
{
    const QUrl parsed(url);
    if (parsed.isRelative())
        return base.resolved(parsed);
    // Content-store CDN only: map absolute /launcher/ and /hints.json onto m_cdn.
    // CIP-faithful tips keep their original host (e.g. static.tibia.com).
    if (useCdnContentStore() && !m_cdn.isEmpty()) {
        const QString path = parsed.path();
        const int launcher = path.indexOf(QStringLiteral("/launcher/"));
        if (launcher >= 0)
            return QUrl(m_cdn + path.mid(launcher));
        if (path == QStringLiteral("/hints.json") || path.endsWith(QStringLiteral("/hints.json")))
            return QUrl(m_cdn + QStringLiteral("/hints.json"));
    }
    return parsed;
}

QUrl PackageClient::fileDownloadUrl(const QUrl& packageUrl, const CipFile& file) const
{
    // Content-store: content-addressed /content/<aa>/<packedhash> avoids tip-dir
    // cache staleness. CIP / foreign tips: resolve files[].url relative to the
    // package/assets manifest URL (official launcher behaviour).
    if (useCdnContentStore() && !m_cdn.isEmpty()) {
        const QString hash = file.packedhash.trimmed().toLower();
        if (hash.size() == 64) {
            bool hex = true;
            for (const QChar c : hash) {
                if (!c.isDigit() && (c < QLatin1Char('a') || c > QLatin1Char('f'))) {
                    hex = false;
                    break;
                }
            }
            if (hex)
                return QUrl(m_cdn + QLatin1String("/content/") + hash.left(2) + QLatin1Char('/') + hash);
        }
    }
    const QUrl base = packageUrl.adjusted(QUrl::RemoveFilename);
    return base.resolved(QUrl(file.url));
}

QByteArray PackageClient::get(const QUrl& url, QString* error)
{
    QUrl resolved = url.isRelative() ? url : resolve(QUrl(), url.toString());
    if (resolved.isRelative() && !m_cdn.isEmpty())
        resolved = QUrl(m_cdn).resolved(resolved);
    QNetworkRequest request(resolved);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    QNetworkReply* reply = m_nam->get(request);
    // Config GETs must not drive the install ETA. Install uses getForInstall.
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const QByteArray body = reply->readAll();
    const auto netError = reply->error();
    const QString errString = reply->errorString();
    reply->deleteLater();
    if (netError != QNetworkReply::NoError) {
        if (error)
            *error = QStringLiteral("Download failed: ") + errString;
        qWarning("Download failed: %s %s", qUtf8Printable(resolved.toString()), qUtf8Printable(errString));
        return {};
    }
    return body;
}

QByteArray PackageClient::getForInstall(const QUrl& url, qint64 donePacked, qint64 totalPacked, int fileIndex,
                                        int fileCount, QString* error)
{
    QUrl resolved = url.isRelative() ? url : resolve(QUrl(), url.toString());
    if (resolved.isRelative() && !m_cdn.isEmpty())
        resolved = QUrl(m_cdn).resolved(resolved);
    QNetworkRequest request(resolved);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    QNetworkReply* reply = m_nam->get(request);
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, donePacked, totalPacked, fileIndex, fileCount](qint64 received, qint64) {
                Q_EMIT progress(donePacked + received, totalPacked, fileIndex, fileCount);
            });
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const QByteArray body = reply->readAll();
    const auto netError = reply->error();
    const QString errString = reply->errorString();
    reply->deleteLater();
    if (netError != QNetworkReply::NoError) {
        if (error)
            *error = QStringLiteral("Download failed: ") + errString;
        qWarning("Download failed: %s %s", qUtf8Printable(resolved.toString()), qUtf8Printable(errString));
        return {};
    }
    return body;
}

void PackageClient::getAsync(const QUrl& url, GetCallback done)
{
    QUrl resolved = url.isRelative() ? url : resolve(QUrl(), url.toString());
    if (resolved.isRelative() && !m_cdn.isEmpty())
        resolved = QUrl(m_cdn).resolved(resolved);
    QNetworkRequest request(resolved);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    QNetworkReply* reply = m_nam->get(request);
    if (m_emitProgress) {
        connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64) {
            Q_EMIT progress(m_progressDone + received, m_progressTotal, m_progressFileIndex, m_progressFileCount);
        });
    }
    connect(reply, &QNetworkReply::finished, this, [this, reply, done = std::move(done), resolved]() {
        const QByteArray body = reply->readAll();
        QString error;
        if (reply->error() != QNetworkReply::NoError) {
            error = QStringLiteral("Download failed: ") + reply->errorString();
            qWarning("Download failed: %s %s", qUtf8Printable(resolved.toString()), qUtf8Printable(error));
        }
        reply->deleteLater();
        if (done)
            done(body, error);
    });
}

CipMetadata PackageClient::parseMetadata(const QByteArray& body, QString* error) const
{
    CipMetadata metadata;
    if (body.isEmpty())
        return metadata;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        if (error)
            *error = QStringLiteral("JSON package configuration file could not be parsed, using default package.");
        return metadata;
    }
    const QJsonObject root = doc.object();
    const QJsonArray launcherBuilds = root.value(QStringLiteral("launcher")).toObject().value(QStringLiteral("builds")).toArray();
    for (const QJsonValue& value : launcherBuilds) {
        if (value.isObject())
            metadata.launcherBuilds.push_back(buildFromJson(value.toObject()));
    }
    const QJsonArray packages = root.value(QStringLiteral("packages")).toArray();
    for (const QJsonValue& value : packages) {
        if (!value.isObject())
            continue;
        const QJsonObject obj = value.toObject();
        CipPackageInfo info;
        info.name = jsonString(obj, "name");
        info.variant = jsonString(obj, "variant");
        info.directoryname = jsonString(obj, "directoryname");
        info.logolinktarget = jsonString(obj, "logolinktarget");
        info.installautomatically = obj.value(QStringLiteral("installautomatically")).toBool();
        const QJsonArray builds = obj.value(QStringLiteral("builds")).toArray();
        for (const QJsonValue& build : builds) {
            if (build.isObject())
                info.builds.push_back(buildFromJson(build.toObject()));
        }
        metadata.packages.push_back(info);
    }
    metadata.hintsUrl = QUrl(jsonString(root, "hints"));
    if (m_dataDir.size()) {
        QDir().mkpath(m_dataDir);
        QSaveFile cache(m_dataDir + QStringLiteral("/launchermetadata.json"));
        if (cache.open(QIODevice::WriteOnly)) {
            cache.write(body);
            cache.commit();
        }
    }
    return metadata;
}

CipPackageDoc PackageClient::parsePackage(const QByteArray& body, QString* error) const
{
    CipPackageDoc doc;
    if (body.isEmpty())
        return doc;
    const QJsonDocument json = QJsonDocument::fromJson(body);
    if (!json.isObject()) {
        if (error)
            *error = QStringLiteral("JSON package configuration file could not be parsed, using default package.");
        return doc;
    }
    const QJsonObject obj = json.object();
    doc.version = jsonString(obj, "version");
    doc.executable = jsonString(obj, "executable");
    doc.generation = jsonString(obj, "generation");
    doc.files = filesFromArray(obj.value(QStringLiteral("files")).toArray());
    return doc;
}

CipMetadata PackageClient::fetchMetadata(QString* error)
{
    return parseMetadata(get(tipUrl(), error), error);
}

QString PackageClient::fetchVersion(const QUrl& versionUrl, QString* error)
{
    return QString::fromUtf8(get(versionUrl, error)).trimmed();
}

CipPackageDoc PackageClient::fetchPackage(const QUrl& packageUrl, QString* error)
{
    return parsePackage(get(packageUrl, error), error);
}

QVector<CipFile> PackageClient::parseAssetFiles(const QByteArray& body, QString* error) const
{
    if (body.isEmpty())
        return {};
    const QJsonDocument json = QJsonDocument::fromJson(body);
    if (!json.isObject()) {
        if (error)
            *error = QStringLiteral("JSON asset configuration file could not be parsed, using default package.");
        return {};
    }
    return filesFromArray(json.object().value(QStringLiteral("files")).toArray());
}

QVector<CipFile> PackageClient::fetchAssetFiles(const QUrl& assetsUrl, QString* error)
{
    return parseAssetFiles(get(assetsUrl, error), error);
}

QString PackageClient::fetchHints(const QUrl& hintsUrl, QString* error)
{
    const QByteArray body = get(hintsUrl, error);
    if (body.isEmpty())
        return {};
    const QJsonDocument json = QJsonDocument::fromJson(body);
    QStringList lines;
    const auto take = [&lines](const QJsonValue& value) {
        if (value.isString())
            lines.push_back(value.toString());
        else if (value.isObject()) {
            const QJsonObject obj = value.toObject();
            const QString text = obj.value(QStringLiteral("text")).toString(obj.value(QStringLiteral("hint")).toString());
            if (!text.isEmpty())
                lines.push_back(text);
        }
    };
    if (json.isArray()) {
        for (const QJsonValue& value : json.array())
            take(value);
    } else if (json.isObject()) {
        const QJsonValue hints = json.object().value(QStringLiteral("hints"));
        if (hints.isArray()) {
            for (const QJsonValue& value : hints.toArray())
                take(value);
        }
    }
    return lines.join(QStringLiteral("\n\n"));
}

QVector<tibia::app::Hint> PackageClient::fetchHintList(const QUrl& hintsUrl, QString* error)
{
    return tibia::app::parseHints(get(hintsUrl, error));
}

bool PackageClient::fileAlreadyInstalled(const CipFile& file, const QString& destDir) const
{
    const QString destPath = QDir(destDir).filePath(file.localfile);
    QFileInfo existing(destPath);
    if (!existing.isFile() || existing.size() != file.unpackedsize)
        return false;
    QFile current(destPath);
    if (!current.open(QIODevice::ReadOnly))
        return false;
    return sha256File(current) == file.unpackedhash;
}

bool PackageClient::hasFileGaps(const CipPackageDoc& doc, const QString& destDir) const
{
    for (const CipFile& file : doc.files) {
        if (!fileAlreadyInstalled(file, destDir))
            return true;
    }
    return false;
}

bool PackageClient::installFiles(const QUrl& packageUrl, const QVector<CipFile>& files, const QString& destDir, QString* error)
{
    QDir().mkpath(destDir);
    QVector<bool> needDownload(files.size());
    qint64 totalPacked = 0;
    for (int i = 0; i < files.size(); ++i) {
        needDownload[i] = !fileAlreadyInstalled(files.at(i), destDir);
        if (needDownload[i])
            totalPacked += qMax(files.at(i).packedsize, qint64(0));
    }
    if (totalPacked <= 0)
        totalPacked = 1;
    qint64 donePacked = 0;
    const int fileCount = files.size();
    int index = 0;
    for (const CipFile& file : files) {
        const bool download = needDownload.at(index);
        ++index;
        if (!download) {
            Q_EMIT progress(donePacked, totalPacked, index, fileCount);
            continue;
        }
        const QString destPath = QDir(destDir).filePath(file.localfile);
        const QByteArray packed = getForInstall(fileDownloadUrl(packageUrl, file), donePacked, totalPacked, index,
                                                fileCount, error);
        if (packed.isEmpty() && file.packedsize != 0) {
            if (error && error->isEmpty())
                *error = QStringLiteral("Download failed");
            return false;
        }
        if (sha256Hex(packed) != file.packedhash) {
            if (error)
                *error = QStringLiteral("Download failed: ") + file.localfile;
            qWarning("packed hash mismatch for %s (%lld bytes)", qUtf8Printable(file.localfile), qint64(packed.size()));
            return false;
        }
        QByteArray unpacked;
        QString unpackError;
        if (!unpackLzmaAlone(packed, unpacked, &unpackError)) {
            if (error)
                *error = unpackError + QStringLiteral(" ") + file.url;
            return false;
        }
        if (sha256Hex(unpacked) != file.unpackedhash || unpacked.size() != file.unpackedsize) {
            if (error)
                *error = QStringLiteral("Download failed: ") + file.localfile;
            return false;
        }
        QDir().mkpath(QFileInfo(destPath).path());
        QSaveFile out(destPath);
        if (!out.open(QIODevice::WriteOnly) || out.write(unpacked) != unpacked.size() || !out.commit()) {
            if (error)
                *error = QStringLiteral("Failed to create folder: '%1'.").arg(destPath);
            return false;
        }
        donePacked += qMax(file.packedsize > 0 ? file.packedsize : qint64(packed.size()), qint64(0));
        Q_EMIT progress(donePacked, totalPacked, index, fileCount);
    }
    return true;
}

static void markExecutable(const QString& path)
{
    QFile file(path);
    if (!file.exists())
        return;
    file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);
}

bool PackageClient::writePackageMarkers(const CipPackageDoc& doc, const QString& destDir, QString* error)
{
    if (!doc.executable.isEmpty())
        markExecutable(QDir(destDir).filePath(doc.executable));
    QSaveFile pkg(QDir(destDir).filePath(QStringLiteral("package.json")));
    if (!pkg.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Failed to create folder: '%1'.").arg(destDir);
        return false;
    }
    QJsonObject obj;
    obj.insert(QStringLiteral("version"), doc.version);
    obj.insert(QStringLiteral("executable"), doc.executable);
    if (!doc.generation.isEmpty())
        obj.insert(QStringLiteral("generation"), doc.generation);
    QJsonArray files;
    for (const CipFile& file : doc.files) {
        QJsonObject item;
        item.insert(QStringLiteral("url"), file.url);
        item.insert(QStringLiteral("localfile"), file.localfile);
        item.insert(QStringLiteral("packedhash"), file.packedhash);
        item.insert(QStringLiteral("packedsize"), QString::number(file.packedsize));
        item.insert(QStringLiteral("unpackedhash"), file.unpackedhash);
        item.insert(QStringLiteral("unpackedsize"), QString::number(file.unpackedsize));
        files.append(item);
    }
    obj.insert(QStringLiteral("files"), files);
    pkg.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    if (!pkg.commit()) {
        if (error)
            *error = QStringLiteral("Failed to create folder: '%1'.").arg(destDir);
        return false;
    }
    QFile version(QDir(destDir).filePath(QStringLiteral("package.json.version")));
    if (!version.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("The installation of %1 failed.").arg(destDir);
        return false;
    }
    version.write(doc.version.toUtf8());
    version.close();
    QFile::remove(QDir(destDir).filePath(QStringLiteral("partial.package.json.version")));
    return true;
}

bool PackageClient::installPackage(const QUrl& packageUrl, const CipPackageDoc& doc, const QString& destDir, QString* error)
{
    QDir().mkpath(destDir);
    QFile partial(QDir(destDir).filePath(QStringLiteral("partial.package.json.version")));
    if (partial.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        partial.write(doc.version.toUtf8());
        partial.close();
    }
    if (!installFiles(packageUrl, doc.files, destDir, error))
        return false;
    return writePackageMarkers(doc, destDir, error);
}

void PackageClient::installFilesAsync(const QUrl& packageUrl, const QVector<CipFile>& files, const QString& destDir,
                                      int index, qint64 donePacked, qint64 totalPacked, InstallCallback done)
{
    if (index >= files.size()) {
        m_emitProgress = false;
        if (done)
            done(true, {});
        return;
    }
    const CipFile file = files.at(index);
    const int fileIndex = index + 1;
    const int fileCount = files.size();
    if (fileAlreadyInstalled(file, destDir)) {
        Q_EMIT progress(donePacked, totalPacked, fileIndex, fileCount);
        installFilesAsync(packageUrl, files, destDir, index + 1, donePacked, totalPacked, std::move(done));
        return;
    }
    const QString destPath = QDir(destDir).filePath(file.localfile);
    m_emitProgress = true;
    m_progressDone = donePacked;
    m_progressTotal = totalPacked;
    m_progressFileIndex = fileIndex;
    m_progressFileCount = fileCount;
    getAsync(fileDownloadUrl(packageUrl, file),
             [this, packageUrl, files, destDir, index, file, fileIndex, fileCount, destPath, donePacked, totalPacked,
              done = std::move(done)](const QByteArray& packed, const QString& error) {
                 if ((packed.isEmpty() && file.packedsize != 0) || !error.isEmpty()) {
                     m_emitProgress = false;
                     if (done)
                         done(false, error.isEmpty() ? QStringLiteral("Download failed") : error);
                     return;
                 }
                 if (sha256Hex(packed) != file.packedhash) {
                     m_emitProgress = false;
                     qWarning("packed hash mismatch for %s (%lld bytes)", qUtf8Printable(file.localfile),
                              qint64(packed.size()));
                     if (done)
                         done(false, QStringLiteral("Download failed: ") + file.localfile);
                     return;
                 }
                 QByteArray unpacked;
                 QString unpackError;
                 if (!unpackLzmaAlone(packed, unpacked, &unpackError)) {
                     m_emitProgress = false;
                     if (done)
                         done(false, unpackError + QStringLiteral(" ") + file.url);
                     return;
                 }
                 if (sha256Hex(unpacked) != file.unpackedhash || unpacked.size() != file.unpackedsize) {
                     m_emitProgress = false;
                     if (done)
                         done(false, QStringLiteral("Download failed: ") + file.localfile);
                     return;
                 }
                 QDir().mkpath(QFileInfo(destPath).path());
                 QSaveFile out(destPath);
                 if (!out.open(QIODevice::WriteOnly) || out.write(unpacked) != unpacked.size() || !out.commit()) {
                     m_emitProgress = false;
                     if (done)
                         done(false, QStringLiteral("Failed to create folder: '%1'.").arg(destPath));
                     return;
                 }
                 const qint64 nextDone = donePacked
                     + qMax(file.packedsize > 0 ? file.packedsize : qint64(packed.size()), qint64(0));
                 Q_EMIT progress(nextDone, totalPacked, fileIndex, fileCount);
                 installFilesAsync(packageUrl, files, destDir, index + 1, nextDone, totalPacked, done);
             });
}

void PackageClient::installPackageAsync(const QUrl& packageUrl, const CipPackageDoc& doc, const QString& destDir,
                                        InstallCallback done)
{
    QDir().mkpath(destDir);
    QFile partial(QDir(destDir).filePath(QStringLiteral("partial.package.json.version")));
    if (partial.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        partial.write(doc.version.toUtf8());
        partial.close();
    }
    qint64 totalPacked = 0;
    for (const CipFile& file : doc.files) {
        if (!fileAlreadyInstalled(file, destDir))
            totalPacked += qMax(file.packedsize, qint64(0));
    }
    if (totalPacked <= 0)
        totalPacked = 1;
    installFilesAsync(packageUrl, doc.files, destDir, 0, 0, totalPacked,
                      [this, doc, destDir, done = std::move(done)](bool ok, const QString& error) {
                          m_emitProgress = false;
                          if (!ok) {
                              if (done)
                                  done(false, error);
                              return;
                          }
                          QString writeError;
                          if (!writePackageMarkers(doc, destDir, &writeError)) {
                              if (done)
                                  done(false, writeError);
                              return;
                          }
                          if (done)
                              done(true, {});
                      });
}

bool PackageClient::installAssets(const QUrl& assetsUrl, const QVector<CipFile>& files, const QString& assetsVersion,
                                  const QString& destDir, QString* error)
{
    if (!installFiles(assetsUrl, files, destDir, error))
        return false;
    QFile stamp(QDir(destDir).filePath(QStringLiteral("assets.json.sha256")));
    if (!stamp.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("The installation of %1 failed.").arg(destDir);
        return false;
    }
    stamp.write(assetsVersion.toUtf8());
    stamp.close();
    return true;
}

void PackageClient::installAssetsAsync(const QUrl& assetsUrl, const QVector<CipFile>& files,
                                       const QString& assetsVersion, const QString& destDir, InstallCallback done)
{
    QDir().mkpath(destDir);
    qint64 totalPacked = 0;
    for (const CipFile& file : files) {
        if (!fileAlreadyInstalled(file, destDir))
            totalPacked += qMax(file.packedsize, qint64(0));
    }
    if (totalPacked <= 0)
        totalPacked = 1;
    installFilesAsync(assetsUrl, files, destDir, 0, 0, totalPacked,
                      [this, assetsVersion, destDir, done = std::move(done)](bool ok, const QString& error) {
                          m_emitProgress = false;
                          if (!ok) {
                              if (done)
                                  done(false, error);
                              return;
                          }
                          QFile stamp(QDir(destDir).filePath(QStringLiteral("assets.json.sha256")));
                          if (!stamp.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                              if (done)
                                  done(false, QStringLiteral("The installation of %1 failed.").arg(destDir));
                              return;
                          }
                          stamp.write(assetsVersion.toUtf8());
                          stamp.close();
                          if (done)
                              done(true, {});
                      });
}
