#! /bin/sh
#
# Wrapper script which starts a new D-Bus session before
# running a program and kills the D-Bus daemon when done.

# start D-Bus session
unset DBUS_SESSION_BUS_ADDRESS DBUS_SESSION_BUS_PID
eval `dbus-launch`
export DBUS_SESSION_BUS_ADDRESS

if [ "$DBUS_SESSION_SH_SYSTEM_BUS" ]; then
    # Use own private bus as system bus, then start a new one.
    DBUS_SYSTEM_BUS_PID=$DBUS_SESSION_BUS_PID
    DBUS_SYSTEM_BUS_ADDRESS=$DBUS_SESSION_BUS_ADDRESS
    export DBUS_SYSTEM_BUS_ADDRESS
    eval `dbus-launch`
fi

# Ensure that XDG dirs exist. Otherwise some daemons do not work correctly.
createxdg() {
    dir="$1"

    if [ "$dir" ]; then
        mkdir -p "$dir"
    fi
}
createxdg "$XDG_CONFIG_HOME"
createxdg "$XDG_CACHE_HOME"
createxdg "$XDG_DATA_HOME"

# Work-around for GNOME keyring daemon not started
# when accessed via org.freedesktop.secrets: start it
# explicitly.
# See https://launchpad.net/bugs/525642 and
# https://bugzilla.redhat.com/show_bug.cgi?id=572137
if [ -x /usr/bin/gnome-keyring-daemon ]; then
    /usr/bin/gnome-keyring-daemon --start --foreground --components=secrets 1>&2 &
    KEYRING_PID=$!
fi

# kill all programs started by us
atexit() {
    [ ! "$KEYRING_PID" ] || ( echo >&2 "dbus-session.sh $$: killing keyring pid $KEYRING_PID"; kill -9 $KEYRING_PID )
    [ ! "$DBUS_SESSION_SH_SYSTEM_BUS" ] || [ ! "$DBUS_SYSTEM_BUS_PID" ] || ( echo >&2 "dbus-session.sh $$: killing system bus daemon $DBUS_SYSTEM_BUS_PID"; kill -9 $DBUS_SYSTEM_BUS_PID )
    [ ! "$DBUS_SESSION_BUS_PID" ] || ( echo >&2 "dbus-session.sh $$: killing session bus daemon $DBUS_SESSION_BUS_PID"; kill -9 $DBUS_SESSION_BUS_PID )
}
trap atexit EXIT

# If DBUS_SESSION_SH_EDS_BASE is set and our main program runs
# under valgrind, then also check EDS. Otherwise start EDS only
# for certain known operations which need EDS (syncevolution, client-test)
# but not others (make, configure).
#
# DBUS_SESSION_SH_EDS_BASE must be the directory which contains
# e-addressbook/calendar-factory.
#
# Similar for DBUS_SESSION_SH_AKONADI, except that we rely on akonadictl
# and don't run it under valgrind. We also don't start it for test-dbus.py,
# because starting it with "normal" XDG env variables and trying to
# connect to it with XDG env set in test-dbus.py failed:
# libakonadi Akonadi::SessionPrivate::init: Akonadi Client Session: connection config file ' akonadi/akonadiconnectionrc can not be found in ' "/data/runtests/work/lucid-amd64/build/src/temp-test-dbus/config" ' nor in any of  ("/etc/xdg", "/etc")
#
E_CAL_PID=
E_BOOK_PID=
case "$@" in *valgrind*) prefix=`echo $@ | perl -p -e 's;.*?(\S*/?valgrind\S*).*;$1;'`;;
             *setup-syncevolution.sh*|*syncevolution\ *|*client-test\ *|*bash*|*testpim.py\ *|*test-dbus.py\ *|*gdb\ *) prefix=env;;
             *) prefix=;; # don't start EDS
esac
case "$TEST_DBUS_PREFIX" in *valgrind*) prefix="$TEST_DBUS_PREFIX";;
esac
akonadi=$prefix
case "$@" in *test-dbus.py\ *) akonadi=;;
esac

if [ "$DBUS_SESSION_SH_AKONADI" ] && [ "$akonadi" ]; then
    # Prevents running unnecessary daemons which do not necessarily
    # work (see "[Kde-pim] configuring Akonadi server").
    export AKONADI_DISABLE_AGENT_AUTOSTART=1
    akonadictl start 1>&2
    SLEEP=5
else
    DBUS_SESSION_SH_AKONADI=
fi
if [ "$DBUS_SESSION_SH_EDS_BASE" ] && [ "$prefix" ]; then
    $prefix $DBUS_SESSION_SH_EDS_BASE/evolution-calendar-factory --keep-running 1>&2 &
    E_CAL_PID=$!
    $prefix $DBUS_SESSION_SH_EDS_BASE/evolution-addressbook-factory --keep-running 1>&2 &
    E_BOOK_PID=$!

    # give daemons some time to start and register with D-Bus
    SLEEP=5
else
    DBUS_SESSION_SH_EDS_BASE=
fi

[ "$SLEEP" ] && sleep $SLEEP

# run program
"$@"
res=$?
echo dbus-session.sh: program returned $res >&2

# Now also shut down EDS, if started by us. Update total result code if any of them
# failed the valgrind check.
shutdown () {
    pid=$1
    program=$2

    # give process 20 seconds to shut down
    i=0
    while [ "$pid" ] && ps | grep -q "$pid" && [ $i -lt 20 ]; do
	sleep 1
	i=`expr $i + 1`
    done
    if [ "$pid" ] && ps | grep -q "$pid"; then
	kill -9 "$pid" 2>/dev/null
    fi
    wait "$pid"
    subres=$?
    case $subres in 0|130|137|143) true;; # 130 and 143 indicate that it was killed, probably by us
                    *) echo $program failed with return code $subres >&2
	               if [ $res -eq 0 ]; then
                           res=$subres
                       fi
		       ;;
    esac
}

if [ "$DBUS_SESSION_SH_AKONADI" ]; then
    akonadictl stop
fi

if [ "$DBUS_SESSION_SH_EDS_BASE" ]; then
    kill "$E_CAL_PID" 2>/dev/null
    kill "$E_BOOK_PID" 2>/dev/null
    shutdown "$E_CAL_PID" "$DBUS_SESSION_SH_EDS_BASE/evolution-calendar-factory"
    shutdown "$E_BOOK_PID" "$DBUS_SESSION_SH_EDS_BASE/evolution-addressbook-factory"
fi

echo dbus-session.sh: final result $res >&2
exit $res
