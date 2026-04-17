#ifndef FILECONTROLLER_H
#define FILECONTROLLER_H

#include <QObject>
#include <QStringList>

class FileController : public QObject
{
Q_OBJECT
public:
    explicit FileController(QObject *parent = nullptr);
    ~FileController();

    Q_INVOKABLE QStringList scanDirectory(const QString &path);
    Q_INVOKABLE QString readFile(const QString &path);
    Q_INVOKABLE bool saveFile(const QString &path, const QString &content);
    Q_INVOKABLE QString getParentPath(const QString &path);

    // Hugo Specific
    Q_INVOKABLE int startHugoServer(const QString &repoPath);
    Q_INVOKABLE void stopHugoServer();
    Q_INVOKABLE QString processImage(const QString &srcPath, const QString &repoPath, const QString &docPath);
    Q_INVOKABLE QString createPost(const QString &repoPath, const QString &title);

    // Config Management
    Q_INVOKABLE QString loadConfigCurrent();
    Q_INVOKABLE QStringList loadConfigSites();
    Q_INVOKABLE bool addSiteAndSetCurrent(const QString &sitePath);

    // Standard Paths
    Q_INVOKABLE QString getDocumentsLocation();
};

#endif // FILECONTROLLER_H
