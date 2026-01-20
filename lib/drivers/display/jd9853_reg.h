#pragma once

// Page 0
#include <stdint.h>
typedef enum {
    sequence_crtl = 0xB0,
    vgpow_set = 0xB2,
    gamma_set = 0xB7,
    otp_set = 0xB8,
    pow_ctrl = 0xB9,
    power_state = 0xBA,
    dcdc_sel = 0xBB,
    vddd_ctrl = 0xBC,
    setrgbif = 0xBD,
    gas_ctrl = 0xBE,
    setstba = 0xC0,
    setpanel = 0xC1,
    set_bist = 0xC2,
    setrgbcyc = 0xC3,
    set_tcon = 0xC4,
    set_r_gamma = 0xC8,
    if_option = 0xCE,
    set_gip_l = 0xD0,
    abe_ctrl = 0xD3,
    otp_repair = 0xD6,
    ramctrl = 0xD7,
    auto_disp_setting = 0xD8,
    opt_prog = 0xD9,
    set_wd = 0xDD,
    set_page = 0xDE,
    set_passwd = 0xDF,
    seteco = 0xE6,
    get_id_version = 0xE7,
    up_start_ctrl = 0xE8,
    setid = 0xE9,
    self_diag = 0xF0,
    spi_cmd = 0xF1,
    f5_cmd = 0xF5,
    up_sratus1 = 0xF8,
    up_starus2 = 0xF9,
    up_status3 = 0xFA,
    up_status4 = 0xFB,
} Jd9853Page0Reg;

// Page 1
typedef enum {
    power_opt = 0xB2,
    power_test = 0xB4,
    otp_esd_set = 0xB5,
    dcdc_otp = 0xB7,
    set_gpo = 0xBB,
    pwr_seq_access = 0xBC,
    pwr_seq_access_rd = 0xBD,
    ma_power = 0xBE,
    setrgbcyc2 = 0xC1,
    setstba2 = 0xC2,
    settcon_otp = 0xC3,
    set_gamma_opt = 0xC4,
    set_oscm = 0xC5,
    set_io_pad = 0xC8,
    set_inh_chksum = 0xD7,
    hwpin_off = 0xE7,
    std_sel = 0xE9,
    set_esd_det = 0xEB,
    set_wrx = 0xEE,
} Jd9853Page1Reg;

// Page 2
typedef enum {
    gram_opt = 0xB0,
    intr_acc_set = 0xB4,
    gram_tmg_set = 0xB5,
    gram_pwr_set = 0xB6,
    gram_xrepeir_l = 0xB8,
    gram_xrepeir_r = 0xB9,
    set_dbgsel = 0xBA,
    setrom = 0xBB,
    test_ctrl = 0xBC,
    inh_chksum_lock = 0xBD,
    inh_chksum_dbg = 0xBE,
    test_pad_sel = 0xBF,
    set_tc_chksum = 0xC0,
    set_tc_other = 0xC1,
    gram_bist_set = 0xE5,
    gram_bist_result = 0xE6,
    gram_chk_sum = 0xE7,
} Jd9853Page2Reg;

// Page 3

typedef enum {
    trimming2 = 0xB3,
    trimming3 = 0xB4,
    trimming4 = 0xB5,
    trimming6 = 0xB7,
    trimming9 = 0xBA,
    trim_gas = 0xBE,
    trimming_oscm1 = 0xC7,
    trimming_oscm2 = 0xC8,
} Jd9853Page3Reg;

// 2.5.1. SEQUENCE CTRL: Power on sequence control (Page0 - B0h)
typedef struct {
    uint8_t dc_dn_mode : 2; // Bits 1:0 — DC_DN_MODE[1:0]
    uint8_t dc_up_mode : 2; // Bits 3:2 — DC_UP_MODE[1:0]
    uint8_t reserved_1 : 4; // Bits 7:4 — Reserved
} Jd9853SequenceCtrlParam1;
typedef struct {
    uint8_t t8p        : 2; // Bits 1:0 — T8P[1:0]
    uint8_t t0p        : 2; // Bits 3:2 — T0P[1:0]
    uint8_t reserved_1 : 4; // Bits 7:4 — Reserved
} Jd9853SequenceCtrlParam2;
typedef struct {
    uint8_t pow_on_state : 1; // Bits 3:0 — POW_ON_STATE
    uint8_t reserved_1   : 7; // Bits 7:4 — Reserved
} Jd9853SequenceCtrlParam3;

// 2.5.2. VCOM_SET: Set VCOM voltage for normal scan direction(Page0 - B2h)
typedef struct {
    uint8_t vgpow_offset : 7; // Bits 6:0 — VGPOW_OFFSET[6:0]
    uint8_t reserved_1   : 1; // Bit 7 — Reserved
} Jd9853VcomSetParam1;
typedef struct {
    uint8_t vgpow_otp_times : 2; // Bits 1:0 — VGPOW_OTP_TIMES[1:0]
    uint8_t reserved_1      : 6; // Bits 7:2 — Reserved
} Jd9853VcomSetParam2;

// 2.5.3. GAMMA_SET: Set positive / negative voltage of Gamma power (Page0 - RB7h)
typedef struct {
    uint8_t vgsp_s     : 7; // Bits 6:0 — VGSP_S[6:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853GammaSetParam1;
typedef struct {
    uint8_t vgmp_s     : 7; // Bits 6:0 — VGMP_S[6:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853GammaSetParam2;
typedef struct {
    uint8_t vgsn_s     : 7; // Bits 6:0 — VGSN_S[6:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853GammaSetParam3;
typedef struct {
    uint8_t vgmn_s     : 7; // Bits 6:0 — VGMN_S[6:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853GammaSetParam4;

// 2.5.4. OTP_SET: Set internal OTP program related setting(Page0 - B8h)
typedef struct {
    uint8_t vpp_vghz_rt : 3; // Bits 2:0 — VPP_VGHZ_RT[1:0]
    uint8_t reserved_1  : 2; // Bits 3:4 — Reserved
    uint8_t vpp_dt      : 2; // Bits 5:4 — VPP_DT[1:0]
    uint8_t reserved_2  : 2; // Bits 7:6 — Reserved
} Jd9853OtpSetParam1;
typedef struct {
    uint8_t vpp_vghz_clk    : 3; // Bits 2:0 — VPP_VGHZ_CLK[2:0]
    uint8_t vpp_pwrclk_div2 : 1; // Bit 3 — VPP_PWRCLK_DIV2
    uint8_t vpp_vghz_s      : 3; // Bits 6:4 — VPP_VGHZ_S[2:0]
    uint8_t reserved_1      : 1; // Bit 7 — Reserved
} Jd9853OtpSetParam2;
typedef struct {
    uint8_t vpp_mvz_s_clk : 3; // Bits 2:0 — VPP_MVZ_S_CLK[2:0]
    uint8_t reserved_1    : 1; // Bit 3 — Reserved
    uint8_t vpp_avddz_s   : 2; // Bits 5:4 — VPP_AVDDZ_S[1:0]
    uint8_t reserved_2    : 2; // Bits 7:6 — Reserved
} Jd9853OtpSetParam3;

// 2.5.5. POWER_CTRL2: Set power function related setting (Page0 - B9h)
typedef struct {
    uint8_t vgm_en     : 1; // Bit 0 — VGM_EN
    uint8_t reserved_1 : 1; // Bit 1 — Reserved
    uint8_t mvz_en     : 1; // Bit 2 — MVZ_EN
    uint8_t reserved_2 : 1; // Bits 3 — Reserved
    uint8_t ap         : 3; // Bits 6:4 — AP[2:0]
    uint8_t reserved_3 : 1; // Bit 7 — Reserved
} Jd9853PowerCtrlParam1;
typedef struct {
    uint8_t vglz_en    : 1; // Bit 0 — VGLZ_EN
    uint8_t vghz_en    : 1; // Bit 1 — VGHZ_EN
    uint8_t reserved_1 : 2; // Bits 3:2 — Reserved
    uint8_t vglz_nc    : 1; // Bit 4 — VGLZ_NC
    uint8_t vghz_nc    : 1; // Bit 5 — VGHZ_NC
    uint8_t aveez_nc   : 1; // Bit 6 — AVEEZ_NC
    uint8_t avddz_nc   : 1; // Bit 7 — AVDDZ_NC
} Jd9853PowerCtrlParam2;

// 2.5.6. POWER_STATE: Power status setting (Page0 - BAh)
typedef struct {
    uint8_t dstby      : 1; // Bits 0 — DSTBY
    uint8_t reserved_1 : 7; // Bits 7:1 — Reserved
} Jd9853PowerStateParam1;

// 2.5.7. DCDC_SEL: Power mode and charge pump related setting (Page0 - BBh)
typedef struct {
    uint8_t vglz_s      : 3; // Bits 2:0 — VGLZ_S[2:0]
    uint8_t pwrclk_div2 : 1; // Bit 3 — PWRCLK_DIV2
    uint8_t vghz_s      : 3; // Bits 6:4 — VGHZ_S[2:0]
    uint8_t reserved_1  : 1; // Bit 7 — Reserved
} Jd9853DcdcSelParam1;
typedef struct {
    uint8_t aveez_s : 2; // Bits 1:0 — AVEEZ_S[1:0]
    uint8_t vglz_rt : 2; // Bits 3:2 — VGLZ_RT[1:0]
    uint8_t avddz_s : 2; // Bits 5:4 — AVDDZ_S[1:0]
    uint8_t vghz_rt : 2; // Bits 7:6 — VGHZ_RT[1:0]
} Jd9853DcdcSelParam2;
typedef struct {
    uint8_t vglz_clk   : 3; // Bits 2:0 — VGLZ_CLK[2:0]
    uint8_t reserved_1 : 1; // Bit 3 — Reserved
    uint8_t vghz_clk   : 3; // Bits 6:4 — VGHZ_CLK[2:0]
    uint8_t reserved_2 : 1; // Bit 7 — Reserved
} Jd9853DcdcSelParam3;
typedef struct {
    uint8_t mvz_g_clk  : 3; // Bits 2:0 — MVZ_G_CLK[2:0]
    uint8_t reserved_1 : 1; // Bit 3 — Reserved
    uint8_t mvz_s_clk  : 3; // Bits 6:4 — MVZ_S_CLK[2:0]
    uint8_t reserved_2 : 1; // Bit 7 — Reserved
} Jd9853DcdcSelParam4;
typedef struct {
    uint8_t mv_clkp    : 2; // Bits 1:0 — MV_CLKP[1:0]
    uint8_t hv_clkp    : 2; // Bits 3:2 — HV_CLKP[1:0]
    uint8_t mvz_ns_clk : 3; // Bits 6:4 — MVZ_NS_CLK[2:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853DcdcSelParam5;
typedef struct {
    uint8_t mvz_ns_clke : 2; // Bits 1:0 — MVZ_NS_CLKE[1:0]
    uint8_t mvz_s_clke  : 2; // Bits 3:2 — MVZ_S_CLKE[1:0]
    uint8_t vglz_clke   : 2; // Bits 5:4 — VGLZ_CLKE[1:0]
    uint8_t vghz_clke   : 2; // Bits 7:6 — VGHZ_CLKE[1:0]
} Jd9853DcdcSelParam6;

// 2.5.8. VDDD_CTRL: Control logic voltage setting (Page0 - BCh)
typedef struct {
    uint8_t vddd_s     : 3; // Bits 2:0 — VDDD_S[2:0]
    uint8_t reserved_1 : 1; // Bit 3 — Reserved
    uint8_t avssn_s    : 2; // Bits 5:4 — AVSSN_S[1:0]
    uint8_t avssn_en   : 1; // Bit 6 — AVSSN_EN
    uint8_t reserved_2 : 1; // Bit 7 — Reserved
} Jd9853VdddCtrlParam1;

// 2.5.9. SETRGBIF: (Page0 - BDh)
typedef struct {
    uint8_t pcpl       : 1; // Bit 0 — PCPL
    uint8_t depl       : 1; // Bit 1 — DEPL
    uint8_t hspl       : 1; // Bit 2 — HSPL
    uint8_t vspl       : 1; // Bit 3 — VSPL
    uint8_t dem        : 1; // Bit 4 — DEM
    uint8_t blk_otp    : 1; // Bit 5 — BLK_OTP
    uint8_t reserved_1 : 2; // Bits 7:6 — Reserved
} Jd9853SetrgbifParam1;
typedef struct {
    uint8_t reserved_1 : 4; // Bits 3:0 — Reserved
    uint8_t rgb_sel    : 2; // Bits 1:0 — RGB_SEL[1:0]
    uint8_t reserved_2 : 2; // Bits 7:6 — Reserved
} Jd9853SetrgbifParam2;
typedef struct {
    uint8_t rgb_sync_ln; // Bits 7:0 — RGB_SYNC_LN[7:0]
} Jd9853SetrgbifParam3;
typedef struct {
    uint8_t rgb_sync_reso_x; // Bits 7:0 — RGB_SYNC_RESO_X[7:0]
} Jd9853SetrgbifParam4;
typedef struct {
    uint8_t rgb_sync_vbp; // Bits 7:0 — RGB_SYNC_VBP[7:0]
} Jd9853SetrgbifParam5;
typedef struct {
    uint8_t rgb_sync_hbp; // Bits 7:0 — RGB_SYNC_HBP[7:0]
} Jd9853SetrgbifParam6;

// 2.5.10. GAS_CTRL: GAS function control  (Page0 - BEh)
typedef struct {
    uint8_t reserved_1 : 8; // Bits 7:0 — Reserved
} Jd9853GasCtrlParam1;
typedef struct {
    uint8_t gas_blk_num  : 2; // Bits 1:0 — GAS_BLK_NUM[1:0]
    uint8_t gas_iovcc_en : 1; // Bit 2 — GAS_IOVCC_EN
    uint8_t gas_vci_en   : 1; // Bit 3 — GAS_VCI_EN
    uint8_t reserved_1   : 1; // Bits 4 — Reserved
    uint8_t gas_slpin_en : 1; // Bit 5 — GAS_SLPIN_EN
    uint8_t reserved_2   : 2; // Bits 7:6 — Reserved
} Jd9853GasCtrlParam2;
typedef struct {
    uint8_t gas_dbs_lth : 5; // Bits 4:0 — GAS_DBS_LTH[4:0]
    uint8_t reserved_1  : 1; // Bit 5 — Reserved
    uint8_t gas_dbs_gen : 1; // Bit 6 — GAS_DBS_GEN
    uint8_t gas_dbs_sen : 1; // Bit 7 — GAS_DBS_SEN
} Jd9853GasCtrlParam3;
typedef struct {
    uint8_t reserved_1    : 1; // Bit 0 — Reserved
    uint8_t dc_gas_mvz_en : 1; // Bit 1 — DC_GAS_MVZ_EN
    uint8_t reserved_2    : 2; // Bit 3:2 — Reserved
    uint8_t dc_gas_vgl_en : 1; // Bit 4 — DC_GAS_VGL_EN
    uint8_t dc_gas_vgh_en : 1; // Bit 5 — DC_GAS_VGH_EN
    uint8_t reserved_3    : 2; // Bits 7:6 — Reserved
} Jd9853GasCtrlParam4;

// 2.5.11. SETSTBA: Set Source Output driving ability (Page0 - C0h)
typedef struct {
    uint8_t sap        : 4; // Bits 3:0 — SAP[3:0]
    uint8_t gap        : 3; // Bits 6:4 — GAP[2:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853SetstbaParam1;
typedef struct {
    uint8_t sap2    : 4; // Bits 3:0 — SAP2[3:0]
    uint8_t gap2    : 3; // Bits 6:4 — GAP2[2:0]
    uint8_t gap2_en : 1; // Bit 7 — GAP2_EN
} Jd9853SetstbaParam2;

// 2.5.12. SETPANEL: Set Panel relate register (Page0 - C1h)
typedef struct {
    uint8_t reserved_1 : 1; // Bits 0 — Reserved
    uint8_t cfhr       : 1; // Bit 1 — CFHR
    uint8_t rev_panel  : 1; // Bit 2 — REV_PANEL
    uint8_t gs_panel   : 1; // Bit 3 — GS_PANEL
    uint8_t ss_panel   : 1; // Bits 4 — SS_PANEL
    uint8_t reserved_2 : 3; // Bits 7:5 — Reserved
} Jd9853SetpanelParam1;

// 2.5.13. SET_BIST: BIST Pattern setting (Page0 - C2h)
typedef struct {
    uint8_t test_pattern : 3; // Bits 2:0 — TEST_PATTERN[2:0]
    uint8_t test_pat_en  : 1; // Bit 3 — TEST_PAT_EN
    uint8_t reserved_1   : 1; // Bit 4 — Reserved
    uint8_t lnperlvl     : 2; // Bits 6:5 — LNPERLVL[1:0]
    uint8_t reserved_2   : 1; // Bit 7 — Reserved
} Jd9853SetBistParam1;

// 2.5.14. SETRGBCYC: Set Display Waveform Cycles of RGB Mode (Page0 - C3h)
typedef struct {
    uint8_t rgb_inv_np : 2; // Bits 1:0 — RGB_INV_NP[1:0]
    uint8_t idle_type  : 1; // Bit 2 — IDLE_TYPE
    uint8_t rgb_inv_i  : 2; // Bits 4:3 — RGB_INV_I[1:0]
    uint8_t rgb_inv_pi : 2; // Bits 6:5 — RGB_INV_PI[1:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853SetRgbCycParam1;
typedef struct {
    uint8_t rgb_gnd    : 4; // Bits 3:0 — RGB_GND[3:0]
    uint8_t reserved_1 : 4; // Bits 7:4 — Reserved
} Jd9853SetRgbCycParam2;
typedef struct {
    uint8_t rgb_eq1    : 5; // Bits 4:0 — RGB_EQ1[4:0]
    uint8_t reserved_1 : 3; // Bits 7:5 — Reserved
} Jd9853SetRgbCycParam3;
typedef struct {
    uint8_t rgb_eq2    : 5; // Bits 4:0 — RGB_EQ2[4:0]
    uint8_t reserved_1 : 3; // Bits 7:5 — Reserved
} Jd9853SetRgbCycParam4;
typedef struct {
    uint8_t rgb_chgen_on  : 6; // Bits 5:0 — RGB_CHGEN_ON[5:0]
    uint8_t rgb_chgen_off : 2; // Bits 7:6 — RGB_CHGEN_OFF[1:0]
} Jd9853SetRgbCycParam5;
typedef struct {
    uint8_t rgb_off; // Bits 7:0 — RGB_OFF[7:0]
} Jd9853SetRgbCycParam6;
typedef struct {
    uint8_t gaopoff         : 5; // Bits 4:0 — GAOPOFF[4:0]
    uint8_t gaopoffen       : 1; // Bit 5 — GAOPOFFEN
    uint8_t gaopoff_pol     : 1; // Bit 6 — GAOPOFF_POL
    uint8_t gaopoff_1st_off : 1; // Bit 7 — GAOPOFF_1ST_OFF
} Jd9853SetRgbCycParam7;
typedef struct {
    uint8_t sap2_on : 4; // Bits 3:0 — SAP2_ON[3:0]
    uint8_t gap2_on : 4; // Bits 7:4 — GAP2_ON[3:0]
} Jd9853SetRgbCycParam8;

// 2.5.15. SET TCON: Timing control setting (Page0 - C4h)
typedef struct {
    uint8_t te_opt     : 2; // Bits 1:0 — TE_OPT[1:0]
    uint8_t vbfp_ratio : 2; // Bits 3:2 — VBFP_RATIO[1:0]
    uint8_t reserved_1 : 4; // Bits 7:4 — Reserved
} Jd9853SetTconParam1;
typedef struct {
    uint8_t te_delay   : 7; // Bits 6:0 — TE_DELAY[6:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853SetTconParam2;
typedef struct {
    uint8_t ln; // Bits 7:0 — LN[7:0]
} Jd9853SetTconParam3;
typedef struct {
    uint8_t slt_np; // Bits 7:0 — SLT_NP[7:0]
} Jd9853SetTconParam4;
typedef struct {
    uint8_t vfp_np     : 7; // Bits 6:0 — VFP_NP[6:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853SetTconParam5;
typedef struct {
    uint8_t vbp_np     : 7; // Bits 6:0 — VBP_NP[6:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853SetTconParam6;
typedef struct {
    uint8_t hbp_np     : 7; // Bits 6:0 — HBP_NP[6:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853SetTconParam7;
typedef struct {
    uint8_t slt_i; // Bits 7:0 — SLT_I[7:0]
} Jd9853SetTconParam8;
typedef struct {
    uint8_t vfp_i      : 7; // Bits 6:0 — VFP_I[6:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853SetTconParam9;
typedef struct {
    uint8_t vbp_i      : 7; // Bits 6:0 — VBP_I[6:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853SetTconParam10;
typedef struct {
    uint8_t hbp_i      : 7; // Bits 6:0 — HBP_I[6:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853SetTconParam11;
typedef struct {
    uint8_t hfp_nck : 4; // Bits 3:0 — HFP_NCK[3:0]
    uint8_t hbp_nck : 4; // Bits 7:4 — HBP_NCK[3:0]
} Jd9853SetTconParam12;

// 2.5.16. SET_R_GAMMA: Set Red Gamma output voltage(Page0 - C8h)
typedef struct {
    uint8_t data       : 6; // Bits 5:0 — Data bits
    uint8_t reserved_1 : 2; // Bits 7:6 — Reserved
} Jd9853SetRGammaParamRegister;
typedef struct {
    Jd9853SetRGammaParamRegister rpa[16]; // Parameters 1-16 — RPA15 to RPA0
    Jd9853SetRGammaParamRegister rna[16]; // Parameters 17-32 — RNA15 to RNA0
} Jd9853SetRGammaParam;

// 2.5.17. IF_OPTION: Interface Optiopn (Page0-CEh)
typedef struct {
    uint8_t if_option  : 7; // Bits 6:0 — IF_OPTION[6:0], default 6Ah
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853IfOptionParam1;
typedef struct {
    uint8_t rgb_bit_rev       : 1; // Bit 0 — RGB_BIT_REV
    uint8_t rgb666_type       : 1; // Bit 1 — RGB666_TYPE
    uint8_t std_csx_low_en    : 1; // Bit 2 — STD_CSX_LOW_EN
    uint8_t std_rd_dcx_low_en : 1; // Bit 3 — STD_RD_DCX_LOW_EN
    uint8_t reserved_1        : 1; // Bit 4 — Reserved
    uint8_t qspi_sdo_type     : 2; // Bits 6:5 — QSPI_SDO_TYPE[1:0]
    uint8_t reserved_2        : 1; // Bit 7 — Reserved
} Jd9853IfOptionParam2;
typedef struct {
    uint8_t db_oe_opt       : 1; // Bit 0 — DB_OE_OPT
    uint8_t db_oe_9b_opt    : 1; // Bit 1 — DB_OE_9B_OPT
    uint8_t dummy_0b_03_en  : 1; // Bit 2 — DUMMY_0B_03_EN, default 1
    uint8_t dpi_stop_sol_en : 1; // Bit 3 — DPI_STOP_SOL_EN
    uint8_t mv_2a2b_ln_en   : 1; // Bit 4 — MV_2A2B_LN_EN
    uint8_t ln_2b_wr_en     : 1; // Bit 5 — LN_2B_WR_EN
    uint8_t reserved_1      : 2; // Bits 7:6 — Reserved
} Jd9853IfOptionParam3;

// 2.5.18. SET_GIP_L: SET CGOUTx_L Signal Mapping, GS_Panel=0 (Page0-D0h)
typedef struct {
    uint8_t gd_lrsw         : 1; // Bit 0 — GD_LRSW
    uint8_t reserved_1      : 1; // Bit 1 — Reserved
    uint8_t gd_gas_gate_clk : 1; // Bit 2 — GD_GAS_GATE_CLK
    uint8_t reserved_2      : 5; // Bits 7:3 — Reserved
} Jd9853SetGipLParam1;
typedef struct {
    uint8_t gd_on      : 6; // Bits 5:0 — GD_ON[5:0]
    uint8_t reserved_1 : 2; // Bits 7:6 — Reserved
} Jd9853SetGipLParam2;
typedef struct {
    uint8_t gd_off; // Bits 7:0 — GD_OFF[7:0]
} Jd9853SetGipLParam3;
typedef struct {
    uint8_t gd_eq_ptr0 : 5; // Bits 4:0 — GD_EQ_PTR0[4:0]
    uint8_t reserved_1 : 3; // Bits 7:5 — Reserved
} Jd9853SetGipLParam4;
typedef struct {
    uint8_t gd_eq_ptr1 : 5; // Bits 4:0 — GD_EQ_PTR1[4:0]
    uint8_t reserved_1 : 3; // Bits 7:5 — Reserved
} Jd9853SetGipLParam5;

// 2.5.19. ABE_CTRL: (Page0-D3h)
typedef struct {
    uint8_t le_abe_base : 5; // Bits 4:0 — LE_ABE_BASE[4:0]
    uint8_t abe_dim_en  : 1; // Bit 5 — ABE_DIM_EN
    uint8_t abe_en      : 1; // Bit 6 — ABE_EN
    uint8_t reserved_1  : 1; // Bit 7 — Reserved
} Jd9853AbeCtrlParam1;
typedef struct {
    uint8_t reserved_1       : 1; // Bit 0 — Reserved
    uint8_t coleng_sp_v63_en : 1; // Bit 1 — COLENG_SP_V63_EN
    uint8_t coleng_sp_v0_en  : 1; // Bit 2 — COLENG_SP_V0_EN
    uint8_t reserved_2       : 1; // Bit 3 — Reserved
    uint8_t abe_dim_fps      : 4; // Bits 7:4 — ABE_DIM_FPS[3:0]
} Jd9853AbeCtrlParam2;
typedef struct {
    uint8_t abe_gating_en; // Bits 7:0 — ABE_GATING_EN[7:0]
} Jd9853AbeCtrlParam3;

// 2.5.20. OTP_REPAIR: (Page0-D6h)
typedef struct {
    uint8_t repair_page : 2; // Bits 1:0 — REPAIR_PAGE[1:0]
    uint8_t reserved_1  : 6; // Bits 7:2 — Reserved
} Jd9853OtpRepairParam1;
typedef struct {
    uint8_t repair_index; // Bits 7:0 — REPAIR_INDEX[7:0]
} Jd9853OtpRepairParam2;
typedef struct {
    uint8_t repair_par; // Bits 7:0 — REPAIR_PAR[7:0]
} Jd9853OtpRepairParam3;
typedef struct {
    uint8_t repair_value; // Bits 7:0 — REPAIR_VALUE[7:0]
} Jd9853OtpRepairParam4;

// 2.5.21. RAMCTRL (Page0 - D7h)
typedef struct {
    uint8_t dm          : 2; // Bits 1:0 — DM[1:0]
    uint8_t mlbit_inv   : 1; // Bit 2 — MLBIT_INV
    uint8_t rm          : 1; // Bit 3 — RM
    uint8_t rp          : 1; // Bit 4 — RP
    uint8_t spi_2lan_en : 1; // Bit 5 — SPI_2LAN_EN
    uint8_t cr_option   : 1; // Bit 6 — CR_OPTION
    uint8_t reserved_1  : 1; // Bit 7 — Reserved
} Jd9853RamctrlParam1;
typedef struct {
    uint8_t mdt       : 2; // Bits 1:0 — MDT[1:0]
    uint8_t endian    : 2; // Bits 3:2 — ENDIAN[1:0]
    uint8_t epf       : 2; // Bits 5:4 — EPF[1:0]
    uint8_t ext_vs_pl : 1; // Bit 6 — EXT_VS_PL
    uint8_t scl_pl    : 1; // Bit 7 — SCL_PL
} Jd9853RamctrlParam2;

//2.5.22. AUTO_DISP_SETTING (Page0 - D8h)
typedef struct {
    uint8_t disp       : 2; // Bits 1:0 — DISP[1:0]
    uint8_t dte        : 1; // Bit 2 — DTE
    uint8_t gon        : 1; // Bit 3 — GON
    uint8_t reserved_1 : 4; // Bits 7:4 — Reserved
} Jd9853AutoDispSettingParam1;

typedef struct {
    uint8_t blk_mode    : 2; // Bits 1:0 — BLK_MODE[1:0]
    uint8_t blk_disable : 1; // Bit 2 — BLK_DISABLE
    uint8_t reserved_1  : 1; // Bit 3 — Reserved
    uint8_t disp_sw_opt : 2; // Bits 5:4 — DISP_SW_OPT[1:0]
    uint8_t reserved_2  : 2; // Bits 7:6 — Reserved
} Jd9853AutoDispSettingParam2;

// 2.5.23. OTP_PROG: OTP Program (Page0 - D9h)
typedef struct {
    uint8_t otp_index  : 7; // Bits 6:0 — OTP_INDEX[6:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853OtpProgParam1;
typedef struct {
    uint8_t otp_auto_prog    : 1; // Bit 0 — OTP_AUTO_PROG
    uint8_t otp_int_vpp      : 1; // Bit 1 — OTP_INT_VPP
    uint8_t otp_load_disable : 1; // Bit 2 — OTP_LOAD_DISABLE
    uint8_t otp_write_single : 1; // Bit 3 — OTP_WRITE_SINGLE
    uint8_t otp_read_single  : 1; // Bit 4 — OTP_READ_SINGLE
    uint8_t reserved_1       : 3; // Bits 7:5 — Reserved
} Jd9853OtpProgParam2;
typedef struct {
    uint8_t otp_rdata; // Bits 7:0 — OTP_RDATA[7:0]
} Jd9853OtpProgParam3;
typedef struct {
    uint8_t otp_wdata; // Bits 7:0 — OTP_WDATA[7:0]
} Jd9853OtpProgParam4;
typedef struct {
    uint8_t reserved_1  : 1; // Bit 0 — Reserved
    uint8_t otp_rld_ptm : 2; // Bits 2:1 — OTP_RLD_PTM[1:0]
    uint8_t reserved_2  : 5; // Bits 7:3 — Reserved
} Jd9853OtpProgParam5;
typedef struct {
    uint8_t otp_ptm     : 2; // Bits 1:0 — OTP_PTM[1:0]
    uint8_t reserved_1  : 2; // Bits 3:2 — Reserved
    uint8_t otp_pwe     : 1; // Bit 4 — OTP_PWE
    uint8_t otp_prd     : 1; // Bit 5 — OTP_PRD
    uint8_t otp_vpp_sel : 1; // Bit 6 — OTP_VPP_SEL
    uint8_t otp_pprog   : 1; // Bit 7 — OTP_PPROG
} Jd9853OtpProgParam6;

// 2.5.24. SET_WD: Setup watch dog (Page0 - DDh)
typedef struct {
    uint8_t wd_clk_sel : 3; // Bits 2:0 — WD_CLK_SEL[2:0]
    uint8_t fblk_en    : 1; // Bit 3 — FBLK_EN
    uint8_t wd_mode    : 2; // Bits 5:4 — WD_MODE[1:0]
    uint8_t wd_off     : 1; // Bit 6 — WD_OFF
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853SetWdParam1;
typedef struct {
    uint8_t wd_timer; // Bits 7:0 — WD_Timer[7:0]
} Jd9853SetWdParam2;
typedef struct {
    uint8_t esd_prt_en   : 1; // Bit 5 — ESD_PRT_EN
    uint8_t esd_sd_opt   : 1; // Bit 6 — ESD_SD_OPT
    uint8_t esd_tcon_opt : 1; // Bit 7 — ESD_TCON_OPT
    uint8_t reserved_1   : 5; // Bits 4:0 — Reserved
} Jd9853SetWdParam3;
typedef struct {
    uint8_t wd_fblk    : 1; // Bit 0 — WD_FBLK
    uint8_t wd_fms     : 1; // Bit 1 — WD_FMS
    uint8_t reserved_1 : 6; // Bits 7:2 — Reserved
} Jd9853SetWdParam4;

// 2.5.25. SET_PAGE(Page0 - DEh)
typedef struct {
    uint8_t page       : 2; // Bits 1:0 — PAGE[1:0]
    uint8_t reserved_1 : 6; // Bits 7:2 — Reserved
} Jd9853SetPageParam1;

//2.5.26. SET_PASSWD: (Page0 - DFh)
typedef struct {
    uint8_t passwd1; // Bits 7:0 — PASSWD1[7:0]
} Jd9853SetPasswdParam1;
typedef struct {
    uint8_t passwd2; // Bits 7:0 — PASSWD2[7:0]
} Jd9853SetPasswdParam2;

// 2.5.27. SETECO: ECO used(Page0 - E6h)
typedef struct {
    uint8_t eco        : 7; // Bits 6:0 — ECO[6:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853SetEcoParam1;

// 2.5.28.  GETIDVER: Get ID Version (Page0 - E7h)
typedef struct {
    uint8_t jd_id1; // Bits 7:0 — JD_ID1[7:0]
} Jd9853GetIdVerParam1;
typedef struct {
    uint8_t jd_id2; // Bits 7:0 — JD_ID2[7:0]
} Jd9853GetIdVerParam2;
typedef struct {
    uint8_t jd_id3; // Bits 7:0 — JD_ID3[7:0]
} Jd9853GetIdVerParam3;
typedef struct {
    uint8_t jd_id_passwd1; // Bits 7:0 — JD_ID_PASSWD1[7:0]
} Jd9853GetIdVerParam4;
typedef struct {
    uint8_t jd_id2_inhouse : 5; // Bits 4:0 — JD_ID2_INHOUSE[4:0]
    uint8_t jd_id1_inhouse : 3; // Bits 7:5 — JD_ID1_INHOUSE[2:0]
} Jd9853GetIdVerParam5;

// 2.5.29. UP_START_CTRL: Micro process control (Page0 - E8h)
typedef struct {
    uint8_t up_key1; // Bits 7:0 — UP_KEY1[7:0]
} Jd9853UpStartCtrlParam1;
typedef struct {
    uint8_t up_start_en; // Bits 7:0 — UP_START_EN[7:0]
} Jd9853UpStartCtrlParam2;
typedef struct {
    uint8_t pwr_key; // Bits 7:0 — PWR_KEY[7:0]
} Jd9853UpStartCtrlParam3;

//2.5.30. SETID: Set ID (Page0 - E9h)
typedef struct {
    uint8_t reserved_1 : 8; // Bits 7:0 — Reserved
} Jd9853SetIdParam1;
typedef struct {
    uint8_t id1; // Bits 7:0 — ID1[7:0]
} Jd9853SetIdParam2;
typedef struct {
    uint8_t id2; // Bits 7:0 — ID2[7:0]
} Jd9853SetIdParam3;
typedef struct {
    uint8_t id3; // Bits 7:0 — ID3[7:0]
} Jd9853SetIdParam4;
typedef struct {
    uint8_t id4; // Bits 7:0 — ID4[7:0]
} Jd9853SetIdParam5;
typedef struct {
    uint8_t id_otp_times : 2; // Bits 1:0 — ID_OTP_TIMES[1:0]
    uint8_t reserved_1   : 6; // Bits 7:2 — Reserved
} Jd9853SetIdParam6;

// 2.5.31. SELF_DIAG: UP debug used (Page0 - F0h)
typedef struct {
    uint8_t reserved_1    : 4; // Bits 3:0 — Reserved
    uint8_t gbreak_detec  : 1; // Bit 4 — GBREAK_DETEC
    uint8_t attach_detec  : 1; // Bit 5 — ATTACH_DETEC
    uint8_t func_detec    : 1; // Bit 6 — FUNC_DETEC
    uint8_t regload_detec : 1; // Bit 7 — REGLOAD_DETEC
} Jd9853SelfDiagParam1;

// 2.5.32. SPI CMD: Set SPI command index (Page0 - F1h)
typedef struct {
    uint8_t spi_cmd_index; // Bits 7:0 — SPI_CMD_INDEX[7:0]
} Jd9853SpiCmdParam1;

// 2.5.34. F5_CMD: Get standard command R44h value (Page0 - F5h)
typedef struct {
    uint8_t telin_value : 1; // Bit 0 — TELIN[8]
    uint8_t reserved_1  : 7; // Bits 7:1 — Reserved
} Jd9853F5CmdParam1;
typedef struct {
    uint8_t teline_value; // Bits 7:0 — TELINE[7:0]
} Jd9853F5CmdParam2;
typedef struct {
    uint8_t sc_value   : 1; // Bit 0 — SC[8]
    uint8_t reserved_1 : 7; // Bits 7:1 — Reserved
} Jd9853F5CmdParam3;
typedef struct {
    uint8_t sc_value; // Bits 7:0 — SC[7:0]
} Jd9853F5CmdParam4;
typedef struct {
    uint8_t ec_value   : 1; // Bit 0 — EC[8]
    uint8_t reserved_1 : 7; // Bits 7:1 — Reserved
} Jd9853F5CmdParam5;
typedef struct {
    uint8_t ec_value; // Bits 7:0 — EC[7:0]
} Jd9853F5CmdParam6;
typedef struct {
    uint8_t sp_value   : 1; // Bit 0 — SP[8]
    uint8_t reserved_1 : 7; // Bits 7:1 — Reserved
} Jd9853F5CmdParam7;
typedef struct {
    uint8_t sp_value; // Bits 7:0 — SP[7:0]
} Jd9853F5CmdParam8;
typedef struct {
    uint8_t ep         : 1; // Bit 0 — EP[8]
    uint8_t reserved_1 : 7; // Bits 7:1 — Reserved
} Jd9853F5CmdParam9;
typedef struct {
    uint8_t ep_value; // Bits 7:0 — EP[7:0]
} Jd9853F5CmdParam10;
typedef struct {
    uint8_t sr_value   : 1; // Bit 0 — SR[8]
    uint8_t reserved_1 : 7; // Bits 7:1 — Reserved
} Jd9853F5CmdParam11;
typedef struct {
    uint8_t sr_value; // Bits 7:0 — SR[7:0]
} Jd9853F5CmdParam12;
typedef struct {
    uint8_t er_value   : 1; // Bit 0 — ER[8]
    uint8_t reserved_1 : 7; // Bits 7:1 — Reserved
} Jd9853F5CmdParam13;
typedef struct {
    uint8_t er_value; // Bits 7:0 — ER[7:0]
} Jd9853F5CmdParam14;
typedef struct {
    uint8_t tfa_value  : 1; // Bit 0 — TFA[8]
    uint8_t reserved_1 : 7; // Bits 7:1 — Reserved
} Jd9853F5CmdParam15;
typedef struct {
    uint8_t tfa_value; // Bits 7:0 — TFA[7:0]
} Jd9853F5CmdParam16;
typedef struct {
    uint8_t vsa_value  : 1; // Bit 0 — VSA[8]
    uint8_t reserved_1 : 7; // Bits 7:1 — Reserved
} Jd9853F5CmdParam17;
typedef struct {
    uint8_t vsa_value; // Bits 7:0 — VSA[7:0]
} Jd9853F5CmdParam18;
typedef struct {
    uint8_t bfa_value  : 1; // Bit 0 — BFA[8]
    uint8_t reserved_1 : 7; // Bits 7:1 — Reserved
} Jd9853F5CmdParam19;
typedef struct {
    uint8_t bfa_value; // Bits 7:0 — BFA[7:0]
} Jd9853F5CmdParam20;
typedef struct {
    uint8_t vsp_value  : 1; // Bit 0 — VSP[8]
    uint8_t reserved_1 : 7; // Bits 7:1 — Reserved
} Jd9853F5CmdParam21;
typedef struct {
    uint8_t vsp_value; // Bits 7:0 — VSP[7:0]
} Jd9853F5CmdParam22;

// 2.5.35. UP_STATUS1: UP check status1(Page0 - F8h)
typedef struct {
    uint8_t up_sta1; // Bits 7:0 — UP_STA1[7:0]
} Jd9853UpStatus1Param1;

// 2.5.36. UP_STATUS2: UP check status2(Page0 - F9h)
typedef struct {
    uint8_t up_sta2; // Bits 7:0 — UP_STA2[7:0]
} Jd9853UpStatus2Param1;

// 2.5.37. UP_STATUS3: UP check status3(Page0 - FAh)
typedef struct {
    uint8_t up_sta3; // Bits 7:0 — UP_STA3[7:0]
} Jd9853UpStatus3Param1;

// 2.5.38. UP_STATUS4: UP check status4(Page0 - FBh)
typedef struct {
    uint8_t up_sta4; // Bits 7:0 — UP_STA4[7:0]
} Jd9853UpStatus4Param1;

// 2.6.1. POWER_OPT:  Power engineer test mode register (Page1 - B2h)
typedef struct {
    uint8_t pds_14_8   : 7; // Bits 6:0 — PDS[14:8]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853PowerOptParam1;
typedef struct {
    uint8_t pds_7_0; // Bits 7:0 — PDS[7:0]
} Jd9853PowerOptParam2;

// 2.6.2. POWER_TEST: Set Power control (Page1 - B4h)
typedef struct {
    uint8_t sft_cont_en : 1; // Bit 0 — SFT_CONT_EN
    uint8_t reserved_1  : 7; // Bits 7:1 — Reserved
} Jd9853PowerTestParam1;

// 2.6.3. OTP_ESD_SET: Auto reload OTP function(Page1 - B5h)
typedef struct {
    uint8_t esd_frm_cnt_en; // Bits 7:0 — ESD_FRM_CNT_EN[7:0]
} Jd9853OtpEsdSetParam1;
typedef struct {
    uint8_t esd_frm_cnt; // Bits 7:0 — ESD_FRM_CNT[7:0]
} Jd9853OtpEsdSetParam2;

// 2.6.4. DCDC_OPT: DCDC design setting (Page1 - B7h)
typedef struct {
    uint8_t dcdc_eq1_ext : 4; // Bits 3:0 — DCDC_EQ1_EXT[3:0]
    uint8_t reserved_1   : 2; // Bits 5:4 — Reserved
    uint8_t mvz_nops     : 1; // Bit 6 — MVZ_NOPS
    uint8_t reserved_2   : 1; // Bit 7 — Reserved
} Jd9853DcdcOptParam1;
typedef struct {
    uint8_t eq2_en     : 1; // Bit 0 — EQ2_EN
    uint8_t eq1_en     : 1; // Bit 1 — EQ1_EN
    uint8_t reserved_1 : 2; // Bits 3:2 — Reserved
    uint8_t hvz_disch  : 1; // Bit 4 — HVZ_DISCH
    uint8_t reserved_2 : 1; // Bit 5 — Reserved
    uint8_t hvz_nops   : 1; // Bit 6 — HVZ_NOPS
    uint8_t reserved_3 : 1; // Bit 7 — Reserved
} Jd9853DcdcOptParam2;
typedef struct {
    uint8_t dcdc_clk_s    : 5; // Bits 4:0 — DCDC_CLK_S[4:0]
    uint8_t vghz_clkgt_en : 1; // Bit 6 — VGHZ_CLKGT_EN
    uint8_t vglz_clkgt_en : 1; // Bit 7 — VGLZ_CLKGT_EN
    uint8_t mv_clkgt_en   : 1; // Bit 8 — MV_CLKGT_EN
} Jd9853DcdcOptParam3;
typedef struct {
    uint8_t dcdc_clk_e    : 7; // Bits 6:0 — DCDC_CLK_E[6:0]
    uint8_t mv_g_clkgt_en : 1; // Bit 7 — MV_G_CLKGT_EN
} Jd9853DcdcOptParam4;
typedef struct {
    uint8_t dcdc_eq1_ext : 7; // Bits 6:0 — DCDC_EQ1_EXT[6:0]
    uint8_t reserved_1   : 1; // Bit 7 — Reserved
} Jd9853DcdcOptParam5;

// 2.6.5. SET_GPO: General Purpose Output setting(Page1 - BBh)
typedef struct {
    uint8_t te_sel     : 4; // Bits 3:0 — TE_SEL[3:0]
    uint8_t reserved_1 : 2; // Bits 5:4 — Reserved
    uint8_t te_rev     : 1; // Bit 6 — TE_REV
    uint8_t reserved_2 : 1; // Bit 7 — Reserved
} Jd9853SetGpoParam1;
typedef struct {
    uint8_t gpo0_sel   : 4; // Bits 3:0 — GPO0_SEL[3:0]
    uint8_t reserved_1 : 1; // Bit 4 — Reserved
    uint8_t gpo0_oe    : 1; // Bit 5 — GPO0_OE
    uint8_t gpo0_rev   : 1; // Bit 6 — GPO0_REV
    uint8_t reserved_2 : 1; // Bit 7 — Reserved
} Jd9853SetGpoParam2;
typedef struct {
    uint8_t gpo1_sel   : 4; // Bits 3:0 — GPO1_SEL[3:0]
    uint8_t reserved_1 : 1; // Bit 4 — Reserved
    uint8_t gpo1_oe    : 1; // Bit 5 — GPO1_OE
    uint8_t gpo1_rev   : 1; // Bit 6 — GPO1_REV
    uint8_t reserved_2 : 1; // Bit 7 — Reserved
} Jd9853SetGpoParam3;
typedef struct {
    uint8_t internal_sel1 : 4; // Bits 3:0 — INTERNAL_SEL1[3:0]
    uint8_t internal_sel2 : 4; // Bits 7:4 — INTERNAL_SEL2[3:0]
} Jd9853SetGpoParam4;

// 2.6.6. PWR_SEQ_ACCESS: power sequence control (Page1 - BCh)
typedef struct {
    uint8_t pwr_soft      : 1; // Bit 0 — PWR_SOFT
    uint8_t reserved_1    : 3; // Bits 3:1 — Reserved
    uint8_t pwr_pow_en    : 1; // Bit 4 — PWR_POW_EN
    uint8_t pwr_pos_disch : 1; // Bit 5 — PWR_POS_DISCH
    uint8_t reserved_2    : 2; // Bits 7:6 — Reserved
} Jd9853PwrSeqAccessParam1;
typedef struct {
    uint8_t pwr_dc; // Bits 7:0 — PWR_DC[7:0]
} Jd9853PwrSeqAccessParam2;

// 2.6.7. PWR_SEQ_ACCESS_RD: power sequence control read (Page1 - BDh)
typedef struct {
    uint8_t up_soft      : 1; // Bit 0 — UP_SOFT
    uint8_t reserved_1   : 3; // Bits 3:1 — Reserved
    uint8_t up_pow_en    : 1; // Bit 4 — UP_POW_EN
    uint8_t up_pos_disch : 1; // Bit 5 — UP_POS_DISCH
    uint8_t reserved_2   : 2; // Bits 7:6 — Reserved
} Jd9853PwrSeqAccessRdParam1;
typedef struct {
    uint8_t up_dc; // Bits 7:0 — UP_DC[7:0]
} Jd9853PwrSeqAccessRdParam2;

// 2.6.8. GAMMA_POWER_TEST: Set GAMMA power related setting (Page1 - BEh)
typedef struct {
    uint8_t vgmn_test  : 1; // Bit 0 — VGMN_TEST
    uint8_t vgsn_test  : 1; // Bit 1 — VGSN_TEST
    uint8_t vgsp_test  : 1; // Bit 2 — VGSP_TEST
    uint8_t vgmp_test  : 1; // Bit 3 — VGMP_TEST
    uint8_t reserved_1 : 4; // Bits 7:4 — Reserved
} Jd9853GammaPowerTestParam1;

// 2.6.9. SETRGBCYC2: Set RGB IF source switch control timing (Page1 - C1h)
typedef struct {
    uint8_t sdporch    : 2; // Bits 1:0 — SDPORCH[1:0]
    uint8_t sdsw       : 2; // Bits 3:2 — SDSW[1:0]
    uint8_t sdsum      : 2; // Bits 5:4 — SDSUM[1:0]
    uint8_t sdprdum    : 1; // Bit 6 — SDPRDUM
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853SetRgbCyc2Param1;
typedef struct {
    uint8_t rgb_spaib          : 2; // Bits 1:0 — RGB_SPAIB[1:0]
    uint8_t rgb_spaif          : 2; // Bits 3:2 — RGB_SPAIF[1:0]
    uint8_t wb_pol_keep_v0_en  : 1; // Bit 4 — WB_POL_KEEP_V0_EN
    uint8_t wb_pol_keep_v63_en : 1; // Bit 5 — WB_POL_KEEP_V63_EN
    uint8_t wb_v0_pgnd_skip_en : 1; // Bit 6 — WB_V0_PGND_SKIP_EN
    uint8_t reserved_1         : 1; // Bit 7 — Reserved
} Jd9853SetRgbCyc2Param2;
typedef struct {
    uint8_t reserved_1; // Bits 7:0 — Reserved (Parameter 3)
} Jd9853SetRgbCyc2Param3;
typedef struct {
    uint8_t sdpartial      : 2; // Bits 1:0 — SDPARTIAL[1:0]
    uint8_t dum_gaopoff_on : 1; // Bit 2 — DUM_GAOPOFF_ON
    uint8_t dum_chgen_on   : 1; // Bit 3 — DUM_CHGEN_ON
    uint8_t sdsw_gas       : 2; // Bits 5:4 — SDSW_GAS[1:0]
    uint8_t wb_sd_v0_flag  : 1; // Bit 6 — WB_SD_V0_FLAG
    uint8_t wb_flag_g63    : 1; // Bit 7 — WB_FLAG_G63
} Jd9853SetRgbCyc2Param4;

// 2.6.10. SETSTBA2: Set internal source option control (Page1 - C2h)
typedef struct {
    uint8_t sdt        : 4; // Bits 3:0 — SDT[3:0]
    uint8_t reserved_1 : 4; // Bits 7:4 — Reserved
} Jd9853SetStba2Param1;
typedef struct {
    uint8_t sds_15_8; // Bits 7:0 — SDS[15:8]
} Jd9853SetStba2Param2;
typedef struct {
    uint8_t sds_7_0; // Bits 7:0 — SDS[7:0]
} Jd9853SetStba2Param3;

// 2.6.11. SETTCON_OPT: (Page1 - C3h)
typedef struct {
    uint8_t dislearnvbp     : 1; // Bit 0 — DISLEARNVBP
    uint8_t eqs_off_opt     : 1; // Bit 1 — EQS_OFF_OPT
    uint8_t te_gating_opt   : 1; // Bit 2 — TE_GATING_OPT
    uint8_t gas_pullgnd_opt : 1; // Bit 3 — GAS_PULLGND_OPT
    uint8_t reserved_1      : 4; // Bits 7:4 —
} Jd9853SetTconOptParam1;
typedef struct {
    uint8_t disp2to3_en    : 1; // Bit 0 — DISP2TO3_EN
    uint8_t disp2to3_opt   : 1; // Bit 1 — DISP2TO3_OPT
    uint8_t pwr_sd_off     : 1; // Bit 2 — PWR_SD_OFF
    uint8_t pwr_tc_clk_off : 1; // Bit 3 — PWR_TC_CLK_OFF
    uint8_t lclk_park_sta  : 1; // Bit 4 — LCLK_PARK_STA
    uint8_t reserved_1     : 3; // Bits 7:5 — Reserved
} Jd9853SetTconOptParam2;
typedef struct {
    uint8_t stpluse_s    : 2; // Bits 1:0 — STPLUSE_S[1:0]
    uint8_t reserved_1   : 2; // Bits 3:2 — Reserved
    uint8_t stpluse_e    : 3; // Bits 6:4 — STPLUSE_E[2:0]
    uint8_t l2en_str_opt : 1; // Bit 7 — L2EN_STR_OPT
} Jd9853SetTconOptParam3;
typedef struct {
    uint8_t l3en_s : 4; // Bits 3:0 — L3EN_S[3:0]
    uint8_t l3en_e : 4; // Bits 7:4 — L3EN_E[3:0]
} Jd9853SetTconOptParam4;
typedef struct {
    uint8_t l2en_offset; // Bits 7:0 — L2EN_OFFSET[7:0]
} Jd9853SetTconOptParam5;
typedef struct {
    uint8_t l2en0_s : 4; // Bits 3:0 — L2EN0_S[3:0]
    uint8_t l2en0_e : 4; // Bits 7:4 — L2EN0_E[3:0]
} Jd9853SetTconOptParam6;
typedef struct {
    uint8_t l2en1_s : 4; // Bits 3:0 — L2EN1_S[3:0]
    uint8_t l2en1_e : 4; // Bits 7:4 — L2EN1_E[3:0]
} Jd9853SetTconOptParam7;
typedef struct {
    uint8_t l2en2_s : 4; // Bits 3:0 — L2EN2_S[3:0]
    uint8_t l2en2_e : 4; // Bits 7:4 — L2EN2_E[3:0]
} Jd9853SetTconOptParam8;
typedef struct {
    uint8_t l2en3_s : 4; // Bits 3:0 — L2EN3_S[3:0]
    uint8_t l2en3_e : 4; // Bits 7:4 — L2EN3_E[3:0]
} Jd9853SetTconOptParam9;
typedef struct {
    uint8_t l2en_str; // Bits 7:0 — L2EN_STR[7:0]
} Jd9853SetTconOptParam10;

// 2.6.12. SET_GAMMA_OP: Set Gamma OP related setting (Page1 - C4h)
typedef struct {
    uint8_t r0p        : 2; // Bits 1:0 — R0P[1:0]
    uint8_t r1p        : 2; // Bits 3:2 — R1P[1:0]
    uint8_t gaopenp    : 1; // Bit 4 — GAOPENP
    uint8_t gaopenc    : 1; // Bit 5 — GAOPENC
    uint8_t gaopenn    : 1; // Bit 6 — GAOPENN
    uint8_t reserved_1 : 1; // Bit 7 — Reserved
} Jd9853SetGammaOpParam1;
typedef struct {
    uint8_t r0n        : 2; // Bits 1:0 — R0N[1:0]
    uint8_t r1n        : 2; // Bits 3:2 — R1N[1:0]
    uint8_t gaop_clamp : 1; // Bit 4 — GAOP_CLAMP
    uint8_t reserved_1 : 3; // Bits 7:5 — Reserved
} Jd9853SetGammaOpParam2;

// 2.6.13. SET_OSCM: Oscillator M setting (Page1 - C5h)
typedef struct {
    uint8_t oscm_adj   : 2; // Bits 1:0 — OSCM_ADJ[1:0]
    uint8_t reserved_1 : 2; // Bits 3:2 — Reserved
    uint8_t osc_div    : 2; // Bits 5:4 — OSC_DIV[1:0]
    uint8_t reserved_2 : 2; // Bits 7:6 — Reserved
} Jd9853SetOscmParam1;
typedef struct {
    uint8_t oscm_bias_en : 1; // Bit 0 — OSCM_BIAS_EN
    uint8_t oscm_en      : 1; // Bit 1 — OSCM_EN
    uint8_t reserved_1   : 6; // Bits 7:2 — Reserved
} Jd9853SetOscmParam2;
typedef struct {
    uint8_t osc_key; // Bits 7:0 — OSC_KEY[7:0]
} Jd9853SetOscmParam3;

// 2.6.14. SET_IO_PAD: Set IO Pad output driving ability (Page1 - C8h)
typedef struct {
    uint8_t io_holddly : 1; // Bit 0 — IO_HOLDDLY
    uint8_t reserved_1 : 7; // Bits 7:1 — Reserved
} Jd9853SetIoPadParam1;
typedef struct {
    uint8_t reserved_1 : 1; // Bit 0 — Reserved
    uint8_t im_en      : 3; // Bits 3:1 — IM_EN[2:0]
    uint8_t im_reg     : 4; // Bits 7:4 — IM_REG[3:0]
} Jd9853SetIoPadParam2;

// 2.6.15. SET_INH_CHKSUM: Set inhosue checksum(Page1 - D7h)
typedef struct {
    uint8_t inh_chksum_sel      : 3; // Bits 2:0 — INH_CHKSUM_SEL[2:0]
    uint8_t inh_chksum_off      : 1; // Bit 3 — INH_CHKSUM_OFF
    uint8_t slpin_reset_en      : 1; // Bit 4 — SLPIN_RESET_EN
    uint8_t inh_chksum_page_sel : 2; // Bits 6:5 — INH_CHKSUM_PAGE_SEL[1:0]
    uint8_t reserved_1          : 1; // Bit 7 — Reserved
} Jd9853SetInhChksumParam1;

// 2.6.16. HWPIN_OFF: TEST_EN Register setting(Page1 - E7h)
typedef struct {
    uint8_t test_en_reg : 4; // Bits 3:0 — TEST_EN_REG[3:0]
    uint8_t reserved_1  : 4; // Bits 7:4 — Reserved
} Jd9853HwpinOffParam1;

// 2.6.17. STD_SEL: Standard command option R0Ah/R0Fh (Page1 - E9h)
typedef struct {
    uint8_t reserved_1  : 2; // Bits 1:0 — Reserved
    uint8_t rx0a_b0_mix : 1; // Bit 2 — RX0A_B0_MIX
    uint8_t rx0a_b2_mix : 1; // Bit 3 — RX0A_B2_MIX
    uint8_t dispon_sel  : 1; // Bit 4 — DISPON_SEL
    uint8_t rx0f_b0_sel : 1; // Bit 5 — RX0F_B0_SEL
    uint8_t reserved_2  : 2; // Bits 7:6 — Reserved
} Jd9853StdSelParam1;

// 2.6.18. SET_ESD_DET: SET ESD detect function (Page1 - EBh)
typedef struct {
    uint8_t cmd_vs_tg_en : 1; // Bit 0 — CMD_VS_TG_EN
    uint8_t esd_det_en   : 1; // Bit 1 — ESD_DET_EN
    uint8_t reserved_1   : 2; // Bits 3:2 — Reserved
    uint8_t err_fg_sel   : 2; // Bits 5:4 — ERR_FG_SEL[1:0]
    uint8_t reserved_2   : 2; // Bits 7:6 — Reserved
} Jd9853SetEsdDetParam1;
typedef struct {
    uint8_t hs_width   : 6; // Bits 5:0 — HS_WIDTH[5:0]
    uint8_t reserved_1 : 2; // Bits 7:6 — Reserved
} Jd9853SetEsdDetParam2;

// 2.6.19. SET_WRX: SET WRX timing (Page1 - EEh)
typedef struct {
    uint8_t wrx_sel    : 1; // Bit 0 — WRX_SEL
    uint8_t reserved_1 : 7; // Bits 7:1 — Reserved
} Jd9853SetWrxParam1;

// 2.7.1. GRAM_OPT_SET: GRAM OPTION SELECTION (Page2 - B0h)
typedef struct {
    uint8_t reserved_1     : 3; // Bits 2:0 — Reserved
    uint8_t gram_data_ow   : 1; // Bit 3 — GRAM_DATA_OW[1:0]
    uint8_t gram_chksum_en : 1; // Bit 4 — GRAM_CHKSUM_EN
    uint8_t reserved_2     : 3; // Bits 7:5 — Reserved
} Jd9853GramOptSetParam1;

// 2.7.2. INTR_ACC_SET: (Page2 - B4h)
typedef struct {
    uint8_t intr_disp_val : 6; // Bits 5:0 — INTR_DISP_VAL[5:0]
    uint8_t reserved_1    : 2; // Bits 7:6 — Reserved
} Jd9853IntrAccSetParam1;

// 2.7.3. GRAM_TMG_SET ( Page2 - B5h)
typedef struct {
    uint8_t gram_swoff    : 2; // Bits 1:0 — GRAM_SWOFF[1:0]
    uint8_t gram_swon     : 2; // Bits 3:2 — GRAM_SWON[1:0]
    uint8_t gram_dmy0_4_2 : 3; // Bits 6:4 — GRAM_DMY0[4:2]
    uint8_t reserved_1    : 1; // Bit 7 — Reserved
} Jd9853GramTmgSetParam1;
typedef struct {
    uint8_t gram_rds      : 6; // Bits 5:0 — GRAM_RDS[5:0]
    uint8_t gram_dmy0_1_0 : 2; // Bits 7:6 — GRAM_DMY0[1:0]
} Jd9853GramTmgSetParam2;

// 2.7.4. GRAM_PWR_SET ( Page2 - B6h)
typedef struct {
    uint8_t gram_off   : 2; // Bits 1:0 — GRAM_OFF[1:0]
    uint8_t reserved_1 : 6; // Bits 7:2 — Reserved
} Jd9853GramPwrSetParam1;
typedef struct {
    uint8_t gram_pwr_l   : 2; // Bits 1:0 — GRAM_PWR_L[1:0]
    uint8_t gram_pwr_r   : 2; // Bits 3:2 — GRAM_PWR_R[1:0]
    uint8_t gram_rst_l   : 1; // Bit 4 — GRAM_RST_L
    uint8_t gram_rst_r   : 1; // Bit 5 — GRAM_RST_R
    uint8_t gram_pwr_opt : 1; // Bit 6 — GRAM_PWR_OPT
    uint8_t reserved_1   : 1; // Bit 7 — Reserved
} Jd9853GramPwrSetParam2;

// 2.7.5. GRAM_XREPAIR_L0 ( Page2 - B8h)
typedef struct {
    uint8_t gram_repair_row1_l_8 : 1; // Bit 0 — GRAM_REPAIR_ROW1_L[8]
    uint8_t gram_repair_row0_l_8 : 1; // Bit 1 — GRAM_REPAIR_ROW0_L[8]
    uint8_t gram_repair_en1_l    : 1; // Bit 2 — GRAM_REPAIR_EN1_L
    uint8_t gram_repair_en0_l    : 1; // Bit 3 — GRAM_REPAIR_EN0_L
    uint8_t reserved_1           : 4; // Bits 7:4 — Reserved
} Jd9853GramXrepairL0Param1;
typedef struct {
    uint8_t gram_repair_row0_l_7_0; // Bits 7:0 — GRAM_REPAIR_ROW0_L[7:0]
} Jd9853GramXrepairL0Param2;
typedef struct {
    uint8_t gram_repair_row1_l_7_0; // Bits 7:0 — GRAM_REPAIR_ROW1_L[7:0]
} Jd9853GramXrepairL0Param3;

// 2.7.6. GRAM_XREPAIR_L0_1(Page2 - B9h)
typedef struct {
    uint8_t gram_repair_row1_r_8 : 1; // Bit 0 — GRAM_REPAIR_ROW1_R[8]
    uint8_t gram_repair_row0_r_8 : 1; // Bit 1 — GRAM_REPAIR_ROW0_R[8]
    uint8_t gram_repair_en1_r    : 1; // Bit 2 — GRAM_REPAIR_EN1_R
    uint8_t gram_repair_en0_r    : 1; // Bit 3 — GRAM_REPAIR_EN0_R
    uint8_t reserved_1           : 4; // Bits 7:4 — Reserved
} Jd9853GramXrepairL01Param1;
typedef struct {
    uint8_t gram_repair_row0_r_7_0; // Bits 7:0 — GRAM_REPAIR_ROW0_R[7:0]
} Jd9853GramXrepairL01Param2;
typedef struct {
    uint8_t gram_repair_row1_r_7_0; // Bits 7:0 — GRAM_REPAIR_ROW1_R[7:0]
} Jd9853GramXrepairL01Param3;

// 2.7.7. SET_DBGSEL(Page2 - BAh)
typedef struct {
    uint8_t dbg_sel; // Bits 7:0 — DBG_SEL[7:0]
} Jd9853SetDbgselParam1;

typedef struct {
    uint8_t dbg_en     : 1; // Bit 0 — DBG_EN
    uint8_t dbg_low_en : 1; // Bit 1 — DBG_LOW_EN
    uint8_t reserved_1 : 5; // Bits 6:2 — Reserved
    uint8_t dbg_sel_8  : 1; // Bit 7 — DBG_SEL[8]
} Jd9853SetDbgselParam2;

// 2.7.8. SETROM (Page2 - BBh)
typedef struct {
    uint8_t ods        : 4; // Bits 3:0 — ODS[3:0]
    uint8_t reserved_1 : 4; // Bits 7:4 — Reserved
} Jd9853SetRomParam1;

// 2.7.9. TEST_CTRL: Source option for CP test (Pag2 - BCh)
typedef struct {
    uint8_t s_cp_pol_gating : 5; // Bits 4:0 — S_CP_POL_GATING[4:0]
    uint8_t reserved_1      : 2; // Bits 6:5 — Reserved
    uint8_t osc_test_en     : 1; // Bit 7 — OSC_TEST_EN
} Jd9853TestCtrlParam1;

// 2.7.10. INH_CHKSUM_LOCK: Inhouse checksum lock setting (Pag2 - BDh)
typedef struct {
    uint8_t no_chksum_en : 2; // Bits 1:0 — NO_CHKSUM_EN[1:0]
    uint8_t reserved_1   : 6; // Bits 7:2 — Reserved
} Jd9853InhChksumLockParam1;
typedef struct {
    uint8_t inh_chksum_lock_en; // Bits 7:0 — INH_CHKSUM_LOCK_EN[7:0]
} Jd9853InhChksumLockParam2;

// 2.7.11. INH_CHKSUM_DBG: Inhouse checksum debug (Page2 - RBEh)
typedef struct {
    uint8_t test_mode_en    : 1; // Bit 0 — TEST_MODE_EN
    uint8_t inh_cmpr_rst    : 1; // Bit 1 — INH_CMPR_RST
    uint8_t up_test_mode_en : 1; // Bit 2 — UP_TEST_MODE_EN
    uint8_t reserved_1      : 5; // Bits 7:3 —
} Jd9853InhChksumDbgParam1;
typedef struct {
    uint8_t inh_fcs; // Bits 7:0 — INH_FCS[7:0]
} Jd9853InhChksumDbgParam2;
typedef struct {
    uint8_t inh_ccs; // Bits 7:0 — INH_CCS[7:0]
} Jd9853InhChksumDbgParam3;
typedef struct {
    uint8_t lock_fcs; // Bits 7:0 — LOCK_FCS[7:0]
} Jd9853InhChksumDbgParam4;
typedef struct {
    uint8_t inh_cs_cmpr : 1; // Bit 0 — INH_CS_CMPR
    uint8_t reserved_1  : 7; // Bits 7:1 — Reserved
} Jd9853InhChksumDbgParam5;
typedef struct {
    uint8_t pn_cs; // Bits 7:0 — PN_CS[7:0]
} Jd9853InhChksumDbgParam6;

// 2.7.12. TEST_PAD_SEL (Page2 - BFh)
typedef struct {
    uint8_t test_gamma_sel : 6; // Bits 5:0 — TEST_GAMMA_SEL[5:0]
    uint8_t reserved_1     : 2; // Bits 7:6 — Reserved
} Jd9853TestPadSelParam1;
typedef struct {
    uint8_t testpow_s  : 3; // Bits 2:0 — TESTPOW_S[2:0]
    uint8_t reserved_1 : 5; // Bits 7:3 — Reserved
} Jd9853TestPadSelParam2;

// 2.7.13. SET_TC_CHKSUM (Page2 - C0h)
typedef struct {
    uint8_t tc_crc_en  : 1; // Bit 0 — TC_CRC_EN
    uint8_t crc_opt    : 1; // Bits 1 — CRC_OPT
    uint8_t reserved_1 : 6; // Bits 7:2 — Reserved
} Jd9853SetTcChksumParam1;
typedef struct {
    uint8_t tc_crc_out; // Bits 7:0 — TC_CRC_OUT[7:0]
} Jd9853SetTcChksumParam2;

// 2.7.14. SET_TC_OTHER (Page2 - C1h) 
typedef struct {
    uint8_t tc_backup; // Bits 7:0 — TC_BACKUP[7:0]
} Jd9853SetTcOtherParam1;

// 2.7.15. GRAM_BIST_SET (Page2 - E5h) 
typedef struct {
    uint8_t gram_bist_en        : 1; // Bit 0 — GRAM_BIST_EN
    uint8_t gram_mbist_scan_dir : 1; // Bit 1 — GRAM_MBIST_SCAN_DIR
    uint8_t reserved_1          : 6; // Bits 7:2 — Reserved
} Jd9853GramBistSetParam1;
typedef struct {
    uint8_t gram_mbist_algo_sel : 3; // Bits 2:0 — GRAM_MBIST_ALGO_SEL[2:0]
    uint8_t reserved_1          : 5; // Bits 7:3 — Reserved
} Jd9853GramBistSetParam2;
typedef struct {
    uint8_t gram_mbist_ud_ptn_17_16 : 2; // Bits 1:0 — GRAM_MBIST_UD_PTN[17:16]
    uint8_t gram_mbist_ud_ptn_toggle : 2; // Bits 3:2 — GRAM_MBIST_UD_PTN_TOGGLE[1:0]
    uint8_t reserved_1               : 4; // Bits 7:4 — Reserved
} Jd9853GramBistSetParam3;
typedef struct {
    uint8_t gram_mbist_ud_ptn_15_8; // Bits 7:0 — GRAM_MBIST_UD_PTN[15:8]
} Jd9853GramBistSetParam4;
typedef struct {
    uint8_t gram_mbist_ud_ptn_7_0; // Bits 7:0 — GRAM_MBIST_UD_PTN[7:0]
} Jd9853GramBistSetParam5;

// 2.7.16. GRAM_BIST_SET ( Page2 - E6h) 
typedef struct {
    uint8_t mbist_fail_num_l     : 2; // Bits 1:0 — MBIST_FAIL_NUM_L[1:0]
    uint8_t mbist_fail_num_r     : 2; // Bits 3:2 — MBIST_FAIL_NUM_R[1:0]
    uint8_t gram_mbist_repair    : 1; // Bit 4 — GRAM_MBIST_REPAIR
    uint8_t gram_mbist_fail      : 1; // Bit 5 — GRAM_MBIST_FAIL
    uint8_t gram_mbist_done      : 1; // Bit 6 — GRAM_MBIST_DONE
    uint8_t reserved_1           : 1; // Bit 7 — Reserved
} Jd9853GramBistSet1Param1;
typedef struct {
    uint8_t mbist_fail_addr_l_0_8 : 1; // Bit 0 — MBIST_FAIL_ADDR_L_0[8]
    uint8_t mbist_fail_addr_l_1_8 : 1; // Bit 1 — MBIST_FAIL_ADDR_L_1[8]
    uint8_t mbist_fail_addr_r_0_8 : 1; // Bit 2 — MBIST_FAIL_ADDR_R_0[8]
    uint8_t mbist_fail_addr_r_1_8 : 1; // Bit 3 — MBIST_FAIL_ADDR_R_1[8]
    uint8_t reserved_1             : 4; // Bits 7:4 — Reserved
} Jd9853GramBistSet1Param2;
typedef struct {
    uint8_t mbist_fail_addr_l_0_7_0; // Bits 7:0 — MBIST_FAIL_ADDR_L_0[7:0]
} Jd9853GramBistSet1Param3;
typedef struct {
    uint8_t mbist_fail_addr_l_1_7_0; // Bits 7:0 — MBIST_FAIL_ADDR_L_1[7:0]
} Jd9853GramBistSet1Param4;
typedef struct {
    uint8_t mbist_fail_addr_r_0_7_0; // Bits 7:0 — MBIST_FAIL_ADDR_R_0[7:0]
} Jd9853GramBistSet1Param5;
typedef struct {
    uint8_t mbist_fail_addr_r_1_7_0; // Bits 7:0 — MBIST_FAIL_ADDR_R_1[7:0]
} Jd9853GramBistSet1Param6;

// 2.7.17. GRAM_CHKSUM_GET ( Page2 - E7h) 
typedef struct {
    uint8_t gram_wr_chksum ; // GRAM_WR_CHKSUM[5:0]
} Jd9853GramChksumGetParam1;
typedef struct {
    uint8_t gram_rd_chksum ; // GRAM_RD_CHKSUM[5:0]
} Jd9853GramChksumGetParam2;

// 2.8.1. TRIMMING2: Trimming for VREF (Page3 - B3h) 
typedef struct {
    uint8_t vref_t     : 5; // Bits 4:0 — VREF_T[4:0]
    uint8_t reserved_1 : 3; // Bits 7:5 — Reserved
} Jd9853Trimming2Param1;

// 2.8.2. TRIMMING3: Trimming for Band Gap (Page3 - B4h) 
typedef struct {
    uint8_t bg1_t      : 3; // Bits 2:0 — BG1_T[2:0]
    uint8_t reserved_2 : 5; // Bits 7:3 — Reserved
} Jd9853Trimming3Param1;

// 2.8.3. TRIMMING4: Trimming for VDDD (Page4 - B5h) 
typedef struct {
    uint8_t vddd_t1    : 2; // Bits 1:0 — VDDD_T1[1:0]
    uint8_t reserved_1 : 3; // Bits 4:2 — Reserved
    uint8_t vddd_t0    : 3; // Bits 7:5 — VDDD_T0[2:0]
} Jd9853Trimming4Param1;

// 2.8.4. TRIMMING6: Trimming for Gamma reference voltage (Page3 - B7h) 
typedef struct {
    uint8_t vgmp_t     : 4; // Bits 3:0 — VGMP_T[3:0]
    uint8_t vgsp_t     : 3; // Bits 6:4 — VGSP_T[2:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved 
} Jd9853Trimming6Param1;
typedef struct {
    uint8_t vgmn_t     : 4; // Bits 3:0 — VGMN_T[3:0]
    uint8_t vgsn_t     : 3; // Bits 6:4 — VGSN_T[2:0]
    uint8_t reserved_1 : 1; // Bit 7 — Reserved 
} Jd9853Trimming6Param2;

// 2.8.5. TRIMMING9: Trimming for VGH/VGL/AVDD/AVEE volatge (Page3 - BAh)
typedef struct {
    uint8_t vglz_t     : 2; // Bits 1:0 — VGLZ_T[1:0]
    uint8_t reserved_1 : 2; // Bits 3:2 — Reserved
    uint8_t vghz_t     : 2; // Bits 5:4 — VGHZ_T[1:0]
    uint8_t reserved_2 : 2; // Bits 7:6 — Reserved
} Jd9853Trimming9Param1;
typedef struct {
    uint8_t aveez_t    : 3; // Bits 2:0 — AVEEZ_T[2:0]
    uint8_t reserved_1 : 1; // Bit 3 — Reserved
    uint8_t avddz_t    : 3; // Bits 6:4 — AVDDZ_T[2:0]
    uint8_t reserved_2 : 1; // Bit 7 — Reserved
} Jd9853Trimming9Param2;

// 2.8.6. TRIM_GAS: Trimming for GAS (Page3 - BEh) 
typedef struct {
    uint8_t gastrim_io  : 3; // Bits 2:0 — GASTRIM_IO[2:0]
    uint8_t reserved_1  : 1; // Bit 3 — Reserved
    uint8_t gastrim_vci : 3; // Bits 6:4 — GASTRIM_VCI[2:0]
    uint8_t reserved_2  : 1; // Bit 7 — Reserved
} Jd9853TrimGasParam1;

// 2.8.7. TRIMMING_OSCM1: Trimming temperature characteristics of Oscillator M(Page4 - C7h) 
typedef struct {
    uint8_t reserved_1 : 4; // Bits 3:0 — Reserved
    uint8_t oscm_temp  : 3; // Bits 6:4 — OSCM_TEMP[2:0]
    uint8_t reserved_2 : 1; // Bit 7 — Reserved
} Jd9853TrimmingOscm1Param1;

// 2.8.8. TRIMMING_OSCM2: Trimming frequency of Oscillator M (Page4 - C8h) 
typedef struct {
    uint8_t oscm_trim  : 5; // Bits 4:0 — OSCM_TRIM[4:0]
    uint8_t reserved_1 : 3; // Bits 7:5 — Reserved
} Jd9853TrimmingOscm2Param1;

