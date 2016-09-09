#ifndef SYNCUI_H
#define SYNCUI_H

#include <QtDBus>

#include <syncevo-server-full.h>
#include <syncevo-session-full.h>

class SyncEvoPeer;
class SyncEvoSession;
class SyncEvo;

class SyncEvoConfig
{
public:
    SyncEvoConfig();
    void setTargetConfig(bool targetConfig);

    bool isEmpty() const;
    const QStringMultiMap& getConfig() const;
    const QStringMultiMap& getRemoteConfig() const;
    QString getSyncConfig(const QString& key) const;
    QString getServerConfig(const QString& key) const;
    QStringList getStoreList() const;
    QStringList getSourceList() const;
    QString getStoreConfig(const QString& source, const QString& key) const;
    QString getSourceConfig(const QString& source, const QString& key) const;
    QString getName() const;
    QString getType() const;

    bool hasTargetConfig() const;
    QString getTargetContext() const;

    void clear();
    void setConfig(const QStringMultiMap& config);
    void mergeConfig(const QStringMultiMap& config);
    void setRemoteConfig(const QStringMultiMap& config);
    void mergeRemoteConfig(const QStringMultiMap& config);
    void setSyncConfig(const QString& key, const QString& value);
    void setServerConfig(const QString& key, const QString& value);
    void setStoreConfig(const QString& source, const QString& key, const QString& value);
    void setSourceConfig(const QString& source, const QString& key, const QString& value);

    void setConfig(const SyncEvoConfig& config);
    void mergeConfig(const SyncEvoConfig& config);

private:
    QStringMultiMap m_local, m_remote;
    bool m_targetConfig;

    void mergeMap(QStringMultiMap& dst, const QStringMultiMap& src);
};

class SyncEvoStoreModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ItemRole {
        IdRole = Qt::UserRole + 1,
        NameRole
    };

    SyncEvoStoreModel(SyncEvoPeer* peer);
    ~SyncEvoStoreModel();
    void init();
    QHash<int, QByteArray> roleNames() const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    void configChanged();

private:
    QPointer<SyncEvoPeer> m_peer;
    QList<QString> m_stores;
};

class SyncEvoSourceModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ItemRole {
        IdRole = Qt::UserRole + 1,
        NameRole
    };

    SyncEvoSourceModel(SyncEvoPeer* peer);
    ~SyncEvoSourceModel();
    void init();
    QHash<int, QByteArray> roleNames() const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    void configChanged();

private:
    QPointer<SyncEvoPeer> m_peer;
    QList<QString> m_sources;
};

class SyncEvoDatabaseModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ItemRole {
        IdRole = Qt::UserRole + 1,
        NameRole,
        DefaultRole,
        ValidRole
    };

    SyncEvoDatabaseModel(SyncEvo* sync, QDBusPendingReply<QArrayOfDatabases> reply);
    ~SyncEvoDatabaseModel();
    void init();
    QHash<int, QByteArray> roleNames() const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;

    Q_INVOKABLE int indexOf(const QString& id) const;
    Q_INVOKABLE QString idOfIndex(int idx) const;
    Q_INVOKABLE bool isValid() const;

private:
    QPointer<SyncEvo> m_sync;
    QArrayOfDatabases m_databases;
    bool m_valid;
};

class SyncEvoPeer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString id READ getId)
    Q_PROPERTY(QString name READ getName)
    Q_PROPERTY(QString type READ getType)
    Q_PROPERTY(QString url READ getUrl)
    Q_PROPERTY(QString icon READ getIcon)
    Q_PROPERTY(QString present READ getPresent NOTIFY presentChanged)
    Q_PROPERTY(SyncEvoStoreModel* storeModel READ getStoreModel)
    Q_PROPERTY(SyncEvoSourceModel* sourceModel READ getSourceModel)

public:
    SyncEvoPeer();
    SyncEvoPeer(SyncEvo* sync, QString id, bool isTemplate, bool exists);
    ~SyncEvoPeer();
    void init();
    QString getId() const;
    bool exists() const;
    void setExists();
    const QStringMultiMap& getConfig() const;
    const QStringMultiMap& getRemoteConfig() const;
    Q_INVOKABLE QString getSyncConfig(const QString& key) const;
    Q_INVOKABLE QString getServerConfig(const QString& key) const;
    Q_INVOKABLE QStringList getStoreList() const;
    Q_INVOKABLE QStringList getSourceList() const;
    Q_INVOKABLE QString getStoreConfig(const QString& source, const QString& key) const;
    Q_INVOKABLE QString getSourceConfig(const QString& source, const QString& key) const;
    Q_INVOKABLE QString getSourceUri(const QString& source) const;
    QString getName() const;
    QString getType() const;
    QString getUrl() const;
    QString getIcon() const;
    QString getPresent() const;
    SyncEvoStoreModel* getStoreModel();
    SyncEvoSourceModel* getSourceModel();
    void setPresent(const QString& present);
    void configChanged();

    bool useTargetConfig() const;
    bool isTargetConfig() const;
    bool hasTargetConfig() const;
    QString getTargetContext() const;
    QString getTargetConfig() const;
    QString getDefaultTargetContext() const;
    QString getDefaultTargetConfig() const;

    void createFromTemplate(SyncEvoPeer* tmpl);
    Q_INVOKABLE void deleteConfig();
    void setConfig(const QStringMultiMap& config);
    void mergeConfig(const QStringMultiMap& config);
    void setRemoteConfig(const QStringMultiMap& config);
    void mergeRemoteConfig(const QStringMultiMap& config);
    Q_INVOKABLE void setSyncConfig(const QString& key, const QString& value);
    Q_INVOKABLE void setServerConfig(const QString& key, const QString& value);
    Q_INVOKABLE void setStoreConfig(const QString& source, const QString& key, const QString& value);
    Q_INVOKABLE void setSourceConfig(const QString& source, const QString& key, const QString& value);
    Q_INVOKABLE void setSourceUri(const QString& source, const QString& uri);
    Q_INVOKABLE void commitConfig();
    Q_INVOKABLE SyncEvoSession* getTempSyncSession();
    Q_INVOKABLE SyncEvoSession* getTempServerSession();

Q_SIGNALS:
    void presentChanged(const QString& present);

private:
    QPointer<SyncEvo> m_sync;
    QString m_id;
    bool m_isTemplate, m_exists;
    bool m_has_config, m_has_present;
    SyncEvoConfig m_curConfig, m_setConfig;
    QString m_present;
    SyncEvoStoreModel m_stores;
    SyncEvoSourceModel m_sources;

    bool m_clearConfig;
    SyncEvoConfig m_newConfig;

    void load_config();
};

class SyncEvoPeerModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ItemRole {
        IdRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        UrlRole,
        IconRole,
        PresentRole
    };

    SyncEvoPeerModel(SyncEvo* sync, bool areTemplates);
    ~SyncEvoPeerModel();
    void init();
    QHash<int, QByteArray> roleNames() const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    SyncEvoPeer* findPeer(const QString& id);
    SyncEvoPeer* createPeer(const QString& id, const QString& peerName);
    void deletePeer(SyncEvoPeer* peer);
    void configChanged();
    void presenceChanged(const QString& id, const QString& status);

private:
    QPointer<SyncEvo> m_sync;
    QList<SyncEvoPeer*> m_peers, m_targets;
    bool m_areTemplates;
    bool m_ready;
    bool m_configChanged;

    void initPeerList();
    void processPeerList();
    void updatePeerList();
    void freePeerList();
};

class SyncEvoSession : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString status READ getStatus NOTIFY statusChanged)
    Q_PROPERTY(uint error READ getError)
    Q_PROPERTY(QString id READ getId)
    Q_PROPERTY(QString name READ getName)
    Q_PROPERTY(QString type READ getType)
    Q_PROPERTY(QString url READ getUrl)
    Q_PROPERTY(QString icon READ getIcon)
    Q_PROPERTY(QString present READ getPresent NOTIFY presentChanged)
    Q_PROPERTY(int progress READ getProgress NOTIFY progressChanged)
    Q_PROPERTY(bool started READ getStarted NOTIFY startedChanged)
    Q_PROPERTY(bool complete READ getComplete NOTIFY completeChanged)
    Q_PROPERTY(QString backlog READ getBacklog)

public:
    SyncEvoSession();
    SyncEvoSession(SyncEvo* sync, QDBusObjectPath path);
    ~SyncEvoSession();
    void init();
    QDBusObjectPath getPath() const;
    QString getStatus() const;
    uint getError() const;
    QString getId() const;
    const QStringMultiMap& getConfig() const;
    Q_INVOKABLE QString getSyncConfig(const QString& key) const;
    Q_INVOKABLE QStringList getStoreList() const;
    Q_INVOKABLE QString getStoreConfig(const QString& source, const QString& key) const;
    QString getName() const;
    QString getType() const;
    QString getUrl() const;
    QString getIcon() const;
    QString getPresent() const;
    int getProgress() const;
    bool getStarted() const;
    bool getComplete() const;
    QString getBacklog() const;

    Q_INVOKABLE SyncEvoDatabaseModel* getDatabaseModel(const QString& source);

    Q_INVOKABLE void startSync(const QString& mode = QString());
    Q_INVOKABLE void suspendSync();
    Q_INVOKABLE void abortSync();

    Q_INVOKABLE void deleteConfig();
    void setConfig(const QStringMultiMap& config);
    void mergeConfig(const QStringMultiMap& config);
    Q_INVOKABLE void setSyncConfig(const QString& key, const QString& value);
    Q_INVOKABLE void setStoreConfig(const QString& source, const QString& key, const QString& value);
    Q_INVOKABLE void commitConfig();

    bool getAttached() const;
    void sessionStarted();
    void sessionEnded();
    void attached(const QString& id, bool nosync, bool tmp);
    bool attach();
    void detach();
    void release();
    void releaseWhenDone();

    void LogOutput(const QDBusObjectPath &path, const QString &level, const QString &output, const QString &procname);

public Q_SLOTS:
    void ProgressChanged(int progress, const QSyncProgressMap &sources);
    void StatusChanged(const QString &status, uint error, const QSyncStatusMap &sources);

    void peerPresentChanged(const QString& present);

Q_SIGNALS:
    void statusChanged(const QString& status);
    void presentChanged(const QString& present);
    void progressChanged(int progress);
    void startedChanged(bool started);
    void completeChanged(bool complete);

    void logOutput(const QString &output);

private:
    org::syncevolution::Session* m_session;
    QPointer<SyncEvo> m_sync;
    QDBusObjectPath m_path;
    bool m_has_id, m_has_config, m_has_status, m_has_progress;
    bool m_tmp, m_attached, m_nosync;
    bool m_ready, m_started, m_complete;
    QString m_status;
    QString m_id;
    QPointer<SyncEvoPeer> m_peer;
    SyncEvoConfig m_curConfig, m_setConfig;
    QString m_present;
    QString m_syncMode;
    uint m_error;

    bool m_release, m_commitConfig, m_clearConfig;
    SyncEvoConfig m_newConfig;

    int m_progress;
    QString m_backlog;

    void doStartSync();
    void doSetConfig();

    void setError(const QDBusError& error);
    void setStatus(const QString& status, uint error);
    void setProgress(int progress);
    void setReady();
    void setStarted();
    void setComplete();

    void loaded_id();
    void load_id();
    void load_config();
    void load_status();
    void load_progress();
    void load();
};

class SyncEvoSessionModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ItemRole {
        StatusRole = Qt::UserRole + 1,
        IdRole,
        NameRole,
        TypeRole,
        UrlRole,
        IconRole,
        PresentRole
    };

    SyncEvoSessionModel(SyncEvo* sync);
    ~SyncEvoSessionModel();
    void init();
    QHash<int, QByteArray> roleNames() const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    SyncEvoSession* findSession(const QDBusObjectPath& path) const;
    SyncEvoSession* findSession(const QString& id) const;
    SyncEvoSession* getSession(const QDBusObjectPath& path);
    void releaseSession(SyncEvoSession *session);
    void sessionChanged(const QDBusObjectPath& path, bool started);

private:
    QPointer<SyncEvo> m_sync;
    QList<SyncEvoSession*> m_sessions;
    bool m_ready;

    SyncEvoSession* createSession(const QDBusObjectPath& path);
    void deleteSession(SyncEvoSession* session);
    void initSessionList();
    void freeSessionList();
};

class SyncEvoReportModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ItemRole {
        DirRole = Qt::UserRole + 1,
        PeerIdRole,
        StatusRole,
        ErrorRole,
        StartRole,
        EndRole
    };

    SyncEvoReportModel(SyncEvo* sync, QDBusPendingReply<QArrayOfStringMap> reply);
    ~SyncEvoReportModel();
    QHash<int, QByteArray> roleNames() const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;

private:
    QPointer<SyncEvo> m_sync;
    QString m_peerId;
    QArrayOfStringMap m_reports;
};

class SyncEvoInfoRequest : public QObject
{
    Q_OBJECT

    Q_PROPERTY(const QStringMap& parameters READ getParameters)

public:
    SyncEvoInfoRequest();
    SyncEvoInfoRequest(SyncEvo* sync, const QString& id, const QStringMap& params);
    ~SyncEvoInfoRequest();

    void setWaiting();
    const QStringMap& getParameters() const;
    Q_INVOKABLE QString getParameter(const QString& key) const;
    Q_INVOKABLE void setResponse(const QString& key, const QString& value);
    Q_INVOKABLE void sendResponse();

private:
    QPointer<SyncEvo> m_sync;
    QString m_id;
    QStringMap m_params;
    QStringMap m_response;
    bool m_waiting;
};

class SyncEvo : public QObject
{
    Q_OBJECT

    Q_PROPERTY(SyncEvoPeerModel* peerModel READ getPeerModel)
    Q_PROPERTY(SyncEvoPeerModel* templateModel READ getTemplateModel)
    Q_PROPERTY(SyncEvoSessionModel* sessionModel READ getSessionModel)

public:
    SyncEvo();
    ~SyncEvo();
    QStringList getConfigs(bool getTemplates);
    QStringMultiMap getConfig(const QString& id, bool getTemplate);
    QString getPresent(const QString& id);
    SyncEvoPeer* createPeer(const QString& id, const QString& peerName);
    QList<QDBusObjectPath> getSessions();
    SyncEvoSession* startSession(const QString& id);
    SyncEvoSession* startSessionNoSync(const QString& id);
    SyncEvoSession* startSessionTempConfig();
    SyncEvoDatabaseModel* getDatabaseModel(const QString& id, const QString& source);
    void infoWorking(const QString& id);
    void infoResponse(const QString& id, const QStringMap& response);
    QString fixId(const QString& id);

public Q_SLOTS:
    void ConfigChanged();
    void InfoRequest(const QString &id, const QDBusObjectPath &path, const QString &state, const QString &handler, const QString &type, const QStringMap &parameters);
    void LogOutput(const QDBusObjectPath &path, const QString &level, const QString &output, const QString &procname);
    void Presence(const QString &server, const QString &status, const QString &transport);
    void SessionChanged(const QDBusObjectPath &session, bool started);
    void TemplatesChanged();

public:
    /***** QML properties *****/

    SyncEvoPeerModel* getPeerModel();
    SyncEvoPeerModel* getTemplateModel();
    SyncEvoSessionModel* getSessionModel();

    /***** QML methods *****/

    Q_INVOKABLE SyncEvoPeer* getPeer(const QString& id);
    Q_INVOKABLE SyncEvoPeer* getTemplate(const QString& tid);
    Q_INVOKABLE SyncEvoSession* openSession(const QString& id);
    Q_INVOKABLE void closeSession(SyncEvoSession *session);

    Q_INVOKABLE bool existsPeer(const QString& id);
    Q_INVOKABLE SyncEvoPeer* createPeer(const QString& id, const QString& peerName, const QString& tid);
    Q_INVOKABLE void deletePeer(const QString& id);

    Q_INVOKABLE SyncEvoReportModel* getReportModel(const QString& id);

Q_SIGNALS:
    void passwordRequest(SyncEvoInfoRequest* request);

private:
    org::syncevolution::Server m_server;
    SyncEvoPeerModel m_peers;
    SyncEvoPeerModel m_templates;
    SyncEvoSessionModel m_sessions;
    QMap<QString, SyncEvoInfoRequest*> m_infoRequests;
    unsigned m_tmpCount;
};

#endif // SYNCUI_H
