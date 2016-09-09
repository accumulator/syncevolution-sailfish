#include <QtQuick>

#include "sync-ui.h"

#include <sailfishapp.h>

#define SYNCEVO_DBUS_SERVICE "org.syncevolution"
#define SYNCEVO_DBUS_PATH "/org/syncevolution/Server"

#define SYNCEVO_QML_PATH "org.syncevolution.qml"

class SyncEvo;

static QPointer<QQmlEngine> engine;

static void setCppOwnership(QObject *obj)
{
    engine->setObjectOwnership(obj, QQmlEngine::CppOwnership);
}

class StdNames
{
public:
    StdNames()
    {
        m_names["addressbook"] = QT_TRANSLATE_NOOP("Source", "Contacts");
        m_names["calendar+todo"] =  QT_TRANSLATE_NOOP("Source", "Calendar/Tasks");
        m_names["calendar"] =  QT_TRANSLATE_NOOP("Source", "Calendar");
        m_names["todo"] =  QT_TRANSLATE_NOOP("Source", "Tasks");
        m_names["memo"] =  QT_TRANSLATE_NOOP("Source", "Notes");

        m_order << "addressbook"
                << "calendar+todo"
                << "calendar"
                << "todo"
                << "memo";
    }

    QString getName(const QString& id) const
    {
        QStringMap::const_iterator it = m_names.find(id);
        if (it != m_names.end())
        {
            return qApp->translate("Source", it->toUtf8());
        }
        return id;
    }

    void sort(QList<QString>& names) const
    {
        for (int idx = 0, dst = 0;
             idx < m_order.size() && dst < names.size();
             idx++)
        {
            for (int src = dst; src < names.size(); src++)
            {
                if (names[src] == m_order[idx]) {
                    if (src > dst) {
                        names[dst].swap(names[src]);
                    }
                    dst++;
                    break;
                }
            }
        }
    }

private:
    QStringMap m_names;
    QList<QString> m_order;
};

static StdNames stdNames;

/***** SyncEvoConfig *****/

SyncEvoConfig::SyncEvoConfig() :
    m_targetConfig(false)
{
}

void SyncEvoConfig::setTargetConfig(bool targetConfig)
{
    m_targetConfig = targetConfig;
}

bool SyncEvoConfig::isEmpty() const
{
    return m_local.isEmpty() && m_remote.isEmpty();
}

const QStringMultiMap& SyncEvoConfig::getConfig() const
{
    return m_local;
}

const QStringMultiMap& SyncEvoConfig::getRemoteConfig() const
{
    return m_remote;
}

QString SyncEvoConfig::getSyncConfig(const QString& key) const
{
    return m_local[""].value(key);
}

QString SyncEvoConfig::getServerConfig(const QString& key) const
{
    return (m_targetConfig ? m_remote : m_local)[""].value(key);
}

QStringList SyncEvoConfig::getStoreList() const
{
    QStringList stores;
    foreach(const QString& key, m_local.keys())
    {
        if (!key.startsWith("source/")) continue;
        stores.append(key.mid(7));
    }
    return stores;
}

QStringList SyncEvoConfig::getSourceList() const
{
    QStringList sources;
    foreach(const QString& key, (m_targetConfig ? m_remote : m_local).keys())
    {
        if (!key.startsWith("source/")) continue;
        sources.append(key.mid(7));
    }
    return sources;
}

QString SyncEvoConfig::getStoreConfig(const QString& source, const QString& key) const
{
    return m_local["source/" + source].value(key);
}

QString SyncEvoConfig::getSourceConfig(const QString& source, const QString& key) const
{
    return (m_targetConfig ? m_remote : m_local)["source/" + source].value(key);
}

QString SyncEvoConfig::getName() const
{
    return getSyncConfig("PeerName");
}

QString SyncEvoConfig::getType() const
{
    return getSyncConfig("peerType");
}

bool SyncEvoConfig::hasTargetConfig() const
{
    QString syncUrl = getSyncConfig("syncURL");
    return syncUrl.startsWith("local://@");
}

QString SyncEvoConfig::getTargetContext() const
{
    QString syncUrl = getSyncConfig("syncURL");
    if (syncUrl.startsWith("local://@")) {
        return syncUrl.mid(9); // 9 = length of "local://@"
    }
    return QString();
}

void SyncEvoConfig::clear()
{
    m_local.clear();
    m_remote.clear();
}

void SyncEvoConfig::setConfig(const QStringMultiMap& config)
{
    m_local = config;
}

void SyncEvoConfig::mergeConfig(const QStringMultiMap& config)
{
    mergeMap(m_local, config);
}

void SyncEvoConfig::setRemoteConfig(const QStringMultiMap& config)
{
    m_remote = config;
}

void SyncEvoConfig::mergeRemoteConfig(const QStringMultiMap& config)
{
    mergeMap(m_remote, config);
}

void SyncEvoConfig::setSyncConfig(const QString& key, const QString& value)
{
    if (value.isEmpty()) {
        m_local[""].remove(key);
    } else {
        m_local[""][key] = value;
    }
}

void SyncEvoConfig::setServerConfig(const QString& key, const QString& value)
{
    if (value.isEmpty()) {
        (m_targetConfig ? m_remote : m_local)[""].remove(key);
    } else {
        (m_targetConfig ? m_remote : m_local)[""][key] = value;
    }
}

void SyncEvoConfig::setStoreConfig(const QString& source, const QString& key, const QString& value)
{
    if (value.isEmpty()) {
        m_local["source/" + source].remove(key);
    } else {
        m_local["source/" + source][key] = value;
    }
}

void SyncEvoConfig::setSourceConfig(const QString& source, const QString& key, const QString& value)
{
    if (value.isEmpty()) {
        (m_targetConfig ? m_remote : m_local)["source/" + source].remove(key);
    } else {
        (m_targetConfig ? m_remote : m_local)["source/" + source][key] = value;
    }
}

void SyncEvoConfig::setConfig(const SyncEvoConfig& config)
{
    setConfig(config.getConfig());
    setRemoteConfig(config.getRemoteConfig());
}

void SyncEvoConfig::mergeConfig(const SyncEvoConfig& config)
{
    mergeConfig(config.getConfig());
    mergeRemoteConfig(config.getRemoteConfig());
}

void SyncEvoConfig::mergeMap(QStringMultiMap& dst, const QStringMultiMap& src)
{
    foreach (const QString& key1, src.keys())
    {
        QStringMap& dstval = dst[key1];
        const QStringMap& srcval = src[key1];
        foreach (const QString& key2, srcval.keys())
        {
            dstval[key2] = srcval[key2];
        }
    }
}

/***** SyncEvoStoreModel *****/

SyncEvoStoreModel::SyncEvoStoreModel(SyncEvoPeer* peer) :
    m_peer(peer)
{
    setCppOwnership(this);
}

SyncEvoStoreModel::~SyncEvoStoreModel()
{
}

void SyncEvoStoreModel::init()
{
    m_stores.clear();
    foreach (const QString& id, m_peer->getStoreList())
    {
        QString backend = m_peer->getSourceConfig(id, "backend");
        m_stores.append(id);
    }
    stdNames.sort(m_stores);
}

QHash<int, QByteArray> SyncEvoStoreModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "storeId";
    roles[NameRole] = "storeName";
    return roles;
}

int SyncEvoStoreModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return m_stores.size();
}

QVariant SyncEvoStoreModel::data(const QModelIndex& index, int role) const
{
    const QString& id = m_stores[index.row()];
    switch (role)
    {
    case IdRole:
        return id;
    case NameRole:
        return stdNames.getName(id);
    default:
        return QVariant();
    }
}

void SyncEvoStoreModel::configChanged()
{
    // Might detect the addition of new stores here.
    // That's rarely needed, though, so probably not needed.
}

/***** SyncEvoSourceModel *****/

SyncEvoSourceModel::SyncEvoSourceModel(SyncEvoPeer* peer) :
    m_peer(peer)
{
    setCppOwnership(this);
}

SyncEvoSourceModel::~SyncEvoSourceModel()
{
}

void SyncEvoSourceModel::init()
{
    // find virtual sources
    QSet<QString> virtuals;
    foreach (const QString& id, m_peer->getSourceList())
    {
        QString backend = m_peer->getSourceConfig(id, "backend");
        if (backend == "virtual") {
            virtuals.unite(QSet<QString>::fromList(m_peer->getSourceConfig(id, "database").split(',')));
        }
    }

    // add all real sources to model
    m_sources.clear();
    foreach (const QString& id, m_peer->getSourceList())
    {
        if (!virtuals.contains(id)) {
            m_sources.append(id);
        }
    }

    stdNames.sort(m_sources);
}

QHash<int, QByteArray> SyncEvoSourceModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "sourceId";
    roles[NameRole] = "sourceName";
    return roles;
}

int SyncEvoSourceModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return m_sources.size();
}

QVariant SyncEvoSourceModel::data(const QModelIndex& index, int role) const
{
    const QString& id = m_sources[index.row()];
    switch (role)
    {
    case IdRole:
        return id;
    case NameRole:
        return stdNames.getName(id);
    default:
        return QVariant();
    }
}

void SyncEvoSourceModel::configChanged()
{
    // Might detect the addition of new sources here.
    // That's rarely needed, though, so probably not needed.
}

/***** SyncEvoDatabaseModel *****/

SyncEvoDatabaseModel::SyncEvoDatabaseModel(SyncEvo* sync, QDBusPendingReply<QArrayOfDatabases> reply) :
    m_sync(sync), m_valid(false)
{
    // we will treat this model as transient
    // (i.e. don't call setCppOwnership)

    // Blocking wait for now.
    // Need to make it non-blocking later.
    reply.waitForFinished();
    if (reply.isValid()) {
        m_databases = reply.value();
        m_valid = true;
    } else {
        qDebug() << "D-Bus error" << reply.error().message();
    }
}

SyncEvoDatabaseModel::~SyncEvoDatabaseModel()
{
}

QHash<int, QByteArray> SyncEvoDatabaseModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "databaseId";
    roles[NameRole] = "databaseName";
    roles[DefaultRole] = "databaseDefault";
    roles[ValidRole] = "databaseValid";
    return roles;
}

int SyncEvoDatabaseModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return m_valid ? m_databases.size() : 1;
}

QVariant SyncEvoDatabaseModel::data(const QModelIndex& index, int role) const
{
    if (m_valid) {
        const SyncDatabase& db = m_databases[index.row()];
        switch (role)
        {
        case IdRole:
            return db.source;
        case NameRole:
            return db.name;
        case DefaultRole:
            return db.flag;
        case ValidRole:
            return true;
        default:
            return QVariant();
        }
    }
    else {
        switch (role)
        {
        case IdRole:
            return QString();
        case NameRole:
            return qApp->translate("Source", "Unavailable");
        case DefaultRole:
            return true;
        case ValidRole:
            return false;
        default:
            return QVariant();
        }
    }
}

int SyncEvoDatabaseModel::indexOf(const QString& id) const
{
    if (id.isEmpty())
    {
        if (!m_valid) {
            return 0;
        }
        // Search for default
        for (int idx = 0; idx < m_databases.size(); idx++)
        {
            if (m_databases[idx].flag) {
                return idx;
            }
        }
    }
    else {
        // Search by ID
        for (int idx = 0; idx < m_databases.size(); idx++)
        {
            if (m_databases[idx].source == id) {
                return idx;
            }
        }
        // Search by name
        for (int idx = 0; idx < m_databases.size(); idx++)
        {
            if (m_databases[idx].name == id) {
                return idx;
            }
        }
    }
    // Fail
    return -1;
}

QString SyncEvoDatabaseModel::idOfIndex(int idx) const
{
    if (idx < 0 || idx >= m_databases.size()) {
        return QString();
    }
    return m_databases[idx].source;
}

bool SyncEvoDatabaseModel::isValid() const
{
    return m_valid;
}

/***** SyncEvoPeer *****/

SyncEvoPeer::SyncEvoPeer() :
    m_isTemplate(false), m_exists(false),
    m_has_config(false), m_has_present(false),
    m_stores(this), m_sources(this),
    m_clearConfig(false)
{
    // placeholder created by QML
}

SyncEvoPeer::SyncEvoPeer(SyncEvo* sync, QString id, bool isTemplate, bool exists) :
    m_sync(sync), m_id(id),
    m_isTemplate(isTemplate), m_exists(exists),
    m_has_config(false), m_has_present(false),
    m_stores(this), m_sources(this),
    m_clearConfig(false)
{
    setCppOwnership(this);
}

SyncEvoPeer::~SyncEvoPeer()
{
}

void SyncEvoPeer::init()
{
    if (!m_exists) {
        return;
    }

    if (!m_has_config) {
        load_config();
        m_has_config = true;
        m_stores.init();
        m_sources.init();
    }

    if (!m_has_present) {
        if (!m_isTemplate) {
            QString present = m_sync->getPresent(m_id);
            // check that we didn't get a Presence
            // signal while calling getPresent...
            if (!m_has_present) {
                setPresent(present);
            }
        } else {
            setPresent("template");
        }
    }
}

QString SyncEvoPeer::getId() const
{
    return m_id;
}

bool SyncEvoPeer::exists() const
{
    return m_exists;
}

void SyncEvoPeer::setExists()
{
    if (!m_exists) {
        m_exists = true;
    }
}

const QStringMultiMap& SyncEvoPeer::getConfig() const
{
    return m_setConfig.getConfig();
}

const QStringMultiMap& SyncEvoPeer::getRemoteConfig() const
{
    return m_setConfig.getRemoteConfig();
}

QString SyncEvoPeer::getSyncConfig(const QString& key) const
{
    return m_setConfig.getSyncConfig(key);
}

QString SyncEvoPeer::getServerConfig(const QString& key) const
{
    return m_setConfig.getServerConfig(key);
}

QStringList SyncEvoPeer::getStoreList() const
{
    return m_setConfig.getStoreList();
}

QStringList SyncEvoPeer::getSourceList() const
{
    return m_setConfig.getSourceList();
}

QString SyncEvoPeer::getStoreConfig(const QString& source, const QString& key) const
{
    return m_setConfig.getStoreConfig(source, key);
}

QString SyncEvoPeer::getSourceConfig(const QString& source, const QString& key) const
{
    return m_setConfig.getSourceConfig(source, key);
}

QString SyncEvoPeer::getSourceUri(const QString& source) const
{
    if (hasTargetConfig()) {
        return m_setConfig.getSourceConfig(source, "database");
    } else {
        return m_setConfig.getStoreConfig(source, "uri");
    }
}

QString SyncEvoPeer::getName() const
{
    QString name = m_setConfig.getName();

    // If there's no PeerName, fall back to the config name.
    if (name.isEmpty())
    {
        name = m_id;
    }

    // Some template names have underscores
    // where spaces would look nicer.
    if (m_isTemplate)
    {
        name = name.replace('_', ' ');
    }

    return name;
}

QString SyncEvoPeer::getType() const
{
    return m_setConfig.getType();
}

QString SyncEvoPeer::getUrl() const
{
    return getSyncConfig("WebURL");
}

QString SyncEvoPeer::getIcon() const
{
    return getSyncConfig("IconURI");
}

QString SyncEvoPeer::getPresent() const
{
    return m_present;
}

SyncEvoStoreModel* SyncEvoPeer::getStoreModel()
{
    return &m_stores;
}

SyncEvoSourceModel* SyncEvoPeer::getSourceModel()
{
    return &m_sources;
}

void SyncEvoPeer::setPresent(const QString& present)
{
    m_has_present = true;
    if (present != m_present) {
        m_present = present;
        emit presentChanged(present);
    }
}

void SyncEvoPeer::configChanged()
{
    if (m_has_config) {
        // Reload configuration, in case it changed
        load_config();
    }
}

bool SyncEvoPeer::useTargetConfig() const
{
    QString type = getType();
    if (type.isEmpty() || type.startsWith("SyncML")) {
        // SyncML needs no special handling
        return false;
    }
    // WebDAV etc needs extra magic
    return true;
}

bool SyncEvoPeer::isTargetConfig() const
{
    if (m_isTemplate) return false;

    // When a setup (like CalDAV sync) needs two configs
    // (one local and one remote), the config for the
    // remote side is, by convention, called "target-config".
    // Assume that this convention applies, since then
    // we don't have to load the configuration to check it.
    return m_id.startsWith("target-config@");
}

bool SyncEvoPeer::hasTargetConfig() const
{
    if (m_isTemplate) return useTargetConfig();

    return m_setConfig.hasTargetConfig();
}

QString SyncEvoPeer::getTargetContext() const
{
    if (m_isTemplate) return getDefaultTargetContext();

    return m_setConfig.getTargetContext();
}

QString SyncEvoPeer::getTargetConfig() const
{
    return "target-config@" + getTargetContext();
}

QString SyncEvoPeer::getDefaultTargetContext() const
{
    int idx = m_id.indexOf('@');
    return m_id.mid(0, idx) + "-target";
}

QString SyncEvoPeer::getDefaultTargetConfig() const
{
    return "target-config@" + getDefaultTargetContext();
}

void SyncEvoPeer::createFromTemplate(SyncEvoPeer* tmpl)
{
    QString peerName = getSyncConfig("PeerName");
    if (!tmpl->useTargetConfig()) {
        // nice and simple SyncML configuration
        setConfig(tmpl->getConfig());
        // the setConfig probably overwrote PeerName, so set it again
        setSyncConfig("PeerName", peerName);
    } else {
        // non-SyncML configuration chosen, must configure two
        // SyncML configurations talking to each other

        // Create target config to use
        m_sync->createPeer(getDefaultTargetConfig(), peerName);

        // Set up the local side
        setSyncConfig("syncURL", "local://@" + getDefaultTargetContext());
        setSyncConfig("PeerIsClient", "1");
        // copy a few neat things from the template...
        setSyncConfig("WebURL", tmpl->getSyncConfig("WebURL"));
        setSyncConfig("IconURI", tmpl->getSyncConfig("IconURI"));
        setSyncConfig("ConsumerReady", tmpl->getSyncConfig("ConsumerReady"));
        // create the sources
        foreach (const QString& source, tmpl->getStoreList()) {
            // by default, use sync mode from template
            QString mode = tmpl->getStoreConfig(source, "sync");
            setStoreConfig(source, "sync", mode);
            // must specify a backend, but using the source as
            // backend usually works for the standard sources
            setStoreConfig(source, "backend", source);
        }

        // Set up the remote side
        setRemoteConfig(tmpl->getConfig());
        setServerConfig("ConsumerReady", "0"); // hides config in other GUIs
        // the setRemoteConfig probably overwrote PeerName, so set it again
        setServerConfig("PeerName", peerName);
    }
}

void SyncEvoPeer::deleteConfig()
{
    m_clearConfig = true;
    m_newConfig.clear();
    m_setConfig.clear();
}

void SyncEvoPeer::setConfig(const QStringMultiMap& config)
{
    m_newConfig.setConfig(config);
    if (m_clearConfig) {
        m_setConfig.setConfig(config);
    } else {
        m_setConfig.setConfig(m_curConfig.getConfig());
        m_setConfig.mergeConfig(config);
    }
}

void SyncEvoPeer::mergeConfig(const QStringMultiMap& config)
{
    m_newConfig.mergeConfig(config);
    m_setConfig.mergeConfig(config);
}

void SyncEvoPeer::setRemoteConfig(const QStringMultiMap& config)
{
    m_newConfig.setRemoteConfig(config);
    if (m_clearConfig) {
        m_setConfig.setRemoteConfig(config);
    } else {
        m_setConfig.setRemoteConfig(m_curConfig.getRemoteConfig());
        m_setConfig.mergeRemoteConfig(config);
    }
}

void SyncEvoPeer::mergeRemoteConfig(const QStringMultiMap& config)
{
    m_newConfig.mergeRemoteConfig(config);
    m_setConfig.mergeRemoteConfig(config);
}

void SyncEvoPeer::setSyncConfig(const QString& key, const QString& value)
{
    m_newConfig.setSyncConfig(key, value);
    m_setConfig.setSyncConfig(key, value);
    if (!m_isTemplate && key == "syncURL" && m_newConfig.hasTargetConfig()) {
        m_newConfig.setTargetConfig(true);
        m_setConfig.setTargetConfig(true);
    }
}

void SyncEvoPeer::setServerConfig(const QString& key, const QString& value)
{
    m_newConfig.setServerConfig(key, value);
    m_setConfig.setServerConfig(key, value);
}

void SyncEvoPeer::setStoreConfig(const QString& source, const QString& key, const QString& value)
{
    m_newConfig.setStoreConfig(source, key, value);
    m_setConfig.setStoreConfig(source, key, value);
}

void SyncEvoPeer::setSourceConfig(const QString& source, const QString& key, const QString& value)
{
    m_newConfig.setSourceConfig(source, key, value);
    m_setConfig.setSourceConfig(source, key, value);
}

void SyncEvoPeer::setSourceUri(const QString& source, const QString& uri)
{
    if (hasTargetConfig()) {
        m_newConfig.setSourceConfig(source, "database", uri);
        m_setConfig.setSourceConfig(source, "database", uri);
    } else {
        m_newConfig.setStoreConfig(source, "uri", uri);
        m_setConfig.setStoreConfig(source, "uri", uri);
    }
}

void SyncEvoPeer::commitConfig()
{
    if (!m_clearConfig && m_newConfig.isEmpty()) {
        // no changes to commit
        return;
    }

    if (!m_isTemplate && hasTargetConfig()) {
        SyncEvoPeer* target = m_sync->getPeer(getTargetConfig());
        if (target) {
            if (m_clearConfig) {
                target->deleteConfig();
            }
            target->setConfig(m_newConfig.getRemoteConfig());
            target->commitConfig();
        }
    }

    SyncEvoSession* session = m_sync->startSessionNoSync(m_id);

    if (m_clearConfig) {
        session->deleteConfig();
    }
    session->setConfig(m_newConfig.getConfig());
    session->commitConfig();
    session->releaseWhenDone();

    m_clearConfig = false;
    m_newConfig.clear();
}

SyncEvoSession* SyncEvoPeer::getTempSyncSession()
{
    SyncEvoSession* session = m_sync->startSessionTempConfig();
    if (m_clearConfig) {
        session->deleteConfig();
    }
    if (m_isTemplate && useTargetConfig()) {
        // Will only bother setting the config options
        // necessary to get databases and such.
        session->setSyncConfig("PeerIsClient", "1");
        foreach (const QString& source, getStoreList()) {
            // use sync mode from template
            QString mode = getStoreConfig(source, "sync");
            session->setStoreConfig(source, "sync", mode);
            // must specify a backend, but using the source as
            // backend usually works for the standard sources
            session->setStoreConfig(source, "backend", source);
        }
    } else {
        session->setConfig(m_setConfig.getConfig());
    }
    session->commitConfig();
    return session;
}

SyncEvoSession* SyncEvoPeer::getTempServerSession()
{
    SyncEvoSession* session = m_sync->startSessionTempConfig();
    if (m_clearConfig) {
        session->deleteConfig();
    }
    if (!m_isTemplate && hasTargetConfig()) {
        session->setConfig(m_setConfig.getRemoteConfig());
    } else {
        session->setConfig(m_setConfig.getConfig());
    }
    session->commitConfig();
    return session;
}

void SyncEvoPeer::load_config()
{
    m_curConfig.setConfig(m_sync->getConfig(m_id, m_isTemplate));
    if (!m_clearConfig) {
        m_setConfig.setConfig(m_curConfig.getConfig());
        m_setConfig.mergeConfig(m_newConfig.getConfig());
    }
    if (!m_isTemplate && hasTargetConfig()) {
        m_curConfig.setTargetConfig(true);
        m_setConfig.setTargetConfig(true);
        m_newConfig.setTargetConfig(true);
        // Hopefully the target config is already loaded at this point.
        SyncEvoPeer* target = m_sync->getPeer(getTargetConfig());
        if (target) {
            m_curConfig.setRemoteConfig(target->getConfig());
            if (!m_clearConfig) {
                m_setConfig.setRemoteConfig(m_curConfig.getRemoteConfig());
                m_setConfig.mergeRemoteConfig(m_newConfig.getRemoteConfig());
            }
        }
    }
}

/***** SyncEvoPeerModel *****/

SyncEvoPeerModel::SyncEvoPeerModel(SyncEvo* sync, bool areTemplates) :
    m_sync(sync),
    m_areTemplates(areTemplates),
    m_ready(false),
    m_configChanged(false)
{
    setCppOwnership(this);
}

SyncEvoPeerModel::~SyncEvoPeerModel()
{
    freePeerList();
}

void SyncEvoPeerModel::init()
{
    initPeerList();
}

QHash<int, QByteArray> SyncEvoPeerModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "peerId";
    roles[NameRole] = "peerName";
    roles[TypeRole] = "peerType";
    roles[UrlRole] = "peerUrl";
    roles[IconRole] = "peerIcon";
    roles[PresentRole] = "peerPresent";
    return roles;
}

int SyncEvoPeerModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return m_peers.size();
}

QVariant SyncEvoPeerModel::data(const QModelIndex& index, int role) const
{
    SyncEvoPeer* peer = m_peers[index.row()];
    switch (role)
    {
    case IdRole:
        return peer->getId();
    case NameRole:
        return peer->getName();
    case TypeRole:
        return peer->getType();
    case UrlRole:
        return peer->getUrl();
    case IconRole:
        return peer->getIcon();
    case PresentRole:
        return peer->getPresent();
    default:
        return QVariant();
    }
}

SyncEvoPeer* SyncEvoPeerModel::findPeer(const QString& id)
{
    int n;
    for (n = 0; n < m_peers.size(); n++)
    {
        SyncEvoPeer* peer = m_peers[n];
        if (peer->getId() == id)
        {
            return peer;
        }
    }
    for (n = 0; n < m_targets.size(); n++)
    {
        SyncEvoPeer* target = m_targets[n];
        if (target->getId() == id)
        {
            return target;
        }
    }
    return NULL;
}

SyncEvoPeer* SyncEvoPeerModel::createPeer(const QString& id, const QString& peerName)
{
    SyncEvoPeer* peer = new SyncEvoPeer(m_sync, id, m_areTemplates, false);
    peer->deleteConfig();
    peer->setSyncConfig("PeerName", peerName);
    if (peer->isTargetConfig()) {
        m_targets.append(peer);
    } else {
        beginInsertRows(QModelIndex(), m_peers.size(), m_peers.size());
        m_peers.append(peer);
        endInsertRows();
    }
    return peer;
}

void SyncEvoPeerModel::deletePeer(SyncEvoPeer *peer)
{
    peer->deleteLater();
    m_peers.removeOne(peer);
}

void SyncEvoPeerModel::configChanged()
{
    if (m_configChanged) {
        return;
    }
    m_configChanged = true;
    if (m_ready) {
        updatePeerList();
    }
}

void SyncEvoPeerModel::presenceChanged(const QString &id, const QString& status)
{
    SyncEvoPeer *peer = findPeer(id);
    if (peer) {
        peer->setPresent(status);
    }
}

void SyncEvoPeerModel::initPeerList()
{
    // Create objects for all peer IDs first, and
    // load configs and presences for them afterwards,
    // so that if we get Presence signals while loading,
    // they get handled correctly.
    QStringList ids = m_sync->getConfigs(m_areTemplates);
    foreach(const QString& id, ids) {
        SyncEvoPeer* peer = new SyncEvoPeer(m_sync, id, m_areTemplates, true);
        if (peer->isTargetConfig()) {
            // Store target configs separately, so the user don't have to see them.
            m_targets.append(peer);
        } else {
            m_peers.append(peer);
        }
    }

    // Now load all unknown configs and presences.
    processPeerList();
}

void SyncEvoPeerModel::processPeerList()
{
    // Loading target configs before the peer configs
    // allows the peers to easily access their targets
    // when initializing their own configs.
    foreach(SyncEvoPeer *target, m_targets) {
        target->init();
    }
    foreach(SyncEvoPeer *peer, m_peers) {
        peer->init();
    }

    m_ready = true;

    // If we got a ConfigChange while loading,
    // handle it now.
    if (m_configChanged) {
        updatePeerList();
    }
}

static bool isInList(const QString& id, const QStringList& ids)
{
    for (int n = 0; n < ids.size(); n++)
    {
        if (ids[n] == id)
        {
            return true;
        }
    }
    return false;
}

void SyncEvoPeerModel::updatePeerList()
{
    m_ready = false; // defer recursive calls
    m_configChanged = false;

    QStringList ids = m_sync->getConfigs(m_areTemplates);
    int n;

    // First, look for deleted configs.
    // (This may be somewhat inefficient, possibly we
    // could take advantage of SyncEvolution giving
    // us the target list in a predictable order.)
    n = 0;
    while (n < m_peers.size())
    {
        SyncEvoPeer* peer = m_peers[n];
        if (isInList(peer->getId(), ids)) {
            n++;
        }
        else {
            beginRemoveRows(QModelIndex(), n, n);
            peer->deleteLater();
            m_peers.removeAt(n);
            endRemoveRows();
        }
    }
    n = 0;
    while (n < m_targets.size())
    {
        SyncEvoPeer* target = m_targets[n];
        if (isInList(target->getId(), ids)) {
            n++;
        }
        else {
            target->deleteLater();
            m_targets.removeAt(n);
        }
    }

    // Next, look for changed configs.
    for (n = 0; n < m_targets.size(); n++)
    {
        SyncEvoPeer* target = m_targets[n];
        target->configChanged();
    }
    for (n = 0; n < m_peers.size(); n++)
    {
        SyncEvoPeer* peer = m_peers[n];
        // Remember old values of roles
        QString oldName = peer->getName();
        QString oldType = peer->getType();
        QString oldUrl  = peer->getUrl();
        QString oldIcon = peer->getIcon();
        // Present is not part of the config, don't check it

        peer->configChanged();

        QVector<int> roles;
        // Check if any of the roles have changed
        if (peer->getName() != oldName) roles << NameRole;
        if (peer->getType() != oldType) roles << TypeRole;
        if (peer->getUrl()  != oldUrl)  roles << UrlRole;
        if (peer->getIcon() != oldIcon) roles << IconRole;

        if (!roles.empty()) {
            QModelIndex midx = createIndex(n, 0);
            emit dataChanged(midx, midx, roles);
        }
    }

    // Finally, look for new configs.
    for (n = 0; n < ids.size(); n++)
    {
        SyncEvoPeer* peer = findPeer(ids[n]);
        if (peer) {
            peer->setExists();
            continue;
        }
        peer = new SyncEvoPeer(m_sync, ids[n], m_areTemplates, true);
        if (peer->isTargetConfig()) {
            m_targets.append(peer);
        } else {
            beginInsertRows(QModelIndex(), m_peers.size(), m_peers.size());
            m_peers.append(peer);
            endInsertRows();
        }
    }

    // Load all unknown configs and presences.
    processPeerList();
}

void SyncEvoPeerModel::freePeerList()
{
    qDeleteAll(m_targets);
    m_targets.clear();
    qDeleteAll(m_peers);
    m_peers.clear();
}

/***** SyncEvoSession *****/

SyncEvoSession::SyncEvoSession() :
    m_has_id(false), m_has_config(false), m_has_status(false), m_has_progress(true),
    m_tmp(false), m_attached(false), m_nosync(false),
    m_ready(false), m_started(true), m_complete(true),
    m_release(false), m_commitConfig(false), m_clearConfig(false),
    m_progress(100)
{
    // placeholder created by QML
}

SyncEvoSession::SyncEvoSession(SyncEvo* sync, QDBusObjectPath path) :
    m_sync(sync), m_path(path),
    m_has_id(false), m_has_config(false), m_has_status(false), m_has_progress(false),
    m_tmp(false), m_attached(false), m_nosync(false),
    m_ready(false), m_started(false), m_complete(false),
    m_release(false), m_commitConfig(false), m_clearConfig(false)
{
    setCppOwnership(this);
    m_session = new org::syncevolution::Session(SYNCEVO_DBUS_SERVICE, path.path(), QDBusConnection::sessionBus());
    connect(m_session, &org::syncevolution::Session::ProgressChanged, this, &SyncEvoSession::ProgressChanged);
    connect(m_session, &org::syncevolution::Session::StatusChanged,   this, &SyncEvoSession::StatusChanged);
}

SyncEvoSession::~SyncEvoSession()
{
    detach();
    delete m_session;
}

void SyncEvoSession::init()
{
    if (!m_has_id) {
        load_id();
    }
}

QDBusObjectPath SyncEvoSession::getPath() const
{
    return m_path;
}

QString SyncEvoSession::getStatus() const
{
    return m_status;
}

uint SyncEvoSession::getError() const
{
    return m_error;
}

QString SyncEvoSession::getId() const
{
    return m_id;
}

const QStringMultiMap& SyncEvoSession::getConfig() const
{
    return m_setConfig.getConfig();
}

QString SyncEvoSession::getSyncConfig(const QString& key) const
{
    return m_setConfig.getSyncConfig(key);
}

QStringList SyncEvoSession::getStoreList() const
{
    return m_setConfig.getStoreList();
}

QString SyncEvoSession::getStoreConfig(const QString& source, const QString& key) const
{
    return m_setConfig.getStoreConfig(source, key);
}

QString SyncEvoSession::getName() const
{
    QString name = m_setConfig.getName();

    // If there's no PeerName, fall back to the config name.
    if (name.isEmpty())
    {
        name = m_id;
    }

    return name;
}

QString SyncEvoSession::getType() const
{
    return m_setConfig.getType();
}

QString SyncEvoSession::getUrl() const
{
    return getSyncConfig("WebURL");
}

QString SyncEvoSession::getIcon() const
{
    return getSyncConfig("IconURI");
}

QString SyncEvoSession::getPresent() const
{
    return m_present;
}

int SyncEvoSession::getProgress() const
{
    return m_progress;
}

bool SyncEvoSession::getStarted() const
{
    return m_started;
}

bool SyncEvoSession::getComplete() const
{
    return m_complete;
}

QString SyncEvoSession::getBacklog() const
{
    return m_backlog;
}

SyncEvoDatabaseModel* SyncEvoSession::getDatabaseModel(const QString& source)
{
    return new SyncEvoDatabaseModel(m_sync, m_session->GetDatabases(source));
}

void SyncEvoSession::startSync(const QString& mode)
{
    if (!m_attached) {
        qDebug() << "tried to start sync while not attached";
        return;
    }
    if (m_started) {
        return;
    }
    m_syncMode = mode;
    if (m_ready) {
        // Start sync immediately.
        setStarted();
        doStartSync();
    } else {
        // Start sync once m_ready becomes true (from setReady).
        // Allow getStarted() to return true in the meantime.
        setStarted();
    }
}

void SyncEvoSession::suspendSync()
{
    QDBusPendingReply<> reply = m_session->Suspend();
    reply.waitForFinished();
}

void SyncEvoSession::abortSync()
{
    QDBusPendingReply<> reply = m_session->Abort();
    reply.waitForFinished();
}

void SyncEvoSession::deleteConfig()
{
    m_clearConfig = true;
    m_newConfig.clear();
    m_setConfig.clear();
}

void SyncEvoSession::setConfig(const QStringMultiMap& config)
{
    m_newConfig.setConfig(config);
    if (m_clearConfig) {
        m_setConfig.setConfig(config);
    } else {
        m_setConfig.setConfig(m_curConfig.getConfig());
        m_setConfig.mergeConfig(config);
    }
}

void SyncEvoSession::mergeConfig(const QStringMultiMap& config)
{
    m_newConfig.mergeConfig(config);
    m_setConfig.mergeConfig(config);
}

void SyncEvoSession::setSyncConfig(const QString& key, const QString& value)
{
    m_newConfig.setSyncConfig(key, value);
    m_setConfig.setSyncConfig(key, value);
}

void SyncEvoSession::setStoreConfig(const QString& source, const QString& key, const QString& value)
{
    m_newConfig.setStoreConfig(source, key, value);
    m_setConfig.setStoreConfig(source, key, value);
}

void SyncEvoSession::commitConfig()
{
    m_commitConfig = true;
    if (m_ready) {
        doSetConfig();
    }
}

bool SyncEvoSession::getAttached() const
{
    return m_attached;
}

void SyncEvoSession::sessionStarted()
{
    // probably no need to do anything here,
    // will rely on StatusChanged instead
}

void SyncEvoSession::sessionEnded()
{
    bool releasing = m_release;

    // just in case we didn't have time to get the status
    // before we received the SessionChange...
    setComplete();

    if (!m_attached && !releasing) {
        release();
    }
}

void SyncEvoSession::attached(const QString& id, bool nosync, bool tmp)
{
    if (!m_has_id) {
        m_id = id;
        loaded_id();
    }
    m_tmp = tmp;
    m_attached = true;
    m_nosync = nosync;
    load();
}

bool SyncEvoSession::attach()
{
    if (!m_attached) {
        QDBusPendingReply<> reply = m_session->Attach();
        reply.waitForFinished();
        if (reply.isValid()) {
            m_attached = true;
        }
    }
    load();
    return m_attached;
}

void SyncEvoSession::detach()
{
    if (m_attached) {
        QDBusPendingReply<> reply = m_session->Detach();
        reply.waitForFinished();
        if (reply.isValid()) {
            m_attached = false;
        }
    }
}

void SyncEvoSession::release()
{
    m_release = false;
    m_sync->closeSession(this);
}

void SyncEvoSession::releaseWhenDone()
{
    m_release = true;
    if (!m_attached || m_complete || !(m_started || m_commitConfig)) {
        release();
    }
}

void SyncEvoSession::LogOutput(const QDBusObjectPath &path, const QString &level, const QString &output, const QString &procname)
{
    Q_UNUSED(path)
    Q_UNUSED(level)
    Q_UNUSED(output)
    Q_UNUSED(procname)

    qDebug() << "Session" << level << procname << output;

    QString msg;
    if (level == "SHOW") {
        msg = output + "\n";
    } else if (procname.isEmpty()) {
        msg = QString("[%1] %2\n").arg(level, output);
    } else {
        msg = QString("[%1] %2: %3\n").arg(level, procname, output);
    }
    m_backlog.append(msg);
    emit logOutput(msg);
}

void SyncEvoSession::ProgressChanged(int progress, const QSyncProgressMap &sources)
{
    Q_UNUSED(sources)

    qDebug() << "received progress" << progress;

    if (!m_complete) {
        setProgress(progress);
    }
}

void SyncEvoSession::StatusChanged(const QString &status, uint error, const QSyncStatusMap &sources)
{
    Q_UNUSED(sources)

    qDebug() << "received status" << status
             << "error" << error;

    setStatus(status, error);
}

void SyncEvoSession::peerPresentChanged(const QString &present)
{
    m_present = present;
    emit presentChanged(m_present);
}

void SyncEvoSession::doStartSync()
{
    QStringMap sourceModes;

    // If a source is disabled, we don't want to override that,
    // so only apply the sync mode on the enabled sources.
    foreach(const QString& source, getStoreList())
    {
        QString mode = getStoreConfig(source, "sync");
        if (mode != "disabled") {
            sourceModes[source] = m_syncMode.isEmpty() ? mode : m_syncMode;
        }
    }

    qDebug() << "Starting sync for" << m_id << "mode" << m_syncMode;
    QDBusPendingReply<> reply = m_session->Sync(QString(), sourceModes);
    reply.waitForFinished();
    if (!reply.isValid()) {
        setError(reply.error());
    }
}

void SyncEvoSession::doSetConfig()
{
    QDBusPendingReply<> reply = m_session->SetConfig(!m_clearConfig, m_tmp, m_newConfig.getConfig());
    reply.waitForFinished();
    if (!reply.isValid()) {
        setError(reply.error());
    }

    m_commitConfig = false;
    m_clearConfig = false;
    m_newConfig.clear();

    if (m_release && (m_complete || !m_started)) {
        release();
    }
}

void SyncEvoSession::setError(const QDBusError& error)
{
    qDebug() << "D-Bus error" << error.message();
    setComplete();
}

void SyncEvoSession::setStatus(const QString& status, uint error)
{
    m_has_status = true;
    if (status != m_status) {
        m_status = status;
        m_error = error;
        emit statusChanged(m_status);
        if (m_status == "queueing") {
            return;
        }
        else if (m_status == "done") {
            setComplete();
        }
        else if (m_status == "idle") {
            setReady();
        }
        else {
            setStarted();
        }
    }
}

void SyncEvoSession::setProgress(int progress)
{
    if (m_nosync) return;
    m_has_progress = true;
    if (progress != m_progress) {
        m_progress = progress;
        emit progressChanged(m_progress);
    }
}

void SyncEvoSession::setReady()
{
    if (!m_ready) {
        m_ready = true;
        if (m_commitConfig) {
            doSetConfig();
        }
        if (m_started && m_has_config) {
            doStartSync();
        }
    }
}

void SyncEvoSession::setStarted()
{
    setReady();
    if (!m_started) {
        m_started = true;
        emit startedChanged(m_started);
    }
}

void SyncEvoSession::setComplete()
{
    setStarted();
    if (!m_complete) {
        m_complete = true;
        emit completeChanged(m_complete);
        setProgress(100);
        if (m_release && !m_commitConfig) {
            release();
        }
    }
}

void SyncEvoSession::loaded_id()
{
    m_peer = m_id.isEmpty() ? NULL : m_sync->getPeer(m_id);
    if (m_peer) {
        m_present = m_peer->getPresent();
        connect(m_peer.data(), &SyncEvoPeer::presentChanged, this, &SyncEvoSession::peerPresentChanged);
    }
    m_has_id = true;
    emit presentChanged(m_present);
}

void SyncEvoSession::load_id()
{
    QDBusPendingReply<QString> reply = m_session->GetConfigName();
    reply.waitForFinished();
    if (reply.isValid()) {
        m_id = reply.value();
        loaded_id();
    } else {
        setError(reply.error());
    }
}

void SyncEvoSession::load_config()
{
    if (m_nosync) return;
    QDBusPendingReply<QStringMultiMap> reply = m_session->GetConfig(false);
    reply.waitForFinished();
    if (reply.isValid()) {
        m_curConfig.setConfig(reply.value());
        if (!m_clearConfig) {
            m_setConfig.setConfig(m_curConfig);
            m_setConfig.mergeConfig(m_newConfig);
        }
        m_has_config = true;
    } else {
        setError(reply.error());
    }
}

void SyncEvoSession::load_status()
{
    QDBusPendingReply<QString, uint, QSyncStatusMap> reply = m_session->GetStatus();
    reply.waitForFinished();
    if (m_has_status) {
        // already got a StatusChanged signal
        return;
    }
    if (reply.isValid()) {
        qDebug() << "initial status" << reply.argumentAt<0>()
                 << "error" << reply.argumentAt<1>();

        setStatus(reply.argumentAt<0>(),
                  reply.argumentAt<1>());
    } else {
        setError(reply.error());
    }
}

void SyncEvoSession::load_progress()
{
    if (m_nosync) return;
    QSyncProgressMap sources;
    QDBusReply<int> reply = m_session->GetProgress(sources);
    if (m_has_progress) {
        // already got a ProgressChanged signal
        return;
    }
    if (reply.isValid()) {
        setProgress(reply.value());
    } else {
        setError(reply.error());
    }
}

void SyncEvoSession::load()
{
    if (!m_has_id) {
        load_id();
    }
    if (!m_has_config) {
        load_config();
    }
    if (!m_has_status) {
        load_status();
    }
    if (!m_has_progress) {
        load_progress();
    }
}

/***** SyncEvoSessionModel *****/

SyncEvoSessionModel::SyncEvoSessionModel(SyncEvo* sync) :
    m_sync(sync),
    m_ready(false)
{
    setCppOwnership(this);
}

SyncEvoSessionModel::~SyncEvoSessionModel()
{
    freeSessionList();
}

void SyncEvoSessionModel::init()
{
    initSessionList();
}

QHash<int, QByteArray> SyncEvoSessionModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[StatusRole] = "sessionStatus";
    roles[IdRole] = "peerId";
    roles[NameRole] = "peerName";
    roles[TypeRole] = "peerType";
    roles[UrlRole] = "peerUrl";
    roles[IconRole] = "peerIcon";
    roles[PresentRole] = "peerPresent";
    return roles;
}

int SyncEvoSessionModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return m_sessions.size();
}

QVariant SyncEvoSessionModel::data(const QModelIndex& index, int role) const
{
    SyncEvoSession* session = m_sessions[index.row()];
    switch (role)
    {
    case StatusRole:
        return session->getStatus();
    case IdRole:
        return session->getId();
    case NameRole:
        return session->getName();
    case TypeRole:
        return session->getType();
    case UrlRole:
        return session->getUrl();
    case IconRole:
        return session->getIcon();
    case PresentRole:
        return session->getPresent();
    default:
        return QVariant();
    }
}

SyncEvoSession* SyncEvoSessionModel::findSession(const QDBusObjectPath& path) const
{
    foreach(SyncEvoSession* session, m_sessions)
    {
        if (session->getPath() == path)
        {
            return session;
        }
    }
    return NULL;
}

SyncEvoSession* SyncEvoSessionModel::findSession(const QString& id) const
{
    foreach(SyncEvoSession* session, m_sessions)
    {
        if (session->getId() == id)
        {
            return session;
        }
    }
    return NULL;
}

SyncEvoSession* SyncEvoSessionModel::getSession(const QDBusObjectPath& path)
{
    SyncEvoSession* session = findSession(path);
    if (!session) {
        session = createSession(path);
    }
    return session;
}

void SyncEvoSessionModel::releaseSession(SyncEvoSession *session)
{
    if (session->getComplete() && !session->getAttached()) {
        deleteSession(session);
        qDebug() << "Deleted session for" << session->getId() << "path" << session->getPath().path();
    }
}

void SyncEvoSessionModel::sessionChanged(const QDBusObjectPath &path, bool started)
{
    SyncEvoSession* session = findSession(path);
    if (started) {
        if (session) {
            session->sessionStarted();
        } else {
            session = createSession(path);
            session->init();
        }
    } else {
        if (session) {
            session->sessionEnded();
        }
    }
}

SyncEvoSession* SyncEvoSessionModel::createSession(const QDBusObjectPath& path)
{
    SyncEvoSession* session = new SyncEvoSession(m_sync, path);
    m_sessions.append(session);
    return session;
}

void SyncEvoSessionModel::deleteSession(SyncEvoSession* session)
{
    session->deleteLater();
    m_sessions.removeOne(session);
}

void SyncEvoSessionModel::initSessionList()
{
    // Create objects for all paths first, and
    // load IDs for them afterwards, so that if we
    // get SessionChanged signals while loading,
    // they get handled correctly.
    QList<QDBusObjectPath> paths = m_sync->getSessions();
    foreach(const QDBusObjectPath& path, paths)
    {
        createSession(path);
    }

    // Now load all unknown IDs.
    foreach(SyncEvoSession *session, m_sessions) {
        session->init();
    }

    m_ready = true;
}

void SyncEvoSessionModel::freeSessionList()
{
    foreach(SyncEvoSession *session, m_sessions) {
        session->deleteLater();
    }
    m_sessions.clear();
}

/***** SyncEvoReportModel *****/

SyncEvoReportModel::SyncEvoReportModel(SyncEvo* sync, QDBusPendingReply<QArrayOfStringMap> reply) :
    m_sync(sync)
{
    // we will treat this model as transient
    // (i.e. don't call setCppOwnership)

    // Blocking wait for now.
    // Could make it non-blocking later, if needed.
    reply.waitForFinished();
    if (reply.isValid()) {
        m_reports = reply.value();
    } else {
        qDebug() << "D-Bus error" << reply.error().message();
    }
}

SyncEvoReportModel::~SyncEvoReportModel()
{
}

QHash<int, QByteArray> SyncEvoReportModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[DirRole] = "reportDir";
    roles[PeerIdRole] = "reportPeerId";
    roles[StatusRole] = "reportStatus";
    roles[ErrorRole] = "reportError";
    roles[StartRole] = "reportStart";
    roles[EndRole] = "reportEnd";
    return roles;
}

int SyncEvoReportModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return m_reports.size();
}

QVariant SyncEvoReportModel::data(const QModelIndex& index, int role) const
{
    const QStringMap& map = m_reports[index.row()];
    switch (role)
    {
    case DirRole:
        return map.value("dir");
    case PeerIdRole:
        return map.value("peer");
    case StatusRole:
        return map.value("status").toInt();
    case ErrorRole:
        return map.value("error", "Success");
    case StartRole:
        return QDateTime::fromTime_t(map.value("start").toUInt()).toString();
    case EndRole:
        return QDateTime::fromTime_t(map.value("end").toUInt()).toString();
    default:
        return QVariant();
    }
}

/***** SyncEvoInfoRequest *****/

SyncEvoInfoRequest::SyncEvoInfoRequest() :
    m_waiting(false)
{
    // placeholder created by QML
}

SyncEvoInfoRequest::SyncEvoInfoRequest(SyncEvo* sync, const QString& id, const QStringMap& params) :
    m_sync(sync), m_id(id), m_params(params), m_waiting(false)
{
    setCppOwnership(this);
}

SyncEvoInfoRequest::~SyncEvoInfoRequest()
{
    if (m_waiting) {
        sendResponse();
    }
}

void SyncEvoInfoRequest::setWaiting()
{
    m_waiting = true;
}

const QStringMap& SyncEvoInfoRequest::getParameters() const
{
    return m_params;
}

QString SyncEvoInfoRequest::getParameter(const QString& key) const
{
    return m_params.value(key);
}

void SyncEvoInfoRequest::setResponse(const QString& key, const QString& value)
{
    m_response[key] = value;
}

void SyncEvoInfoRequest::sendResponse()
{
    if (!m_waiting) return;
    m_waiting = false;
    m_sync->infoResponse(m_id, m_response);
}

/***** SyncEvo *****/

SyncEvo::SyncEvo() :
    m_server(SYNCEVO_DBUS_SERVICE, SYNCEVO_DBUS_PATH, QDBusConnection::sessionBus()),
    m_peers(this, false),
    m_templates(this, true),
    m_sessions(this),
    m_tmpCount(0)
{
    connect(&m_server, &org::syncevolution::Server::LogOutput,        this, &SyncEvo::LogOutput);

    connect(&m_server, &org::syncevolution::Server::ConfigChanged,    this, &SyncEvo::ConfigChanged);
    connect(&m_server, &org::syncevolution::Server::Presence,         this, &SyncEvo::Presence);
    m_peers.init();

    connect(&m_server, &org::syncevolution::Server::TemplatesChanged, this, &SyncEvo::TemplatesChanged);
    m_templates.init();

    connect(&m_server, &org::syncevolution::Server::SessionChanged,   this, &SyncEvo::SessionChanged);
    m_sessions.init();

    connect(&m_server, &org::syncevolution::Server::InfoRequest,      this, &SyncEvo::InfoRequest);
}

SyncEvo::~SyncEvo()
{
    qDeleteAll(m_infoRequests);
    m_infoRequests.clear();
}

QStringList SyncEvo::getConfigs(bool getTemplates)
{
    QDBusPendingReply<QStringList> reply = m_server.GetConfigs(getTemplates);
    reply.waitForFinished();
    if (reply.isValid()) {
        return reply.value();
    } else {
        qDebug() << "D-Bus error" << reply.error().message();
        return QStringList();
    }
}

QStringMultiMap SyncEvo::getConfig(const QString& id, bool getTemplate)
{
    QDBusPendingReply<QStringMultiMap> reply = m_server.GetConfig(id, getTemplate);
    reply.waitForFinished();
    if (reply.isValid()) {
        return reply.value();
    } else {
        qDebug() << "D-Bus error" << reply.error().message();
        return QStringMultiMap();
    }
}

QString SyncEvo::getPresent(const QString& id)
{
    QStringList transports;
    QDBusReply<QString> reply = m_server.CheckPresence(id, transports);
    if (reply.isValid()) {
        return reply.value();
    } else {
        qDebug() << "D-Bus error" << reply.error().message();
        return "error";
    }
}

SyncEvoPeer* SyncEvo::createPeer(const QString& id, const QString& peerName)
{
    return m_peers.createPeer(id, peerName);
}

QList<QDBusObjectPath> SyncEvo::getSessions()
{
    QDBusPendingReply<QList<QDBusObjectPath> > reply = m_server.GetSessions();
    reply.waitForFinished();
    if (reply.isValid()) {
        return reply.value();
    } else {
        qDebug() << "D-Bus error" << reply.error().message();
        return QList<QDBusObjectPath>();
    }
}

SyncEvoSession* SyncEvo::startSession(const QString& id)
{
    QDBusPendingReply<QDBusObjectPath> reply = m_server.StartSession(id);
    reply.waitForFinished();
    if (reply.isValid()) {
        qDebug() << "Created new session for" << id << "path" << reply.value().path();
        SyncEvoSession *session = m_sessions.getSession(reply.value());
        session->attached(id, false, false);
        return session;
    } else {
        qDebug() << "D-Bus error" << reply.error().message();
    }
    return NULL;
}

SyncEvoSession* SyncEvo::startSessionNoSync(const QString& id)
{
    QDBusPendingReply<QDBusObjectPath> reply = m_server.StartSessionWithFlags(id, QStringList() << "no-sync");
    reply.waitForFinished();
    if (reply.isValid()) {
        qDebug() << "Created new no-sync session for" << id << "path" << reply.value().path();
        SyncEvoSession *session = m_sessions.getSession(reply.value());
        session->attached(id, true, false);
        return session;
    } else {
        qDebug() << "D-Bus error" << reply.error().message();
    }
    return NULL;
}

SyncEvoSession* SyncEvo::startSessionTempConfig()
{
    // generate some unique session ID
    QString tmpId = QString("qt-sync-ui-%1-%2-%3")
            .arg(qApp->applicationPid())
            .arg(QDateTime::currentDateTime().toTime_t())
            .arg(m_tmpCount++);
    QDBusPendingReply<QDBusObjectPath> reply = m_server.StartSessionWithFlags(tmpId, QStringList() << "no-sync");
    reply.waitForFinished();
    if (reply.isValid()) {
        qDebug() << "Created new temp-config session" << tmpId << "path" << reply.value().path();
        SyncEvoSession *session = m_sessions.getSession(reply.value());
        session->attached(tmpId, true, true);
        return session;
    } else {
        qDebug() << "D-Bus error" << reply.error().message();
    }
    return NULL;
}

SyncEvoDatabaseModel* SyncEvo::getDatabaseModel(const QString& id, const QString& source)
{
    return new SyncEvoDatabaseModel(this, m_server.GetDatabases(id, source));
}

void SyncEvo::infoWorking(const QString& id)
{
    QDBusPendingReply<> reply = m_server.InfoResponse(id, "working", QStringMap());
    reply.waitForFinished();
    if (!reply.isValid()) {
        qDebug() << "D-Bus error" << reply.error().message();
    }
}

void SyncEvo::infoResponse(const QString& id, const QStringMap& response)
{
    qDebug() << "sending InfoResponse" << id;
    QDBusPendingReply<> reply = m_server.InfoResponse(id, "response", response);
    reply.waitForFinished();
    if (!reply.isValid()) {
        qDebug() << "D-Bus error" << reply.error().message();
    }
}

QString SyncEvo::fixId(const QString& id)
{
    return id.toLower().replace(' ', '-');
}

void SyncEvo::ConfigChanged()
{
    qDebug() << "received ConfigChanged";
    m_peers.configChanged();
}

void SyncEvo::InfoRequest(const QString &id, const QDBusObjectPath &path, const QString &state, const QString &handler, const QString &type, const QStringMap &parameters)
{
    Q_UNUSED(path)

    qDebug() << "received InfoRequest" << id << state << type << parameters;

    SyncEvoInfoRequest* req;
    QMap<QString, SyncEvoInfoRequest*>::iterator it = m_infoRequests.find(id);
    if (it == m_infoRequests.end()) {
        if (state == "request") {
            req = new SyncEvoInfoRequest(this, id, parameters);
            m_infoRequests.insert(id, req);

            // For now, only handle passwords.
            if (type != "password") return;

            // Should emit a signal to ensure that
            // we have a password handler, but for now,
            // just assume that we do. Tell the server.
            infoWorking(id);
        }
    } else {
        req = *it;
        if (state == "done") {
            req->deleteLater();
            m_infoRequests.erase(it);
            return;
        }
        else if (state == "waiting") {
            if (handler == m_server.connection().baseService()) {
                // we got the job
                req->setWaiting();
                qDebug() << "launching password prompt";
                emit passwordRequest(req);
            }
        }
    }
}

void SyncEvo::LogOutput(const QDBusObjectPath &path, const QString &level, const QString &output, const QString &procname)
{
    if (path.path() == SYNCEVO_DBUS_PATH) {
        qDebug() << "Global" << level << procname << output;
    } else {
        SyncEvoSession *session = m_sessions.findSession(path);
        if (session) {
            session->LogOutput(path, level, output, procname);
        } else {
            qDebug() << "Unknown LogOutput" << path.path() << level << procname << output;
        }
    }
}

void SyncEvo::Presence(const QString &server, const QString &status, const QString &transport)
{
    Q_UNUSED(transport)
    m_peers.presenceChanged(server, status);
}

void SyncEvo::SessionChanged(const QDBusObjectPath &session, bool started)
{
    m_sessions.sessionChanged(session, started);
}

void SyncEvo::TemplatesChanged()
{
    m_templates.configChanged();
}

SyncEvoPeerModel* SyncEvo::getPeerModel()
{
    return &m_peers;
}

SyncEvoPeerModel* SyncEvo::getTemplateModel()
{
    return &m_templates;
}

SyncEvoSessionModel* SyncEvo::getSessionModel()
{
    return &m_sessions;
}

SyncEvoPeer* SyncEvo::getPeer(const QString& id)
{
    return m_peers.findPeer(id);
}

SyncEvoPeer* SyncEvo::getTemplate(const QString& id)
{
    return m_templates.findPeer(id);
}

SyncEvoSession* SyncEvo::openSession(const QString& id)
{
    // check whether a session was already started for this peer
    SyncEvoSession *session = m_sessions.findSession(id);
    if (session) {
        // yup, just return it
        qDebug() << "Using existing session" << session->getPath().path();
        session->attach();
        return session;
    }
    // otherwise, start a new session
    return startSession(id);
}

void SyncEvo::closeSession(SyncEvoSession *session)
{
    session->detach();
    m_sessions.releaseSession(session);
}

bool SyncEvo::existsPeer(const QString& id)
{
    return getPeer(fixId(id)) != NULL;
}

SyncEvoPeer* SyncEvo::createPeer(const QString& id, const QString& peerName, const QString& tid)
{
    SyncEvoPeer *tmpl = getTemplate(tid);
    SyncEvoPeer *peer = createPeer(fixId(id), peerName);
    peer->createFromTemplate(tmpl);

    // Set some nice options
    peer->setSyncConfig("printChanges", "0");

    return peer;
}

void SyncEvo::deletePeer(const QString& id)
{
    SyncEvoPeer* peer = getPeer(id);
    if (peer) {
        if (peer->hasTargetConfig()) {
            // only necessary if !peer->exists(),
            // but doesn't hurt to do always.
            deletePeer(peer->getTargetConfig());
        }
        if (peer->exists()) {
            peer->deleteConfig();
            peer->commitConfig();
        } else {
            m_peers.deletePeer(peer);
        }
    }
}

SyncEvoReportModel* SyncEvo::getReportModel(const QString& id)
{
    // Arbitrarily choose a max of 100 reports.
    // By default, SyncEvolution only keeps 10 reports around anyway.
    return new SyncEvoReportModel(this, m_server.GetReports(id, 0, 100));
}

static QObject *syncevolution_singletontype_provider(QQmlEngine *, QJSEngine *)
{
    return new SyncEvo();
}


int main(int argc, char *argv[])
{
    syncevolution_qt_dbus_register_types();

    // Start QML engine
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    QScopedPointer<QQuickView> view(SailfishApp::createView());
    engine = view->engine();

    // Register object types
    qmlRegisterType<SyncEvoStoreModel>();
    qmlRegisterType<SyncEvoSourceModel>();
    qmlRegisterType<SyncEvoDatabaseModel>();
    qmlRegisterType<SyncEvoPeer>(SYNCEVO_QML_PATH, 1, 0, "SyncEvoPeer");
    qmlRegisterType<SyncEvoPeerModel>();
    qmlRegisterType<SyncEvoSession>(SYNCEVO_QML_PATH, 1, 0, "SyncEvoSession");
    qmlRegisterType<SyncEvoSessionModel>();
    qmlRegisterType<SyncEvoReportModel>();
    qmlRegisterType<SyncEvoInfoRequest>(SYNCEVO_QML_PATH, 1, 0, "SyncEvoInfoRequest");

    // Register singleton (non-instantiable) type "SyncEvo"
    qmlRegisterSingletonType<SyncEvo>(SYNCEVO_QML_PATH, 1, 0, "SyncEvo", syncevolution_singletontype_provider);

    // Load QML GUI
    view->setSource(QString("/usr/share/syncevolution/qml/sync-ui.qml"));
    view->show();

    return app->exec();
}
