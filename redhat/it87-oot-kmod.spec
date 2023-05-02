# Placeholder information will be used if not inserted above (e.g. by a build wrapper)
%{!?source_modname: %global source_modname it87}
%{!?repo_name: %global repo_name it87}
%{!?repo_owner: %global repo_owner frankcrawford}
%{!?repo_commit: %global repo_commit 77abcbe0c49d7d8dc4530dcf51cecb40ef39f49a}
%{!?package_timestamp:%global package_timestamp %{lua:print(os.date('!%Y%m%d'))}}
# Placeholder information end

%define commit_short() %{lua:print(string.sub(rpm.expand('%repo_commit'),1,arg[1]))}
%global debug_package %{nil}


Name:           %{source_modname}-oot-kmod
Version:        %{!?version_override:0^%{package_timestamp}.git%{commit_short 7}}%{?version_override:%{version_override}} 
Release:        1%{?dist}
Summary:        kmodtool package for the out-of-tree version of the "%{source_modname}" module forked by "%{repo_owner}".
License:        GPLv2+

URL:            https://github.com/%{repo_owner}/%{repo_name}
Source0:        %{url}/tarball/%{repo_commit}/%{repo_name}.tar.gz

BuildRequires:  kmodtool

%description
Out-of-tree fork of the %{source_modname} kernel module with support for more chips. 

%{expand:%(kmodtool --target %{_target_cpu} --kmodname "%{name}" --akmod %{?kernels:--for-kernels "%{?kernels}"} 2>/dev/null)}

%prep
%{?kmodtool_check}
%setup -q -c

%global source_dirname %(tar -tzf %{SOURCE0} | head -n 1 | head -c -2)
if [ ! -f "%{source_dirname}/Makefile" ]; then
	echo "ERROR: Makefile not found in source archive, we expect the archive to contain a single directory with the source code."
	exit 1
fi

for kernel_version in %{?kernel_versions}; do
	cp -a "%{source_dirname}" "_kmod_build_${kernel_version%%___*}"
done

%build
for kernel_version in %{?kernel_versions}; do
	(cd "_kmod_build_${kernel_version%%___*}/" &&
	make %{?_smp_mflags} TARGET="${kernel_version%%___*}" DRIVER_VERSION="%{version}" modules
	# xz -f "%{source_modname}.ko" # No longer used, kmodtool compresses when appropriate, doing it here causes 'brp-kmodsign' to get indefinitely stuck.
	) || exit 1
done

%install
for kernel_version in %{?kernel_versions}; do
	install -D -m 0755 "_kmod_build_${kernel_version%%___*}/%{source_modname}.ko" "%{buildroot}%{kmodinstdir_prefix}/${kernel_version%%___*}/%{kmodinstdir_postfix}/%{source_modname}.ko"
done
%{?akmod_install}

%changelog
# Nothing so far
