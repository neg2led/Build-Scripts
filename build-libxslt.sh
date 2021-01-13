#!/usr/bin/env bash

# Written and placed in public domain by Jeffrey Walton
# This script builds libxslt from sources.

XSLT_VER=1.1.34
XSLT_TAR="libxslt-${XSLT_VER}.tar.gz"
XSLT_DIR="libxslt-${XSLT_VER}"
PKG_NAME=libxslt

###############################################################################

# Get the environment as needed.
if [[ "${SETUP_ENVIRON_DONE}" != "yes" ]]; then
    if ! source ./setup-environ.sh
    then
        echo "Failed to set environment"
        exit 1
    fi
fi

if [[ -e "${INSTX_PKG_CACHE}/${PKG_NAME}" ]]; then
    echo ""
    echo "$PKG_NAME is already installed."
    exit 0
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

if ! ./build-patchelf.sh
then
    echo "Failed to build patchelf"
    exit 1
fi

###############################################################################

if ! ./build-libxml2.sh
then
    echo "Failed to build libxml2"
    exit 1
fi

###############################################################################

if ! ./build-libgcrypt.sh
then
    echo "Failed to build libgcrypt"
    exit 1
fi

###############################################################################

echo ""
echo "========================================"
echo "================ libxslt ==============="
echo "========================================"

echo ""
echo "************************"
echo "Downloading package"
echo "************************"

if ! "$WGET" -q -O "$XSLT_TAR" --ca-certificate="$LETS_ENCRYPT_ROOT" \
     "ftp://xmlsoft.org/libxml2/$XSLT_TAR"
then
    echo "Failed to download libxslt"
    exit 1
fi

rm -rf "$XSLT_DIR" &>/dev/null
gzip -d < "$XSLT_TAR" | tar xf -
cd "$XSLT_DIR" || exit 1

# Patches are created with 'diff -u' from the pkg root directory.
if [[ -e ../patch/libxslt.patch ]]; then
    patch -u -p0 < ../patch/libxslt.patch
    echo ""
fi

# Fix sys_lib_dlsearch_path_spec
bash ../fix-configure.sh

echo "************************"
echo "Configuring package"
echo "************************"

    PKG_CONFIG_PATH="${INSTX_PKGCONFIG}" \
    CPPFLAGS="${INSTX_CPPFLAGS}" \
    ASFLAGS="${INSTX_ASFLAGS}" \
    CFLAGS="${INSTX_CFLAGS}" \
    CXXFLAGS="${INSTX_CXXFLAGS}" \
    LDFLAGS="${INSTX_LDFLAGS}" \
    LIBS="-lxml2 ${INSTX_LDLIBS}" \
./configure \
    --build="${AUTOCONF_BUILD}" \
    --prefix="${INSTX_PREFIX}" \
    --libdir="${INSTX_LIBDIR}" \
    --enable-static --enable-shared \
    --with-libxml-prefix="${INSTX_PREFIX}" \
    --with-crypto \
    --without-python \
    --without-profiler

if [[ "$?" -ne 0 ]]; then
    echo "Failed to configure libxslt"
    exit 1
fi

# Escape dollar sign for $ORIGIN in makefiles. Required so
# $ORIGIN works in both configure tests and makefiles.
bash ../fix-makefiles.sh

echo "************************"
echo "Building package"
echo "************************"

MAKE_FLAGS=("-j" "${INSTX_JOBS}" "V=1")
if ! "${MAKE}" "${MAKE_FLAGS[@]}"
then
    echo "************************"
    echo "Failed to build libxslt"
    echo "************************"
    bash ../collect-logs.sh "${PKG_NAME}"
    exit 1
fi

# Fix flags in *.pc files
bash ../fix-pkgconfig.sh

# Fix runpaths
bash ../fix-runpath.sh

echo "************************"
echo "Testing package"
echo "************************"

MAKE_FLAGS=("check" "-k" "V=1")
if ! "${MAKE}" "${MAKE_FLAGS[@]}"
then
    echo "************************"
    echo "Failed to test libxslt"
    echo "************************"
    bash ../collect-logs.sh "${PKG_NAME}"
    exit 1
fi

# Fix runpaths again
bash ../fix-runpath.sh

echo "************************"
echo "Installing package"
echo "************************"

MAKE_FLAGS=("install")
if [[ -n "$SUDO_PASSWORD" ]]; then
    printf "%s\n" "$SUDO_PASSWORD" | sudo ${SUDO_ENV_OPT} -S "${MAKE}" "${MAKE_FLAGS[@]}"
    printf "%s\n" "$SUDO_PASSWORD" | sudo ${SUDO_ENV_OPT} -S bash ../fix-permissions.sh "${INSTX_PREFIX}"
else
    "${MAKE}" "${MAKE_FLAGS[@]}"
    bash ../fix-permissions.sh "${INSTX_PREFIX}"
fi

###############################################################################

touch "${INSTX_PKG_CACHE}/${PKG_NAME}"

cd "$CURR_DIR" || exit 1

###############################################################################

# Set to false to retain artifacts
if true;
then
    ARTIFACTS=("$XSLT_TAR" "$XSLT_DIR")
    for artifact in "${ARTIFACTS[@]}"; do
        rm -rf "$artifact"
    done
fi

exit 0
