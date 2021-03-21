#!/usr/bin/env bash

# Written and placed in public domain by Jeffrey Walton
# This script builds TinyXML from sources.

TXML2_TAR=7.1.0.tar.gz
TXML2_DIR=tinyxml2-7.1.0
PKG_NAME=tinyxml

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

echo ""
echo "========================================"
echo "=============== TinyXML2 ==============="
echo "========================================"

echo ""
echo "**********************"
echo "Downloading package"
echo "**********************"

if ! "$WGET" -q -O "$TXML2_TAR" --ca-certificate="$GITHUB_CA_ZOO" \
     "https://github.com/leethomason/tinyxml2/archive/$TXML2_TAR"
then
    echo "Failed to download tinyxml2"
    exit 1
fi

rm -rf "$TXML2_DIR" &>/dev/null
gzip -d < "$TXML2_TAR" | tar xf -
cd "$TXML2_DIR"

echo "**********************"
echo "Building package"
echo "**********************"

# Since we call the makefile directly, we need to escape dollar signs.
PKG_CONFIG_PATH="${INSTX_PKGCONFIG}"
CPPFLAGS=$(echo "${INSTX_CPPFLAGS}" | sed 's/\$/\$\$/g')
ASFLAGS=$(echo "${INSTX_ASFLAGS}" | sed 's/\$/\$\$/g')
CFLAGS=$(echo "${INSTX_CFLAGS}" | sed 's/\$/\$\$/g')
CXXFLAGS=$(echo "${INSTX_CXXFLAGS}" | sed 's/\$/\$\$/g')
LDFLAGS=$(echo "${INSTX_LDFLAGS}" | sed 's/\$/\$\$/g')
LIBS="${INSTX_LDLIBS}"

MAKE_FLAGS=("-j" "${INSTX_JOBS}")
MAKE_FLAGS+=("CPPFLAGS=${CPPFLAGS}")
MAKE_FLAGS+=("ASFLAGS=${ASFLAGS}")
MAKE_FLAGS+=("CFLAGS=${CFLAGS}")
MAKE_FLAGS+=("CXXFLAGS=${CXXFLAGS}")
MAKE_FLAGS+=("LDFLAGS=${LDFLAGS}")
MAKE_FLAGS+=("LIBS=${LIBS}")

if ! "${MAKE}" "${MAKE_FLAGS[@]}"
then
    echo "Failed to build tinyxml2"
    exit 1
fi

# Fix flags in *.pc files
bash ../fix-pkgconfig.sh

echo "**********************"
echo "Testing package"
echo "**********************"

MAKE_FLAGS=("test")
if ! "${MAKE}" "${MAKE_FLAGS[@]}"
then
   echo "Failed to test tinyxml2"
   exit 1
fi

echo "**********************"
echo "Installing package"
echo "**********************"

# TODO... fix this simple copy
echo "Installing TinyXML2"
if [[ -n "$SUDO_PASSWORD" ]]; then
    printf "%s\n" "$SUDO_PASSWORD" | sudo ${SUDO_ENV_OPT} -S cp tinyxml2.h "${INSTX_PREFIX}/include"
    printf "%s\n" "$SUDO_PASSWORD" | sudo ${SUDO_ENV_OPT} -S cp libtinyxml2.a "${INSTX_LIBDIR}"
    printf "%s\n" "$SUDO_PASSWORD" | sudo ${SUDO_ENV_OPT} -S bash ../fix-permissions.sh "${INSTX_PREFIX}"
    echo ""
else
    cp tinyxml2.h "${INSTX_PREFIX}/include"
    cp libtinyxml2.a "${INSTX_LIBDIR}"
    bash ../fix-permissions.sh "${INSTX_PREFIX}"
    echo ""
fi

###############################################################################

touch "${INSTX_PKG_CACHE}/${PKG_NAME}"

cd "${CURR_DIR}" || exit 1

###############################################################################

# Set to false to retain artifacts
if true;
then
    ARTIFACTS=("$TXML2_TAR" "$TXML2_DIR")
    for artifact in "${ARTIFACTS[@]}"; do
        rm -rf "$artifact"
    done
fi

exit 0
