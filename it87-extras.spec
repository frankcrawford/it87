%if 0%{?fedora}
%global debug_package %{nil}
%endif

%global srcname it87
%global srcversion master

Name:     it87-extras
Version:  {{{ git_dir_version }}}
Release:  1%{?dist}
Summary:  Linux Driver for ITE LPC chips
License:  GPLv2
URL:      https://github.com/frankcrawford/it87


Source:   %{url}/archive/refs/heads/%{srcversion}.tar.gz

Provides: %{name}-kmod-common = %{version}
Requires: %{name}-kmod >= %{version}

BuildRequires: systemd-rpm-macros

%description
Linux Driver for ITE LPC chips

%prep
%setup -q -c %{srcname}-%{srcversion}

%install
mkdir -p %{buildroot}%{_prefix}/lib/modprobe.d/
install -p -m 0644 %{srcname}-%{srcversion}/install/modprobe.conf %{buildroot}%{_prefix}/lib/modprobe.d/%{name}.conf

%files
%doc %{srcname}-%{srcversion}/README
%license %{srcname}-%{srcversion}/LICENSE
%attr(0644,root,root) %{_prefix}/lib/modprobe.d/%{name}.conf

%changelog
{{{ git_dir_changelog }}}
