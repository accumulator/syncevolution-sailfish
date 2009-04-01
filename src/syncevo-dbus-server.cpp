
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>


#include <dbus/dbus-glib-bindings.h>

#include "syncevo-dbus-server.h"
#include "syncevo-marshal.h"

static gboolean syncevo_start_sync (SyncevoDBusServer *obj, char *server, GPtrArray *sources, GError **error);
static gboolean syncevo_abort_sync (SyncevoDBusServer *obj, char *server, GError **error);
static gboolean syncevo_set_password (SyncevoDBusServer *obj, char *server, char *password, GError **error);
static gboolean syncevo_get_servers (SyncevoDBusServer *obj, char ***servers, GError **error);
static gboolean syncevo_get_server_config (SyncevoDBusServer *obj, char *server, GPtrArray **options, GError **error);
static gboolean syncevo_set_server_config (SyncevoDBusServer *obj, char *server, GPtrArray *options, GError **error);
#include "syncevo-dbus-glue.h"

#define SYNCEVO_SOURCE_TYPE (dbus_g_type_get_struct ("GValueArray", G_TYPE_STRING, G_TYPE_INT, G_TYPE_INVALID))
typedef GValueArray SyncevoSource;

#define SYNCEVO_OPTION_TYPE (dbus_g_type_get_struct ("GValueArray", G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID))
typedef GValueArray SyncevoOption;

SyncevoSource*
syncevo_source_new (char *name, int mode)
{
	GValue val = {0, };

	g_value_init (&val, SYNCEVO_SOURCE_TYPE);
	g_value_take_boxed (&val, dbus_g_type_specialized_construct (SYNCEVO_SOURCE_TYPE));
	dbus_g_type_struct_set (&val, 0, name, 1, mode, G_MAXUINT);

	return (SyncevoSource*) g_value_get_boxed (&val);
}

void
syncevo_source_get (SyncevoSource *source, const char **name, int *mode)
{
	if (name) {
		*name = g_value_get_string (g_value_array_get_nth (source, 0));
	}
	if (mode) {
		*mode = g_value_get_int (g_value_array_get_nth (source, 1));
	}
}

static void
syncevo_source_add_to_set (SyncevoSource *source, set<string> source_set)
{
	const char *str;
	
	syncevo_source_get (source, &str, NULL);
	source_set.insert (g_strdup (str));
}

void
syncevo_source_free (SyncevoSource *source)
{
	if (source) {
		g_boxed_free (SYNCEVO_SOURCE_TYPE, source);
	}
}

SyncevoOption*
syncevo_option_new (char *ns, char *key, char *value)
{
	GValue val = {0, };

	g_value_init (&val, SYNCEVO_OPTION_TYPE);
	g_value_take_boxed (&val, dbus_g_type_specialized_construct (SYNCEVO_OPTION_TYPE));
	dbus_g_type_struct_set (&val, 0, ns, 1, key, 2, value, G_MAXUINT);

	return (SyncevoOption*) g_value_get_boxed (&val);
}

void
syncevo_option_get (SyncevoOption *option, const char **ns, const char **key, const char **value)
{
	if (ns) {
		*ns = g_value_get_string (g_value_array_get_nth (option, 0));
	}
	if (key) {
		*key = g_value_get_string (g_value_array_get_nth (option, 1));
	}
	if (value) {
		*value = g_value_get_string (g_value_array_get_nth (option, 2));
	}
}

void
syncevo_option_free (SyncevoOption *option)
{
	if (option) {
		g_boxed_free (SYNCEVO_OPTION_TYPE, option);
	}
}


enum {
	PROGRESS,
	SOURCE_PROGRESS,
	SERVER_MESSAGE,
	NEED_PASSWORD,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (SyncevoDBusServer, syncevo_dbus_server, G_TYPE_OBJECT);

void
emit_progress (int type,
               int extra1,
               int extra2,
               int extra3,
               gpointer data)
{
	SyncevoDBusServer *obj = (SyncevoDBusServer *)data;

	g_signal_emit (obj, signals[PROGRESS], 0,
	               "servername",
	               type,
	               extra1,
	               extra2);

	/* TODO should check for aborted */
}

void
emit_source_progress (const char *source,
                      int type,
                      int extra1,
                      int extra2,
                      int extra3,
                      gpointer data)
{
	SyncevoDBusServer *obj = (SyncevoDBusServer *)data;

	g_signal_emit (obj, signals[SOURCE_PROGRESS], 0,
	               "servername",
	               source,
	               type,
	               extra1,
	               extra2);

	/* TODO should check for aborted */
}

void
emit_server_message (const char *message,
                     gpointer data)
{
	SyncevoDBusServer *obj = (SyncevoDBusServer *)data;

	g_signal_emit (obj, signals[SERVER_MESSAGE], 0,
	               "servername",
	               message);

	/* TODO  should check for aborted */
}

char*
need_password (const char *message,
               gpointer data)
{
	SyncevoDBusServer *obj = (SyncevoDBusServer *)data;

	g_signal_emit (obj, signals[NEED_PASSWORD], 0);

	/* TODO */
}



static gboolean 
do_sync (SyncevoDBusServer *obj)
{
	GError *error = NULL;
	int ret;

	SyncReport report;
	g_debug ("syncing...");
	ret = (*obj->client).sync(&report);
	if (ret != 0) {
		g_printerr ("SyncEvolution returned error %d\n", ret);
	}

	/* TODO need to add a signal for 'sync finished' with ret as payload */
	delete obj->client;
	g_free (obj->server);
	obj->server = NULL;
	obj->sources = NULL;

	return FALSE;
}

static gboolean  
syncevo_start_sync (SyncevoDBusServer *obj, 
                    char *server,
                    GPtrArray *sources,
                    GError **error)
{
	if (obj->server) {
		*error = g_error_new (g_quark_from_static_string ("syncevo-dbus-server"),
		                      1, "Sync already in progress. Concurrent syncs are currently not supported");
		return FALSE;
	}
	if (!server) {
		*error = g_error_new (g_quark_from_static_string ("syncevo-dbus-server"),
		                      1, "Server variable must be set");
		return FALSE;
	}

	obj->aborted = FALSE;
	obj->server = g_strdup (server);

	set<string> source_set = set<string>();
	g_ptr_array_foreach (sources, (GFunc)syncevo_source_add_to_set, &source_set);

	obj->client = new DBusSyncClient (string (server), source_set, 
	                                  emit_progress, emit_source_progress, emit_server_message, need_password,
	                                  obj);

	g_idle_add ((GSourceFunc)do_sync, obj); 

	return TRUE;
}

static gboolean 
syncevo_abort_sync (SyncevoDBusServer *obj,
                            char *server,
                            GError **error)
{
	if (!server) {
		*error = g_error_new (g_quark_from_static_string ("syncevo-dbus-server"),
		                      1, "Server variable must be set");
		return FALSE;
	}

	if (strcmp (server, obj->server) != 0) {
		*error = g_error_new (g_quark_from_static_string ("syncevo-dbus-server"),
		                      1, "Not syncing server '%s'", server);
		return FALSE;
	}

	obj->aborted = TRUE;

	return TRUE;
}

static gboolean 
syncevo_set_password (SyncevoDBusServer *obj,
                              char *server,
                              char *password,
                              GError **error)
{
	*error = g_error_new (g_quark_from_static_string ("syncevo-dbus-server"),
	                      1, "SetPassword not supported yet");

	return FALSE;
	
}
static gboolean 
syncevo_get_servers (SyncevoDBusServer *obj,
                     char ***servers,
                     GError **error)
{
	int i = 0;

	if (!servers) {
		return FALSE;
	}

	EvolutionSyncConfig::ServerList list = EvolutionSyncConfig::getServers();
	*servers = (char**)g_malloc0 (sizeof(char*) * list.size() + 1);

	BOOST_FOREACH(const EvolutionSyncConfig::ServerList::value_type &server,list) {
		*servers[i++] = g_strdup (server.first.c_str());
	}
	
	return TRUE;
}
static gboolean 
syncevo_get_server_config (SyncevoDBusServer *obj,
                           char *server,
                           GPtrArray **options,
                           GError **error)
{
	SyncevoOption *option;

	if (!server || !options) {
		*error = g_error_new (g_quark_from_static_string ("syncevo-dbus-server"),
		                      1, "Server and options parameters must be given");
		return FALSE;
	}

	if (obj->server) {
		*error = g_error_new (g_quark_from_static_string ("syncevo-dbus-server"),
		                      1, "GetServerConfig is currently not supported when a sync is in progress");
		return FALSE;
	}

	*options = g_ptr_array_new ();
	const string srv (server);

	boost::shared_ptr<EvolutionSyncConfig> config(new EvolutionSyncConfig (string (server)));

	option = syncevo_option_new (NULL, g_strdup ("syncURL"), g_strdup(config->getSyncURL()));
	g_ptr_array_add (*options, option);
	option = syncevo_option_new (NULL, g_strdup("username"), g_strdup(config->getUsername()));
	g_ptr_array_add (*options, option);


	list<string> sources = config->getSyncSources();
	BOOST_FOREACH(const string &name, sources) {
		boost::shared_ptr<EvolutionSyncSourceConfig> source_config = config->getSyncSourceConfig(name);

		option = syncevo_option_new (g_strdup (name.c_str()), g_strdup ("sync"), g_strdup (source_config->getSync()));
		option = syncevo_option_new (g_strdup (name.c_str()), g_strdup ("sync"), g_strdup (source_config->getURI()));

	}

	/* TODO which options do we need ? */

	return TRUE;
}

static gboolean 
syncevo_set_server_config (SyncevoDBusServer *obj,
                                   char *server,
                                   GPtrArray *options,
                                   GError **error)
{
	if (!server || !options) {
		*error = g_error_new (g_quark_from_static_string ("syncevo-dbus-server"),
		                      1, "Server and options parameters must be given");
		return FALSE;
	}

	if (obj->server) {
		*error = g_error_new (g_quark_from_static_string ("syncevo-dbus-server"),
		                      1, "GetServers is currently not supported when a sync is in progress");
		return FALSE;
	}

	/* TODO */

	*error = g_error_new (g_quark_from_static_string ("syncevo-dbus-server"),
	                      1, "not implemented yet");

	return FALSE;
}

static void
syncevo_dbus_server_class_init(SyncevoDBusServerClass *klass)
{
	GError *error = NULL;

	klass->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (klass->connection == NULL)
	{
		g_warning("Unable to connect to dbus: %s", error->message);
		g_error_free (error);
		return;
	}

	signals[PROGRESS] = g_signal_new ("progress",
	                                  G_TYPE_FROM_CLASS (klass),
	                                  (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE),
	                                  G_STRUCT_OFFSET (SyncevoDBusServerClass, progress),
	                                  NULL, NULL,
	                                  syncevo_marshal_VOID__STRING_INT_INT_INT_INT,
	                                  G_TYPE_NONE, 
	                                  5, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);
	signals[SOURCE_PROGRESS] = g_signal_new ("source-progress",
	                                  G_TYPE_FROM_CLASS (klass),
	                                  (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE),
	                                  G_STRUCT_OFFSET (SyncevoDBusServerClass, source_progress),
	                                  NULL, NULL,
	                                  syncevo_marshal_VOID__STRING_STRING_INT_INT_INT_INT,
	                                  G_TYPE_NONE, 
	                                  6, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);
	signals[SERVER_MESSAGE] = g_signal_new ("server-message",
	                                  G_TYPE_FROM_CLASS (klass),
	                                  (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE),
	                                  G_STRUCT_OFFSET (SyncevoDBusServerClass, server_message),
	                                  NULL, NULL,
	                                  syncevo_marshal_VOID__STRING_STRING,
	                                  G_TYPE_NONE, 
	                                  2, G_TYPE_STRING, G_TYPE_STRING);
	signals[NEED_PASSWORD] = g_signal_new ("need-password",
	                                  G_TYPE_FROM_CLASS (klass),
	                                  (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE),
	                                  G_STRUCT_OFFSET (SyncevoDBusServerClass, need_password),
	                                  NULL, NULL,
	                                  g_cclosure_marshal_VOID__VOID,
	                                  G_TYPE_NONE, 
	                                  0);

	/* dbus_glib_syncevo_object_info is provided in the generated glue file */
	dbus_g_object_type_install_info (SYNCEVO_TYPE_DBUS_SERVER, &dbus_glib_syncevo_object_info);
}

static void
syncevo_dbus_server_init(SyncevoDBusServer *obj)
{
	GError *error = NULL;
	DBusGProxy *proxy;
	guint request_ret;
	SyncevoDBusServerClass *klass = SYNCEVO_DBUS_SERVER_GET_CLASS (obj);

	dbus_g_connection_register_g_object (klass->connection,
	                                     "/org/Moblin/SyncEvolution",
	                                     G_OBJECT (obj));

	proxy = dbus_g_proxy_new_for_name (klass->connection,
	                                   DBUS_SERVICE_DBUS,
	                                   DBUS_PATH_DBUS,
	                                   DBUS_INTERFACE_DBUS);

	if(!org_freedesktop_DBus_request_name (proxy,
	                                       "org.Moblin.SyncEvolution",
	                                       0, &request_ret,
	                                       &error)) {
		g_warning("Unable to register service: %s", error->message);
		g_error_free (error);
	}
	g_object_unref (proxy);

}

GMainLoop *loop;

void niam(int sig)
{
	g_main_loop_quit (loop);
}

int main()
{
	SyncevoDBusServer *server;

	signal(SIGTERM, niam);
	signal(SIGINT, niam);

	g_type_init ();

	server = (SyncevoDBusServer*)g_object_new (SYNCEVO_TYPE_DBUS_SERVER, NULL);

	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);

	g_main_loop_unref (loop);
	g_object_unref (server);
	return 0;
}
