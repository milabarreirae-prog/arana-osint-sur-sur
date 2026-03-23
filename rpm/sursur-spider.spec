%define debug_package %{nil}
Name:           sursur-spider
Version:        0.1.0
Release:        8%{?dist}
Summary:        Motor de Inteligencia Contrahegemónica OSINT Sur-Sur

License:        GPLv3
URL:            http://localhost:1973
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++, cmake, systemd-rpm-macros, libcurl-devel, pugixml-devel, spdlog-devel
Requires:       systemd, curl, ffmpeg-free, nginx

%description
Demonio de recolección y curación algorítmica.

%prep
%setup -q -c

%build

export CXXFLAGS="-O2"
export LDFLAGS="-lcurl"
%cmake
%cmake_build

%install
%cmake_install
mkdir -p %{buildroot}/etc/sursur
mkdir -p %{buildroot}/var/lib/sursur/media
mkdir -p %{buildroot}/var/lib/sursur/ui
mkdir -p %{buildroot}%{_unitdir}
install -m 0644 systemd/sursur-daemon.service %{buildroot}%{_unitdir}/
install -m 0644 systemd/sursur-daemon.timer %{buildroot}%{_unitdir}/
# Copiar configuraciones y frontend desde el origen (.. ya que %setup entró a %{name}-%{version})
cp conf/sursur_config.ini %{buildroot}/etc/sursur/sursur_config.ini
cp config/seeds_arsenal_soberania.txt %{buildroot}/etc/sursur/seeds_arsenal_soberania.txt
cp -r ui/* %{buildroot}/var/lib/sursur/ui/

%pre
getent group milab >/dev/null || groupadd -r milab
getent passwd milab >/dev/null || useradd -r -g milab -d /var/lib/sursur -s /sbin/nologin -c "Araña OSINT" milab
exit 0

%post
%systemd_post sursur-daemon.service sursur-daemon.timer

%preun
%systemd_preun sursur-daemon.service sursur-daemon.timer

%postun
%systemd_postun_with_restart sursur-daemon.service

%files
%{_bindir}/sur-sur-daemon
%{_unitdir}/sursur-daemon.service
%{_unitdir}/sursur-daemon.timer
%config(noreplace) /etc/sursur/sursur_config.ini
/etc/sursur/seeds_arsenal_soberania.txt
/var/lib/sursur/ui/
%dir /var/lib/sursur/media

%changelog
* Sat Mar 21 2026 Camila Angola <milab@localhost> - 0.1.0-2
- Integración de Sandboxing y Timer Systemd.
