# Placeholder information will be used if not inserted above (e.g. by a build wrapper)
%{!?source_modname: %global source_modname it87}
%{!?package_timestamp:%global package_timestamp %{lua:print(os.date('!%Y%m%d'))}}
# Placeholder information end


Name:           %{source_modname}-oot-ignore_resource_conflict
Version:        %{!?version_override:0^%{package_timestamp}}%{?version_override:%{version_override}} 
Release:        1%{?dist}
Summary:        Optional "modprobe.d" entry for the "%{source_modname}" module to ignore ACPI resource conflicts.
License:        n/a

URL:            https://github.com/%{repo_owner}/%{repo_name}

Provides:       %{name} = %{version}

BuildArch:      noarch

%description
Package to ignore resource conflicts for the %{source_modname} kernel module.

%prep
%setup -q -c -T

printf '%s\n' "options %{source_modname} ignore_resource_conflict" >"modprobe_%{name}.conf"

%install
install -D -m 0644 "modprobe_%{name}.conf" "%{buildroot}%{_prefix}/lib/modprobe.d/%{name}.conf"

%files
%{_prefix}/lib/modprobe.d/%{name}.conf

%changelog
# Nothing so far
