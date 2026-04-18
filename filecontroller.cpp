#include "filecontroller.h"
#include "backend.h"
#include <QDirIterator>
#include <QUrl>
#include <QDebug>
#include <QDateTime>
#include <QRegularExpression>
#include <QStandardPaths>

FileController::FileController(QObject *parent) : QObject(parent) {
    InitBackend(); // Initialize Go runtime
}

FileController::~FileController() {
    StopHugo();
}

QStringList FileController::scanDirectory(const QString &path) {
    QString localPath = path;
    if (path.startsWith("file://")) {
        localPath = QUrl(path).toLocalFile();
    }

    QStringList fileList;
    QDir dir(localPath);
    if (!dir.exists()) {
        return fileList;
    }

    QDirIterator it(localPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        QString filePath = it.next();

        // Skip heavy folders and build artifacts
        if (filePath.contains("/.git/") || 
            filePath.contains("/node_modules/") || 
            filePath.contains("/public/") ||
            filePath.contains("/resources/")) {
            continue;
        }

        // Only index markdown files in FuzzyFinder
        if (!filePath.endsWith(".md")) {
            continue;
        }

        fileList.append(filePath);
        
        if (fileList.size() >= 20000) {
            break;
        }
    }
    
    return fileList;
}

QString FileController::getParentPath(const QString &path) {
    QString localPath = path;
    if (path.startsWith("file://")) {
        localPath = QUrl(path).toLocalFile();
    }

    QDir dir(localPath);
    if (dir.cdUp()) {
        return QUrl::fromLocalFile(dir.absolutePath()).toString();
    }
    return path;
}

QString FileController::readFile(const QString &path) {
    QString localPath = path;
    if (path.startsWith("file://")) {
        localPath = QUrl(path).toLocalFile();
    }

    // Keep the QByteArray alive: toUtf8() returns a temporary that would be
    // destroyed at the end of this statement, leaving .data() dangling before
    // we hand it to Go. On macOS that dangles to random memory and the read
    // returns "".
    QByteArray pathBytes = localPath.toUtf8();
    char* content = ReadFileContent(pathBytes.data());
    QString result = QString::fromUtf8(content);
    FreeString(content);
    return result;
}

bool FileController::saveFile(const QString &path, const QString &content) {
    QString localPath = path;
    if (path.startsWith("file://")) {
        localPath = QUrl(path).toLocalFile();
    }
    return SaveFileContent(localPath.toUtf8().data(), content.toUtf8().data()) == 1;
}

int FileController::startHugoServer(const QString &repoPath) {
    QString localPath = repoPath;
    if (repoPath.startsWith("file://")) {
        localPath = QUrl(repoPath).toLocalFile();
    }
    return StartHugo(localPath.toUtf8().data());
}

void FileController::stopHugoServer() {
    StopHugo();
}

QString FileController::processImage(const QString &srcPath, const QString &repoPath, const QString &docPath) {
    QString localSrc = srcPath;
    if (srcPath.startsWith("file://")) localSrc = QUrl(srcPath).toLocalFile();
    
    QString localRepo = repoPath;
    if (repoPath.startsWith("file://")) localRepo = QUrl(repoPath).toLocalFile();
    
    QString localDoc = docPath;
    if (docPath.startsWith("file://")) localDoc = QUrl(docPath).toLocalFile();

    char* res = ProcessImage(localSrc.toUtf8().data(), localRepo.toUtf8().data(), localDoc.toUtf8().data());
    QString qRes = QString::fromUtf8(res);
    FreeString(res);
    return qRes;
}

QString FileController::createPost(const QString &repoPath, const QString &title) {
    QString localRepo = repoPath;
    if (repoPath.startsWith("file://")) {
        localRepo = QUrl(repoPath).toLocalFile();
    }

    QString slug = title.toLower().replace(QRegularExpression("[^a-z0-9]+"), "-");
    QString year = QDateTime::currentDateTime().toString("yyyy");

    // Page bundle: create directory named after slug, with index.md inside
    QString contentDir = localRepo + "/content/post/" + year + "/" + slug;
    QString contentPath = contentDir + "/index.md";

    QString frontmatter = "---\n";
    frontmatter += "title: \"" + title + "\"\n";
    frontmatter += "date: " + QDateTime::currentDateTime().toString(Qt::ISODate) + "\n";
    frontmatter += "draft: true\n";
    frontmatter += "---\n\n";

    QDir().mkpath(contentDir);
    saveFile(contentPath, frontmatter);
    return contentPath;
}

QString FileController::getHugoURL(const QString &filePath, const QString &repoPath) {
    QString localFile = filePath;
    if (filePath.startsWith("file://")) {
        localFile = QUrl(filePath).toLocalFile();
    }

    QString localRepo = repoPath;
    if (repoPath.startsWith("file://")) {
        localRepo = QUrl(repoPath).toLocalFile();
    }

    char* result = GetHugoURL(localFile.toUtf8().data(), localRepo.toUtf8().data());
    QString qResult = QString::fromUtf8(result);
    FreeString(result);
    return qResult;
}

QString FileController::loadConfigCurrent() {
    char* result = LoadConfigCurrent();
    QString qResult = QString::fromUtf8(result);
    FreeString(result);
    return qResult;
}

QStringList FileController::loadConfigSites() {
    char* result = LoadConfigSites();
    QString qResult = QString::fromUtf8(result);
    FreeString(result);

    QStringList sites;
    if (!qResult.isEmpty()) {
        // Parse JSON array
        QString trimmed = qResult.trimmed();
        if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
            trimmed = trimmed.mid(1, trimmed.length() - 2);
            // Split by commas, handling quoted strings
            QRegularExpression re("\\s*,\\s*");
            QStringList parts = trimmed.split(re);
            for (QString part : parts) {
                part = part.trimmed();
                // Remove quotes
                if ((part.startsWith("\"") && part.endsWith("\"")) ||
                    (part.startsWith("'") && part.endsWith("'"))) {
                    part = part.mid(1, part.length() - 2);
                }
                if (!part.isEmpty()) {
                    sites.append(part);
                }
            }
        }
    }
    return sites;
}

bool FileController::addSiteAndSetCurrent(const QString &sitePath) {
    QString localPath = sitePath;
    if (sitePath.startsWith("file://")) {
        localPath = QUrl(sitePath).toLocalFile();
    }
    return AddSiteAndSetCurrent(localPath.toUtf8().data()) == 1;
}

QString FileController::getDocumentsLocation() {
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

bool FileController::isBundleDirectory(const QString &dirPath) {
    QString localPath = dirPath;
    if (dirPath.startsWith("file://")) {
        localPath = QUrl(dirPath).toLocalFile();
    }
    QDir dir(localPath);
    return dir.exists("index.md");
}
