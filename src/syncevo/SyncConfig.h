/*
 * Copyright (C) 2008-2009 Patrick Ohly <patrick.ohly@gmx.de>
 * Copyright (C) 2009 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */
#ifndef INCL_SYNC_EVOLUTION_CONFIG
# define INCL_SYNC_EVOLUTION_CONFIG

#include <syncevo/FilterConfigNode.h>
#include <syncevo/SafeConfigNode.h>
#include <syncevo/FileConfigNode.h>

#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/foreach.hpp>
#include <list>
#include <string>
#include <sstream>
#include <set>

#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#include <syncevo/declarations.h>
SE_BEGIN_CXX
using namespace std;

/**
 * @defgroup ConfigHandling Configuration Handling
 * @{
 */

/**
 * The SyncEvolution configuration is versioned, so that incompatible
 * changes to the on-disk config and files can be made more reliably.
 *
 * The on-disk configuration is versioned at three levels:
 * - root level
 * - context
 * - peer
 *
 * This granularity allows migrating individual peers, contexts or
 * everything to a new format.
 *
 * For each of these levels, two numbers are stored on disk and
 * hard-coded in the binary:
 * - current version = incremented each time the format is extended
 * - minimum version = set to current version each time a backwards
 *                     incompatible change is made
 *
 * This mirrors the libtool library versioning.
 *
 * Reading must check that the on-disk minimum version is <= the
 * binary's current version. Otherwise the config is too recent to
 * be used.
 *
 * Writing will bump minimum and current version on disk to the
 * versions in the binary. It will never decrease versions. This
 * works when the more recent format adds information that can
 * be safely ignored by older releases. If that is not possible,
 * then the "minimum" version must be increased to prevent older
 * releases from using the config.
 *
 * If bumping the versions increases the minimum version
 * beyond the version supported by the release which wrote the config,
 * that release will no longer work. Experimental releases will throw
 * an error and users must explicitly migrate to the current
 * format. Stable releases will migrate automatically.
 *
 * The on-disks current version can be checked to determine how to
 * handle it. It may be more obvious to simple check for the existence
 * of certain properties (that's how this was handled before the
 * introduction of versioning).
 *
 * Here are some simple rules for handling the versions:
 * - increase CUR version when adding new properties or files
 * - set MIN to CUR when it is not safe that older releases
 *   read and write a config with the current format
 *
 * SyncEvolution < 1.2 had no versioning. It's format is 0.
 * SyncEvolution 1.2:
 * - config peer min/cur version 1, because
 *   of modified libsynthesis binfiles and
 * - context min/cur version 1, because
 *   evolutionsource->database, evolutionuser/password->databaseUser/Password
 */
static const int CONFIG_ROOT_MIN_VERSION = 0;
static const int CONFIG_ROOT_CUR_VERSION = 0;
static const int CONFIG_CONTEXT_MIN_VERSION = 1;
static const int CONFIG_CONTEXT_CUR_VERSION = 1;
static const int CONFIG_PEER_MIN_VERSION = 1;
static const int CONFIG_PEER_CUR_VERSION = 1;

enum ConfigLevel {
    CONFIG_LEVEL_ROOT,      /**< = GLOBAL_SHARING */
    CONFIG_LEVEL_CONTEXT,   /**< = SOURCE_SET_SHARING */
    CONFIG_LEVEL_PEER,      /**< = NO_SHARING */
    CONFIG_LEVEL_MAX
};

std::string ConfigLevel2String(ConfigLevel level);

enum ConfigLimit {
    CONFIG_MIN_VERSION,
    CONFIG_CUR_VERSION,
    CONFIG_VERSION_MAX
};

extern int ConfigVersions[CONFIG_LEVEL_MAX][CONFIG_VERSION_MAX];

class SyncSourceConfig;
typedef SyncSourceConfig PersistentSyncSourceConfig;
class ConfigTree;
class ConfigUserInterface;
class SyncSourceNodes;
class ConstSyncSourceNodes;

/** name of the per-source admin data property */
extern const char *const SourceAdminDataName;

/** simplified creation of string lists: InitList("foo") + "bar" + ... */
template<class T> class InitList : public list<T> {
 public:
    InitList() {}
    InitList(const T &initialValue) {
        list<T>::push_back(initialValue);
    }
    InitList &operator + (const T &rhs) {
        list<T>::push_back(rhs);
        return *this;
    }
    InitList &operator += (const T &rhs) {
        list<T>::push_back(rhs);
        return *this;
    }
};
typedef InitList<string> Aliases;
typedef InitList<Aliases> Values;

enum PropertyType {
    /** sync properties occur once per config */
    SYNC_PROPERTY_TYPE,
    /** source properties occur once per source in each config */
    SOURCE_PROPERTY_TYPE,
    /** exact type is unknown */
    UNKNOWN_PROPERTY_TYPE
};

/**
 * A property name with optional source and context.
 * String format is [<source>/]<property>[@<context>|@<peer>@<context>]
 *
 * Note that the part after the @ sign without another @ is always
 * a context. The normal shorthand of just <peer> without context
 * does not work here.
 */
class PropertySpecifier {
 public:
    std::string m_source;    /**< source name, empty if applicable to all or sync property */
    std::string m_property;  /**< property name, must not be empty */
    std::string m_config;    /**< config name, empty if none, otherwise @<context> or <peer>@<context> */

    enum {
        NO_NORMALIZATION = 0,
        NORMALIZE_SOURCE = 1,
        NORMALIZE_CONFIG = 2
    };

    /** parse, optionally also normalize source and config */
    static PropertySpecifier StringToPropSpec(const std::string &spec, int flags = NORMALIZE_SOURCE|NORMALIZE_CONFIG);
    std::string toString();
};

/**
 * A property has a name and a comment. Derived classes might have
 * additional code to read and write the property from/to a
 * ConfigNode. They might also one or more  of the properties
 * on the fly, therefore the virtual get methods which return a
 * string value and not just a reference.
 *
 * In addition to the name, it may also have aliases. When reading
 * from a ConfigNode, all specified names are checked in the order in
 * which they are listed, and the first one found is used. When
 * writing, an existing key is overwritten, otherwise the main name is
 * created as a new key.
 *
 * A default value is returned if the ConfigNode doesn't have
 * a value set (= empty string). Invalid values in the configuration
 * trigger an exception. Setting invalid values does not because
 * it is not known where the value comes from - the caller should
 * check it himself.
 *
 * Properties are either registered as source properties or
 * source-independent sync properties. In each of these two sets of
 * properties, the names must be unique.
 *
 * Properties can be either user-visible (and editable) or
 * internal. Internal properties are used to cache some information
 * and may get lost when copying configurations. Therefore it
 * must be possible to recreate them somehow, if necessary with
 * an expensive operation like a slow sync.
 *
 * Starting with SyncEvolution 1.0, the internal storage of
 * property values was reorganized so that some properties
 * can be shared between peers. The concept of two property
 * sets (source and sync properties) was preserved because of
 * the simplicity that it brings to the APIs. Now this concept
 * is implemented by mapping properties into "views" that
 * contain the properties relevant for a particular peer.
 * Setting a shared value in one view updates the value in
 * another. For details, see:
 * http://syncevolution.org/development/configuration-handling
 *
 * As in the migration from the Sync4j config layout (internal and
 * user-visible properties in the same file), the code is written so
 * that properties are mapped to config nodes according to the most
 * recent layout. Older layouts are accessed by using the same config
 * node instance multiple times.
 */
class ConfigProperty {
 public:
        ConfigProperty(const string &name, const string &comment,
                       const string &def = string(""), const string &descr = string("")) :
        m_obligatory(false),
        m_hidden(false),
        m_sharing(NO_SHARING),
        m_flags(0),
        m_names(name),
        m_comment(boost::trim_right_copy(comment)),
        m_defValue(def),
        m_descr(descr)
        {}

    ConfigProperty(const Aliases &names, const string &comment,
                   const string &def = string(""), const string &descr = string("")) :
        m_obligatory(false),
        m_hidden(false),
        m_sharing(NO_SHARING),
        m_flags(0),
        m_names(names),
        m_comment(boost::trim_right_copy(comment)),
            m_defValue(def),
        m_descr(descr)
        {}
    virtual ~ConfigProperty() {}

    /** name to be used for a specific node: first name if not in node, otherwise existing key */
    string getName(const ConfigNode &node) const;

    /** primary name */
    string getMainName() const { return m_names.front(); }

    /* virtual so that derived classes like SourceBackendConfigProperty can generate the result dynamically */
    virtual const Aliases &getNames() const { return m_names; }
    virtual string getComment() const { return m_comment; }
    virtual string getDefValue() const { return m_defValue; }
    virtual string getDescr() const { return m_descr; }

    /**
     * Check whether the given value is okay.
     * If not, then set an error string (one line, no punctuation).
     *
     * @return true if value is okay
     */
    virtual bool checkValue(const string &value, string &error) const { return true; }

    /**
     * Only useful when a config property wants to check itself whether to retrieve password
     * Check the password and cache the result in the filter node on the fly if a property needs.
     * sourceName and sourceConfigNode might be not set by caller. They only affect
     * when checking password for syncsourceconfig 
     * @param ui user interface
     * @param serverName server name
     * @param globalConfigNode the sync global config node for a server
     * @param sourceName the source name used for source config properties
     * @param sourceConfigNode the config node for the source
     */
    virtual void checkPassword(ConfigUserInterface &ui,
                               const string &serverName,
                               FilterConfigNode &globalConfigNode,
                               const string &sourceName = string(),
                               const boost::shared_ptr<FilterConfigNode> &sourceConfigNode = boost::shared_ptr<FilterConfigNode>()) const {}

    /**
     * Try to save password if a config property wants.
     * It firstly check password and then invoke ui's savePassword
     * function to save the password if necessary
     */
    virtual void savePassword(ConfigUserInterface &ui,
                              const string &serverName,
                              FilterConfigNode &globalConfigNode,
                              const string &sourceName = string(),
                              const boost::shared_ptr<FilterConfigNode> &sourceConfigNode = boost::shared_ptr<FilterConfigNode>()) const {}

    /**
     * This is used to generate description dynamically according to the context information
     * Defalut implmenentation is to return value set in the constructor.
     * Derived classes can override this function. Used by 'checkPassword' and 'savePassword'
     * to generate description for user interface.
     */
    virtual const string getDescr(const string &serverName,
                                  FilterConfigNode &globalConfigNode,
                                  const string &sourceName = string(),
                                  const boost::shared_ptr<FilterConfigNode> &sourceConfigNode=boost::shared_ptr<FilterConfigNode>()) const { return m_descr; }


    /** split \n separated comment into lines without \n, appending them to commentLines */
    static void splitComment(const string &comment, list<string> &commentLines);

    /** internal property? */
    bool isHidden() const { return m_hidden; }
    void setHidden(bool hidden) { m_hidden = hidden; }

    /** config is invalid without setting this property? */
    bool isObligatory() const { return m_obligatory; }
    void setObligatory(bool obligatory) { m_obligatory = obligatory; }

    /**
     * determines how a property is shared between different views
     */
    enum Sharing {
        GLOBAL_SHARING,         /**< shared between all views,
                                   for example the "default peer" property */
        SOURCE_SET_SHARING,     /**< shared between all peers accessing
                                   the same source set, for example the
                                   logdir property */
        NO_SHARING              /**< each peer has his own values */
    };
    Sharing getSharing() const { return m_sharing; }
    void setSharing(Sharing sharing) { m_sharing = sharing; }

    /** set value unconditionally, even if it is not valid */
    void setProperty(ConfigNode &node, const string &value) const { node.setProperty(getName(node), value, getComment()); }
    void setProperty(FilterConfigNode &node, const string &value, bool temporarily = false) const {
        string name = getName(node);
        if (temporarily) {
            node.addFilter(name, value);
        } else {
            node.setProperty(name, value, getComment());
        }
    }

    /** set default value of a property, marked as default unless forced setting */
    void setDefaultProperty(ConfigNode &node, bool force) const {
        string name = getName(node);
        string defValue = getDefValue();
        node.setProperty(name, defValue, getComment(), force ? NULL : &defValue);
    }

    /**
     * String representation of property value. getPropertyValue() in
     * some derived classes returns the value in some other, class specific
     * representation.
     *
     * @retval isDefault    return true if the node had no value set and
     *                      the default was returned instead
     */
    virtual string getProperty(const ConfigNode &node, bool *isDefault = NULL) const {
        string name = getName(node);
        string value = node.readProperty(name);
        if (!value.empty()) {
            string error;
            if (!checkValue(value, error)) {
                throwValueError(node, name, value, error);
            }
            if (isDefault) {
                *isDefault = false;
            }
            return value;
        } else {
            if (isDefault) {
                *isDefault = true;
            }
            return getDefValue();
        }
    }

    // true if property is set to non-empty value
    bool isSet(const ConfigNode &node) const {
        string name = getName(node);
        string value = node.readProperty(name);
        return !value.empty();
    }

 protected:
    void throwValueError(const ConfigNode &node, const string &name, const string &value, const string &error) const;

 private:
    bool m_obligatory;
    bool m_hidden;
    Sharing m_sharing;
    int m_flags;
    const Aliases m_names;
    const string m_comment, m_defValue, m_descr;
};

/**
 * A string property which maps multiple different possible value
 * strings to one generic value, ignoring the case. Values not listed
 * are passed through unchanged. The first value in the list of
 * aliases is the generic one.
 *
 * The addition operator is defined for the aliases so that they
 * can be constructed more easily.
 */
class StringConfigProperty : public ConfigProperty {
 public:
    StringConfigProperty(const string &name, const string &comment,
                         const string &def = string(""),
                         const string &descr = string(""),
                         const Values &values = Values()) :
    ConfigProperty(name, comment, def, descr),
        m_values(values)
        {}

    /**
     * @return false if aliases are defined and the string is not one of them
     */
    bool normalizeValue(string &res) const {
        Values values = getValues();
        BOOST_FOREACH(const Values::value_type &value, values) {
            BOOST_FOREACH(const string &alias, value) {
                if (boost::iequals(res, alias)) {
                    res = *value.begin();
                    return true;
                }
            }
        }
        return values.empty();
    }

    /**
     * This implementation accepts all values if no aliases
     * are given, otherwise the value must be part of the aliases.
     */
    virtual bool checkValue(const string &propValue, string &error) const {
        Values values = getValues();
        if (values.empty()) {
            return true;
        }

        ostringstream err;
        err << "not one of the valid values (";
        bool firstval = true;
        BOOST_FOREACH(const Values::value_type &value, values) {
            if (!firstval) {
                err << ", ";
            } else {
                firstval = false;
            }
            bool firstalias = true;
            BOOST_FOREACH(const string &alias, value) {
                if (!firstalias) {
                    err << " = ";
                } else {
                    firstalias = false;
                }
                if (alias.empty()) {
                    err << "\"\"";
                } else {
                    err << alias;
                }
                
                if (boost::iequals(propValue, alias)) {
                    return true;
                }
            }
        }
        err << ")";
        error = err.str();
        return false;
    }

    virtual string getProperty(const ConfigNode &node, bool *isDefault = NULL) const {
        string res = ConfigProperty::getProperty(node, isDefault);
        normalizeValue(res);
        return res;
    }

 protected:
    virtual Values getValues() const { return m_values; }

 private:
    const Values m_values;
};


/**
 * Instead of reading and writing strings, this class interprets the content
 * as a specific type.
 */
template<class T> class TypedConfigProperty : public ConfigProperty {
 public:
    TypedConfigProperty(const string &name, const string &comment, const string &defValue = string("0"), const string &descr = string("")) :
    ConfigProperty(name, comment, defValue, descr)
        {}

    /**
     * This implementation accepts all values that can be converted
     * to the required type.
     */
    virtual bool checkValue(const string &value, string &error) const {
        istringstream in(value);
        T res;
        if (in >> res) {
            return true;
        } else {
            error = "cannot parse value";
            return false;
        }
    }

    void setProperty(ConfigNode &node, const T &value) const {
        ostringstream out;

        out << value;
        node.setProperty(getName(node), out.str(), getComment());
    }
    void setProperty(FilterConfigNode &node, const T &value, bool temporarily = false) const {
        ostringstream out;
        string name = getName(node);

        out << value;
        if (temporarily) {
            node.addFilter(name, out.str());
        } else {
            node.setProperty(name, out.str(), getComment());
        }
    }

    T getPropertyValue(const ConfigNode &node, bool *isDefault = NULL) const {
        string name = getName(node);
        string value = node.readProperty(name);
        istringstream in(value);
        T res;
        if (value.empty()) {
            istringstream defStream(getDefValue());
            defStream >> res;
            if (isDefault) {
                *isDefault = true;
            }
            return res;
        } else {
            if (!(in >> res)) {
                throwValueError(node, name, value, "cannot parse value");
            }
            if (isDefault) {
                *isDefault = false;
            }
            return res;
        }
    }
};

/**
 * The "in >> res" check in TypedConfigProperty::checkValue
 * is to loose. For example, the standard library accepts
 * -1 for an unsigned type.
 *
 * This class accepts a function pointer to strtoul() or
 * strtol() and uses that function to do strict value checking.
 *
 * @param T       type to be converted to and from (like int)
 * @param Tmin    minimum value of T for range checking
 * @param Tmax    maximum value of T for range checking
 * @param C       intermediate type for conversion  (like long)
 * @param Cmin    minimum value of C for range checking
 * @param Cmax    maximum value of C for range checking
 * @param strto   conversion function
 */
template <class T, T Tmin, T Tmax,
    class C, C Cmin, C Cmax,
    C (*strto)(const char *, char **, int)>
class ScalarConfigProperty : public TypedConfigProperty<T>
{
 public:
    ScalarConfigProperty(const string &name, const string &comment, const string &defValue = string("0"), const string &descr = string("")) :
    TypedConfigProperty<T>(name, comment, defValue, descr)
        {}

    virtual bool checkValue(const string &value, string &error) const {
        errno = 0;
        const char *nptr = value.c_str();
        char *endptr;
        // use base 10 because not specifying a base 
        // would interpret 077 as octal value, which
        // could be confusing for users
        C val = strto(nptr, &endptr, 10);
        if ((errno == ERANGE && (val == Cmin || val == Cmax))) {
            error = "range error";
            return false;
        }
        if (errno != 0 && val == 0) {
            error = "cannot parse";
            return false;
        }
        if (endptr == nptr) {
            error = "decimal value expected";
            return false;
        }
        while (isspace(*endptr)) {
            endptr++;
        }
        if (*endptr != '\0') {
            error = "unexpected trailing non-whitespace characters: ";
            error += endptr;
            return false;
        }
        // comparison might be always true for some types
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
        if (val > Tmax || val < Tmin) {
            error = "range error";
            return false;
        }
#ifdef __clang__
#pragma clang diagnostic pop
#endif
        if (Tmin == 0) {
            // check that we didn't accidentally accept a negative value,
            // strtoul() does that
            const char *start = nptr;
            while (*start && isspace(*start)) {
                start++;
            }
            if (*start == '-') {
                error = "range error";
                return false;
            }
        }

        return true;
    }
};

typedef ScalarConfigProperty<int, INT_MIN, INT_MAX, long, LONG_MIN, LONG_MAX, strtol> IntConfigProperty;
typedef ScalarConfigProperty<unsigned int, 0, UINT_MAX, unsigned long, 0, ULONG_MAX, strtoul> UIntConfigProperty;
typedef ScalarConfigProperty<long, LONG_MIN, LONG_MAX, long, LONG_MIN, LONG_MAX, strtol> LongConfigProperty;
typedef ScalarConfigProperty<unsigned long, 0, ULONG_MAX, unsigned long, 0, ULONG_MAX, strtoul> ULongConfigProperty;

/**
 * Time interval >= 0. Values are formatted as number of seconds
 * and accepted in a variety of formats, following ISO 8601:
 * - x = x seconds
 * - d = x[YyWwDdHhMmSs] = x years/weeks/days/hours/minutes/seconds
 * - d[+]d... = combination of the previous durations
 *
 * As an extension of ISO 8601, white spaces are silently ignored,
 * suffix checks are case-insensitive and s (or S) for seconds
 * can be omitted.
 */
class SecondsConfigProperty : public UIntConfigProperty
{
 public:
    SecondsConfigProperty(const string &name, const string &comment,
                          const string &defValue = string("0"), const string &descr = "") :
        UIntConfigProperty(name, comment, defValue, descr)
        {}

    virtual bool checkValue(const string &value, string &error) const;
    unsigned int getPropertyValue(const ConfigNode &node, bool *isDefault = NULL) const;

    static bool parseDuration(const string &value, string &error, unsigned int &seconds);
};

/**
 * This struct wraps keys for storing passwords
 * in configuration system. Some fields might be empty
 * for some passwords. Each field might have different 
 * meaning for each password. Fields using depends on
 * what user actually wants.
 */
struct ConfigPasswordKey {
 public:
    ConfigPasswordKey() : port(0) {}

    /** the user for the password */
    string user;
    /** the server for the password */
    string server;
    /** the domain name */
    string domain;
    /** the remote object */
    string object;
    /** the network protocol */
    string protocol;
    /** the authentication type */
    string authtype;
    /** the network port */
    unsigned int port;
};

/**
 * This interface has to be provided by the user of the config
 * to let the config code interact with the user.
 */
class ConfigUserInterface {
 public:
    virtual ~ConfigUserInterface() {}

    /**
     * A helper function which interactively asks the user for
     * a certain password. May throw errors.
     *
     * @param passwordName the name of the password in the config file, such as 'proxyPassword'
     * @param descr        A simple string explaining what the password is needed for,
     *                     e.g. "SyncML server". This string alone has to be enough
     *                     for the user to know what the password is for, i.e. the
     *                     string has to be unique.
     * @param key          the key used to retrieve password. Using this instead of ConfigNode is
     *                     to make user interface independent on Configuration Tree
     * @return entered password
     */
    virtual string askPassword(const string &passwordName, const string &descr, const ConfigPasswordKey &key) = 0;

    /**
     * A helper function which is used for user interface to save
     * a certain password. Currently possibly syncml server. May
     * throw errors.
     * @param passwordName the name of the password in the config file, such as 'proxyPassword'
     * @param password     password to be saved
     * @param key          the key used to store password
     * @return true if ui saves the password and false if not
     */
    virtual bool savePassword(const string &passwordName, const string &password, const ConfigPasswordKey &key) = 0;
};

class PasswordConfigProperty : public ConfigProperty {
 public:
    PasswordConfigProperty(const string &name, const string &comment, const string &def = string(""),const string &descr = string("")) :
       ConfigProperty(name, comment, def, descr)
           {}
    PasswordConfigProperty(const Aliases &names, const string &comment, const string &def = string(""),const string &descr = string("")) :
       ConfigProperty(names, comment, def, descr)
           {}

    /**
     * Check the password and cache the result.
     */
    virtual void checkPassword(ConfigUserInterface &ui,
                               const string &serverName,
                               FilterConfigNode &globalConfigNode,
                               const string &sourceName = "",
                               const boost::shared_ptr<FilterConfigNode> &sourceConfigNode =
                               boost::shared_ptr<FilterConfigNode>()) const;

    /**
     * It firstly check password and then invoke ui's savePassword
     * function to save the password if necessary
     */
    virtual void savePassword(ConfigUserInterface &ui,
                              const string &serverName,
                              FilterConfigNode &globalConfigNode,
                              const string &sourceName = "",
                              const boost::shared_ptr<FilterConfigNode> &sourceConfigNode =
                              boost::shared_ptr<FilterConfigNode>()) const;

    /**
     * Get password key for storing or retrieving passwords
     * The default implemention is for 'password' in global config.
     * @param descr decription for password
     * @param globalConfigNode the global config node 
     * @param sourceConfigNode the source config node. It might be empty
     */
    virtual ConfigPasswordKey getPasswordKey(const string &descr,
                                             const string &serverName,
                                             FilterConfigNode &globalConfigNode,
                                             const string &sourceName = string(),
                                             const boost::shared_ptr<FilterConfigNode> &sourceConfigNode =
                                             boost::shared_ptr<FilterConfigNode>()) const; 

    /**
     * return the cached value if necessary and possible
     */
    virtual string getCachedProperty(const ConfigNode &node,
                                     const string &cachedPassword);
};

/**
 * A derived ConfigProperty class for the property "proxyPassword"
 */
class ProxyPasswordConfigProperty : public PasswordConfigProperty {
 public:
    ProxyPasswordConfigProperty(const string &name, const string &comment, const string &def = string(""), const string &descr = string("")) :
        PasswordConfigProperty(name,comment,def,descr)
    {}
    /**
     * re-implement this function for it is necessary to do a check 
     * before retrieving proxy password
     */
    virtual void checkPassword(ConfigUserInterface &ui,
                               const string &serverName,
                               FilterConfigNode &globalConfigNode,
                               const string &sourceName,
                               const boost::shared_ptr<FilterConfigNode> &sourceConfigNode) const;
    virtual ConfigPasswordKey getPasswordKey(const string &descr,
                                             const string &serverName,
                                             FilterConfigNode &globalConfigNode,
                                             const string &sourceName = string(),
                                             const boost::shared_ptr<FilterConfigNode> &sourceConfigNode=boost::shared_ptr<FilterConfigNode>()) const; 
};

/**
 * A derived ConfigProperty class for the property "evolutionpassword"
 */
class DatabasePasswordConfigProperty : public PasswordConfigProperty {
 public:
    DatabasePasswordConfigProperty(const Aliases &names,
                                   const string &comment, 
                                   const string &def = string(""),
                                   const string &descr = string("")): 
    PasswordConfigProperty(names,comment,def,descr)
    {}
    virtual ConfigPasswordKey getPasswordKey(const string &descr,
                                             const string &serverName,
                                             FilterConfigNode &globalConfigNode,
                                             const string &sourceName = string(),
                                             const boost::shared_ptr<FilterConfigNode> &sourceConfigNode=boost::shared_ptr<FilterConfigNode>()) const; 
    virtual const string getDescr(const string &serverName,
                                  FilterConfigNode &globalConfigNode,
                                  const string &sourceName,
                                  const boost::shared_ptr<FilterConfigNode> &sourceConfigNode) const {
        string descr = sourceName;
        descr += " ";
        descr += ConfigProperty::getDescr();
        return descr;
    }
};

/**
 * Instead of reading and writing strings, this class interprets the content
 * as boolean with T/F or 1/0 (default format).
 */
class BoolConfigProperty : public StringConfigProperty {
 public:
    BoolConfigProperty(const string &name, const string &comment, const string &defValue = string("F"),const string &descr = string("")) :
    StringConfigProperty(name, comment, defValue,descr,
                         Values() + (Aliases("1") + "T" + "TRUE") + (Aliases("0") + "F" + "FALSE"))
        {}

    void setProperty(ConfigNode &node, bool value) {
        StringConfigProperty::setProperty(node, value ? "1" : "0");
    }
    void setProperty(FilterConfigNode &node, bool value, bool temporarily = false) {
        StringConfigProperty::setProperty(node, value ? "1" : "0", temporarily);
    }
    int getPropertyValue(const ConfigNode &node, bool *isDefault = NULL) const {
        string res = ConfigProperty::getProperty(node, isDefault);

        return boost::iequals(res, "T") ||
            boost::iequals(res, "TRUE") ||
            atoi(res.c_str()) != 0;
    }
};

/**
 * A property for arbitrary strings.
 */
class SafeConfigProperty : public ConfigProperty {
 public:
    SafeConfigProperty(const string &name, const string &comment) :
    ConfigProperty(name, comment)
    {}

    void setProperty(ConfigNode &node, const string &value) {
        ConfigProperty::setProperty(node, StringEscape::escape(value, '!', StringEscape::INI_WORD));
    }
    virtual string getProperty(const ConfigNode &node, bool *isDefault = NULL) const {
        string res = ConfigProperty::getProperty(node, isDefault);
        res = StringEscape::unescape(res, '!');
        return res;
    }
};

/**
 * A registry for all properties which might be saved in the same ConfigNode.
 * Currently the same as a simple list. Someone else owns the instances.
 */
class ConfigPropertyRegistry : public list<const ConfigProperty *> {
 public:
    /** case-insensitive search for property */
    const ConfigProperty *find(const string &propName) const {
        BOOST_FOREACH(const ConfigProperty *prop, *this) {
            BOOST_FOREACH(const string &name, prop->getNames()) {
                if (boost::iequals(name, propName)) {
                    return prop;
                }
            }
        }
        return NULL;
    }
};

/**
 * This class implements the client library configuration interface
 * by mapping values to properties to entries in a ConfigTree. The
 * mapping is either the traditional one used by SyncEvolution <= 0.7
 * and client library <= 6.5 or the new layout introduced with
 * SyncEvolution >= 0.8. If for a given server name the old config
 * exists, then it is used. Otherwise the new layout is used.
 *
 * This class can be instantiated on its own and then provides access
 * to properties actually stored in files. SyncContext
 * inherits from this class so that a derived client has the chance to
 * override every single property (although it doesn't have to).
 * Likewise SyncSource is derived from
 * SyncSourceConfig.
 *
 * Properties can be set permanently (this changes the underlying
 * ConfigNode) and temporarily (this modifies the FilterConfigNode
 * which wraps the ConfigNode).
 *
 * The old layout is:
 * - $HOME/.sync4j/evolution/<server>/spds/syncml/config.txt
 * -- spds/sources/<source>/config.txt
 * ---                      changes_<changeid>/config.txt
 *
 * The new layout is:
 * - ${XDG_CONFIG:-${HOME}/.config}/syncevolution/foo - base directory for server foo
 * -- config.ini - constant per-server settings
 * -- .internal.ini - read/write server properties - hidden from users because of the leading dot
 * -- sources/bar - base directory for source bar
 * --- config.ini - constant per-source settings
 * --- .internal.ini - read/write source properties
 * --- .changes_<changeid>.ini - change tracking node (content under control of sync source)
 *
 * Because this class needs to handle different file layouts it always
 * uses a FileConfigTree instance. Other implementations would be
 * possible.
 */
class SyncConfig {
 public:
    /**
     * Opens the configuration for a specific server,
     * searching for the config files in the usual
     * places. Will succeed even if config does not
     * yet exist: flushing such a config creates it.
     *
     * Does a version check to ensure that the config can be
     * read. Users of the instance must to an explicit
     * prepareConfigForWrite() if the config or the files associated
     * with it (Synthesis bin files) are going to be written.
     *
     * @param peer   string that identifies the peer,
     *               matching regex (.*)(@([^@]*))? 
     *               where the $1 (the first part) is 
     *               the peer name and the optional $2
     *               (the part after the last @) is the
     *               context, "default" if not given.
     *               For example "scheduleworld" =
     *               "ScheduleWorld" =
     *               "scheduleworld@default", but not the same as
     *               "scheduleworld@other_context"
     *
     * @param tree   if non-NULL, then this is used
     *               as configuration tree instead of
     *               searching for it; always uses the
     *               current layout in that tree
     *
     * @param redirectPeerRootPath
     *               Can be used to redirect the per-peer
     *               files into a different directory. Only works
     *               in non-peer context configs.
     *               Used by SyncContext for local sync.
     */
    SyncConfig(const string &peer,
               boost::shared_ptr<ConfigTree> tree = boost::shared_ptr<ConfigTree>(),
               const string &redirectPeerRootPath = "");


    /**
     * Creates a temporary configuration.
     * Can be copied around, but not flushed.
     */
    SyncConfig();

    /**
     * determines whether the need to migrate a config causes a
     * STATUS_MIGRATION_NEEDED error or does the migration
     * automatically; default is to migrate automatically in
     * stable releases and to ask in development releases
     */
    enum ConfigWriteMode {
        MIGRATE_AUTOMATICALLY,
        ASK_USER_TO_MIGRATE
    };
    ConfigWriteMode getConfigWriteMode() const { return m_configWriteMode; }
    void setConfigWriteMode(ConfigWriteMode mode) { m_configWriteMode = mode; }

    /**
     * This does another version check which ensures that the config
     * is not unintentionally altered so that it cannot be read by
     * older SyncEvolution releases. If the config cannot be written
     * without breaking older releases, then either the call will fail
     * (development releases) or migrate the config (stable releases).
     * Can be controlled via setConfigWriteMode();
     *
     * Also writes the current config versions into the config.
     */
    void prepareConfigForWrite();

   /** absolute directory name of the configuration root */
    string getRootPath() const;

    typedef list< std::pair<std::string, std::string> > ConfigList;

    /** A simple description of the template or the configuration based on a
     * template. The rank field is used to indicate how good it matches the
     * user input <MacAddress, DeviceName> */
    struct TemplateDescription {
        // The unique identifier of the template
        std::string m_templateId;
        // The description of the template (eg. the web server URL for a
        // SyncML server. This is not used for UI, only CMD line used this.
        std::string m_description;
        // The matched percentage of the template, larger the better.
        int m_rank;

        //a unique identity of the device that the template is for, used by caller
        std::string m_deviceId;

        // A string identify which fingerprint the template is matched with.
        std::string m_fingerprint;

        // A unique string identify the template path, so that a later operation
        // fetching this config will be much easier
        std::string m_path;

        // A string indicates the original fingerprint in the matched template, this
        // will not necessarily the same with m_fingerprint
        std::string m_matchedModel;

        // The template name (device class) presented
        std::string m_templateName;

        TemplateDescription (const std::string &templateId, const std::string &description, 
                const int rank, const std::string deviceId, const std::string &fingerprint, const std::string &path, const std::string &model, const std::string &templateName)
            :   m_templateId (templateId),
                m_description (description),
                m_rank (rank),
                m_deviceId (deviceId),
                m_fingerprint (fingerprint),
                m_path (path),
                m_matchedModel(model),
                m_templateName (templateName)
        {
        }

        TemplateDescription (const std::string &name, const std::string &description);

        static bool compare_op (boost::shared_ptr<TemplateDescription> &left, boost::shared_ptr<TemplateDescription> &right);
    };

    enum MatchMode {
        /*Match templates when we work as SyncML server, i.e. the peer is the client*/
        MATCH_FOR_SERVER_MODE,
        /*Match templates when work as SyncML client, i.e. the peer is the server*/
        MATCH_FOR_CLIENT_MODE,
        /*Match templates for both SyncML server and SyncML client*/
        MATCH_ALL,
        INVALID
    };

    typedef list<boost::shared_ptr <TemplateDescription> > TemplateList;

    struct DeviceDescription {
        /** the id of the device */
        std::string m_deviceId;
        /** the finger print of the device used for matching templates */
        std::string m_fingerprint;
        /** match mode used for matching templates */
        MatchMode m_matchMode;
        DeviceDescription(const std::string &deviceId,
                          const std::string &fingerprint,
                          MatchMode mode)
            :m_deviceId(deviceId), m_fingerprint(fingerprint), m_matchMode(mode)
        {}
        DeviceDescription() : m_matchMode(INVALID)
        {}
    };

    typedef list<DeviceDescription> DeviceList;

    /**
     * returns list of servers in either the old (.sync4j) or
     * new config directory (.config), given as server name
     * and absolute root of config
     *
     * Guaranteed to be sorted by the (context, peer name, path) tuple,
     * in increasing order (foo@bar < abc@xyz < abc.old@xyz).
     */
    static ConfigList getConfigs();

    /**
     * returns list of available config templates:
     * for each peer listed in @peers, matching against the fingerprint information
     * from the peer (deviceName likely), sorted by the matching score,
     * templates failed to match(as long as it's for SyncML server) will also
     * be returned as a fallback mechanism so that user can select a configuration
     * template manually.
     * Any templates for SyncMl Client is also returned, with a default rank.
     * The assumption currently is only work for SyncML client peers.
     * DeviceList is a list of matching tuples <fingerprint, SyncConfig::MatchMode>.
     */
    static TemplateList getPeerTemplates(const DeviceList &peers);

    /**
     * match the built-in templates against @param fingerprint, return a list of
     * servers sorted by the matching rank.
     * */
    static TemplateList matchPeerTemplates(const DeviceList &peers, bool fuzzyMatch = true);

    /**
     * get the built-in default templates
     */
    static TemplateList getBuiltInTemplates ();

    /**
     * Creates a new instance of a configuration template.
     * The result can be modified to set filters, but it
     * cannot be flushed.
     *
     * @param peer   a configuration name, *without* a context (scheduleworld, not scheduleworld@default),
     * or a configuration path in the system directory which can avoid another fuzzy match process.
     * "none" returns an empty template (default sync properties and dev ID set).
     * @return NULL if no such template
     */
    static boost::shared_ptr<SyncConfig> createPeerTemplate(const string &peer);

    /**
     * true if the main configuration file already exists;
     * "main" here means the per-peer config or context config,
     * depending on what the config refers to
     */
    bool exists() const;

    /**
     * true if the config files for the selected level exist;
     * false is returned for CONFIG_LEVEL_PEER and a config
     * which refers to a context
     */
    bool exists(ConfigLevel level) const;

    /**
     * The normalized, unique config name used by this instance.
     * Empty if not backed up by a real config.
     */
    string getConfigName() const { return m_peer; }

    /**
     * The normalized context used by this instance.
     * Includes @ sign.
     */
    string getContextName() const;

    /**
     * the normalized peer name, empty if not a peer config
     */
    string getPeerName() const;

    /**
     * true if the config is for a peer, false if a context config
     */
    bool hasPeerProperties() const { return !m_peerPath.empty(); }

    /**
     * returns names of peers inside this config;
     * empty if not a context
     */
    list<string> getPeers() const;

    /**
     * Do something before doing flush to files. This is particularly
     * useful when user interface wants to do preparation jobs, such
     * as savePassword and others.
     */
    virtual void preFlush(ConfigUserInterface &ui); 

    void flush();

    /**
     * Remove the configuration. Config directories are removed if
     * empty.
     *
     * When the configuration is peer-specific, only the peer's
     * properties and config nodes are removed. Otherwise the complete
     * configuration is removed, including all peers.
     *
     * Does *not* remove logs associated with the configuration.
     * For that use the logdir handling in SyncContext
     * before removing the configuration.
     */
    void remove();

    /**
     * A list of all properties. Can be extended by derived clients.
     */
    static ConfigPropertyRegistry &getRegistry();

    enum NormalizeFlags {
        NORMALIZE_LONG_FORMAT = 0,  /**< include context in normal form */
        NORMALIZE_SHORTHAND = 1,    /**< keep normal form shorter by not specifying @default */
        NORMALIZE_IS_NEW = 2,       /**< does not refer to an existing config, do not search
                                       for it among existing configs */
        NORMALIZE_MAX = 0xFFFF
    };

    /**
     * Normalize a config string:
     * - lower case
     * - non-printable and unsafe characters (colon, slash, backslash)
     *   replaced by underscore
     * - when no context specified and NORMALIZE_IS_NEW not set:
     *   search for peer config first in @default, then also in other contexts
     *   in alphabetical order
     * - NORMALIZE_SHORTHAND set: @default stripped  (dangerous: result "foo"
     *   may incorrectly be mapped to "foo@bar" if the "foo@default" config gets removed),
     *   otherwise added if missing
     * - empty string replaced with "@default"
     */
    static string normalizeConfigString(const string &config, NormalizeFlags flags = NORMALIZE_SHORTHAND);

    /**
     * Split a config string (normalized or not) into the peer part
     * (before final @) and the context (after that @, not including
     * it), return "default" as context if not specified otherwise.
     *
     * @return true when the context was specified explicitly
     */
    static bool splitConfigString(const string &config, string &peer, string &context);

    /**
     * Replaces the property filter of either the sync properties or
     * all sources. This can be used to e.g. temporarily override
     * the active sync mode.
     *
     * All future calls of getSyncSourceNodes() will have these
     * filters applied.
     *
     * @param sync     true if the filter applies to sync properties,
     *                 false if it applies to sources
     * @param source   empty string if filter applies to all sources,
     *                 otherwise the source name to which it applies
     * @param filter   key (case insensitive)/value pairs of properties
     *                 which are to be overridden
     */
    void setConfigFilter(bool sync,
                         const std::string &source,
                         const FilterConfigNode::ConfigFilter &filter);

    /**
     * Read-write access to all configurable properties of the server.
     * The visible properties are passed through the config filter,
     * which can be modified.
     */
    boost::shared_ptr<FilterConfigNode> getProperties(bool hidden = false) { return m_props[hidden]; }
    boost::shared_ptr<const FilterConfigNode> getProperties(bool hidden = false) const { return const_cast<SyncConfig *>(this)->getProperties(hidden); }

    /**
     * Returns the right config node for a certain property,
     * depending on visibility and sharing.
     */
    boost::shared_ptr<FilterConfigNode> getNode(const ConfigProperty &prop);
    boost::shared_ptr<const FilterConfigNode> getNode(const ConfigProperty &prop) const 
    {
        return const_cast<SyncConfig *>(this)->getNode(prop);
    }

    /**
     * Returns the right config node for a certain registered property,
     * looked up by name. NULL if not found.
     */
    boost::shared_ptr<FilterConfigNode> getNode(const std::string &propName);
    boost::shared_ptr<const FilterConfigNode> getNode(const std::string &propName) const
    {
        return const_cast<SyncConfig *>(this)->getNode(propName);
    }

    /**
     * Returns a wrapper around all properties of the given source
     * which are saved in the config tree. Note that this is different
     * from the set of sync source configs used by the SyncManager:
     * the SyncManger uses the AbstractSyncSourceConfig. In
     * SyncEvolution those are implemented by the
     * SyncSource's actually instantiated by
     * SyncContext. Those are complete whereas
     * PersistentSyncSourceConfig only provides access to a
     * subset of the properties.
     *
     * Can be called for sources which do not exist yet.
     */
    virtual boost::shared_ptr<PersistentSyncSourceConfig> getSyncSourceConfig(const string &name);
    virtual boost::shared_ptr<const PersistentSyncSourceConfig> getSyncSourceConfig(const string &name) const {
        return const_cast<SyncConfig *>(this)->getSyncSourceConfig(name);
    }

    /**
     * Returns list of all configured (not active!) sync sources.
     */
    virtual list<string> getSyncSources() const;

    /**
     * Creates config nodes for a certain node. The nodes are not
     * yet created in the backend if they do not yet exist.
     *
     * Calling this for the same name repeatedly will return the
     * same set of node instances. This allows to set properties
     * temporarily in one place and have them used elsewhere.
     *
     * setConfigFilter() resets this cache of nodes. Requesting nodes
     * after that call will create a new set of nodes with properties
     * modified temporarily according to these filters.
     *
     * @param name       the name of the sync source
     * @param trackName  additional part of the tracking node name (used for unit testing)
     */
    SyncSourceNodes getSyncSourceNodes(const string &name,
                                       const string &trackName = "");
    ConstSyncSourceNodes getSyncSourceNodes(const string &name,
                                            const string &trackName = "") const;

    /**
     * Creates config nodes for a certain node. The nodes are not
     * yet created in the backend if they do not yet exist.
     * In contrast to the normal set of nodes, the tracking node
     * is empty and discards all changes. This is useful when
     * trying to initialize a SyncSource without a peer (normally
     * has a tracking node which rejects writes with an exception)
     * or with a peer without interfering with normal change tracking
     * (normally SyncSource might overwrite change tracking).
     *
     * @param name          the name of the sync source
     */
    SyncSourceNodes getSyncSourceNodesNoTracking(const string &name);

    /**
     * initialize all properties with their default value
     */
    void setDefaults(bool force = true);

    /**
     * create a new sync source configuration with default values
     */
    void setSourceDefaults(const string &name, bool force = true);

    /**
     * Remove sync source configuration. And remove the directory
     * if it has no other files.
     *
     * When the configuration is peer-specific, only the peer's
     * properties are removed. Otherwise the complete source
     * configuration is removed, including properties stored
     * for in any of the peers.
     */
    void removeSyncSource(const string &name);

    /**
     * clear existing visible source properties selected by the
     * configuration: with or without peer-specific properties,
     * depending on the current view
     */
    void clearSyncSourceProperties(const string &name);

    /**
     * clear all global sync properties, with or without
     * peer-specific properties, depending on the current view
     */
    void clearSyncProperties();

    /**
     * Copy all registered properties (hidden and visible) and the
     * tracking node into the current config. This is done by reading
     * all properties from the source config, which implies the unset
     * properties will be set to their default values.  The current
     * config is not cleared so additional, unregistered properties
     * (should they exist) will continue to exist unchanged.
     *
     * The current config still needs to be flushed to make the
     * changes permanent.
     *
     * @param sources   if NULL, then copy all sources; if not NULL,
     *                  then copy exactly the sources listed here
     *                  (regardless whether they exist or not)
     */
    void copy(const SyncConfig &other,
              const set<string> *sources);

    /**
     * @name Settings specific to SyncEvolution
     *
     * See the property definitions in SyncConfig.cpp
     * for the user-visible explanations of
     * these settings.
     */
    /**@{*/

    virtual string getDefaultPeer() const;
    virtual void setDefaultPeer(const string &value);

    virtual std::string getLogDir() const;
    virtual void setLogDir(const string &value, bool temporarily = false);

    virtual int getMaxLogDirs() const;
    virtual void setMaxLogDirs(int value, bool temporarily = false);

    virtual int getLogLevel() const;
    virtual void setLogLevel(int value, bool temporarily = false);

    virtual bool getPrintChanges() const;
    virtual void setPrintChanges(bool value, bool temporarily = false);

    virtual bool getDumpData() const;
    virtual void setDumpData(bool value, bool temporarily = false);

    virtual std::string getWebURL() const;
    virtual void setWebURL(const std::string &url, bool temporarily = false);

    virtual std::string getIconURI() const;
    virtual void setIconURI(const std::string &uri, bool temporarily = false);

    /**
     * A property of server template configs. True if the server is
     * ready for use by "normal" users (everyone can get an account
     * and some kind of support, we have tested the server well
     * enough, ...).
     */
    virtual bool getConsumerReady() const;
    virtual void setConsumerReady(bool ready);

    virtual unsigned long getHashCode() const;
    virtual void setHashCode(unsigned long hashCode);

    virtual std::string getConfigDate() const;
    virtual void setConfigDate(); /* set current time always */

    /**@}*/

    /**
     * @name SyncML Settings
     *
     * These settings are required by the Synthesis engine.
     * Some of them are hard-coded in this class. A derived class could
     * make them configurable again, should that be desired.
     */
    /**@{*/

    virtual std::string getSyncUsername() const;
    virtual void setSyncUsername(const string &value, bool temporarily = false);
    virtual std::string getSyncPassword() const;
    virtual void setSyncPassword(const string &value, bool temporarily = false);

    /**
     * Look at the password setting and if it requires user interaction,
     * get it from the user. Then store it for later usage in getSyncPassword().
     * Without this call, getSyncPassword() returns the original, unmodified
     * config string.
     */
    virtual void checkSyncPassword(ConfigUserInterface &ui);

    /**
     * Look at the password setting and if it needs special mechanism to
     * save password, this function is used to store specified password
     * in the config tree.
     * @param ui the ui pointer
     */
    virtual void saveSyncPassword(ConfigUserInterface &ui); 

    virtual bool getPreventSlowSync() const;
    virtual void setPreventSlowSync(bool value, bool temporarily = false);
    virtual bool getUseProxy() const;
    virtual void setUseProxy(bool value, bool temporarily = false);
    virtual std::string getProxyHost() const;
    virtual void setProxyHost(const string &value, bool temporarily = false);
    virtual int getProxyPort() const { return 0; }
    virtual std::string getProxyUsername() const;
    virtual void setProxyUsername(const string &value, bool temporarily = false);
    virtual std::string getProxyPassword() const;
    virtual void checkProxyPassword(ConfigUserInterface &ui);
    virtual void saveProxyPassword(ConfigUserInterface &ui);
    virtual void setProxyPassword(const string &value, bool temporarily = false);
    virtual vector<string>  getSyncURL() const;
    virtual void setSyncURL(const string &value, bool temporarily = false);
    virtual void setSyncURL(const vector<string> &value, bool temporarily = false);
    virtual std::string getClientAuthType() const;
    virtual void setClientAuthType(const string &value, bool temporarily = false);
    virtual unsigned long getMaxMsgSize() const;
    virtual void setMaxMsgSize(unsigned long value, bool temporarily = false);
    virtual unsigned int getMaxObjSize() const;
    virtual void setMaxObjSize(unsigned int value, bool temporarily = false);
    virtual unsigned long getReadBufferSize() const { return 0; }
    virtual std::string getSSLServerCertificates() const;

    /**
     * iterate over files mentioned in getSSLServerCertificates()
     * and return name of first one which is found, empty string
     * if none
     */
    std::string findSSLServerCertificate();

    virtual void setSSLServerCertificates(const string &value, bool temporarily = false);
    virtual bool getSSLVerifyServer() const;
    virtual void setSSLVerifyServer(bool value, bool temporarily = false);
    virtual bool getSSLVerifyHost() const;
    virtual void setSSLVerifyHost(bool value, bool temporarily = false);
    virtual int getRetryInterval() const;
    virtual void setRetryInterval(int value, bool temporarily = false);
    virtual int getRetryDuration() const;
    virtual void setRetryDuration(int value, bool temporarily = false);
    virtual bool  getCompression() const;
    virtual void setCompression(bool value, bool temporarily = false);
    virtual unsigned int getResponseTimeout() const { return 0; }
    virtual std::string getDevID() const;
    virtual void setDevID(const string &value, bool temporarily = false);

    /*Used for Server Alerted Sync*/
    virtual std::string getRemoteIdentifier() const;
    virtual void setRemoteIdentifier (const string &value, bool temporaritly = false);
    virtual bool getPeerIsClient () const;
    virtual void setPeerIsClient (bool value, bool temporarily = false);
    virtual std::string getSyncMLVersion() const;
    virtual void setSyncMLVersion (const string &value, bool temporarily = false);

    /**
     * An arbitrary name assigned to the peer configuration,
     * not necessarily unique. Can be used by a GUI instead
     * of the config name.
     */
    virtual string getUserPeerName() const;
    virtual void setUserPeerName(const string &name);

    /**
     * The Device ID of our peer. Typically only relevant when the
     * peer is a client. Servers don't have a Device ID, just some
     * unique way of contacting them.
     */
    virtual string getRemoteDevID() const;
    virtual void setRemoteDevID(const string &value);

    /**
     * The opaque nonce value stored for a peer, required for MD5
     * authentication. Only used when acting as server.
     */
    virtual string getNonce() const;
    virtual void setNonce(const string &value);

    /**
     * The opaque per-peer admin data managed by the Synthesis
     * engine. Only used when acting as server.
     */
    virtual string getDeviceData() const;
    virtual void setDeviceData(const string &value);

    /**
     * Automatic sync related properties, used to control its behaviors
     */
    virtual string getAutoSync() const; 
    virtual void setAutoSync(const string &value, bool temporarily = false);
    virtual unsigned int getAutoSyncInterval() const;
    virtual void setAutoSyncInterval(unsigned int value, bool temporarily = false);
    virtual unsigned int getAutoSyncDelay() const;
    virtual void setAutoSyncDelay(unsigned int value, bool temporarily = false);

    /**
     * Specifies whether WBXML is to be used (default).
     * Otherwise XML is used.
     */
    virtual bool getWBXML() const;
    virtual void setWBXML(bool isWBXML, bool temporarily = false);

    virtual std::string getUserAgent() const { return "SyncEvolution"; }
    virtual std::string getMan() const { return "Patrick Ohly"; }
    virtual std::string getMod() const { return "SyncEvolution"; }
    virtual std::string getOem() const { return "Open Source"; }
    virtual std::string getHwv() const { return "unknown"; }
    virtual std::string getSwv() const;
    virtual std::string getDevType() const;
    /**@}*/

    enum Layout {
        SYNC4J_LAYOUT,        /**< .syncj4/evolution/<server>, SyncEvolution <= 0.7.x */
        HTTP_SERVER_LAYOUT,   /**< .config/syncevolution/<server> with sources
                                 underneath, SyncEvolution <= 0.9.x */
        SHARED_LAYOUT         /**< .config/syncevolution/<context> containing sources 
                                 and peers, with source settings shared by peers,
                                 SyncEvolution >= 1.0 */
    };

    /** config versioning; setting is done internally */
    int getConfigVersion(ConfigLevel level, ConfigLimit limit) const;

    /** file layout used by config */
    Layout getLayout() const { return m_layout; }

private:
    /**
     * scans for peer configurations
     * @param root         absolute directory path
     * @param configname   expected name of config files (config.ini or config.txt)
     * @retval res         filled with new peer configurations found
     */
    static void addPeers(const string &root,
                         const std::string &configname,
                         SyncConfig::ConfigList &res);

    /* internal access to configuration versioning */
    void setConfigVersion(ConfigLevel level, ConfigLimit limit, int version);

    /**
     * migrate root (""), context or peer config and everything contained in them to
     * the current config format
     */
    void migrate(const std::string &config);

    /**
     * set tree and nodes to VolatileConfigTree/Node
     */
    void makeVolatile();

    /**
     * String that identifies the peer, see constructor.
     * This is a normalized string (normalizePeerString()).
     * The name is a bit of a misnomer, because the config
     * might also reference just a context without any
     * peer-specific properties ("@some-context", or "@default").
     */
    string m_peer;

    /**
     * Lower case path to peer configuration,
     * relative to configuration tree root.
     * For example "scheduleworld" for "ScheduleWorld" when
     * using the old config layouts, "default/peers/scheduleworld"
     * in the new layout.
     *
     * Empty if configuration view has no peer-specific properties.
     */
    string m_peerPath;

    /**
     * lower case path to source set properties,
     * unused for old layouts, else something like
     * "default" or "other_context"
     */
    string m_contextPath;

    Layout m_layout;
    string m_redirectPeerRootPath;
    string m_cachedPassword;
    string m_cachedProxyPassword;
    ConfigWriteMode m_configWriteMode;

    /** holds all config nodes relative to the root that we found */
    boost::shared_ptr<ConfigTree> m_tree;

    /** access to global sync properties, independent of
        the context (for example, "defaultPeer") */
    boost::shared_ptr<FilterConfigNode> m_globalNode;
    boost::shared_ptr<ConfigNode> m_globalHiddenNode;

    /** access to properties shared between peers */
    boost::shared_ptr<FilterConfigNode> m_contextNode;
    boost::shared_ptr<ConfigNode> m_contextHiddenNode;

    /** access to properties specific to a peer */
    boost::shared_ptr<FilterConfigNode> m_peerNode;
    boost::shared_ptr<ConfigNode> m_hiddenPeerNode;

    /** multiplexer for the other config nodes */
    boost::shared_ptr<FilterConfigNode> m_props[2];

    /**
     * temporary override for all sync source settings
     * ("" as key) or specific sources (source name as key)
     */
    SourceProps m_sourceFilters;

    static string getOldRoot() {
        return getHome() + "/.sync4j/evolution";
    }

    static string getNewRoot() {
        const char *xdg_root_str = getenv("XDG_CONFIG_HOME");
        return xdg_root_str ? string(xdg_root_str) + "/syncevolution" :
            getHome() + "/.config/syncevolution";
    }

    /** remember all SyncSourceNodes so that temporary changes survive */
    map<string, SyncSourceNodes> m_nodeCache;
};

/**
 * This set of config nodes is to be used by SyncSourceConfig
 * to accesss properties. Note that "const SyncSourceNodes"
 * only implies that the nodes cannot be changed; the properties
 * are still read- and writable. ConstSyncSourceNodes grants
 * only read access to properties.
 */
class SyncSourceNodes {
 public:
    SyncSourceNodes() : m_havePeerNode(false) {}

    /**
     * @param havePeerNode    false when peerNode is a dummy instance which has to
     *                        be ignored for properties which may exist there as
     *                        well as in the shared node (for example, "type")
     * @param sharedNode      node for user-visible properties, shared between peers
     * @param peerNode        node for user-visible, per-peer properties (the same
     *                        as sharedNode in SYNC4J_LAYOUT and HTTP_SERVER_LAYOUT)
     * @param hiddenPeerNode  node for internal, per-peer properties (the same as
     *                        sharedNode in SYNC4J_LAYOUT)
     * @param trackingNode    node for tracking changes (always different than the
     *                        other nodes)
     * @param serverNode      node for tracking items in a server (always different
     *                        than the other nodes)
     * @param cacheDir        a per-peer, per-source directory for exclusive use by
     *                        the SyncSource, must be created if needed; not available
     *                        when source is accessed independently of peer
     */
    SyncSourceNodes(bool havePeerNode,
                    const boost::shared_ptr<FilterConfigNode> &sharedNode,
                    const boost::shared_ptr<FilterConfigNode> &peerNode,
                    const boost::shared_ptr<ConfigNode> &hiddenPeerNode,
                    const boost::shared_ptr<ConfigNode> &trackingNode,
                    const boost::shared_ptr<ConfigNode> &serverNode,
                    const string &cacheDir);

    friend class SyncConfig;

    /** true if the peer-specific config node exists */
    bool exists() const { return m_peerNode->exists(); }

    /** true if the context-specific config node exists */
    bool dataConfigExists() const { return m_sharedNode->exists(); }

    /**
     * Returns the right config node for a certain property,
     * depending on visibility and sharing.
     */
    boost::shared_ptr<FilterConfigNode> getNode(const ConfigProperty &prop) const;

    /**
     * Read-write access to all configurable properties of the source.
     * The visible properties are passed through the config filter,
     * which can be modified.
     */
    boost::shared_ptr<FilterConfigNode> getProperties(bool hidden = false) const { return m_props[hidden]; }

    /** read-write access to SyncML server specific config node */
    boost::shared_ptr<ConfigNode> getServerNode() const { return m_serverNode; }

    /** read-write access to backend specific tracking node */
    boost::shared_ptr<ConfigNode> getTrackingNode() const { return m_trackingNode; }

    string getCacheDir() const { return m_cacheDir; }

 protected:
    const bool m_havePeerNode;
    const boost::shared_ptr<FilterConfigNode> m_sharedNode;
    const boost::shared_ptr<FilterConfigNode> m_peerNode;
    const boost::shared_ptr<ConfigNode> m_hiddenPeerNode;
    const boost::shared_ptr<ConfigNode> m_trackingNode;
    const boost::shared_ptr<ConfigNode> m_serverNode;
    const string m_cacheDir;

    /** multiplexer for the other nodes */
    boost::shared_ptr<FilterConfigNode> m_props[2];
};

/**
 * nop deleter for boost::shared_ptr<SyncConfig>
 */
struct SyncConfigNOP
{
    void operator() (SyncConfig *) {}
};

/**
 * same as SyncSourceNodes, but with only read access to properties
 */
class ConstSyncSourceNodes : private SyncSourceNodes
{
 public:
    ConstSyncSourceNodes(const SyncSourceNodes &other) :
       SyncSourceNodes(other)
    {}

    boost::shared_ptr<const FilterConfigNode> getProperties(bool hidden = false) const {
        return const_cast<SyncSourceNodes *>(static_cast<const SyncSourceNodes *>(this))->getProperties(hidden);
    }
    boost::shared_ptr<const ConfigNode> getServerNode() const { return m_serverNode; }
    boost::shared_ptr<const ConfigNode> getTrackingNode() const { return m_trackingNode; } 
};

struct SourceType {
    SourceType():m_forceFormat(false)
    {}

    /**
     * Parses the SyncEvolution <= 1.1 type specifier:
     * <backend>[:<format>[:<version>][!]]
     *
     * The <version> part is not stored anymore (was required by file
     * backend, but not actually used).
     */
    SourceType(const string &type);

    /**
     * converts back to SyncEvolution <= 1.1 type specifier
     */
    string toString() const;

    string m_backend; /**< identifies the SyncEvolution backend (either via a generic term like "addressbook" or a specific one like "Evolution Contacts") */
    string m_localFormat;  /**< the format to be used inside the backend for storing items; typically
                              hard-coded and not configurable */
    string m_format; /**< the format to be used (typically a MIME type) when talking to our peer */
    bool   m_forceFormat; /**< force to use the client's preferred format instead giving the engine and server a choice */
};

/**
 * This class maps per-source properties to ConfigNode properties.
 * Some properties are not configurable and have to be provided
 * by derived classes.
 */
class SyncSourceConfig {
 public:
    SyncSourceConfig(const string &name, const SyncSourceNodes &nodes);

    static ConfigPropertyRegistry &getRegistry();

    /**
     * Read-write access to all configurable properties of the source.
     * The visible properties are passed through the config filter,
     * which can be modified.
     */
    boost::shared_ptr<FilterConfigNode> getProperties(bool hidden = false) {
        return m_nodes.getProperties(hidden);
    }
    boost::shared_ptr<const FilterConfigNode> getProperties(bool hidden = false) const { return const_cast<SyncSourceConfig *>(this)->getProperties(hidden); }

    virtual std::string getName() const { return m_name.c_str(); }

    /**
     * Directory to be used by source when it needs to store
     * something per-peer in the file system. Currently not
     * configurable, set via SyncSourceNodes.
     */
    string getCacheDir() const { return m_nodes.getCacheDir(); }

    /**
     * Returns the right config node for a certain property,
     * depending on visibility and sharing.
     */
    boost::shared_ptr<FilterConfigNode> getNode(const ConfigProperty &prop) {
        return m_nodes.getNode(prop);
    }
    boost::shared_ptr<const FilterConfigNode> getNode(const ConfigProperty &prop) const {
        return m_nodes.getNode(prop);
    }

    /** access to SyncML server specific config node */
    boost::shared_ptr<ConfigNode> getServerNode() { return m_nodes.getServerNode(); }
    boost::shared_ptr<const ConfigNode> getServerNode() const { return m_nodes.getServerNode(); }

    /** access to backend specific tracking node */
    boost::shared_ptr<ConfigNode> getTrackingNode() { return m_nodes.getTrackingNode(); }
    boost::shared_ptr<const ConfigNode> getTrackingNode() const { return m_nodes.getTrackingNode(); }

    /** sync mode for sync sources */
    static StringConfigProperty m_sourcePropSync;

    /** true if the source config exists with view-specific properties (not just default or shared ones) */
    bool exists() const { return m_nodes.exists(); }

    /** checks if a certain property is set to a non-empty value */
    bool isSet(ConfigProperty &prop) {
        return prop.isSet(*getProperties(prop.isHidden()));
    }

    virtual std::string getUser() const;
    virtual void setUser(const string &value, bool temporarily = false);

    virtual std::string getPassword() const;
    virtual void setPassword(const string &value, bool temporarily = false);

    /** same as SyncConfig::checkPassword() but with
     * an extra argument globalConfigNode for source config property
     * may need global config node to check password */
    virtual void checkPassword(ConfigUserInterface &ui, const string &serverName, FilterConfigNode& globalConfigNode);

    /** same as SyncConfig::savePassword() */
    virtual void savePassword(ConfigUserInterface &ui, const string &serverName, FilterConfigNode& globalConfigNode);

    /** selects the backend database to use */
    virtual std::string getDatabaseID() const;
    virtual void setDatabaseID(const string &value, bool temporarily = false);

    /**
     * internal property: unique integer ID for the source, needed by Synthesis XML <dbtypeid>,
     * zero if unset
     */
    virtual const int getSynthesisID() const;
    virtual void setSynthesisID(int value, bool temporarily = false);

    /**
     * Returns the data source type configured as part of the given
     * configuration; different SyncSources then check whether
     * they support that type. This call has to work before instantiating
     * a source and thus gets passed a node to read from.
     */
    static SourceType getSourceType(const SyncSourceNodes &nodes);
    virtual SourceType getSourceType() const;

    /** set source backend and formats in one step */
    virtual void setSourceType(const SourceType &type, bool temporarily = false);

    virtual void setBackend(const std::string &value, bool temporarily = false);
    virtual std::string getBackend() const;
    virtual void setDatabaseFormat(const std::string &value, bool temporarily = false);
    virtual std::string getDatabaseFormat() const;
    virtual void setSyncFormat(const std::string &value, bool temporarily = false);
    virtual std::string getSyncFormat() const;
    virtual void setForceSyncFormat(bool value, bool temporarily = false);
    virtual bool getForceSyncFormat() const;

    /**
     * Returns the SyncSource URI: used in SyncML to address the data
     * on the server.
     *
     * Each URI has to be unique during a sync session, i.e.
     * two different sync sources cannot access the same data at
     * the same time.
     */
    virtual std::string getURI() const;
    virtual void setURI(const string &value, bool temporarily = false);

    /**
     * like getURI(), but instead of returning an empty string when
     * not configured, return the source name
     */
    virtual std::string getURINonEmpty() const;


    /**
     * Gets the default syncMode.
     *
     * Sync modes can be one of:
     * - disabled
     * - slow
     * - two-way
     * - one-way-from-server
     * - one-way-from-client
     * - refresh-from-server
     * - refresh-from-client
     */
    virtual std::string getSync() const;
    virtual void setSync(const string &value, bool temporarily = false);

 private:
    string m_name;
    SyncSourceNodes m_nodes;
    string m_cachedPassword;
};

class SingleFileConfigTree;

/**
 * Representing a configuration template node used for fuzzy matching.
 */
class TemplateConfig
{
    boost::shared_ptr<SingleFileConfigTree> m_template;
    ConfigProps m_metaProps;
    string m_id;
    string m_templateName;
public:
    TemplateConfig (const string &path);
    enum {
        NO_MATCH = 0,
        LEVEL1_MATCH = 1,
        LEVEL2_MATCH = 2,
        LEVEL3_MATCH = 3,
        LEVEL4_MATCH = 4,
        BEST_MATCH=5
    };
    static bool isTemplateConfig (const string &path);
    bool isTemplateConfig() const;
    virtual int metaMatch (const string &fingerprint, SyncConfig::MatchMode mode);
    virtual int serverModeMatch (SyncConfig::MatchMode mode);
    virtual int fingerprintMatch (const string &fingerprint);
    virtual string getTemplateId ();
    virtual string getDescription();
    virtual string getFingerprint();
    virtual string getTemplateName();
};


/**@}*/


SE_END_CXX
#endif
