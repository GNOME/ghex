
%define name ghex
%define version 1.1.3
%define release 1
%define prefix /usr

Summary: GNOME binary editor

Name: %{name}
Version: %{version}
Release: %{release}
Group: Applications/Internet
Copyright: GPL

Url: http://pluton.ijs.si/~jaka/gnome.html#GHEX

Source: ftp://ftp.gnome.org/pub/GNOME/stable/sources/ghex/ghex-%{version}.tar.gz
Buildroot: /var/tmp/%{name}-%{version}-%{release}-root

%description
GHex allows the user to load data from any file, view and edit it in either
hex or ascii. A must for anyone playing games that use non-ascii format for
saving.

%prep

%setup

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix}
make

%install
if [ -d $RPM_BUILD_ROOT ]; then rm -r $RPM_BUILD_ROOT; fi
mkdir -p $RPM_BUILD_ROOT%{prefix}
make prefix=$RPM_BUILD_ROOT%{prefix} install-strip

%files
%defattr(-,root,root)
%doc README
%attr(755,root,root) %{prefix}/bin/ghex
%{prefix}/share/gnome/apps/Applications/ghex.desktop
%{prefix}/share/gnome/help/ghex
%{prefix}/share/pixmaps/gnome-ghex.png
%{prefix}/share/locale/*/*/*

%clean
rm -r $RPM_BUILD_ROOT

%changelog
* Sun May 14 2000 John Gotts <jgotts@linuxsavvy.com>
- New SPEC file.
