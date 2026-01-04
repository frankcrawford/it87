#!/bin/bash

if [[ $EUID -ne 0 ]]; then
  echo "You must run this with superuser priviliges.  Try \"sudo ./dkms-install.sh\"" 2>&1
  exit 1
else
  echo "About to run dkms install steps..."
fi

DRV_NAME=$(basename $(pwd))
if git rev-parse --git-dir >/dev/null 2>&1; then
  # In a git repo
  GIT_DESC=$(git describe --long --always 2>/dev/null || true)
  GIT_DATE=$(git show -s --format=%ci HEAD 2>/dev/null || date -u +'%Y-%m-%d 00:00:00')
  DRV_VERSION="${GIT_DESC}.$(date -u -d "${GIT_DATE}" +%Y%m%d)"
else
  # Not a git repo at all
  DRV_VERSION="local-$(date -u +%Y%m%d%H%M%S)"
fi

DKMS_DIR=/usr/src/${DRV_NAME}-${DRV_VERSION}

make -f Makefile DRIVER=$DRV_NAME DRIVER_VERSION=$DRV_VERSION DKMS_ROOT_PATH=$DKMS_DIR dkms

echo "Finished running dkms install steps."

exit $RESULT
