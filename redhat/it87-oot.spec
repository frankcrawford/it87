# Placeholder information will be used if not inserted above (e.g. by a build wrapper)
%{!?source_modname: %global source_modname it87}
%{!?repo_name: %global repo_name it87}
# WARNING: This placeholder will download a very old version, ensure the repo info and commit hash are up-to-date
%{!?repo_owner: %global repo_owner frankcrawford}
%{!?repo_commit: %global repo_commit 77abcbe0c49d7d8dc4530dcf51cecb40ef39f49a}
%{!?package_timestamp:%global package_timestamp %{lua:print(os.date('!%Y%m%d'))}}
# Placeholder information end

%define commit_short() %{lua:print(string.sub(rpm.expand('%repo_commit'),1,arg[1]))}


Name:           %{source_modname}-oot
Version:        %{!?version_override:0^%{package_timestamp}.git%{commit_short 7}}%{?version_override:%{version_override}} 
Release:        1%{?dist}
Summary:        Userland package for the out-of-tree version of the "%{source_modname}" module forked by "%{repo_owner}".
License:        GPLv2+

URL:            https://github.com/%{repo_owner}/%{repo_name}
Source0:        %{url}/tarball/%{repo_commit}/%{repo_name}.tar.gz

Requires:       %{name}-kmod >= %{version}
Provides:       %{name}-kmod-common = %{version}

BuildArch:      noarch

%description
Out-of-tree fork of the %{source_modname} kernel module with support for more chips. This is the userland package.

%prep
%setup -q -c

%global source_dirname %(tar -tzf %{SOURCE0} | head -n 1 | head -c -2)
if [ ! -f "%{source_dirname}/Makefile" ]; then
	echo "ERROR: Makefile not found in source archive, we expect the archive to contain a single directory with the source code."
	exit 1
fi

printf '%s\n' "override %{source_modname} * extra/%{name}" >"depmod_%{name}.conf"
printf '%s\n' "%{source_modname}" >"modload_%{name}.conf"

%install
install -D -m 0644 "depmod_%{name}.conf" "%{buildroot}%{_prefix}/lib/depmod.d/%{name}.conf"
install -D -m 0644 "modload_%{name}.conf" "%{buildroot}%{_prefix}/lib/modules-load.d/%{name}.conf"
mkdir -p "%{buildroot}%{_docdir}/%{name}"
cp -r "%{source_dirname}/Sensors configs" "%{buildroot}%{_docdir}/%{name}/Sensors configs"

%files
%doc "%{source_dirname}/README.md" "%{source_dirname}/ISSUES" "%{source_dirname}/debian/changelog"
# Unlike "doc", "docdir" doesn't automatically install the directory.
"%{_docdir}/%{name}/Sensors configs"
%docdir "%{_docdir}/%{name}/Sensors configs"
%license "%{source_dirname}/LICENSE"
"%{_prefix}/lib/depmod.d/%{name}.conf"
"%{_prefix}/lib/modules-load.d/%{name}.conf"

%changelog
# Nothing so far
