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

// @file am64x_wiz.c
// @brief AM64x SERDES Wrapper (WIZ) driver implementation.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <string.h>
#include <stdio.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "am64x_phy.h"

// MACROS /////////////////////////////////////////////////////////////////////////
// ------------------------------------------------------------------
// Platform hooks - override before including or via compiler flags
// ------------------------------------------------------------------

#ifndef WIZ_LOG

#define WIZ_LOG(...)                                                                                                                       \
    do                                                                                                                                     \
    {                                                                                                                                      \
        printf("[WIZ] ");                                                                                                                  \
        printf(__VA_ARGS__);                                                                                                               \
        printf("\n");                                                                                                                      \
    } while (0)
#endif

// Provide a real implementation via your BSP (e.g. ClockP_usleep).
#ifndef WIZ_MDELAY_MS
#define WIZ_MDELAY_MS(ms)                                                                                                                  \
    do                                                                                                                                     \
    {                                                                                                                                      \
        volatile uint32_t _c = (ms) * 10000U;                                                                                              \
        while (_c--)                                                                                                                       \
            ;                                                                                                                              \
    } while (0)
#endif

// ------------------------------------------------------------------
// Register offsets inside the WIZ SERDES block
// ------------------------------------------------------------------

#define WIZ_SERDES_CTRL     0x404U
#define WIZ_SERDES_TOP_CTRL 0x408U
#define WIZ_SERDES_RST      0x40CU
#define WIZ_SERDES_TYPEC    0x410U
#define WIZ_LANECTL(n)      (0x480U + (0x40U * (n)))
#define WIZ_LANEDIV(n)      (0x484U + (0x40U * (n)))

#define REF_CLK_100MHZ    100000000UL
#define REF_CLK_156_25MHZ 156250000UL

// AM243x: single USB3 SERDES lane
#define WIZ_MAX_LANES      1U
#define WIZ_MUX_NUM_CLOCKS 3U // PLL0_REFCLK, PLL1_REFCLK, REFCLK_DIG

// p_enable field values
#define P_ENABLE       2U
#define P_ENABLE_FORCE 1U

// TYPES //////////////////////////////////////////////////////////////////////////
typedef struct
{
    struct regmap regmap; // holds peripheral base address

    // global control fields
    struct regmap_field por_en;
    struct regmap_field phy_reset_n;
    struct regmap_field phy_en_refclk;
    struct regmap_field pma_cmn_refclk_int_mode;
    struct regmap_field pma_cmn_refclk1_int_mode;
    struct regmap_field pma_cmn_refclk_mode;
    struct regmap_field pma_cmn_refclk_dig_div;
    struct regmap_field typec_ln10_swap;
    struct regmap_field typec_ln23_swap;
    uint32_t typec_swap;

    // clock mux selects: [0]=PLL0_REFCLK [1]=PLL1_REFCLK [2]=REFCLK_DIG
    struct regmap_field mux_sel[WIZ_MUX_NUM_CLOCKS];

    // per-lane fields (WIZ_MAX_LANES == 1 for AM243x)
    struct regmap_field p_enable[WIZ_MAX_LANES];
    struct regmap_field p_align[WIZ_MAX_LANES];
    struct regmap_field p_raw_auto_start[WIZ_MAX_LANES];
    struct regmap_field p_standard_mode[WIZ_MAX_LANES];
    struct regmap_field p0_fullrt_div[WIZ_MAX_LANES];
    struct regmap_field p0_mac_src_sel[WIZ_MAX_LANES];
    struct regmap_field p0_rxfclk_sel[WIZ_MAX_LANES];
    struct regmap_field p0_refclk_sel[WIZ_MAX_LANES];
    struct regmap_field p_mac_div_sel0[WIZ_MAX_LANES];
    struct regmap_field p_mac_div_sel1[WIZ_MAX_LANES];

    uint32_t lane_phy_type[WIZ_MAX_LANES];
} am64x_wiz_t;

// VARIABLES //////////////////////////////////////////////////////////////////////
// Driver state - statically allocated, zero heap
static am64x_wiz_t _wiz; // single static instance

// ------------------------------------------------------------------
// Static reg_field descriptors
// ------------------------------------------------------------------

// WIZ_SERDES_CTRL
static const struct reg_field RF_POR_EN = REG_FIELD(WIZ_SERDES_CTRL, 31, 31);

// WIZ_SERDES_RST  - mux selects and resets
static const struct reg_field RF_PHY_RESET_N = REG_FIELD(WIZ_SERDES_RST, 31, 31);
static const struct reg_field RF_PHY_EN_REFCLK = REG_FIELD(WIZ_SERDES_RST, 30, 30);
static const struct reg_field RF_PLL1_REFCLK_MUX_SEL = REG_FIELD(WIZ_SERDES_RST, 29, 29);
static const struct reg_field RF_PLL0_REFCLK_MUX_SEL = REG_FIELD(WIZ_SERDES_RST, 28, 28);
static const struct reg_field RF_REFCLK_DIG_SEL = REG_FIELD(WIZ_SERDES_RST, 24, 24);

// WIZ_SERDES_TOP_CTRL - PMA common clock settings
static const struct reg_field RF_PMA_CMN_REFCLK_INT_MODE = REG_FIELD(WIZ_SERDES_TOP_CTRL, 28, 29);
static const struct reg_field RF_PMA_CMN_REFCLK1_INT_MODE = REG_FIELD(WIZ_SERDES_TOP_CTRL, 20, 21);
static const struct reg_field RF_PMA_CMN_REFCLK_MODE = REG_FIELD(WIZ_SERDES_TOP_CTRL, 30, 31);
static const struct reg_field RF_PMA_CMN_REFCLK_DIG_DIV = REG_FIELD(WIZ_SERDES_TOP_CTRL, 26, 27);

static const struct reg_field RF_TYPEC_LN10_SWAP = REG_FIELD(WIZ_SERDES_TYPEC, 30, 30);
static const struct reg_field RF_TYPEC_LN23_SWAP = REG_FIELD(WIZ_SERDES_TYPEC, 31, 31);

// WIZ_LANECTL(0) - lane 0 control
static const struct reg_field RF_P_ENABLE = REG_FIELD(WIZ_LANECTL(0), 30, 31);
static const struct reg_field RF_P_ALIGN = REG_FIELD(WIZ_LANECTL(0), 29, 29);
static const struct reg_field RF_P_RAW_AUTO_START = REG_FIELD(WIZ_LANECTL(0), 28, 28);
static const struct reg_field RF_P_STANDARD_MODE = REG_FIELD(WIZ_LANECTL(0), 24, 25);
static const struct reg_field RF_P0_FULLRT_DIV = REG_FIELD(WIZ_LANECTL(0), 22, 23);
static const struct reg_field RF_P0_MAC_SRC_SEL = REG_FIELD(WIZ_LANECTL(0), 20, 21);
static const struct reg_field RF_P0_RXFCLK_SEL = REG_FIELD(WIZ_LANECTL(0), 6, 7);
static const struct reg_field RF_P0_REFCLK_SEL = REG_FIELD(WIZ_LANECTL(0), 18, 19);

// WIZ_LANEDIV(0) - lane 0 dividers
static const struct reg_field RF_P_MAC_DIV_SEL0 = REG_FIELD(WIZ_LANEDIV(0), 16, 22);
static const struct reg_field RF_P_MAC_DIV_SEL1 = REG_FIELD(WIZ_LANEDIV(0), 0, 8);

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static void field_init(struct regmap_field *f, struct regmap *rm, struct reg_field rf);
static void wiz_regfield_init(am64x_wiz_t *w);
static int wiz_reset(am64x_wiz_t *w);
static void wiz_clock_init(am64x_wiz_t *w, uint32_t core_hz);
static int wiz_mode_select(am64x_wiz_t *w);
static int wiz_p_mac_div_sel(am64x_wiz_t *w);
static int wiz_init_raw_interface(am64x_wiz_t *w);
static int wiz_phy_fullrt_div(am64x_wiz_t *w);
static int wiz_init(am64x_wiz_t *w);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
// ------------------------------------------------------------------
// Internal helpers
// ------------------------------------------------------------------

static void field_init(struct regmap_field *f, struct regmap *rm, struct reg_field rf)
{
    f->regmap = rm;
    f->reg = rf.reg;
    f->shift = rf.lsb;
    f->mask = _regmap_mask(rf.lsb, rf.msb);
}

static void wiz_regfield_init(am64x_wiz_t *w)
{
    struct regmap *rm = &w->regmap;

    field_init(&w->por_en, rm, RF_POR_EN);
    field_init(&w->phy_reset_n, rm, RF_PHY_RESET_N);
    field_init(&w->phy_en_refclk, rm, RF_PHY_EN_REFCLK);
    field_init(&w->pma_cmn_refclk_int_mode, rm, RF_PMA_CMN_REFCLK_INT_MODE);
    field_init(&w->pma_cmn_refclk1_int_mode, rm, RF_PMA_CMN_REFCLK1_INT_MODE);
    field_init(&w->pma_cmn_refclk_mode, rm, RF_PMA_CMN_REFCLK_MODE);
    field_init(&w->pma_cmn_refclk_dig_div, rm, RF_PMA_CMN_REFCLK_DIG_DIV);
    field_init(&w->typec_ln10_swap, rm, RF_TYPEC_LN10_SWAP);
    field_init(&w->typec_ln23_swap, rm, RF_TYPEC_LN23_SWAP);

    // clock mux selects (AM64x uses WIZ_SERDES_RST fields directly)
    field_init(&w->mux_sel[0], rm, RF_PLL0_REFCLK_MUX_SEL);
    field_init(&w->mux_sel[1], rm, RF_PLL1_REFCLK_MUX_SEL);
    field_init(&w->mux_sel[2], rm, RF_REFCLK_DIG_SEL);

    // lane 0
    field_init(&w->p_enable[0], rm, RF_P_ENABLE);
    field_init(&w->p_align[0], rm, RF_P_ALIGN);
    field_init(&w->p_raw_auto_start[0], rm, RF_P_RAW_AUTO_START);
    field_init(&w->p_standard_mode[0], rm, RF_P_STANDARD_MODE);
    field_init(&w->p0_fullrt_div[0], rm, RF_P0_FULLRT_DIV);
    field_init(&w->p0_mac_src_sel[0], rm, RF_P0_MAC_SRC_SEL);
    field_init(&w->p0_rxfclk_sel[0], rm, RF_P0_RXFCLK_SEL);
    field_init(&w->p0_refclk_sel[0], rm, RF_P0_REFCLK_SEL);
    field_init(&w->p_mac_div_sel0[0], rm, RF_P_MAC_DIV_SEL0);
    field_init(&w->p_mac_div_sel1[0], rm, RF_P_MAC_DIV_SEL1);
}

// POR pulse - resets the SERDES block
static int wiz_reset(am64x_wiz_t *w)
{
    int ret;

    ret = regmap_field_write(&w->por_en, 0x1);
    if (ret)
    {
        return ret;
    }

    WIZ_MDELAY_MS(1);

    return regmap_field_write(&w->por_en, 0x0);
}

// * Configure common clock registers from the core reference clock frequency.
//  *
//  * All three mux outputs (PLL0, PLL1, REFCLK_DIG) are routed to the core
//  * reference clock - the external reference clock is not used, so
//  * pma_cmn_refclk_mode is derived from core_hz as well.
static void wiz_clock_init(am64x_wiz_t *w, uint32_t core_hz)
{
    regmap_field_write(&w->pma_cmn_refclk_int_mode, (core_hz >= REF_CLK_100MHZ) ? 0x1U : 0x3U);
    regmap_field_write(&w->pma_cmn_refclk1_int_mode, (core_hz >= REF_CLK_100MHZ) ? 0x1U : 0x3U);

    switch (core_hz)
    {
    case REF_CLK_100MHZ:
        regmap_field_write(&w->pma_cmn_refclk_dig_div, 0x2U);
        break;
    case REF_CLK_156_25MHZ:
        regmap_field_write(&w->pma_cmn_refclk_dig_div, 0x3U);
        break;
    default:
        regmap_field_write(&w->pma_cmn_refclk_dig_div, 0x0U);
        break;
    }

    regmap_field_write(&w->pma_cmn_refclk_mode, (core_hz >= REF_CLK_100MHZ) ? 0x0U : 0x2U);

    // Route core ref clk -> PLL0, PLL1, REFCLK_DIG
    regmap_field_write(&w->mux_sel[0], 1U);
    regmap_field_write(&w->mux_sel[1], 1U);
    regmap_field_write(&w->mux_sel[2], 1U);
}

// Set lane standard mode and MAC source for the configured PHY type
static int wiz_mode_select(am64x_wiz_t *w)
{
    uint32_t i;

    for (i = 0U; i < WIZ_MAX_LANES; i++)
    {
        uint32_t mode;

        switch (w->lane_phy_type[i])
        {
        case PHY_TYPE_DP:
            mode = 0U; // LANE_MODE_GEN1
            break;
        case PHY_TYPE_QSGMII:
            mode = 1U; // LANE_MODE_GEN2
            break;
        case PHY_TYPE_USXGMII:
            regmap_field_write(&w->p0_mac_src_sel[i], 0x3U);
            regmap_field_write(&w->p0_rxfclk_sel[i], 0x3U);
            regmap_field_write(&w->p0_refclk_sel[i], 0x2U);
            mode = 1U; // LANE_MODE_GEN2
            break;
        case PHY_TYPE_PCIE:
            mode = 2U; // LANE_MODE_GEN3
            break;
        case PHY_TYPE_USB3:
        default:
            continue;
        }

        if (regmap_field_write(&w->p_standard_mode[i], mode))
        {
            return -EINVAL;
        }
    }

    return 0;
}

// Configure MAC dividers for Ethernet PHY types
static int wiz_p_mac_div_sel(am64x_wiz_t *w)
{
    uint32_t i;

    for (i = 0U; i < WIZ_MAX_LANES; i++)
    {
        uint32_t t = w->lane_phy_type[i];

        if (t == PHY_TYPE_SGMII || t == PHY_TYPE_QSGMII || t == PHY_TYPE_USXGMII)
        {
            if (regmap_field_write(&w->p_mac_div_sel0[i], 1U))
            {
                return -EINVAL;
            }
            if (regmap_field_write(&w->p_mac_div_sel1[i], 2U))
            {
                return -EINVAL;
            }
        }
    }

    return 0;
}

// Enable raw (non-framed) alignment on all lanes
static int wiz_init_raw_interface(am64x_wiz_t *w)
{
    uint32_t i;

    for (i = 0U; i < WIZ_MAX_LANES; i++)
    {
        if (regmap_field_write(&w->p_align[i], 1U))
        {
            return -EINVAL;
        }
        if (regmap_field_write(&w->p_raw_auto_start[i], 1U))
        {
            return -EINVAL;
        }
    }

    return 0;
}

// Configure full-rate divider for the lane PHY type
static int wiz_phy_fullrt_div(am64x_wiz_t *w)
{
    uint32_t i;

    for (i = 0U; i < WIZ_MAX_LANES; i++)
    {
        uint32_t div = 0U;

        switch (w->lane_phy_type[i])
        {
        case PHY_TYPE_PCIE:
            div = 1U;
            break;
        case PHY_TYPE_USB3:
        default:
            continue; // Skip writing for USB3 to match Linux
        }

        if (regmap_field_write(&w->p0_fullrt_div[i], div))
        {
            return -EINVAL;
        }
    }

    return 0;
}

// Full WIZ hardware initialisation sequence
static int wiz_init(am64x_wiz_t *w)
{
    int ret;

    ret = wiz_reset(w);
    if (ret)
    {
        WIZ_LOG("reset failed (%d)", ret);
        return ret;
    }

    ret = wiz_mode_select(w);
    if (ret)
    {
        WIZ_LOG("mode select failed (%d)", ret);
        return ret;
    }

    ret = wiz_phy_fullrt_div(w);
    if (ret)
    {
        WIZ_LOG("fullrt div failed (%d)", ret);
        return ret;
    }

    ret = wiz_p_mac_div_sel(w);
    if (ret)
    {
        WIZ_LOG("MAC div sel failed (%d)", ret);
        return ret;
    }

    ret = wiz_init_raw_interface(w);
    if (ret)
    {
        WIZ_LOG("raw interface init failed (%d)", ret);
        return ret;
    }

    // phy_en_refclk must be enabled before configuring Torrent registers
    regmap_field_write(&w->phy_en_refclk, 1U);

    // cdns_torrent_phy_init(): write Torrent register tables BEFORE
    //      * deasserting the WIZ resets (matches Linux driver order).
    am64x_torrent_phy_configure((uintptr_t)w->regmap.base, 1); // 0 = no SSC

    // cdns_torrent_phy_on() step 1: deassert lane reset (p_enable).
    //      * cdns_torrent_phy_on() step 2: deassert PHY reset (phy_reset_n).
    if (w->lane_phy_type[0] == PHY_TYPE_USB3)
    {
        regmap_field_write(&w->typec_ln10_swap, w->typec_swap ? 1U : 0U);
        regmap_field_write(&w->typec_ln23_swap, 0U);
    }

    uint32_t p_en = (w->lane_phy_type[0] == PHY_TYPE_DP) ? P_ENABLE : P_ENABLE_FORCE;
    regmap_field_write(&w->p_enable[0], p_en);

    regmap_field_write(&w->phy_reset_n, 1U);

    // cdns_torrent_phy_on() step 3: poll CMN_READY + PCS link ready.
    return am64x_torrent_phy_wait_ready((uintptr_t)w->regmap.base, 100000U);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
// ------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------

// *
//  * am64x_wiz_init() - Initialise the AM64x WIZ SERDES PHY wrapper
//  *
//  * @base_address:        Byte address of the WIZ SERDES register block
//  *                    (e.g. 0x00900000 for SERDES0 on AM243x)
//  * @phy_type:         PHY_TYPE_USB3, PHY_TYPE_PCIE, PHY_TYPE_SGMII, ...
//  *                    (see phy.h)
//  * @lane_swap:        Set nonzero to enable USB3 lane swap for lane 0.
//  * @core_ref_clk_hz:  Core reference clock frequency in Hz
//  *                    (e.g. 100000000 for 100 MHz)
//  *
//  * Returns 0 on success, negative errno on failure.
//  * Skips hardware re-init if the lane is already enabled (warm boot).
int am64x_wiz_init(uintptr_t base_address, uint32_t phy_type, uint32_t lane_swap, uint32_t core_ref_clk_hz)
{
    am64x_wiz_t *w = &_wiz;
    unsigned int val;
    uint32_t i;
    int already_up = 0;

    memset(w, 0, sizeof(*w));

    w->regmap.base = (volatile uint32_t *)base_address;

    for (i = 0U; i < WIZ_MAX_LANES; i++)
    {
        w->lane_phy_type[i] = phy_type;
    }
    w->typec_swap = lane_swap ? 1U : 0U;

    wiz_regfield_init(w);
    wiz_clock_init(w, core_ref_clk_hz);

    // Skip hardware init if already running (e.g. after warm reset)
    for (i = 0U; i < WIZ_MAX_LANES; i++)
    {
        regmap_field_read(&w->p_enable[i], &val);
        if (val & (P_ENABLE | P_ENABLE_FORCE))
        {
            already_up = 1;
            break;
        }
    }

    if (!already_up)
    {
        return wiz_init(w);
    }

    return 0;
}
// EOF /////////////////////////////////////////////////////////////////////////////
