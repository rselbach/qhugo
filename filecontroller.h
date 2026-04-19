#ifndef FILECONTROLLER_H
#define FILECONTROLLER_H

#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>

class FileController : public QObject
{
Q_OBJECT
public:
    explicit FileController(QObject *parent = nullptr);
    ~FileController();

    Q_INVOKABLE QStringList scanDirectory(const QString &path);
    Q_INVOKABLE void invalidateScanCache();
    Q_INVOKABLE QString readFile(const QString &path);
    Q_INVOKABLE bool saveFile(const QString &path, const QString &content);
    Q_INVOKABLE QString getParentPath(const QString &path);

    // Hugo Specific
    Q_INVOKABLE int startHugoServer(const QString &repoPath);
    Q_INVOKABLE void stopHugoServer();
    Q_INVOKABLE QString processImage(const QString &srcPath, const QString &repoPath, const QString &docPath);
    Q_INVOKABLE QString createPost(const QString &repoPath, const QString &title);
    Q_INVOKABLE QString getHugoURL(const QString &filePath, const QString &repoPath);
    Q_INVOKABLE bool isBundleDirectory(const QString &dirPath);

    // Config Management
    Q_INVOKABLE QString loadConfigCurrent();
    Q_INVOKABLE QStringList loadConfigSites();
    Q_INVOKABLE bool addSiteAndSetCurrent(const QString &sitePath);

    // Standard Paths
    Q_INVOKABLE QString getDocumentsLocation();

    // File Operations
    Q_INVOKABLE void openInFileBrowser(const QString &path);
    Q_INVOKABLE bool deleteFile(const QString &path);

private:
    QString m_cachedScanPath;
    QStringList m_cachedScanResult;
    QFileSystemWatcher m_scanWatcher;
};

#endif // FILECONTROLLER_H
