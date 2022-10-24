#!/bin/bash
# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#
# Copyright (c) 2019 Amlogic, Inc. All rights reserved.
#

ROOT_DIR=`pwd`

ARCH=arm64

DEFCONFIG=amlogic_gx64_defconfig

KCONFIG_CONFIG=${ROOT_DIR}/common/common_drivers/arch/arm64/configs/${DEFCONFIG}
export KCONFIG_CONFIG

${ROOT_DIR}/common/scripts/kconfig/merge_config.sh -m -r \
	${ROOT_DIR}/common/arch/arm64/configs/gki_defconfig  \
	${ROOT_DIR}/common/common_drivers/arch/arm64/configs/amlogic_gki.fragment  \
	${ROOT_DIR}/common/common_drivers/arch/arm64/configs/amlogic_gki.10  \
	${ROOT_DIR}/common/common_drivers/arch/arm64/configs/amlogic_gki.debug \
	${ROOT_DIR}/common/common_drivers/arch/arm64/configs/amlogic_gcc64_deconfig

CROSS_COMPILE_TOOL=${ROOT_DIR}/prebuilts/gcc/linux-x86/host/x86_64-aarch64-10.3-2021.07/bin/aarch64-none-linux-gnu-

source ${ROOT_DIR}/common/common_drivers/scripts/amlogic/mk_smarthome_common.sh $@

rm ${KCONFIG_CONFIG}
