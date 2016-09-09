Name: syncevolution
Summary: SyncEvolution - a SyncML and CalDAV/CardDAV client
Version: 1.3.99.7
Release: 1
License: GPL
Source: syncevolution-%{version}-%{release}.tar.gz
BuildRequires: gettext glib2-devel boost-devel pcre-devel cppunit-devel
BuildRequires: libcurl-devel sqlite-devel libxml2-devel openssl-devel zlib-devel
BuildRequires: mkcal-qt5-devel kcalcore-qt5-devel libical-devel
BuildRequires: libaccounts-glib-devel libsignon-glib-devel dbus-glib-devel

%description
SyncEvolution allows you to sync your contacts and calendar with SyncML
and CalDAV/CardDAV servers on the Internet, such as Funambol, Ovi,
Google Contacts, and Google Calendar.

# Host is used for DAV autoconfiguration
%global hostdir host-20000331
%global host_bin %{hostdir}/host

# Neon is used for DAV connections
%global neondir neon27-0.29.3
%global neon_build %{neondir}/src/libneon.la
%global neon_flags --disable-shared --enable-static --enable-threadsafe-ssl=posix --with-libxml2 --with-ssl=openssl
%global neon_cflags -I%{neondir}/src -D_LARGEFILE64_SOURCE -DNE_LFS
%global neon_libs -L%{neondir}/src -lneon -lssl -lcrypto -lxml2 -lz

%prep
%setup -q -n syncevolution

%build
# build Host
(cd %{hostdir} && make)

# build Neon
(cd %{neondir} && ./configure --disable-shared --enable-static \
 --enable-threadsafe-ssl=posix --with-libxml2 --with-ssl=openssl \
 --prefix=/usr --docdir=/usr/share/syncevolution/doc --mandir=\$${prefix}/share/man --infodir=\$${prefix}/share/info \
 --sysconfdir=/etc && make)

# build SyncEvolution itself
LDFLAGS="-Wl,--as-needed" \
NEON_CFLAGS="%{neon_cflags}" \
NEON_LIBS="%{neon_libs}" \
./configure --enable-sailfish --enable-shared --disable-static \
 --disable-ssl-certificate-check --disable-goa \
 --disable-libsoup --enable-libcurl --disable-ecal --disable-ebook \
 --enable-qtcontacts --enable-kcalextended \
 --disable-bluetooth --enable-dav --with-synthesis-src=libsynthesis \
 --enable-dbus-service --enable-qt-dbus \
 --prefix=/usr --docdir=/usr/share/syncevolution/doc --mandir=\$${prefix}/share/man --infodir=\$${prefix}/share/info \
 --sysconfdir=/etc && make

# build the Sailfish UI
(cd qt-ui && qmake sailfish-ui.pro && make)

%install
make DESTDIR=%{buildroot} install
install %{host_bin} %{buildroot}/usr/lib/syncevolution
install qt-ui/sync-ui %{buildroot}/usr/bin
cp -r qt-ui/sailfish/qml %{buildroot}/usr/share/syncevolution
install -d %{buildroot}/usr/share/applications
install -d %{buildroot}/usr/share/icons/hicolor/86x86/apps
install qt-ui/sailfish/syncevolution.desktop %{buildroot}/usr/share/applications
install qt-ui/sailfish/syncevolution.png %{buildroot}/usr/share/icons/hicolor/86x86/apps
%find_lang %{name}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files -f %{name}.lang
# The VirtualBox filesystem doesn't preserve Unix permissions,
# so it's best to explicitly set the permissions to use.
%defattr(755, root, root, 755)
/usr/bin/sync-ui
# The process that needs to access PIM data must be sgid "privileged".
# If D-Bus is enabled, that would be syncevo-dbus-server,
# otherwise it would be syncevolution itself.
/usr/bin/syncevolution
/usr/bin/syncevo-phone-config
/usr/bin/syncevo-webdav-lookup
/usr/bin/synclog2html
/usr/bin/synccompare
/usr/libexec/syncevo-local-sync
%defattr(644, root, root, 755)
/usr/lib/libsyncevolution.so.0.*
/usr/lib/libgdbussyncevo.so.0.*
/usr/lib/libsmltk.so.0.*
/usr/lib/libsynthesis.so.0.*
/usr/lib/libsyncevolution.so.0
/usr/lib/libgdbussyncevo.so.0
/usr/lib/libsmltk.so.0
/usr/lib/libsynthesis.so.0
/usr/lib/syncevolution
/usr/share/syncevolution
/usr/share/applications/syncevolution.desktop
/usr/share/icons/hicolor/86x86/apps/syncevolution.png
%doc NEWS README COPYING
# The following files are only generated with --enable-dbus-service
/etc/xdg/autostart/syncevo-dbus-server.desktop
%defattr(755, root, root, 755)
/usr/bin/syncevo-http-server
/usr/libexec/syncevo-dbus-helper
%attr(2755, -, privileged) /usr/libexec/syncevo-dbus-server
/usr/libexec/syncevo-dbus-server-startup.sh
%defattr(644, root, root, 755)
/usr/share/dbus-1/services/org.syncevolution.service
# The following files are only generated with --enable-qt-dbus
/usr/lib/libsyncevolution-qt-dbus.so.0
/usr/lib/libsyncevolution-qt-dbus.so.0.*

%package devel
Summary: SynceEvolution development files

%description devel
Headers and libraries for SyncEvolution development.

%files devel
%defattr(644, root, root, 755)
/usr/include/syncevo
/usr/include/synthesis
/usr/lib/libsyncevolution.so
/usr/lib/libgdbussyncevo.so
/usr/lib/libsmltk.so
/usr/lib/libsynthesis.so
/usr/lib/libsynthesissdk.a
/usr/lib/libsynthesisstubs.a
/usr/lib/libsyncevolution.la
/usr/lib/libgdbussyncevo.la
/usr/lib/libsmltk.la
/usr/lib/libsynthesis.la
/usr/lib/libsynthesissdk.la
/usr/lib/libsynthesisstubs.la
/usr/lib/pkgconfig/syncevolution.pc
/usr/lib/pkgconfig/synthesis-sdk.pc
/usr/lib/pkgconfig/synthesis.pc
# The following files are only generated with --enable-qt-dbus
/usr/include/syncevolution-qt-dbus
/usr/lib/libsyncevolution-qt-dbus.so
/usr/lib/libsyncevolution-qt-dbus.la
/usr/lib/pkgconfig/syncevolution-qt-dbus.pc
