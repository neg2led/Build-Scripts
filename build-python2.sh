#!/usr/bin/env bash

# Written and placed in public domain by Jeffrey Walton
# This script builds Python 2.x from sources.
# Python 2.7.18 is the last official 2.x release from the project.

PYTHON_VER=2.7.18
PYTHON_TAR=Python-${PYTHON_VER}.tgz
PYTHON_DIR=Python-${PYTHON_VER}
PKG_NAME=python2

###############################################################################

# Get the environment as needed.
if [[ "${SETUP_ENVIRON_DONE}" != "yes" ]]; then
 if ! source ./setup-environ.sh
 then
 echo "Failed to set environment"
 exit 1
 fi
fi

# The password should die when this subshell goes out of scope
if [[ "${SUDO_PASSWORD_DONE}" != "yes" ]]; then
 if ! source ./setup-password.sh
 then
 echo "Failed to process password"
 exit 1
 fi
fi

###############################################################################

if ! ./build-cacert.sh
then
 echo "Failed to install CA Certs"
 exit 1
fi

###############################################################################

if ! ./build-libexpat.sh
then
 echo "Failed to install Expat"
 exit 1
fi

###############################################################################

if ! ./build-gdbm.sh
then
 echo "Failed to install GDBM"
 exit 1
fi

###############################################################################

echo ""
echo "========================================"
echo "============== Python 2.7 =============="
echo "========================================"

echo ""
echo "*************************"
echo "Downloading package"
echo "*************************"

if ! "$WGET" -q -O "$PYTHON_TAR" --ca-certificate="$DIGICERT_ROOT"
 "https://www.python.org/ftp/python/$PYTHON_VER/$PYTHON_TAR"
then
 echo "Failed to download Python 2.7"
 exit 1
fi

rm -rf "$PYTHON_DIR" &>/dev/null
gzip -d < "$PYTHON_TAR" | tar xf -
cd "$PYTHON_DIR" || exit 1

# Patches are created with 'diff -u' from the pkg root directory.
if [[ -e ../patch/python2.patch ]]; then
 patch -u -p0 < ../patch/python2.patch
 echo ""
fi

# Fix sys_lib_dlsearch_path_spec
bash ../fix-configure.sh

echo "*************************"
echo "Configuring package"
echo "*************************"

 PKG_CONFIG_PATH="${INSTX_PKGCONFIG}"
 CPPFLAGS="${INSTX_CPPFLAGS}"
 ASFLAGS="${INSTX_ASFLAGS}"
 CFLAGS="${INSTX_CFLAGS}"
 CXXFLAGS="${INSTX_CXXFLAGS}"
 LDFLAGS="${INSTX_LDFLAGS}"
 LIBS="${INSTX_LDLIBS}"
./configure
 --build="${AUTOCONF_BUILD}"
 --prefix="${INSTX_PREFIX}"
 --libdir="${INSTX_LIBDIR}"
 --enable-shared
 --with-ensurepip=yes

if [[ "$?" -ne 0 ]]; then
 echo "Failed to configure Python 2.7"
 exit 1
fi

# Escape dollar sign for $ORIGIN in makefiles. Required so
# $ORIGIN works in both configure tests and makefiles.
bash ../fix-makefiles.sh

echo "*************************"
echo "Building package"
echo "*************************"

MAKE_FLAGS=("-j" "${INSTX_JOBS}" "V=1")
if ! "${MAKE}" "${MAKE_FLAGS[@]}"
then
 echo "Failed to build Python 2.7"
 exit 1
fi

# Fix flags in *.pc files
bash ../fix-pkgconfig.sh

echo "*************************"
echo "Testing package"
echo "*************************"

MAKE_FLAGS=("test" "-k" "V=1")
if ! "${MAKE}" "${MAKE_FLAGS[@]}"
then
 echo "*************************"
 echo "Failed to test Python 2.7"
 echo "*************************"
 exit 1
fi

echo "*************************"
echo "Installing package"
echo "*************************"

MAKE_FLAGS=("install")
if [[ -n "$SUDO_PASSWORD" ]]; then
 printf "%s n" "$SUDO_PASSWORD" | sudo ${SUDO_ENV_OPT} -S "${MAKE}" "${MAKE_FLAGS[@]}"
 printf "%s n" "$SUDO_PASSWORD" | sudo ${SUDO_ENV_OPT} -S bash ../fix-permissions.sh "${INSTX_PREFIX}"
else
 "${MAKE}" "${MAKE_FLAGS[@]}"
 bash ../fix-permissions.sh "${INSTX_PREFIX}"
fi

###############################################################################

echo ""
echo "*****************************************************************************"
echo "Please run Bash's 'hash -r' to update program cache in the current shell"
echo "*****************************************************************************"

###############################################################################

touch "${INSTX_PKG_CACHE}/${PKG_NAME}"

cd "${CURR_DIR}" || exit 1

###############################################################################

# Set to false to retain artifacts
if true;
then
 ARTIFACTS=("$PYTHON_TAR" "$PYTHON_DIR")
 for artifact in "${ARTIFACTS[@]}"; do
 rm -rf "$artifact"
 done
fi

exit 0
