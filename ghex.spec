%define nam   ghex
%define ver   2.6.2
%define rel   0.2

Summary: GNOME binary editor
Name:            %{nam}
Version:         %{ver}
Release:         %{rel}
Group: Applications/Editors
Copyright: GPL
Url:             "http://pluton.ijs.si/~jaka/gnome.html#GHEX"
Source: ftp://ftp.gnome.org/pub/GNOME/stable/sources/ghex/ghex-%{version}.tar.gz
Buildroot: /var/tmp/%{name}-%{version}-%{release}-root
BuildRequires:   gnome-libs-devel, ORBit
BuildRequires:   gtk+-devel >= 1.2.0
BuildRequires:   gnome-print-devel >= 0.24

%description
GHex allows the user to load data from any file, view and edit it in either
hex or ascii. A must for anyone playing games that use non-ascii format for
saving.

%prep

%setup -q

%build
%ifarch alpha
  MYARCH_FLAGS="--host=alpha-redhat-linux"
%endif

if [ ! -f configure ]; then
    CFLAGS="$RPM_OPT_FLAGS" ./autogen.sh $MYARCH_FLAGS \
	--prefix=%{_prefix} --bindir=%{_bindir} --datadir=%{_datadir} \
	--sysconfdir=%{_sysconfdir}
else
    CFLAGS="$RPM_OPT_FLAGS" ./configure $MYARCH_FLAGS --prefix=%{_prefix} \
        --bindir=%{_bindir} --datadir=%{_datadir} --sysconfdir=%{_sysconfdir}
fi


if [ "$SMP" != "" ]; then
  (make "MAKE=make -k -j $SMP"; exit 0)
  make
else
  make
fi

%install
if [ -d $RPM_BUILD_ROOT ]; then rm -r $RPM_BUILD_ROOT; fi
mkdir -p $RPM_BUILD_ROOT%{_prefix}

make prefix=$RPM_BUILD_ROOT%{_prefix} bindir=$RPM_BUILD_ROOT%{_bindir} \
    datadir=$RPM_BUILD_ROOT%{_datadir} \
    sysconfdir=$RPM_BUILD_ROOT%{_sysconfdir} install

%files
%defattr(-,root,root)
%doc README COPYING AUTHORS
%attr(755,root,root) %{_bindir}/*
%{_datadir}/applications/*
%{_datadir}/gnome/help/ghex2
%{_datadir}/pixmaps/*
%{_datadir}/locale/*/*/*
%{_datadir}/gnome-2.0/ui/*
%{_sysconfdir}/gconf/schemas/*

%clean
rm -r $RPM_BUILD_ROOT

%changelog
* Sun Oct 27 2002 Dan Hensley <dan.hensley@attbi.com>
- Fix RPM build errors

* Wed Feb 21 2001 Gregory Leblanc <gleblanc@cu-portland.edu>
- removed hard-coded paths, updated macros.

* Sun Oct 22 2000 John Gotts <jgotts@linuxsavvy.com>
- Minor modifications.

* Sun May 14 2000 John Gotts <jgotts@linuxsavvy.com>
- New SPEC file.
