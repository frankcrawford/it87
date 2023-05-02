#!/usr/bin/env bash
set -e
software_name='it87' # Hardcoded default (see "gather_repo_info()")

# -----------------
# Utility functions
# -----------------
print_usage() {
	usage="$(cat <<EOF
Usage: $0 [options]
Options:
	(Optional) --print_repo_info
		Print detected information about the current repository and exit.
		The shown variables can be set manually in the environment or hardcoded in the script.
	(Required) --container_runtime=CONTAINER_RUNTIME
		Container runtime to use. Valid values are 'podman', 'docker', and 'docker-buildx.
	(Required) --package_system=PACKAGE_SYSTEM
		Package system to target. Valid values are 'apk', 'deb', and 'rpm'.
	(Optional) --run_build_tests
		Test the package install and dynamic module build process after building the package.
		May require additional privileges in some environments.
	(Optional) --inspect_container
		Inspect the container with a shell after building (and testing) the package.
		Note: 'exit'-ing the container with a non-zero exit code will stop the script as well.
	(Optional) --container_security_privileged
		Run the container with the '--privileged' flag. Primarily needed by Docker for package tests.
		TODO: More granular privileges
	(Optional) --local_cache_dir=LOCAL_CACHE_DIR
		Directory to use as a local cache for the build process.
		* Will only try to load cache if 'index.json' exists in the directory (will created upon writing cache).
		Requires 'docker-buildx' container runtime.
	(Optional) --local_cache_ci_mode
		For caching systems that only allow one-time cache writing.
		* Will only write cache if no cache exists ('index.json' not in directory).
		Note: Very basic implementation, requires separate caches for different package systems.
	(Optional) --keep_temp_dir
		Do not delete the temporary directory after the script exits.
		Also fixes issues with 'docker' runtime preventing temporary directory deletion. (TODO: Why??)
	(Optional) --print_temp_dir=PRINT_TEMP_DIR
		Print contents of the temporary directory when the script exits. Valid values are 'none', 'normal', and 'verbose'.
		'none' is the default and prints nothing.
		'normal' uses 'tree' or 'ls -R' to print the contents of the temporary directory.
		'verbose' uses 'ls -laR' to print the contents of the temporary directory.
		--print_temp_dir is equivalent to --print_temp_dir=normal.
	(Optional) --help, -h
		Print this help message and exit.
EOF
)"
	printf '%s\n' "$usage"
}

parse_arguments() {
	will_exit_with_err='false'
	will_exit_with_ok='false'
	while [ "$1" ]; do
		case "$1" in
			--print_repo_info)
				print_repo_info
				will_exit_with_ok='true'
				shift
				;;
			--container_runtime=*)
				container_runtime="${1#*=}"
				valid_container_runtimes='(podman|docker|docker-buildx)'
				if [[ ! "$container_runtime" =~ ^${valid_container_runtimes}$ ]]; then
					printf '%s\n' "Error: Invalid runtime '$container_runtime', must be one of '$valid_container_runtimes'"
					will_exit_with_err='true'
				fi
				shift
				;;
			--package_system=*)
				package_system="${1#*=}"
				valid_package_systems='(apk|deb|rpm)'
				if [[ ! "$package_system" =~ ^${valid_package_systems}$ ]]; then
					printf '%s\n' "Error: Invalid package system '$package_system', must be one of '$valid_package_systems'"
					will_exit_with_err='true'
				fi
				shift
				;;
			--run_build_tests)
				container_run_pkg_tests='true'
				shift
				;;
			--inspect_container)
				inspect_container='true'
				shift
				;;
			--container_security_privileged)
				container_security_privileged='true'
				shift
				;;
			--local_cache_dir=*)
				local_cache_dir="${1#*=}"
				[ "$local_cache_dir" ] || { printf '%s\n' "Error: No value specified for LOCAL_CACHE_DIR"; will_exit_with_err='true'; }
				[ -d "$local_cache_dir" ] || { printf '%s\n' "Error: LOCAL_CACHE_DIR '$local_cache_dir' doesn't exist or isn't a directory"; will_exit_with_err='true'; }
				[ -w "$local_cache_dir" ] || { printf '%s\n' "Error: LOCAL_CACHE_DIR '$local_cache_dir' isn't writable"; will_exit_with_err='true'; }
				shift
				;;
			--local_cache_ci_mode)
				local_cache_ci_mode='true'
				shift
				;;
			--keep_temp_dir)
				keep_temp_dir='true'
				shift
				;;
			--print_temp_dir=*)
				print_temp_dir="${1#*=}"
				[ "$print_temp_dir" ] || { printf '%s\n' "Error: No value specified for PRINT_TEMP_DIR"; will_exit_with_err='true'; }
				valid_print_temp_dir='(none|normal|verbose)'
				if [[ ! "$print_temp_dir" =~ ^${valid_print_temp_dir}$ ]]; then
					printf '%s\n' "Error: Invalid PRINT_TEMP_DIR '$print_temp_dir', must be one of '$valid_print_temp_dir'"
					will_exit_with_err='true'
				fi
				shift
				;;
			--print_temp_dir)
				print_temp_dir='normal'
				shift
				;;
			--help|-h)
				print_usage
				will_exit_with_ok='true'
				shift
				;;
			*) # Unknown option
				will_print_usage='true'
				printf '%s\n' "Error: Unknown argument '$1'"
				will_exit_with_err='true'
				shift
				;;
		esac
	done
	
	# Set defaults
	if [ ! "$container_run_pkg_tests" ]; then
		container_run_pkg_tests='false'
	fi

	# Verify required options and combinations
	required_options=(
	"container_runtime"
	"package_system"
	)	
	for option in "${required_options[@]}"; do
		if [ -z "${!option}" ]; then
			printf '%s\n' "Error: Required option '${option^^}' is unset"
			will_exit_with_err='true'
		fi
	done
	if [ "$local_cache_dir" ] && [ "$container_runtime" != 'docker-buildx' ]; then
		printf '%s\n' "Error: LOCAL_CACHE_DIR requires 'docker-buildx' container runtime"
		will_exit_with_err='true'
	fi
	if [ "$local_cache_ci_mode" == 'true' ] && [ ! "$local_cache_dir" ]; then
		printf '%s\n' "Error: LOCAL_CACHE_CI_MODE requires LOCAL_CACHE_DIR"
		will_exit_with_err='true'
	fi

	case "$container_runtime" in
		docker-buildx)
			command -v docker &>/dev/null || { printf '%s\n' "Error: Container runtime 'docker' not found in PATH"; will_exit_with_err='true'; }
			docker buildx version &>/dev/null || { printf '%s\n' "Error: 'docker buildx' plugin is not available"; will_exit_with_err='true'; }
			;;
		*)
			command -v "$container_runtime" &>/dev/null || { printf '%s\n' "Error: Container runtime '$container_runtime' not found in PATH"; will_exit_with_err='true'; }
			;;
	esac
	
	# Exit modes
	if [ "$will_exit_with_err" == 'true' ]; then
		printf '%s\n' "Error: Exiting due to argument parsing error, see above for details"
		exit 1
	elif [ "$will_exit_with_ok" == 'true' ]; then
		printf '%s\n' "Info: A supplied option requests an early exit, exiting now"
		exit 0
	fi
}

gather_repo_info() {
	# Variables are only set if they haven't been set already (in the environment or previously in the script).
	[ "$working_tree_changed" ] ||
		working_tree_changed="$(git diff-index --quiet HEAD -- &>/dev/null; printf '%s' "$?")" # Whether the working tree has been modified

	if [ "$working_tree_changed" == '0' ]; then # If the working tree is clean, use the commit date. $working_tree_changed is also >0 if we're not a git repository.
		[ "$current_commit" ] ||
			current_commit="$(git rev-parse HEAD 2>/dev/null || printf 'unknown')" # Commit hash of the current commit
		[ "$working_tree_timestamp" ] ||
			working_tree_timestamp="$(git show -s --format=%ct "${current_commit}" 2>/dev/null || printf '%s' "$(date '+%s')")"
	else # If working tree is dirty or invalid, use the current date
		[ "$current_commit" ] ||
			current_commit='unknown'
		[ "$working_tree_timestamp" ] ||
			working_tree_timestamp="$(date '+%s')"
	fi

	[ "$origin_url" ] ||
		origin_url="$(git remote get-url origin 2>/dev/null || printf 'unknown')" # URL of the origin remote
	# These parameter substitutions always expand to 'unknown' if $origin_url is 'unknown'
	[ "$origin_name" ] ||
		origin_name="${origin_url##*/}" # Name of the origin remote
	[ "$origin_owner" ] ||
		{ origin_owner="${origin_url%/*}"; origin_owner="${origin_owner##*/}"; } # Owner of the origin remote
	[ "$software_name" ] ||
		software_name="${origin_name}" # Name of the software (for built modules)
}

print_repo_info() {
	printf '%s\n' "-> Determined the following information about the current repository:"
	printf '\t%s="%s"\n' \
		"software_name" "$software_name" \
		"current_commit" "$current_commit" \
		"working_tree_changed" "$working_tree_changed" \
		"working_tree_timestamp" "$working_tree_timestamp" \
		"origin_url" "$origin_url" \
		"origin_name" "$origin_name" \
		"origin_owner" "$origin_owner"
}

container_build_and_run() {
	container_name="${1}"
	container_run_command="${2}"
	
	build_opts=()
	if [ "$local_cache_dir" ] && [ "$container_runtime" == 'docker-buildx' ]; then
		if [ -f "${local_cache_dir}/index.json" ]; then # Only trying to load if the cache is not empty.
			printf '%s\n' "-> Will try to load from local container build cache at '${local_cache_dir}'."
			build_opts+=("--cache-from" "type=local,src=${local_cache_dir},compression=uncompressed") # GitHub actions cache always uses zstd compression, so we skip it here.
		fi
		if [ "$local_cache_ci_mode" != 'true' ] || [ ! -f "${local_cache_dir}/index.json" ]; then # If CI mode is NOT enabled, we always save to the cache, if CI mode is enabled, we only save if it's empty.
			printf '%s\n' "-> Will try to save to local container build cache at '${local_cache_dir}'."
			build_opts+=("--cache-to" "type=local,dest=${local_cache_dir},mode=max,compression=uncompressed")
		fi
	fi

	run_opts=(${container_runtime_opts[@]})
	[ "$inspect_container" == 'true' ] && run_opts+=("-it")
	[ "$container_security_privileged" == 'true' ] && run_opts+=("--privileged")

	case "$container_runtime" in
		podman)
			printf '%s\n' "${containerfile}" | podman build ${build_opts[@]} --tag "${container_name}" --file - ${temp_dir} ||
				{ printf '%s\n' "Error: Failed to build '${container_name}' image."; exit 1; }
			podman run ${run_opts[@]} --rm "${container_name}" ${container_run_command} ||
				{ printf '%s\n' "Error: '${container_name}' exited with non-zero status '$?'. Aborting."; exit 1; }
			;;
		docker)
			printf '%s\n' "${containerfile}" | docker build ${build_opts[@]} --tag "${container_name}" --file - ${temp_dir} ||
				{ printf '%s\n' "Error: Failed to build '${container_name}' image."; exit 1; }
			docker run ${run_opts[@]} --rm "${container_name}" ${container_run_command} ||
				{ printf '%s\n' "Error: '${container_name}' exited with non-zero status '$?'. Aborting."; exit 1; }
			;;
		docker-buildx)
			printf '%s\n' "${containerfile}" | docker buildx build ${build_opts[@]} --load --tag "${container_name}" --file - ${temp_dir} ||
				{ printf '%s\n' "Error: Failed to build '${container_name}' image."; exit 1; }
			docker run ${run_opts[@]} --rm "${container_name}" ${container_run_command} ||
				{ printf '%s\n' "Error: '${container_name}' exited with non-zero status '$?'. Aborting."; exit 1; }
			;;
		*) # container_runtime should be validated by the parser, so this should never happen.
			printf '%s\n' "Error: Unknown container runtime '${container_runtime}'."; exit 1
			;;
	esac
}

startup() {
	printf '%s' "-> Deleting old .release/ folder and creating temporary directory..."
	rm -rf "./.release/" || { printf '\n%s\n' "Error: Failed to delete previous release directory."; exit 1; }
	temp_dir="$(mktemp -t --directory ${software_name}_tmp.XXXXXXXXXX)" || { printf '\n%s\n' "Error: Failed to create temporary directory."; exit 1; }
	printf '%s\n' " OK."
}

cleanup() {
	last_exit_status="$?"
	if [ "$print_temp_dir" == 'normal' ]; then
		printf '%s\n' "-> Contents of temporary directory at '${temp_dir}' ('normal' verbosity):"
		tree "${temp_dir}" 2>/dev/null || ls -R "${temp_dir}" # Fallback if "tree" is not installed.
	elif [ "$print_temp_dir" == 'verbose' ]; then
		printf '%s\n' "-> Contents of temporary directory at '${temp_dir}' ('verbose' verbosity):"
		ls -laR "${temp_dir}"
	fi
	if [ "$keep_temp_dir" == 'true' ]; then
		printf '%s\n' "-> Not removing '${temp_dir}' as per '--keep_temp_dir'."
	else 
		printf '%s' "-> Deleting temporary directory at '${temp_dir}'..."
		rm -rf "${temp_dir}" || { printf '\n%s\n' "Error: Failed to delete temporary directory."; exit 1; }
		printf '%s\n' " OK."
	fi
	[ "$last_exit_status" == '0' ] && { printf '%s\n' "-> Program completed successfully"; exit 0; }
	[ "$last_exit_status" != '0' ] && { printf '%s\n' "-> Program exited with non-zero status '$last_exit_status'"; exit "$last_exit_status"; }
}

# -------------------
# Packaging functions
# -------------------
build_apk() {
	# Prepare source and build files
	mkdir -p "${temp_dir}/"{APKBUILD,packages} # Create the shared directories
	build_overrides=(
		"_source_modname=\"${software_name}\""
		"_repo_name=\"${origin_name}\""
		"_repo_owner=\"${origin_owner}\""
		"_repo_commit=\"${current_commit}\""
		"_package_timestamp=\"$(date -u -d "@${working_tree_timestamp}" '+%Y%m%d')\""
		"source=\"${origin_name}.tar.gz\""
	)
	printf '%s\n' "${build_overrides[@]}" >"${temp_dir}/APKBUILD/APKBUILD" # Write overrides
	cat "./alpine/APKBUILD" >>"${temp_dir}/APKBUILD/APKBUILD" # Append the APKBUILD

	tar \
		--verbose \
		--exclude="${PWD##*/}/.git" \
		--create \
		--gzip \
		--file "${temp_dir}/APKBUILD/${origin_name}.tar.gz" \
		--directory '../' \
		"${PWD##*/}" # Create the source tarball, for compatibility with GitHub tarballs we put the repo in a subdirectory of the tarball

	# Prepare the container
	containerfile="$(cat <<-'EOF'
	FROM docker.io/library/alpine:latest

	# Install the build dependencies
	RUN apk add abuild build-base
	EOF
	)"
	containerfile_test="$(cat <<-'EOF'
	# Install the test dependencies
	RUN apk add linux-lts-dev akms

	# Remove /proc mount from akms-runas. This works around 'Can't mount proc on /newroot/proc: Operation not permitted' in GitHub Actions.
	# Update: This removes the --privileged requirement for Podman, but Docker needs --privileged regardless. For now it's commented out and we will be using --privileged for any bwrap related issues.
	# RUN sed -i '/--proc \/proc \\/d' /usr/libexec/akms/akms-runas

	# Save kernel dev name
	RUN printf '%s\n' "$(ls /lib/modules/ | head -n 1)" >/kernel_dev_name.txt
	EOF
	)"
	container_run_script=(
		"("
		"cd /APKBUILD"
		"&& abuild-keygen -a -n" # We can't not sign the package, so we generate a one time use key, the user has to install it with `--allow-untrusted`
		"&& abuild -F checksum"
		"&& abuild -F srcpkg"
		"&& abuild -F"
		")"
		"&& printf '%s\n' '-> Package building complete.'"
	)
	if [ "$container_run_pkg_tests" == 'true' ]; then # Testing installing the package and akmod dynamic builds
		container_run_script+=(
			"&& apk add --allow-untrusted /root/packages/*/*.apk"
			"&& akms --kernel \$(cat /kernel_dev_name.txt) build ${software_name}-oot"
			"&& akms --kernel \$(cat /kernel_dev_name.txt) install ${software_name}-oot"
			"&& modinfo /lib/modules/\$(cat /kernel_dev_name.txt)/kernel/extra/akms/${software_name}.ko*" # * in case compression is used
			"&& printf '%s\n' '-> Checking if module is removed on package uninstall.'"
			"&& apk del ${software_name}-oot*"
			"&& { ! modinfo --filename /lib/modules/\$(cat /kernel_dev_name.txt)/kernel/extra/akms/${software_name}.ko* &>/dev/null || { printf '%s\n' '-> Error: Module was not removed on package uninstall.'; return 1; }; }"
			"&& printf '%s\n' '-> Package installation and akms dynamic build tests successful.'"
		)
	fi
	if [ "$inspect_container" == 'true' ]; then
		container_run_script+=(
			"; printf '%s\n' '-> Dropping into container shell.'"
			"; ash"
		)
	fi
	install -D -m 0755 <(printf '%s\n\n%s\n' '#!/bin/sh' "${container_run_script[*]}") "${temp_dir}/run_script.sh" # Write the run command

	# Build and run the container
	container_runtime_opts=(
		"--mount type=bind,source=${temp_dir}/APKBUILD,target=/APKBUILD"
		"--mount type=bind,source=${temp_dir}/packages,target=/root/packages"
		"--mount type=bind,source=${temp_dir}/run_script.sh,target=/run_script.sh"
	)
	if [ "$container_run_pkg_tests" == 'true' ]; then
		containerfile="$(printf '%s\n\n%s\n' "${containerfile}" "${containerfile_test}")" # Append test dependencies and setup
		container_build_and_run "${software_name}-apk-build-and-test" "/run_script.sh"
	else
		containerfile="$(printf '%s\n' "${containerfile}")" # Add newline for consistency
		container_build_and_run "${software_name}-apk-build" "/run_script.sh"
	fi

	# Prepare to copy out files
	apk_packages_folder="./.release/apk"
	akms_manual_root_folder='./.release/akms'
	mkdir -p "${apk_packages_folder}" "${akms_manual_root_folder}"

	# Copy out the apk packages
	cp "${temp_dir}/packages/"*/*.apk "${apk_packages_folder}"
	
	# Extract akms files for manual install
	if tar --version | grep -q 'busybox'; then
		no_warning_option='' # busybox tar doesn't support (and doesn't need) --warning=no-unknown-keyword
	else
		no_warning_option=' --warning=no-unknown-keyword' # tar can act weird if we have a stray space, so we have it in the variable
	fi

	mainpkg=$(ls "${apk_packages_folder}/"*.apk | head -n 1) # ls with a glob '*.apk' returns the full relative path.
	mainpkgfiles=(
		"etc/depmod.d/${software_name}-oot.conf"
		"etc/modules-load.d/${software_name}-oot.conf"
	)
	tar -xf "${mainpkg}" -C "${akms_manual_root_folder}/" "${mainpkgfiles[@]}"${no_warning_option}
	akmspkg=$(ls "${apk_packages_folder}/"*akms*.apk | head -n 1)
	akmspkgfiles=(
		"usr/src/${software_name}-oot/"
	)
	tar -xf "${akmspkg}" -C "${akms_manual_root_folder}/" "${akmspkgfiles[@]}"${no_warning_option}
	ircpkg=$(ls "${apk_packages_folder}/"*ignore_resource_conflict*.apk | head -n 1)
	ircpkgfiles=(
		"etc/modprobe.d/${software_name}-oot.conf"
	)
	tar -xf "${ircpkg}" -C "${akms_manual_root_folder}/" "${ircpkgfiles[@]}"${no_warning_option}
}

build_rpm() {
	# Prepare the source and build files
	mkdir -p "${temp_dir}/rpmbuild/"{SOURCES,SPECS} # Create shared build directories in temp dir
	spec_overrides=(
		"%global source_modname ${software_name}"
		"%global repo_name ${origin_name}"
		"%global repo_owner ${origin_owner}"
		"%global repo_commit ${current_commit}"
		"%global package_timestamp $(date -u -d "@${working_tree_timestamp}" '+%Y%m%d')"
	)
	for spec_file in "./redhat/"*.spec; do # Write overrides, then insert the original spec file
		spec_file_target="${temp_dir}/rpmbuild/SPECS/${spec_file##*/}"
		printf '%s\n' "${spec_overrides[@]}" >"${spec_file_target}"
		cat "${spec_file}" >>"${spec_file_target}"
	done

	tar \
		--verbose \
		--exclude="${PWD##*/}/.git" \
		--exclude="${PWD##*/}/Research" \
		--create \
		--gzip \
		--file="${temp_dir}/rpmbuild/SOURCES/${origin_name}.tar.gz" \
		--directory='../' \
		"${PWD##*/}" # The spec files expect the sources in a subdirectory of the archive (as with GitHub tarballs)
		# Note: We exclude ./Research here since we can't just not install it through the spec file (kmodtool always includes the entire tarball).
		# This isn't ideal, but anything else (modifying the source tarball itself during package building) seems like it would be sketchy.

	# Prepare the container
	containerfile="$(cat <<-'EOF'
	FROM registry.fedoraproject.org/fedora-minimal:latest

	# Install the build dependencies
	RUN microdnf install -y rpmdevtools kmodtool
	EOF
	)"
	containerfile_test="$(cat <<-'EOF'
	# Install the test dependencies
	# Note: Unlike with Alpine, we can't get away with only the -dev(el) package, we need the full kernel package.
	# TODO: Unfortunately the kernel package has a ton of dependencies, TODO: somehow skip dependencies.
	# DNF is needed by akmods to install the resulting package.
	RUN microdnf install -y kernel kernel-devel akmods dnf

	# Save kernel dev name
	RUN printf '%s\n' "$(ls /lib/modules/ | head -n 1)" >/kernel_dev_name.txt
	EOF
	)"
	container_run_script=(
		"rpmdev-setuptree"
		"&& rpmbuild -ba /root/rpmbuild/SPECS/*.spec"
		"&& printf '%s\n' '-> Package building complete.'"
	)
	if [ "$container_run_pkg_tests" == 'true' ]; then # Testing installing the package and akms dynamic builds
		container_run_script+=(
			"&& rpm --install /root/rpmbuild/RPMS/*/*.rpm"
			"&& akmods --kernels \$(cat /kernel_dev_name.txt) --akmod ${software_name}-oot"
			"&& modinfo /lib/modules/\$(cat /kernel_dev_name.txt)/extra/${software_name}-oot/${software_name}.ko*"
			"&& printf '%s\n' '-> Checking if module is removed on package uninstall.'"
			"&& rpm --query --all '*${software_name}-oot*' | xargs rpm --erase"
			"&& { ! modinfo --filename /lib/modules/\$(cat /kernel_dev_name.txt)/extra/${software_name}-oot/${software_name}.ko* &>/dev/null || { printf '%s\n' '-> Error: Module was not removed on package uninstall.'; return 1; }; }"
			"&& printf '%s\n' '-> Package installation and akms dynamic build tests successful.'"
		)
	fi
	if [ "$inspect_container" == 'true' ]; then
		container_run_script+=(
			"; printf '%s\n' '-> Dropping into container shell.'"
			"; bash"
		)
	fi
	install -D -m 0755 <(printf '%s\n\n%s\n' '#!/bin/sh' "${container_run_script[*]}") "${temp_dir}/run_script.sh"

	# Build and run the container
	container_runtime_opts=(
		"--mount type=bind,source=${temp_dir}/rpmbuild,target=/root/rpmbuild"
		"--mount type=bind,source=${temp_dir}/run_script.sh,target=/run_script.sh"
	)
	if [ "$container_run_pkg_tests" == 'true' ]; then
		containerfile="$(printf '%s\n\n%s\n' "${containerfile}" "${containerfile_test}")" # Append test dependencies and setup
		container_build_and_run "${software_name}-rpm-build-and-test" "/run_script.sh"
	else
		containerfile="$(printf '%s\n' "${containerfile}")" # Add newline for consistency
		container_build_and_run "${software_name}-rpm-build" "/run_script.sh"
	fi

	# Copy out the built packages
	mkdir -p "./.release/"{SRPMS,RPMS}
	cp "${temp_dir}/rpmbuild/SRPMS/"*.src.rpm "./.release/SRPMS/"
	cp "${temp_dir}/rpmbuild/RPMS/"*/*.rpm "./.release/RPMS/"
}

build_deb() { # TODO: Support this packaging method like apk and rpm
	# Copy the source files to the temp directory
	cp -r "../${PWD##*/}" "${temp_dir}/${software_name}"

	# Prepare the container
	containerfile="$(cat <<-'EOF'
	FROM docker.io/library/debian:stable-slim

	RUN apt-get update && apt-get install -y debhelper dkms
	EOF
	)"
	containerfile_test="$(cat <<-'EOF'
	# Building the package already needs dkms for some reason, which includes the kernel dev stuff, so this is kind of pointless.
	# Save kernel dev name
	RUN printf '%s\n' "$(ls /lib/modules/ | head -n 1)" >/kernel_dev_name.txt
	EOF
	)"
	container_run_script=(
		"("
		"cd /root/${software_name}"
		"&& dpkg-buildpackage --no-sign"
		")"
		"&& printf '%s\n' '-> Package building complete.'"
	)
	if [ "$container_run_pkg_tests" == 'true' ]; then # Testing installing the package and dkms dynamic builds
		container_run_script+=(
			"&& dpkg --install /root/*.deb"
			"&& modinfo /lib/modules/\$(cat /kernel_dev_name.txt)/updates/dkms/${software_name}.ko*"
			"&& printf '%s\n' '-> Checking if module is removed on package uninstall.'"
			"&& dpkg-query --show --showformat='\${Package}' '*${software_name}*' | xargs dpkg --remove"
			"&& { ! modinfo --filename /lib/modules/\$(cat /kernel_dev_name.txt)/updates/dkms/${software_name}.ko* &>/dev/null || { printf '%s\n' '-> Error: Module was not removed on package uninstall.'; return 1; }; }"
			"&& printf '%s\n' '-> Package installation and akms dynamic build tests successful.'"
		)
	fi
	if [ "$inspect_container" == 'true' ]; then
		container_run_script+=(
			"; printf '%s\n' '-> Dropping into container shell.'"
			"; bash"
		)
	fi
	install -D -m 0755 <(printf '%s\n\n%s\n' '#!/bin/sh' "${container_run_script[*]}") "${temp_dir}/run_script.sh"

	# Build and run the container
	container_runtime_opts=(
		"--mount type=bind,source=${temp_dir}/,target=/root"
		"--mount type=bind,source=${temp_dir}/run_script.sh,target=/run_script.sh"
	)
	if [ "$container_run_pkg_tests" == 'true' ]; then
		containerfile="$(printf '%s\n\n%s\n' "${containerfile}" "${containerfile_test}")" # Append test dependencies and setup
		container_build_and_run "${software_name}-deb-build-and-test" "/run_script.sh"
	else
		containerfile="$(printf '%s\n' "${containerfile}")" # Add newline for consistency
		container_build_and_run "${software_name}-deb-build" "/run_script.sh"
	fi	

	# Copy out the built packages
	mkdir -p "./.release/"
	cp "${temp_dir}/"*.deb "./.release/"
}


# ----
# Main
# ----
gather_repo_info
parse_arguments "$@"
trap cleanup EXIT

startup
print_repo_info

case "$package_system" in
	apk)
		build_apk
		;;
	rpm)
		build_rpm
		;;
	deb)
		build_deb
		;;
	*)
		printf '%s\n' "Error: Unknown package system '$package_system'" # This should never happen since the argument parser validates the input
		exit 1
		;;
esac
