// Copyright 2022 alambe94

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file am64x_phy.h
// @brief AM64x PHY type constants and MMIO register-map access helpers.

#ifndef AM64X_PHY_H
#define AM64X_PHY_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusb_regmap.h"

#ifdef __cplusplus
extern "C"
{
#endif

// MACROS //////////////////////////////////////////////////////////////////////

// PHY type identifiers
#define PHY_NONE         0
#define PHY_TYPE_SATA    1
#define PHY_TYPE_PCIE    2
#define PHY_TYPE_USB2    3
#define PHY_TYPE_USB3    4
#define PHY_TYPE_UFS     5
#define PHY_TYPE_DP      6
#define PHY_TYPE_XPCS    7
#define PHY_TYPE_SGMII   8
#define PHY_TYPE_QSGMII  9
#define PHY_TYPE_DPHY    10
#define PHY_TYPE_CPHY    11
#define PHY_TYPE_USXGMII 12
#define PHY_TYPE_XAUI    13

// Lane polarity
#define PHY_POL_NORMAL 0
#define PHY_POL_INVERT 1
#define PHY_POL_AUTO   2

// errno shim - POSIX reserves 22 for EINVAL; defined here only if the
// toolchain headers do not provide <errno.h> or a compatible definition.
#ifndef EINVAL
#define EINVAL 22
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    int am64x_wiz_init(uintptr_t base_address, uint32_t phy_type, uint32_t lane_swap, uint32_t core_ref_clk_hz);
    void am64x_torrent_phy_configure(uintptr_t sd_base, int enable_ssc);
    int am64x_torrent_phy_wait_ready(uintptr_t sd_base, uint32_t timeout_us);

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif // AM64X_PHY_H
// EOF /////////////////////////////////////////////////////////////////////////////
