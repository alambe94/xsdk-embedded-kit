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

// @file am64x_torrent.c
// @brief AM64x Torrent SERDES PHY driver implementation.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "am64x_phy.h"

// MACROS /////////////////////////////////////////////////////////////////////////
// ------------------------------------------------------------------
// Platform hooks
// ------------------------------------------------------------------

#ifndef TORRENT_LOG
#define TORRENT_LOG(...)                                                                                                                   \
    do                                                                                                                                     \
    {                                                                                                                                      \
        printf("[TORRENT] ");                                                                                                              \
        printf(__VA_ARGS__);                                                                                                               \
        printf("\n");                                                                                                                      \
    } while (0)
#endif

// ------------------------------------------------------------------
// SD register block base addresses (all offsets from sd_base)
// block_offset_shift = 0  ->  TORRENT_*_OFFSET(ln, 0, 1)
// ------------------------------------------------------------------

#define CMN_BASE(sd)     ((sd) + 0x00000U) // Common CDB
#define TX0_BASE(sd)     ((sd) + 0x04000U) // TX lane 0 CDB
#define RX0_BASE(sd)     ((sd) + 0x08000U) // RX lane 0 CDB
#define PCS_CMN_BASE(sd) ((sd) + 0x0C000U) // PHY PCS common
#define PCS_LN0_BASE(sd) ((sd) + 0x0D000U) // PHY PCS lane 0
#define PMA_CMN_BASE(sd) ((sd) + 0x0E000U) // PHY PMA common

// 16-bit register read/write (reg_offset_shift = 1 -> byte offset = index * 2)
#define SD_WR(base, index, val) (*(volatile uint16_t *)((uint8_t *)(base) + ((index) << 1)) = (uint16_t)(val))
#define SD_RD(base, index)      (*(const volatile uint16_t *)((const uint8_t *)(base) + ((index) << 1)))

// ------------------------------------------------------------------
// Register index constants (from phy-cadence-torrent.c)
// ------------------------------------------------------------------

// Common CDB
#define CMN_PDIAG_PLL0_CLK_SEL_M0    0x01A1U
#define CMN_PLL0_VCOCAL_TCTRL        0x0082U
#define CMN_PLL1_VCOCAL_TCTRL        0x00C2U
#define CMN_PLL1_DSM_FBH_OVRD_M0     0x00D5U
#define CMN_PLL1_DSM_FBL_OVRD_M0     0x00D6U
#define CMN_PDIAG_PLL1_CP_PADJ_M0    0x01C4U
#define CMN_CDIAG_CDB_PWRI_OVRD      0x0041U
#define CMN_CDIAG_XCVRC_PWRI_OVRD    0x0047U
#define CMN_PLL0_DSM_DIAG_M0         0x0094U
#define CMN_PLL1_DSM_DIAG_M0         0x00D4U
#define CMN_PDIAG_PLL0_CP_PADJ_M0    0x01A4U
#define CMN_PDIAG_PLL0_CP_IADJ_M0    0x01A5U
#define CMN_PDIAG_PLL1_CP_IADJ_M0    0x01C5U
#define CMN_PDIAG_PLL0_FILT_PADJ_M0  0x01A6U
#define CMN_PDIAG_PLL1_FILT_PADJ_M0  0x01C6U
#define CMN_PLL0_INTDIV_M0           0x0090U
#define CMN_PLL1_INTDIV_M0           0x00D0U
#define CMN_PLL0_FRACDIVH_M0         0x0092U
#define CMN_PLL1_FRACDIVH_M0         0x00D2U
#define CMN_PLL0_HIGH_THR_M0         0x0093U
#define CMN_PLL1_HIGH_THR_M0         0x00D3U
#define CMN_PDIAG_PLL0_CTRL_M0       0x01A0U
#define CMN_PDIAG_PLL1_CTRL_M0       0x01C0U
#define CMN_PLL0_SS_CTRL1_M0         0x0098U
#define CMN_PLL1_SS_CTRL1_M0         0x00D8U
#define CMN_PLL0_SS_CTRL2_M0         0x0099U
#define CMN_PLL1_SS_CTRL2_M0         0x00D9U
#define CMN_PLL0_SS_CTRL3_M0         0x009AU
#define CMN_PLL1_SS_CTRL3_M0         0x00DAU
#define CMN_PLL0_SS_CTRL4_M0         0x009BU
#define CMN_PLL1_SS_CTRL4_M0         0x00DBU
#define CMN_PLL0_VCOCAL_REFTIM_START 0x0086U
#define CMN_PLL1_VCOCAL_REFTIM_START 0x00C6U
#define CMN_PLL0_VCOCAL_PLLCNT_START 0x0088U
#define CMN_PLL1_VCOCAL_PLLCNT_START 0x00C8U
#define CMN_PLL0_LOCK_REFCNT_START   0x009CU
#define CMN_PLL1_LOCK_REFCNT_START   0x00DCU
#define CMN_PLL0_LOCK_PLLCNT_START   0x009EU
#define CMN_PLL1_LOCK_PLLCNT_START   0x00DEU
#define CMN_PLL0_LOCK_PLLCNT_THR     0x009FU
#define CMN_PLL1_LOCK_PLLCNT_THR     0x00DFU
#define CMN_TXPUCAL_TUNE             0x0103U
#define CMN_TXPDCAL_TUNE             0x010BU

// PHY PCS common
#define PHY_PLL_CONFIG                  0x000EU
#define PHY_PIPE_USB3_GEN2_PRE_CONFIG0  0x0020U
#define PHY_PIPE_USB3_GEN2_POST_CONFIG0 0x0022U
#define PHY_PIPE_USB3_GEN2_POST_CONFIG1 0x0023U

// PHY PMA common - for CMN_READY poll
#define PHY_PMA_CMN_CTRL1 0x0000U

// PHY PCS lane 0 - for link-ready poll
#define PHY_PCS_ISO_LINK_CTRL 0x000BU

// TX lane 0 CDB
#define TX_PSC_A0             0x0100U
#define TX_PSC_A1             0x0101U
#define TX_PSC_A2             0x0102U
#define TX_PSC_A3             0x0103U
#define TX_TXCC_CTRL          0x0040U
#define TX_TXCC_CPOST_MULT_01 0x004DU
#define XCVR_DIAG_PSC_OVRD    0x00EBU
#define XCVR_DIAG_HSCLK_SEL   0x00E6U
#define XCVR_DIAG_HSCLK_DIV   0x00E7U
#define XCVR_DIAG_PLLDRC_CTRL 0x00E5U

// RX lane 0 CDB
#define RX_PSC_A0              0x0000U
#define RX_PSC_A1              0x0001U
#define RX_PSC_A2              0x0002U
#define RX_PSC_A3              0x0003U
#define RX_SIGDET_HL_FILT_TMR  0x0090U
#define RX_REE_GCSM1_CTRL      0x0108U
#define RX_REE_ATTEN_THR       0x0149U
#define RX_REE_SMGM_CTRL1      0x0177U
#define RX_REE_SMGM_CTRL2      0x0178U
#define RX_REE_PEAK_UTHR       0x0142U
#define RX_REE_PEAK_LTHR       0x0143U
#define RX_REE_TAP1_CLIP       0x0171U
#define RX_REE_TAP2TON_CLIP    0x0172U
#define RX_DIAG_SIGDET_TUNE    0x01E8U
#define RX_DIAG_NQST_CTRL      0x01E5U
#define RX_DIAG_DFE_AMP_TUNE_2 0x01E2U
#define RX_DIAG_DFE_AMP_TUNE_3 0x01E3U
#define RX_DIAG_PI_CAP         0x01F5U
#define RX_DIAG_PI_RATE        0x01F4U
#define RX_DIAG_ACYA           0x01FFU
#define RX_CDRLF_CNFG          0x0080U
#define RX_CDRLF_CNFG3         0x0082U

// TYPES //////////////////////////////////////////////////////////////////////////
// ------------------------------------------------------------------
// Register tables - USB3 single-link, 100 MHz with optional internal SSC
// (Tables selected by J7200/AM64x variant for TYPE_USB + TYPE_NONE)
// ------------------------------------------------------------------

typedef struct
{
    uint16_t val;
    uint16_t reg;
} reg_pair_t;

// VARIABLES //////////////////////////////////////////////////////////////////////
// * 1a. PHY_PLL_CONFIG -> PHY PCS common block (0xC000).
//  *     In the Linux driver this is sl_usb_link_cmn_regs[0], written via
//  *     phy_pll_config which is a regmap_field in regmap_phy_pcs_common_cdb.
#define LINK_CMN_PHY_PLL_CONFIG_VAL 0x0000U

// * 1b. Remaining link_cmn entries -> Common CDB (0x0000).
//  *     sl_usb_link_cmn_regs[1..] written to regmap_common_cdb.
static const reg_pair_t link_cmn_cmn[] = {
    {0x8600, CMN_PDIAG_PLL0_CLK_SEL_M0},
};

// 2. Transceiver diagnostic (written to TX lane 0 CDB)
static const reg_pair_t xcvr_diag[] = {
    {0x0000, XCVR_DIAG_HSCLK_SEL},
    {0x0001, XCVR_DIAG_HSCLK_DIV},
    {0x0041, XCVR_DIAG_PLLDRC_CTRL},
};

// 3. PHY PCS common
static const reg_pair_t pcs_cmn[] = {
    {0x0A0A, PHY_PIPE_USB3_GEN2_PRE_CONFIG0},
    {0x1000, PHY_PIPE_USB3_GEN2_POST_CONFIG0},
    {0x0010, PHY_PIPE_USB3_GEN2_POST_CONFIG1},
};

// 4. PMA common - clock calibration (sl_usb_100_no_ssc_cmn, written to Common CDB)
static const reg_pair_t cmn_vals_no_ssc[] = {
    {0x0028, CMN_PDIAG_PLL1_CP_PADJ_M0}, {0x001E, CMN_PLL1_DSM_FBH_OVRD_M0}, {0x000C, CMN_PLL1_DSM_FBL_OVRD_M0},
    {0x0003, CMN_PLL0_VCOCAL_TCTRL},     {0x0003, CMN_PLL1_VCOCAL_TCTRL},    {0x8200, CMN_CDIAG_CDB_PWRI_OVRD},
    {0x8200, CMN_CDIAG_XCVRC_PWRI_OVRD},
};

// 4b. PMA common - clock calibration with internal SSC (usb_100_int_ssc_cmn, written to Common CDB)
static const reg_pair_t cmn_vals_ssc[] = {
    {0x0004, CMN_PLL0_DSM_DIAG_M0},
    {0x0004, CMN_PLL1_DSM_DIAG_M0},
    {0x0509, CMN_PDIAG_PLL0_CP_PADJ_M0},
    {0x0509, CMN_PDIAG_PLL1_CP_PADJ_M0},
    {0x0F00, CMN_PDIAG_PLL0_CP_IADJ_M0},
    {0x0F00, CMN_PDIAG_PLL1_CP_IADJ_M0},
    {0x0F08, CMN_PDIAG_PLL0_FILT_PADJ_M0},
    {0x0F08, CMN_PDIAG_PLL1_FILT_PADJ_M0},
    {0x0064, CMN_PLL0_INTDIV_M0},
    {0x0064, CMN_PLL1_INTDIV_M0},
    {0x0002, CMN_PLL0_FRACDIVH_M0},
    {0x0002, CMN_PLL1_FRACDIVH_M0},
    {0x0044, CMN_PLL0_HIGH_THR_M0},
    {0x0044, CMN_PLL1_HIGH_THR_M0},
    {0x0002, CMN_PDIAG_PLL0_CTRL_M0},
    {0x0002, CMN_PDIAG_PLL1_CTRL_M0},
    {0x0001, CMN_PLL0_SS_CTRL1_M0},
    {0x0001, CMN_PLL1_SS_CTRL1_M0},
    {0x011B, CMN_PLL0_SS_CTRL2_M0},
    {0x011B, CMN_PLL1_SS_CTRL2_M0},
    {0x006E, CMN_PLL0_SS_CTRL3_M0},
    {0x006E, CMN_PLL1_SS_CTRL3_M0},
    {0x000E, CMN_PLL0_SS_CTRL4_M0},
    {0x000E, CMN_PLL1_SS_CTRL4_M0},
    {0x0C5E, CMN_PLL0_VCOCAL_REFTIM_START},
    {0x0C5E, CMN_PLL1_VCOCAL_REFTIM_START},
    {0x0C56, CMN_PLL0_VCOCAL_PLLCNT_START},
    {0x0C56, CMN_PLL1_VCOCAL_PLLCNT_START},
    {0x0003, CMN_PLL0_VCOCAL_TCTRL},
    {0x0003, CMN_PLL1_VCOCAL_TCTRL},
    {0x00C7, CMN_PLL0_LOCK_REFCNT_START},
    {0x00C7, CMN_PLL1_LOCK_REFCNT_START},
    {0x00C7, CMN_PLL0_LOCK_PLLCNT_START},
    {0x00C7, CMN_PLL1_LOCK_PLLCNT_START},
    {0x0005, CMN_PLL0_LOCK_PLLCNT_THR},
    {0x0005, CMN_PLL1_LOCK_PLLCNT_THR},
    {0x8200, CMN_CDIAG_CDB_PWRI_OVRD},
    {0x8200, CMN_CDIAG_XCVRC_PWRI_OVRD},
    {0x007F, CMN_TXPUCAL_TUNE},
    {0x007F, CMN_TXPDCAL_TUNE},
};

// 5. TX lane 0 (usb_100_no_ssc_tx_ln)
static const reg_pair_t tx_ln[] = {
    {0x02FF, TX_PSC_A0},          {0x06AF, TX_PSC_A1},    {0x06AE, TX_PSC_A2},
    {0x06AE, TX_PSC_A3},          {0x2A82, TX_TXCC_CTRL}, {0x0014, TX_TXCC_CPOST_MULT_01},
    {0x0003, XCVR_DIAG_PSC_OVRD},
};

// 6. RX lane 0 (usb_100_no_ssc_rx_ln - 22 entries)
static const reg_pair_t rx_ln[] = {
    {0x0D1D, RX_PSC_A0},
    {0x0D1D, RX_PSC_A1},
    {0x0D00, RX_PSC_A2},
    {0x0500, RX_PSC_A3},
    {0x0013, RX_SIGDET_HL_FILT_TMR},
    {0x0000, RX_REE_GCSM1_CTRL},
    {0x0C02, RX_REE_ATTEN_THR},
    {0x0330, RX_REE_SMGM_CTRL1},
    {0x0300, RX_REE_SMGM_CTRL2},
    {0x0000, RX_REE_PEAK_UTHR},
    {0x01F5, RX_REE_PEAK_LTHR},
    {0x0019, RX_REE_TAP1_CLIP},
    {0x0019, RX_REE_TAP2TON_CLIP},
    {0x1004, RX_DIAG_SIGDET_TUNE},
    {0x00F9, RX_DIAG_NQST_CTRL},
    {0x0C01, RX_DIAG_DFE_AMP_TUNE_2},
    {0x0002, RX_DIAG_DFE_AMP_TUNE_3},
    {0x0000, RX_DIAG_PI_CAP},
    {0x0031, RX_DIAG_PI_RATE},
    {0x0001, RX_DIAG_ACYA},
    {0x018C, RX_CDRLF_CNFG},
    {0x0003, RX_CDRLF_CNFG3},
};

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static void write_regs(uintptr_t base, const reg_pair_t *tbl, size_t n);
static int poll_bit(uintptr_t base, uint16_t reg_index, uint16_t mask, uint16_t expected, uint32_t timeout_us);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
// ------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void write_regs(uintptr_t base, const reg_pair_t *tbl, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        SD_WR(base, tbl[i].reg, tbl[i].val);
    }
}

// * poll_bit - poll a 16-bit SD register until (value & mask) == expected.
//  * Retries once per microsecond tick (caller-provided) up to timeout_us times.
//  * Returns 0 on success, -1 on timeout.
static int poll_bit(uintptr_t base, uint16_t reg_index, uint16_t mask, uint16_t expected, uint32_t timeout_us)
{
    uint32_t t;
    for (t = 0; t < timeout_us; t++)
    {
        uint16_t v = SD_RD(base, reg_index);
        if ((v & mask) == expected)
        {
            return 0;
        }
        // 1 us busy wait - replace with your BSP delay if available
        volatile uint32_t d = 20U;
        while (d--)
            ;
    }
    return -1;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
// ------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------

// *
//  * am64x_torrent_phy_configure() - Write Torrent PHY register tables for USB3
//  *
//  * Must be called BEFORE the WIZ resets are deasserted (phy_reset_n, p_enable).
//  * Matches cdns_torrent_phy_init() in the Linux driver.
//  *
//  * @enable_ssc: Enable internal spread spectrum clocking (SSC)
void am64x_torrent_phy_configure(uintptr_t sd_base, int enable_ssc)
{
    uintptr_t cmn = CMN_BASE(sd_base);
    uintptr_t tx0 = TX0_BASE(sd_base);
    uintptr_t rx0 = RX0_BASE(sd_base);
    uintptr_t pcs_cmn_b = PCS_CMN_BASE(sd_base);

    // link_cmn[0]: PHY_PLL_CONFIG -> PCS CMN (via phy_pll_config field in Linux)
    SD_WR(pcs_cmn_b, PHY_PLL_CONFIG, LINK_CMN_PHY_PLL_CONFIG_VAL);

    // link_cmn[1..]: remaining entries -> Common CDB
    write_regs(cmn, link_cmn_cmn, ARRAY_SIZE(link_cmn_cmn));

    // xcvr_diag -> TX lane 0
    write_regs(tx0, xcvr_diag, ARRAY_SIZE(xcvr_diag));

    // pcs_cmn -> PHY PCS common
    write_regs(pcs_cmn_b, pcs_cmn, ARRAY_SIZE(pcs_cmn));

    // cmn_vals -> Common CDB (100 MHz clock calibration)
    if (enable_ssc)
    {
        write_regs(cmn, cmn_vals_ssc, ARRAY_SIZE(cmn_vals_ssc));
    }
    else
    {
        write_regs(cmn, cmn_vals_no_ssc, ARRAY_SIZE(cmn_vals_no_ssc));
    }

    // tx_ln -> TX lane 0
    write_regs(tx0, tx_ln, ARRAY_SIZE(tx_ln));

    // rx_ln -> RX lane 0
    write_regs(rx0, rx_ln, ARRAY_SIZE(rx_ln));
}

// *
//  * am64x_torrent_phy_wait_ready() - Poll for Torrent PHY ready after WIZ reset deassert
//  *
//  * Must be called AFTER phy_reset_n=1 and p_enable=P_ENABLE_FORCE.
//  * Matches cdns_torrent_phy_on() polling in the Linux driver.
//  *
//  * Returns 0 on success, -1 on timeout.
int am64x_torrent_phy_wait_ready(uintptr_t sd_base, uint32_t timeout_us)
{
    uintptr_t pma_cmn = PMA_CMN_BASE(sd_base);
    uintptr_t pcs_ln0 = PCS_LN0_BASE(sd_base);

    // PHY_PMA_CMN_CTRL1[0] asserts when PLLs are locked (CMN_READY)
    if (poll_bit(pma_cmn, PHY_PMA_CMN_CTRL1, 0x0001U, 0x0001U, timeout_us))
    {
        TORRENT_LOG("timeout waiting for CMN_READY");
        return -1;
    }

    // PHY_PCS_ISO_LINK_CTRL[1] deasserts when PIPE interface is ready
    if (poll_bit(pcs_ln0, PHY_PCS_ISO_LINK_CTRL, 0x0002U, 0x0000U, timeout_us))
    {
        TORRENT_LOG("timeout waiting for PCS link ready");
        return -1;
    }

    return 0;
}
// EOF /////////////////////////////////////////////////////////////////////////////
