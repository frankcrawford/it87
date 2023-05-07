# `packagetool.sh` quick start guide

`packagetool.sh` assists in the creation of a packaged version of the `it87` module for Alpine Linux (via AKMS), Fedora & co (including Silverblue) (via akmods), and Debian & co (via DKMS).
It uses a container to ensure that the build environment is reproducible and to avoid touching the host system.

## Basic usage, for this example on Fedora Silverblue 38:
1. Docker (optionally with `buildx`) or Podman is required to use the program.
	
	The container runtime doesn't matter too much, usually the pre-installed or already in use one should be preferred. Podman comes pre-installed on Fedora Silverblue.

2. Run `./packagetool.sh --container_runtime=podman --package_system=rpm --container_security_privileged`
	
	`--container_runtime=` and `--package_system=` are always required.
	
	`--container_security_privileged` is only required here because of Silverblue's SELinux restrictions on mounted directories. (More granular security options are "TODO".)
	
	For more information on the available options, run the program with `--help`.

	The program also includes tests for package installation and module building via the target distribution's dynamic module building mechanism. For this, run the program with `--run_build_tests`. Please note that this requires additional dependencies which increase the size of the container image, in Fedora's case by quite a lot.

3. The resulting packages can be found in the `./.releases/` directory.

The tool uses the following base images:
* `docker.io/library/alpine:latest`
* `registry.fedoraproject.org/fedora-minimal:latest`
* `docker.io/library/debian:stable-slim`

The resulting images are named `it87-{package_system}-build` or `it87-{package_system}-build-and-test`, for example `it87-rpm-build`.

It is recommended to delete the base images after some time to ensure that up-to-date packaging tools are used. The resulting images can also be deleted to save disk space.

# GitHub Actions

A wrapper for GitHub Actions is provided in `./.github/workflows/packagetool.yml`, the workflow resets its build cache with each week of the year. An option to not use the cache is provided when dispatching or calling the workflow.

`./.github/workflows/package_and_release.yml` automatically builds and tests the packages, then creates a draft release with the packages as assets. It automatically replaces the latest draft release to avoid cluttering the releases page. If configured to be triggered by a push, it can be skipped by adding `[skip ci]` to the commit message (this is enforced by GitHub, not the workflow).

# Package overview
## Packages
| `package_system` | Package (`.release/*`) | Contents | Notes |
| ------------------ | ------- | -------- | ----- |
| `apk` | `apk/it87-oot-*.apk` | Userland files | Depends on AKMS package. |
| `apk` | `apk/it87-oot-akms-*.apk` | AKMS files | Depends on userland package. |
| `apk` | `apk/it87-oot-doc-*.apk` | Documentation files | |
| `apk` | `apk/it87-ignore_resource_conflict-*.apk` | `ignore_resource_conflict` option | |
| `apk` | `akms/*` | File system tree containing the files for manual installation | `/etc/modprobe.d/it87-oot.conf` is the `ignore_resource_conflict` option and should only be installed if needed |
| `rpm` | `RPM/it87-oot-*.rpm` | Userland files | Depends on akmods package. |
| `rpm` | `RPM/akmod-it87-oot-*.rpm` | akmods files | Depends on userland package. |
| `rpm` | `RPM/it87-oot-ignore_resource_conflict-*.rpm` | `ignore_resource_conflict` option | |
| `rpm` | `RPM/kmod-it87-oot-*.rpm` | "Metapackage which tracks in the it87-oot kernel module for newest kernel" | I am not entirely sure what the point of this is since we're building the module dynamically |
| `rpm` | `SRPM/*` | Source RPMs (for debugging and inspection) | |
| `deb` | `it87-dkms_*.deb` | DKMS files | The `.deb` process is currently much more basic than the others, `ignore_resource_conflict` has to be manually configured in `/etc/modprobe.d/` |
<!-- | Archive name | Contents | Notes |
| ------------ | -------- | ----- |
| `release-alpine-akms-apk.tar.gz` | Module and supporting files as Alpine Linux '.apk' packages. | |
| `release-alpine-akms-manual.tar.gz` | Module and supporting files for Alpine Linux via manual installation. | |
| `release-debian-dkms-deb.tar.gz` | Module as Debian '.deb' package. | Currently does not include package for `ignore_resource_conflict` option (can be configured manually in `/etc/modprobe.d/`). |
| `release-redhat-akmods-rpm.tar.gz` | Module and supporting files as Red Hat '.rpm' packages, also works with Fedora Silverblue & co. | |
| `release-redhat-akmods-source-rpm.tar.gz` | Source RPMs (for debugging and inspection). | | -->

## General information:
- For Alpine Linux with either method, installing the `linux-{flavor}-dev` (usually `linux-lts-dev`) package is recommended, otherwise `akms` will temporarily download it at the time of building, requiring an internet connection.
- The `ignore_resource_conflict` packages should only be installed if the module fails to load otherwise.
- In the manual Alpine installation, `/etc/modprobe.d/it87-oot.conf` corresponds to the aforementioned package.
- A reboot is recommended after installing the module or (un)installing the `ignore_resource_conflict` package.