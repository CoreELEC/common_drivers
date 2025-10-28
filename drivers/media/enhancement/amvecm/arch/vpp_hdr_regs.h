/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef VPP_HDR_REGS_H
#define VPP_HDR_REGS_H

#define XVYCC_LUT_R_ADDR_PORT	0x315e
#define XVYCC_LUT_R_DATA_PORT	0x315f
#define XVYCC_LUT_G_ADDR_PORT	0x3160
#define XVYCC_LUT_G_DATA_PORT	0x3161
#define XVYCC_LUT_B_ADDR_PORT	0x3162
#define XVYCC_LUT_B_DATA_PORT	0x3163
#define XVYCC_INV_LUT_CTL		0x3164
#define XVYCC_LUT_CTL			0x3165

#define XVYCC_INV_LUT_Y_ADDR_PORT     0x3158
#define XVYCC_INV_LUT_Y_DATA_PORT     0x3159

extern struct am_regs_s r_lut_hdr_hdr;
extern struct am_regs_s r_lut_sdr_sdr;
extern struct am_regs_s r_lut_hdr_sdr_level1;
extern struct am_regs_s r_lut_hdr_sdr_level2;
extern struct am_regs_s r_lut_hdr_sdr_level3;

#define MUXIO_IPATH_SEL                0x4512
#define MUXIO_OPATH_SEL                0x4513
#define AXIRD_PATH_CTRL                0x6d02
#define VPU_AXIRD_DOLBY_PATH_CTRL      0x6d30

#define VD1_HDR2_CTRL                      0x3800
#define VD1_HDR2_CLK_GATE                  0x3801
#define VD1_HDR2_MATRIXI_COEF00_01         0x3802
#define VD1_HDR2_MATRIXI_COEF02_10         0x3803
#define VD1_HDR2_MATRIXI_COEF11_12         0x3804
#define VD1_HDR2_MATRIXI_COEF20_21         0x3805
#define VD1_HDR2_MATRIXI_COEF22            0x3806
#define VD1_HDR2_MATRIXI_COEF30_31         0x3807
#define VD1_HDR2_MATRIXI_COEF32_40         0x3808
#define VD1_HDR2_MATRIXI_COEF41_42         0x3809
#define VD1_HDR2_MATRIXI_OFFSET0_1         0x380a
#define VD1_HDR2_MATRIXI_OFFSET2           0x380b
#define VD1_HDR2_MATRIXI_PRE_OFFSET0_1     0x380c
#define VD1_HDR2_MATRIXI_PRE_OFFSET2       0x380d
#define VD1_HDR2_MATRIXO_COEF00_01         0x380e
#define VD1_HDR2_MATRIXO_COEF02_10         0x380f
#define VD1_HDR2_MATRIXO_COEF11_12         0x3810
#define VD1_HDR2_MATRIXO_COEF20_21         0x3811
#define VD1_HDR2_MATRIXO_COEF22            0x3812
#define VD1_HDR2_MATRIXO_COEF30_31         0x3813
#define VD1_HDR2_MATRIXO_COEF32_40         0x3814
#define VD1_HDR2_MATRIXO_COEF41_42         0x3815
#define VD1_HDR2_MATRIXO_OFFSET0_1         0x3816
#define VD1_HDR2_MATRIXO_OFFSET2           0x3817
#define VD1_HDR2_MATRIXO_PRE_OFFSET0_1     0x3818
#define VD1_HDR2_MATRIXO_PRE_OFFSET2       0x3819
#define VD1_HDR2_MATRIXI_CLIP              0x381a
#define VD1_HDR2_MATRIXO_CLIP              0x381b
#define VD1_HDR2_CGAIN_OFFT                0x381c
/*TL1 hist read is 0x3840, tm2 is 0x381d*/
#define VD1_HDR2_HIST_RD_2                 0x381d
#define VD1_EOTF_LUT_ADDR_PORT             0x381e
#define VD1_EOTF_LUT_DATA_PORT             0x381f
#define VD1_OETF_LUT_ADDR_PORT             0x3820
#define VD1_OETF_LUT_DATA_PORT             0x3821
#define VD1_CGAIN_LUT_ADDR_PORT            0x3822
#define VD1_CGAIN_LUT_DATA_PORT            0x3823
#define VD1_HDR2_CGAIN_COEF0               0x3824
#define VD1_HDR2_CGAIN_COEF1               0x3825
#define VD1_OGAIN_LUT_ADDR_PORT            0x3826
#define VD1_OGAIN_LUT_DATA_PORT            0x3827
#define VD1_HDR2_ADPS_CTRL                 0x3828
#define VD1_HDR2_ADPS_ALPHA0               0x3829
#define VD1_HDR2_ADPS_ALPHA1               0x382a
#define VD1_HDR2_ADPS_BETA0                0x382b
#define VD1_HDR2_ADPS_BETA1                0x382c
#define VD1_HDR2_ADPS_BETA2                0x382d
#define VD1_HDR2_ADPS_COEF0                0x382e
#define VD1_HDR2_ADPS_COEF1                0x382f
#define VD1_HDR2_GMUT_CTRL                 0x3830
#define VD1_HDR2_GMUT_COEF0                0x3831
#define VD1_HDR2_GMUT_COEF1                0x3832
#define VD1_HDR2_GMUT_COEF2                0x3833
#define VD1_HDR2_GMUT_COEF3                0x3834
#define VD1_HDR2_GMUT_COEF4                0x3835
#define VD1_HDR2_PIPE_CTRL1                0x3836
#define VD1_HDR2_PIPE_CTRL2                0x3837
#define VD1_HDR2_PIPE_CTRL3                0x3838
#define VD1_HDR2_PROC_WIN1                 0x3839
#define VD1_HDR2_PROC_WIN2                 0x383a
#define VD1_HDR2_MATRIXI_EN_CTRL           0x383b
#define VD1_HDR2_MATRIXO_EN_CTRL           0x383c
#define VD1_HDR2_HIST_CTRL                 0x383d
#define VD1_HDR2_HIST_H_START_END          0x383e
#define VD1_HDR2_HIST_V_START_END          0x383f
#define VD1_OGAIN_LUT1_ADDR_PORT           0x3840
#define VD1_OGAIN_LUT1_DATA_PORT           0x3841
#define VD1_HDR2_CGAIN_VIVID               0x3842
#define VD1_HDR2_GMUT_COMP0                0x3843
#define VD1_HDR2_GMUT_COMP1                0x3844
#define VD1_HDR2_GMUT_COMP2                0x3845
#define VD1_HDR2_GMUT_COMP3                0x3846
#define VD1_HDR2_GMUT_COMP4                0x3847
#define VD1_HDR2_GMUT_COMP5                0x3848
#define VD1_HDR2_GMUT_COMP6                0x3849
#define VD1_HDR2_GMUT_COMP7                0x384a
#define VD1_HDR2_GMUT_COMP8                0x384b

//T6W add
#define VD1_HDR2_HIST_SLC_X_ST_ED_0        0x384c
#define VD1_HDR2_HIST_SLC_X_ST_ED_1        0x384d
#define VD1_HDR2_HIST_SLC_X_ST_ED_2        0x384e
#define VD1_HDR2_HIST_SLC_X_ST_ED_3        0x384f
//T6X add
#define VD1_HDR2_ADPSCL1_COEF0             0x3850
#define VD1_HDR2_ADPSCL1_COEF1             0x3851
#define VD1_HDR2_RGB_GM_CTRL               0x3852

#define VD2_HDR2_CTRL                      0x3850
#define VD2_HDR2_CLK_GATE                  0x3851
#define VD2_HDR2_MATRIXI_COEF00_01         0x3852
#define VD2_HDR2_MATRIXI_COEF02_10         0x3853
#define VD2_HDR2_MATRIXI_COEF11_12         0x3854
#define VD2_HDR2_MATRIXI_COEF20_21         0x3855
#define VD2_HDR2_MATRIXI_COEF22            0x3856
#define VD2_HDR2_MATRIXI_COEF30_31         0x3857
#define VD2_HDR2_MATRIXI_COEF32_40         0x3858
#define VD2_HDR2_MATRIXI_COEF41_42         0x3859
#define VD2_HDR2_MATRIXI_OFFSET0_1         0x385a
#define VD2_HDR2_MATRIXI_OFFSET2           0x385b
#define VD2_HDR2_MATRIXI_PRE_OFFSET0_1     0x385c
#define VD2_HDR2_MATRIXI_PRE_OFFSET2       0x385d
#define VD2_HDR2_MATRIXO_COEF00_01         0x385e
#define VD2_HDR2_MATRIXO_COEF02_10         0x385f
#define VD2_HDR2_MATRIXO_COEF11_12         0x3860
#define VD2_HDR2_MATRIXO_COEF20_21         0x3861
#define VD2_HDR2_MATRIXO_COEF22            0x3862
#define VD2_HDR2_MATRIXO_COEF30_31         0x3863
#define VD2_HDR2_MATRIXO_COEF32_40         0x3864
#define VD2_HDR2_MATRIXO_COEF41_42         0x3865
#define VD2_HDR2_MATRIXO_OFFSET0_1         0x3866
#define VD2_HDR2_MATRIXO_OFFSET2           0x3867
#define VD2_HDR2_MATRIXO_PRE_OFFSET0_1     0x3868
#define VD2_HDR2_MATRIXO_PRE_OFFSET2       0x3869
#define VD2_HDR2_MATRIXI_CLIP              0x386a
#define VD2_HDR2_MATRIXO_CLIP              0x386b
#define VD2_HDR2_CGAIN_OFFT                0x386c
/*TL1 hist read is 0x3890, tm2 is 0x386d*/
#define VD2_HDR2_HIST_RD_2                 0x386d
#define VD2_EOTF_LUT_ADDR_PORT             0x386e
#define VD2_EOTF_LUT_DATA_PORT             0x386f
#define VD2_OETF_LUT_ADDR_PORT             0x3870
#define VD2_OETF_LUT_DATA_PORT             0x3871
#define VD2_CGAIN_LUT_ADDR_PORT            0x3872
#define VD2_CGAIN_LUT_DATA_PORT            0x3873
#define VD2_HDR2_CGAIN_COEF0               0x3874
#define VD2_HDR2_CGAIN_COEF1               0x3875
#define VD2_OGAIN_LUT_ADDR_PORT            0x3876
#define VD2_OGAIN_LUT_DATA_PORT            0x3877
#define VD2_HDR2_ADPS_CTRL                 0x3878
#define VD2_HDR2_ADPS_ALPHA0               0x3879
#define VD2_HDR2_ADPS_ALPHA1               0x387a
#define VD2_HDR2_ADPS_BETA0                0x387b
#define VD2_HDR2_ADPS_BETA1                0x387c
#define VD2_HDR2_ADPS_BETA2                0x387d
#define VD2_HDR2_ADPS_COEF0                0x387e
#define VD2_HDR2_ADPS_COEF1                0x387f
#define VD2_HDR2_GMUT_CTRL                 0x3880
#define VD2_HDR2_GMUT_COEF0                0x3881
#define VD2_HDR2_GMUT_COEF1                0x3882
#define VD2_HDR2_GMUT_COEF2                0x3883
#define VD2_HDR2_GMUT_COEF3                0x3884
#define VD2_HDR2_GMUT_COEF4                0x3885
#define VD2_HDR2_PIPE_CTRL1                0x3886
#define VD2_HDR2_PIPE_CTRL2                0x3887
#define VD2_HDR2_PIPE_CTRL3                0x3888
#define VD2_HDR2_PROC_WIN1                 0x3889
#define VD2_HDR2_PROC_WIN2                 0x388a
#define VD2_HDR2_MATRIXI_EN_CTRL           0x388b
#define VD2_HDR2_MATRIXO_EN_CTRL           0x388c
#define VD2_HDR2_HIST_CTRL                 0x388d
#define VD2_HDR2_HIST_H_START_END          0x388e
#define VD2_HDR2_HIST_V_START_END          0x388f
#define VD2_OGAIN_LUT1_ADDR_PORT           0x3890
#define VD2_OGAIN_LUT1_DATA_PORT           0x3891
#define VD2_HDR2_CGAIN_VIVID               0x3892
#define VD2_HDR2_GMUT_COMP0                0x3893
#define VD2_HDR2_GMUT_COMP1                0x3894
#define VD2_HDR2_GMUT_COMP2                0x3895
#define VD2_HDR2_GMUT_COMP3                0x3896
#define VD2_HDR2_GMUT_COMP4                0x3897
#define VD2_HDR2_GMUT_COMP5                0x3898
#define VD2_HDR2_GMUT_COMP6                0x3899
#define VD2_HDR2_GMUT_COMP7                0x389a
#define VD2_HDR2_GMUT_COMP8                0x389b

//T6W add
#define VD2_HDR2_HIST_SLC_X_ST_ED_0        0x389c
#define VD2_HDR2_HIST_SLC_X_ST_ED_1        0x389d
#define VD2_HDR2_HIST_SLC_X_ST_ED_2        0x389e
#define VD2_HDR2_HIST_SLC_X_ST_ED_3        0x389f
//T6X add
#define VD2_HDR2_ADPSCL1_COEF0             0x38a0
#define VD2_HDR2_ADPSCL1_COEF1             0x38a1
#define VD2_HDR2_RGB_GM_CTRL               0x38a2

// vd3 to do, use correct register address
#define VD3_HDR2_CTRL                      0x5930
#define VD3_HDR2_CLK_GATE                  0x5931
#define VD3_HDR2_MATRIXI_COEF00_01         0x5932
#define VD3_HDR2_MATRIXI_COEF02_10         0x5933
#define VD3_HDR2_MATRIXI_COEF11_12         0x5934
#define VD3_HDR2_MATRIXI_COEF20_21         0x5935
#define VD3_HDR2_MATRIXI_COEF22            0x5936
#define VD3_HDR2_MATRIXI_COEF30_31         0x5937
#define VD3_HDR2_MATRIXI_COEF32_40         0x5938
#define VD3_HDR2_MATRIXI_COEF41_42         0x5939
#define VD3_HDR2_MATRIXI_OFFSET0_1         0x593a
#define VD3_HDR2_MATRIXI_OFFSET2           0x593b
#define VD3_HDR2_MATRIXI_PRE_OFFSET0_1     0x593c
#define VD3_HDR2_MATRIXI_PRE_OFFSET2       0x593d
#define VD3_HDR2_MATRIXO_COEF00_01         0x593e
#define VD3_HDR2_MATRIXO_COEF02_10         0x593f
#define VD3_HDR2_MATRIXO_COEF11_12         0x5940
#define VD3_HDR2_MATRIXO_COEF20_21         0x5941
#define VD3_HDR2_MATRIXO_COEF22            0x5942
#define VD3_HDR2_MATRIXO_COEF30_31         0x5943
#define VD3_HDR2_MATRIXO_COEF32_40         0x5944
#define VD3_HDR2_MATRIXO_COEF41_42         0x5945
#define VD3_HDR2_MATRIXO_OFFSET0_1         0x5946
#define VD3_HDR2_MATRIXO_OFFSET2           0x5947
#define VD3_HDR2_MATRIXO_PRE_OFFSET0_1     0x5948
#define VD3_HDR2_MATRIXO_PRE_OFFSET2       0x5949
#define VD3_HDR2_MATRIXI_CLIP              0x594a
#define VD3_HDR2_MATRIXO_CLIP              0x594b
#define VD3_HDR2_CGAIN_OFFT                0x594c
#define VD3_HDR2_HIST_RD_2                 0x594d
#define VD3_EOTF_LUT_ADDR_PORT             0x594e
#define VD3_EOTF_LUT_DATA_PORT             0x594f
#define VD3_OETF_LUT_ADDR_PORT             0x5950
#define VD3_OETF_LUT_DATA_PORT             0x5951
#define VD3_CGAIN_LUT_ADDR_PORT            0x5952
#define VD3_CGAIN_LUT_DATA_PORT            0x5953
#define VD3_HDR2_CGAIN_COEF0               0x5954
#define VD3_HDR2_CGAIN_COEF1               0x5955
#define VD3_OGAIN_LUT_ADDR_PORT            0x5956
#define VD3_OGAIN_LUT_DATA_PORT            0x5957
#define VD3_HDR2_ADPS_CTRL                 0x5958
#define VD3_HDR2_ADPS_ALPHA0               0x5959
#define VD3_HDR2_ADPS_ALPHA1               0x595a
#define VD3_HDR2_ADPS_BETA0                0x595b
#define VD3_HDR2_ADPS_BETA1                0x595c
#define VD3_HDR2_ADPS_BETA2                0x595d
#define VD3_HDR2_ADPS_COEF0                0x595e
#define VD3_HDR2_ADPS_COEF1                0x595f
#define VD3_HDR2_GMUT_CTRL                 0x5960
#define VD3_HDR2_GMUT_COEF0                0x5961
#define VD3_HDR2_GMUT_COEF1                0x5962
#define VD3_HDR2_GMUT_COEF2                0x5963
#define VD3_HDR2_GMUT_COEF3                0x5964
#define VD3_HDR2_GMUT_COEF4                0x5965
#define VD3_HDR2_PIPE_CTRL1                0x5966
#define VD3_HDR2_PIPE_CTRL2                0x5967
#define VD3_HDR2_PIPE_CTRL3                0x5968
#define VD3_HDR2_PROC_WIN1                 0x5969
#define VD3_HDR2_PROC_WIN2                 0x596a
#define VD3_HDR2_MATRIXI_EN_CTRL           0x596b
#define VD3_HDR2_MATRIXO_EN_CTRL           0x596c
#define VD3_HDR2_HIST_CTRL                 0x596d
#define VD3_HDR2_HIST_H_START_END          0x596e
#define VD3_HDR2_HIST_V_START_END          0x596f
#define VD3_OGAIN_LUT1_ADDR_PORT           0x5970
#define VD3_OGAIN_LUT1_DATA_PORT           0x5971

// end of vd3 part for t7
// osd1 hdr
#define OSD1_HDR2_CTRL                      0x38a0
#define OSD1_HDR2_CLK_GATE                  0x38a1
#define OSD1_HDR2_MATRIXI_COEF00_01         0x38a2
#define OSD1_HDR2_MATRIXI_COEF02_10         0x38a3
#define OSD1_HDR2_MATRIXI_COEF11_12         0x38a4
#define OSD1_HDR2_MATRIXI_COEF20_21         0x38a5
#define OSD1_HDR2_MATRIXI_COEF22            0x38a6
#define OSD1_HDR2_MATRIXI_COEF30_31         0x38a7
#define OSD1_HDR2_MATRIXI_COEF32_40         0x38a8
#define OSD1_HDR2_MATRIXI_COEF41_42         0x38a9
#define OSD1_HDR2_MATRIXI_OFFSET0_1         0x38aa
#define OSD1_HDR2_MATRIXI_OFFSET2           0x38ab
#define OSD1_HDR2_MATRIXI_PRE_OFFSET0_1     0x38ac
#define OSD1_HDR2_MATRIXI_PRE_OFFSET2       0x38ad
#define OSD1_HDR2_MATRIXO_COEF00_01         0x38ae
#define OSD1_HDR2_MATRIXO_COEF02_10         0x38af
#define OSD1_HDR2_MATRIXO_COEF11_12         0x38b0
#define OSD1_HDR2_MATRIXO_COEF20_21         0x38b1
#define OSD1_HDR2_MATRIXO_COEF22            0x38b2
#define OSD1_HDR2_MATRIXO_COEF30_31         0x38b3
#define OSD1_HDR2_MATRIXO_COEF32_40         0x38b4
#define OSD1_HDR2_MATRIXO_COEF41_42         0x38b5
#define OSD1_HDR2_MATRIXO_OFFSET0_1         0x38b6
#define OSD1_HDR2_MATRIXO_OFFSET2           0x38b7
#define OSD1_HDR2_MATRIXO_PRE_OFFSET0_1     0x38b8
#define OSD1_HDR2_MATRIXO_PRE_OFFSET2       0x38b9
#define OSD1_HDR2_MATRIXI_CLIP              0x38ba
#define OSD1_HDR2_MATRIXO_CLIP              0x38bb
#define OSD1_HDR2_CGAIN_OFFT                0x38bc
#define OSD1_EOTF_LUT_ADDR_PORT             0x38be
#define OSD1_EOTF_LUT_DATA_PORT             0x38bf
#define OSD1_OETF_LUT_ADDR_PORT             0x38c0
#define OSD1_OETF_LUT_DATA_PORT             0x38c1
#define OSD1_CGAIN_LUT_ADDR_PORT            0x38c2
#define OSD1_CGAIN_LUT_DATA_PORT            0x38c3
#define OSD1_HDR2_CGAIN_COEF0               0x38c4
#define OSD1_HDR2_CGAIN_COEF1               0x38c5
#define OSD1_OGAIN_LUT_ADDR_PORT            0x38c6
#define OSD1_OGAIN_LUT_DATA_PORT            0x38c7
#define OSD1_HDR2_ADPS_CTRL                 0x38c8
#define OSD1_HDR2_ADPS_ALPHA0               0x38c9
#define OSD1_HDR2_ADPS_ALPHA1               0x38ca
#define OSD1_HDR2_ADPS_BETA0                0x38cb
#define OSD1_HDR2_ADPS_BETA1                0x38cc
#define OSD1_HDR2_ADPS_BETA2                0x38cd
#define OSD1_HDR2_ADPS_COEF0                0x38ce
#define OSD1_HDR2_ADPS_COEF1                0x38cf
#define OSD1_HDR2_GMUT_CTRL                 0x38d0
#define OSD1_HDR2_GMUT_COEF0                0x38d1
#define OSD1_HDR2_GMUT_COEF1                0x38d2
#define OSD1_HDR2_GMUT_COEF2                0x38d3
#define OSD1_HDR2_GMUT_COEF3                0x38d4
#define OSD1_HDR2_GMUT_COEF4                0x38d5
#define OSD1_HDR2_PIPE_CTRL1                0x38d6
#define OSD1_HDR2_PIPE_CTRL2                0x38d7
#define OSD1_HDR2_PIPE_CTRL3                0x38d8
#define OSD1_HDR2_PROC_WIN1                 0x38d9
#define OSD1_HDR2_PROC_WIN2                 0x38da
#define OSD1_HDR2_MATRIXI_EN_CTRL           0x38db
#define OSD1_HDR2_MATRIXO_EN_CTRL           0x38dc
#define OSD1_HDR2_HIST_CTRL                 0x38dd
#define OSD1_HDR2_HIST_H_START_END          0x38de
#define OSD1_HDR2_HIST_V_START_END          0x38df

//osd1 pq
#define OSD1_PQ_MATRIX_COEF00_01            0x38f0
#define OSD1_PQ_MATRIX_COEF02_10            0x38f1
#define OSD1_PQ_MATRIX_COEF11_12            0x38f2
#define OSD1_PQ_MATRIX_COEF20_21            0x38f3
#define OSD1_PQ_MATRIX_COEF22               0x38f4
#define OSD1_PQ_MATRIX_COEF13_14            0x38f5
#define OSD1_PQ_MATRIX_COEF23_24            0x38f6
#define OSD1_PQ_MATRIX_COEF15_25            0x38f7
#define OSD1_PQ_MATRIX_CLIP                 0x38f8
#define OSD1_PQ_MATRIX_OFFSET0_1            0x38f9
#define OSD1_PQ_MATRIX_OFFSET2              0x38fa
#define OSD1_PQ_MATRIX_PRE_OFFSET0_1        0x38fb
#define OSD1_PQ_MATRIX_EN_CTRL              0x38fc
#define OSD1_PQ_MATRIX_PRE_OFFSET2          0x38fd

/* osd2 hdr  */
/* t7 do not have osd2 hdr, t3 only have MATRIXI. */
#define OSD2_HDR2_CTRL                             0x5b00
#define OSD2_HDR2_CLK_GATE                         0x5b01
#define OSD2_HDR2_MATRIXI_COEF00_01                0x5b02
#define OSD2_HDR2_MATRIXI_COEF02_10                0x5b03
#define OSD2_HDR2_MATRIXI_COEF11_12                0x5b04
#define OSD2_HDR2_MATRIXI_COEF20_21                0x5b05
#define OSD2_HDR2_MATRIXI_COEF22                   0x5b06
#define OSD2_HDR2_MATRIXI_COEF30_31                0x5b07
#define OSD2_HDR2_MATRIXI_COEF32_40                0x5b08
#define OSD2_HDR2_MATRIXI_COEF41_42                0x5b09
#define OSD2_HDR2_MATRIXI_OFFSET0_1                0x5b0a
#define OSD2_HDR2_MATRIXI_OFFSET2                  0x5b0b
#define OSD2_HDR2_MATRIXI_PRE_OFFSET0_1            0x5b0c
#define OSD2_HDR2_MATRIXI_PRE_OFFSET2              0x5b0d
#define OSD2_HDR2_MATRIXO_COEF00_01                0x5b0e
#define OSD2_HDR2_MATRIXO_COEF02_10                0x5b0f
#define OSD2_HDR2_MATRIXO_COEF11_12                0x5b10
#define OSD2_HDR2_MATRIXO_COEF20_21                0x5b11
#define OSD2_HDR2_MATRIXO_COEF22                   0x5b12
#define OSD2_HDR2_MATRIXO_COEF30_31                0x5b13
#define OSD2_HDR2_MATRIXO_COEF32_40                0x5b14
#define OSD2_HDR2_MATRIXO_COEF41_42                0x5b15
#define OSD2_HDR2_MATRIXO_OFFSET0_1                0x5b16
#define OSD2_HDR2_MATRIXO_OFFSET2                  0x5b17
#define OSD2_HDR2_MATRIXO_PRE_OFFSET0_1            0x5b18
#define OSD2_HDR2_MATRIXO_PRE_OFFSET2              0x5b19
#define OSD2_HDR2_MATRIXI_CLIP                     0x5b1a
#define OSD2_HDR2_MATRIXO_CLIP                     0x5b1b
#define OSD2_HDR2_CGAIN_OFFT                       0x5b1c
#define OSD2_HDR2_HIST_RD                          0x5b1d
#define OSD2_EOTF_LUT_ADDR_PORT                    0x5b1e
#define OSD2_EOTF_LUT_DATA_PORT                    0x5b1f
#define OSD2_OETF_LUT_ADDR_PORT                    0x5b20
#define OSD2_OETF_LUT_DATA_PORT                    0x5b21
#define OSD2_CGAIN_LUT_ADDR_PORT                   0x5b22
#define OSD2_CGAIN_LUT_DATA_PORT                   0x5b23
#define OSD2_HDR2_CGAIN_COEF0                      0x5b24
#define OSD2_HDR2_CGAIN_COEF1                      0x5b25
#define OSD2_OGAIN_LUT_ADDR_PORT                   0x5b26
#define OSD2_OGAIN_LUT_DATA_PORT                   0x5b27
#define OSD2_HDR2_ADPS_CTRL                        0x5b28
#define OSD2_HDR2_ADPS_ALPHA0                      0x5b29
#define OSD2_HDR2_ADPS_ALPHA1                      0x5b2a
#define OSD2_HDR2_ADPS_BETA0                       0x5b2b
#define OSD2_HDR2_ADPS_BETA1                       0x5b2c
#define OSD2_HDR2_ADPS_BETA2                       0x5b2d
#define OSD2_HDR2_ADPS_COEF0                       0x5b2e
#define OSD2_HDR2_ADPS_COEF1                       0x5b2f
#define OSD2_HDR2_GMUT_CTRL                        0x5b30
#define OSD2_HDR2_GMUT_COEF0                       0x5b31
#define OSD2_HDR2_GMUT_COEF1                       0x5b32
#define OSD2_HDR2_GMUT_COEF2                       0x5b33
#define OSD2_HDR2_GMUT_COEF3                       0x5b34
#define OSD2_HDR2_GMUT_COEF4                       0x5b35
#define OSD2_HDR2_PIPE_CTRL1                       0x5b36
#define OSD2_HDR2_PIPE_CTRL2                       0x5b37
#define OSD2_HDR2_PIPE_CTRL3                       0x5b38
#define OSD2_HDR2_PROC_WIN1                        0x5b39
#define OSD2_HDR2_PROC_WIN2                        0x5b3a
#define OSD2_HDR2_MATRIXI_EN_CTRL                  0x5b3b
#define OSD2_HDR2_MATRIXO_EN_CTRL                  0x5b3c
#define OSD2_HDR2_HIST_CTRL                        0x5b3d
#define OSD2_HDR2_HIST_H_START_END                 0x5b3e
#define OSD2_HDR2_HIST_V_START_END                 0x5b3f

/* osd3 hdr */
/* t7 have osd3 hdr, t3 only have MATRIXI. */
#define OSD3_HDR2_CTRL                             0x5b50
#define OSD3_HDR2_CLK_GATE                         0x5b51
#define OSD3_HDR2_MATRIXI_COEF00_01                0x5b52
#define OSD3_HDR2_MATRIXI_COEF02_10                0x5b53
#define OSD3_HDR2_MATRIXI_COEF11_12                0x5b54
#define OSD3_HDR2_MATRIXI_COEF20_21                0x5b55
#define OSD3_HDR2_MATRIXI_COEF22                   0x5b56
#define OSD3_HDR2_MATRIXI_COEF30_31                0x5b57
#define OSD3_HDR2_MATRIXI_COEF32_40                0x5b58
#define OSD3_HDR2_MATRIXI_COEF41_42                0x5b59
#define OSD3_HDR2_MATRIXI_OFFSET0_1                0x5b5a
#define OSD3_HDR2_MATRIXI_OFFSET2                  0x5b5b
#define OSD3_HDR2_MATRIXI_PRE_OFFSET0_1            0x5b5c
#define OSD3_HDR2_MATRIXI_PRE_OFFSET2              0x5b5d
#define OSD3_HDR2_MATRIXO_COEF00_01                0x5b5e
#define OSD3_HDR2_MATRIXO_COEF02_10                0x5b5f
#define OSD3_HDR2_MATRIXO_COEF11_12                0x5b60
#define OSD3_HDR2_MATRIXO_COEF20_21                0x5b61
#define OSD3_HDR2_MATRIXO_COEF22                   0x5b62
#define OSD3_HDR2_MATRIXO_COEF30_31                0x5b63
#define OSD3_HDR2_MATRIXO_COEF32_40                0x5b64
#define OSD3_HDR2_MATRIXO_COEF41_42                0x5b65
#define OSD3_HDR2_MATRIXO_OFFSET0_1                0x5b66
#define OSD3_HDR2_MATRIXO_OFFSET2                  0x5b67
#define OSD3_HDR2_MATRIXO_PRE_OFFSET0_1            0x5b68
#define OSD3_HDR2_MATRIXO_PRE_OFFSET2              0x5b69
#define OSD3_HDR2_MATRIXI_CLIP                     0x5b6a
#define OSD3_HDR2_MATRIXO_CLIP                     0x5b6b
#define OSD3_HDR2_CGAIN_OFFT                       0x5b6c
#define OSD3_HDR2_HIST_RD                          0x5b6d
#define OSD3_EOTF_LUT_ADDR_PORT                    0x5b6e
#define OSD3_EOTF_LUT_DATA_PORT                    0x5b6f
#define OSD3_OETF_LUT_ADDR_PORT                    0x5b70
#define OSD3_OETF_LUT_DATA_PORT                    0x5b71
#define OSD3_CGAIN_LUT_ADDR_PORT                   0x5b72
#define OSD3_CGAIN_LUT_DATA_PORT                   0x5b73
#define OSD3_HDR2_CGAIN_COEF0                      0x5b74
#define OSD3_HDR2_CGAIN_COEF1                      0x5b75
#define OSD3_OGAIN_LUT_ADDR_PORT                   0x5b76
#define OSD3_OGAIN_LUT_DATA_PORT                   0x5b77
#define OSD3_HDR2_ADPS_CTRL                        0x5b78
#define OSD3_HDR2_ADPS_ALPHA0                      0x5b79
#define OSD3_HDR2_ADPS_ALPHA1                      0x5b7a
#define OSD3_HDR2_ADPS_BETA0                       0x5b7b
#define OSD3_HDR2_ADPS_BETA1                       0x5b7c
#define OSD3_HDR2_ADPS_BETA2                       0x5b7d
#define OSD3_HDR2_ADPS_COEF0                       0x5b7e
#define OSD3_HDR2_ADPS_COEF1                       0x5b7f
#define OSD3_HDR2_GMUT_CTRL                        0x5b80
#define OSD3_HDR2_GMUT_COEF0                       0x5b81
#define OSD3_HDR2_GMUT_COEF1                       0x5b82
#define OSD3_HDR2_GMUT_COEF2                       0x5b83
#define OSD3_HDR2_GMUT_COEF3                       0x5b84
#define OSD3_HDR2_GMUT_COEF4                       0x5b85
#define OSD3_HDR2_PIPE_CTRL1                       0x5b86
#define OSD3_HDR2_PIPE_CTRL2                       0x5b87
#define OSD3_HDR2_PIPE_CTRL3                       0x5b88
#define OSD3_HDR2_PROC_WIN1                        0x5b89
#define OSD3_HDR2_PROC_WIN2                        0x5b8a
#define OSD3_HDR2_MATRIXI_EN_CTRL                  0x5b8b
#define OSD3_HDR2_MATRIXO_EN_CTRL                  0x5b8c
#define OSD3_HDR2_HIST_CTRL                        0x5b8d
#define OSD3_HDR2_HIST_H_START_END                 0x5b8e
#define OSD3_HDR2_HIST_V_START_END                 0x5b8f

#define DI_HDR2_CTRL                      0x3770
#define DI_HDR2_CLK_GATE                  0x3771
#define DI_HDR2_MATRIXI_COEF00_01         0x3772
#define DI_HDR2_MATRIXI_COEF02_10         0x3773
#define DI_HDR2_MATRIXI_COEF11_12         0x3774
#define DI_HDR2_MATRIXI_COEF20_21         0x3775
#define DI_HDR2_MATRIXI_COEF22            0x3776
#define DI_HDR2_MATRIXI_COEF30_31         0x3777
#define DI_HDR2_MATRIXI_COEF32_40         0x3778
#define DI_HDR2_MATRIXI_COEF41_42         0x3779
#define DI_HDR2_MATRIXI_OFFSET0_1         0x377a
#define DI_HDR2_MATRIXI_OFFSET2           0x377b
#define DI_HDR2_MATRIXI_PRE_OFFSET0_1     0x377c
#define DI_HDR2_MATRIXI_PRE_OFFSET2       0x377d
#define DI_HDR2_MATRIXO_COEF00_01         0x377e
#define DI_HDR2_MATRIXO_COEF02_10         0x377f
#define DI_HDR2_MATRIXO_COEF11_12         0x3780
#define DI_HDR2_MATRIXO_COEF20_21         0x3781
#define DI_HDR2_MATRIXO_COEF22            0x3782
#define DI_HDR2_MATRIXO_COEF30_31         0x3783
#define DI_HDR2_MATRIXO_COEF32_40         0x3784
#define DI_HDR2_MATRIXO_COEF41_42         0x3785
#define DI_HDR2_MATRIXO_OFFSET0_1         0x3786
#define DI_HDR2_MATRIXO_OFFSET2           0x3787
#define DI_HDR2_MATRIXO_PRE_OFFSET0_1     0x3788
#define DI_HDR2_MATRIXO_PRE_OFFSET2       0x3789
#define DI_HDR2_MATRIXI_CLIP              0x378a
#define DI_HDR2_MATRIXO_CLIP              0x378b
#define DI_HDR2_CGAIN_OFFT                0x378c
#define DI_EOTF_LUT_ADDR_PORT             0x378e
#define DI_EOTF_LUT_DATA_PORT             0x378f
#define DI_OETF_LUT_ADDR_PORT             0x3790
#define DI_OETF_LUT_DATA_PORT             0x3791
#define DI_CGAIN_LUT_ADDR_PORT            0x3792
#define DI_CGAIN_LUT_DATA_PORT            0x3793
#define DI_HDR2_CGAIN_COEF0               0x3794
#define DI_HDR2_CGAIN_COEF1               0x3795
#define DI_OGAIN_LUT_ADDR_PORT            0x3796
#define DI_OGAIN_LUT_DATA_PORT            0x3797
#define DI_HDR2_ADPS_CTRL                 0x3798
#define DI_HDR2_ADPS_ALPHA0               0x3799
#define DI_HDR2_ADPS_ALPHA1               0x379a
#define DI_HDR2_ADPS_BETA0                0x379b
#define DI_HDR2_ADPS_BETA1                0x379c
#define DI_HDR2_ADPS_BETA2                0x379d
#define DI_HDR2_ADPS_COEF0                0x379e
#define DI_HDR2_ADPS_COEF1                0x379f
#define DI_HDR2_GMUT_CTRL                 0x37a0
#define DI_HDR2_GMUT_COEF0                0x37a1
#define DI_HDR2_GMUT_COEF1                0x37a2
#define DI_HDR2_GMUT_COEF2                0x37a3
#define DI_HDR2_GMUT_COEF3                0x37a4
#define DI_HDR2_GMUT_COEF4                0x37a5
#define DI_HDR2_PIPE_CTRL1                0x37a6
#define DI_HDR2_PIPE_CTRL2                0x37a7
#define DI_HDR2_PIPE_CTRL3                0x37a8
#define DI_HDR2_PROC_WIN1                 0x37a9
#define DI_HDR2_PROC_WIN2                 0x37aa
#define DI_HDR2_MATRIXI_EN_CTRL           0x37ab
#define DI_HDR2_MATRIXO_EN_CTRL           0x37ac
#define DI_HDR2_HIST_CTRL                 0x37ad
#define DI_HDR2_HIST_H_START_END          0x37ae
#define DI_HDR2_HIST_V_START_END          0x37af

#define VDIN0_HDR2_CTRL                    0x1280
#define VDIN0_HDR2_CLK_GATE                0x1281
#define VDIN0_HDR2_MATRIXI_COEF00_01       0x1282
#define VDIN0_HDR2_MATRIXI_COEF02_10       0x1283
#define VDIN0_HDR2_MATRIXI_COEF11_12       0x1284
#define VDIN0_HDR2_MATRIXI_COEF20_21       0x1285
#define VDIN0_HDR2_MATRIXI_COEF22          0x1286
#define VDIN0_HDR2_MATRIXI_COEF30_31       0x1287
#define VDIN0_HDR2_MATRIXI_COEF32_40       0x1288
#define VDIN0_HDR2_MATRIXI_COEF41_42       0x1289
#define VDIN0_HDR2_MATRIXI_OFFSET0_1       0x128a
#define VDIN0_HDR2_MATRIXI_OFFSET2         0x128b
#define VDIN0_HDR2_MATRIXI_PRE_OFFSET0_1   0x128c
#define VDIN0_HDR2_MATRIXI_PRE_OFFSET2     0x128d
#define VDIN0_HDR2_MATRIXO_COEF00_01       0x128e
#define VDIN0_HDR2_MATRIXO_COEF02_10       0x128f
#define VDIN0_HDR2_MATRIXO_COEF11_12       0x1290
#define VDIN0_HDR2_MATRIXO_COEF20_21       0x1291
#define VDIN0_HDR2_MATRIXO_COEF22          0x1292
#define VDIN0_HDR2_MATRIXO_COEF30_31       0x1293
#define VDIN0_HDR2_MATRIXO_COEF32_40       0x1294
#define VDIN0_HDR2_MATRIXO_COEF41_42       0x1295
#define VDIN0_HDR2_MATRIXO_OFFSET0_1       0x1296
#define VDIN0_HDR2_MATRIXO_OFFSET2         0x1297
#define VDIN0_HDR2_MATRIXO_PRE_OFFSET0_1   0x1298
#define VDIN0_HDR2_MATRIXO_PRE_OFFSET2     0x1299
#define VDIN0_HDR2_MATRIXI_CLIP            0x129a
#define VDIN0_HDR2_MATRIXO_CLIP            0x129b
#define VDIN0_HDR2_CGAIN_OFFT              0x129c
#define VDIN0_EOTF_LUT_ADDR_PORT           0x129e
#define VDIN0_EOTF_LUT_DATA_PORT           0x129f
#define VDIN0_OETF_LUT_ADDR_PORT           0x12a0
#define VDIN0_OETF_LUT_DATA_PORT           0x12a1
#define VDIN0_CGAIN_LUT_ADDR_PORT          0x12a2
#define VDIN0_CGAIN_LUT_DATA_PORT          0x12a3
#define VDIN0_HDR2_CGAIN_COEF0             0x12a4
#define VDIN0_HDR2_CGAIN_COEF1             0x12a5
#define VDIN0_OGAIN_LUT_ADDR_PORT          0x12a6
#define VDIN0_OGAIN_LUT_DATA_PORT          0x12a7
#define VDIN0_HDR2_ADPS_CTRL               0x12a8
#define VDIN0_HDR2_ADPS_ALPHA0             0x12a9
#define VDIN0_HDR2_ADPS_ALPHA1             0x12aa
#define VDIN0_HDR2_ADPS_BETA0              0x12ab
#define VDIN0_HDR2_ADPS_BETA1              0x12ac
#define VDIN0_HDR2_ADPS_BETA2              0x12ad
#define VDIN0_HDR2_ADPS_COEF0              0x12ae
#define VDIN0_HDR2_ADPS_COEF1              0x12af
#define VDIN0_HDR2_GMUT_CTRL               0x12b0
#define VDIN0_HDR2_GMUT_COEF0              0x12b1
#define VDIN0_HDR2_GMUT_COEF1              0x12b2
#define VDIN0_HDR2_GMUT_COEF2              0x12b3
#define VDIN0_HDR2_GMUT_COEF3              0x12b4
#define VDIN0_HDR2_GMUT_COEF4              0x12b5
#define VDIN0_HDR2_PIPE_CTRL1              0x12b6
#define VDIN0_HDR2_PIPE_CTRL2              0x12b7
#define VDIN0_HDR2_PIPE_CTRL3              0x12b8
#define VDIN0_HDR2_PROC_WIN1               0x12b9
#define VDIN0_HDR2_PROC_WIN2               0x12ba
#define VDIN0_HDR2_MATRIXI_EN_CTRL         0x12bb
#define VDIN0_HDR2_MATRIXO_EN_CTRL         0x12bc

#define VDIN1_HDR2_CTRL                    0x1380
#define VDIN1_HDR2_CLK_GATE                0x1381
#define VDIN1_HDR2_MATRIXI_COEF00_01       0x1382
#define VDIN1_HDR2_MATRIXI_COEF02_10       0x1383
#define VDIN1_HDR2_MATRIXI_COEF11_12       0x1384
#define VDIN1_HDR2_MATRIXI_COEF20_21       0x1385
#define VDIN1_HDR2_MATRIXI_COEF22          0x1386
#define VDIN1_HDR2_MATRIXI_COEF30_31       0x1387
#define VDIN1_HDR2_MATRIXI_COEF32_40       0x1388
#define VDIN1_HDR2_MATRIXI_COEF41_42       0x1389
#define VDIN1_HDR2_MATRIXI_OFFSET0_1       0x138a
#define VDIN1_HDR2_MATRIXI_OFFSET2         0x138b
#define VDIN1_HDR2_MATRIXI_PRE_OFFSET0_1   0x138c
#define VDIN1_HDR2_MATRIXI_PRE_OFFSET2     0x138d
#define VDIN1_HDR2_MATRIXO_COEF00_01       0x138e
#define VDIN1_HDR2_MATRIXO_COEF02_10       0x138f
#define VDIN1_HDR2_MATRIXO_COEF11_12       0x1390
#define VDIN1_HDR2_MATRIXO_COEF20_21       0x1391
#define VDIN1_HDR2_MATRIXO_COEF22          0x1392
#define VDIN1_HDR2_MATRIXO_COEF30_31       0x1393
#define VDIN1_HDR2_MATRIXO_COEF32_40       0x1394
#define VDIN1_HDR2_MATRIXO_COEF41_42       0x1395
#define VDIN1_HDR2_MATRIXO_OFFSET0_1       0x1396
#define VDIN1_HDR2_MATRIXO_OFFSET2         0x1397
#define VDIN1_HDR2_MATRIXO_PRE_OFFSET0_1   0x1398
#define VDIN1_HDR2_MATRIXO_PRE_OFFSET2     0x1399
#define VDIN1_HDR2_MATRIXI_CLIP            0x139a
#define VDIN1_HDR2_MATRIXO_CLIP            0x139b
#define VDIN1_HDR2_CGAIN_OFFT              0x139c
#define VDIN1_EOTF_LUT_ADDR_PORT           0x139e
#define VDIN1_EOTF_LUT_DATA_PORT           0x139f
#define VDIN1_OETF_LUT_ADDR_PORT           0x13a0
#define VDIN1_OETF_LUT_DATA_PORT           0x13a1
#define VDIN1_CGAIN_LUT_ADDR_PORT          0x13a2
#define VDIN1_CGAIN_LUT_DATA_PORT          0x13a3
#define VDIN1_HDR2_CGAIN_COEF0             0x13a4
#define VDIN1_HDR2_CGAIN_COEF1             0x13a5
#define VDIN1_OGAIN_LUT_ADDR_PORT          0x13a6
#define VDIN1_OGAIN_LUT_DATA_PORT          0x13a7
#define VDIN1_HDR2_ADPS_CTRL               0x13a8
#define VDIN1_HDR2_ADPS_ALPHA0             0x13a9
#define VDIN1_HDR2_ADPS_ALPHA1             0x13aa
#define VDIN1_HDR2_ADPS_BETA0              0x13ab
#define VDIN1_HDR2_ADPS_BETA1              0x13ac
#define VDIN1_HDR2_ADPS_BETA2              0x13ad
#define VDIN1_HDR2_ADPS_COEF0              0x13ae
#define VDIN1_HDR2_ADPS_COEF1              0x13af
#define VDIN1_HDR2_GMUT_CTRL               0x13b0
#define VDIN1_HDR2_GMUT_COEF0              0x13b1
#define VDIN1_HDR2_GMUT_COEF1              0x13b2
#define VDIN1_HDR2_GMUT_COEF2              0x13b3
#define VDIN1_HDR2_GMUT_COEF3              0x13b4
#define VDIN1_HDR2_GMUT_COEF4              0x13b5
#define VDIN1_HDR2_PIPE_CTRL1              0x13b6
#define VDIN1_HDR2_PIPE_CTRL2              0x13b7
#define VDIN1_HDR2_PIPE_CTRL3              0x13b8
#define VDIN1_HDR2_PROC_WIN1               0x13b9
#define VDIN1_HDR2_PROC_WIN2               0x13ba
#define VDIN1_HDR2_MATRIXI_EN_CTRL         0x13bb
#define VDIN1_HDR2_MATRIXO_EN_CTRL         0x13bc

#define VPP_VD2_HDR_IN_SIZE                0x1df0
#define VPP_VD3_HDR_IN_SIZE                0x1a59

/* VPP WRAP OSD1 matrix */
#define VPP_WRAP_OSD1_MATRIX_COEF00_01             0x3d60
#define VPP_WRAP_OSD1_MATRIX_COEF02_10             0x3d61
#define VPP_WRAP_OSD1_MATRIX_COEF11_12             0x3d62
#define VPP_WRAP_OSD1_MATRIX_COEF20_21             0x3d63
#define VPP_WRAP_OSD1_MATRIX_COEF22                0x3d64
#define VPP_WRAP_OSD1_MATRIX_COEF13_14             0x3d65
#define VPP_WRAP_OSD1_MATRIX_COEF23_24             0x3d66
#define VPP_WRAP_OSD1_MATRIX_COEF15_25             0x3d67
#define VPP_WRAP_OSD1_MATRIX_CLIP                  0x3d68
#define VPP_WRAP_OSD1_MATRIX_OFFSET0_1             0x3d69
#define VPP_WRAP_OSD1_MATRIX_OFFSET2               0x3d6a
#define VPP_WRAP_OSD1_MATRIX_PRE_OFFSET0_1         0x3d6b
#define VPP_WRAP_OSD1_MATRIX_PRE_OFFSET2           0x3d6c
#define VPP_WRAP_OSD1_MATRIX_EN_CTRL               0x3d6d

#define VPP_OSD2_MATRIX_COEF00_01                  0x3920
#define VPP_OSD2_MATRIX_COEF02_10                  0x3921
#define VPP_OSD2_MATRIX_COEF11_12                  0x3922
#define VPP_OSD2_MATRIX_COEF20_21                  0x3923
#define VPP_OSD2_MATRIX_COEF22                     0x3924
#define VPP_OSD2_MATRIX_COEF13_14                  0x3925
#define VPP_OSD2_MATRIX_COEF23_24                  0x3926
#define VPP_OSD2_MATRIX_COEF15_25                  0x3927
#define VPP_OSD2_MATRIX_CLIP                       0x3928
#define VPP_OSD2_MATRIX_OFFSET0_1                  0x3929
#define VPP_OSD2_MATRIX_OFFSET2                    0x392a
#define VPP_OSD2_MATRIX_PRE_OFFSET0_1              0x392b
#define VPP_OSD2_MATRIX_PRE_OFFSET2                0x392c
#define VPP_OSD2_MATRIX_EN_CTRL                    0x392d

/*t6x hdr*/
#define VPU_HDR2_SIZE_IN                           0x4515
#define VPU_HDR2_FRM2_SIZE                         0x4518
//Bit 31:29         reserved
//Bit 28:16         reg_hdr2_vsize1_in
//Bit 15:13         reserved
//Bit 12:0          reg_hdr2_hsize1_in
#define VPU_HDR2_TOP_CTRL                          0x4519
//Bit 31            pls_hdr2_frm_start
//Bit 30:24         reserved
//Bit 23            reg_hdr2_sel_en
//Bit 22:21         reg_hdr2_gclk_ctrl
//Bit 20            reg_hdr2_dma_mode
//Bit 19:17         reserved
//Bit 16:4          reg_hdr2_hold_line_num
//Bit 3:2           reserved
//Bit 1             reg_hdr2_go_line_sel
//Bit 0             reg_hdr2_frm_start_sel
#define VD1_HDR_IN_SIZE                            0x1a57
#define VD2_HDR_IN_SIZE                            0x1a58
#define VD3_HDR_IN_SIZE                            0x1a59

#define VD1_1_HDR_IN_SIZE                          0x1aa5
#define VD2_1_HDR_IN_SIZE                          0x1aa6
//Bit   31:29     reserved
//Bit   28:16     vd2_1_hdr_in_vsize     unsigned,default = 1920
//Bit   15:13     reserved
//Bit   12:0      vd2_1_hdr_in_hsize     unsigned,default = 1080
#define PLAYER_2X2_CTRL                            0x1aa7
//Bit 31:0      player_2x2_ctrl          unsigned,default = 0

#define DOLBY_HDR2_CTRL                            0x4c00
#define DOLBY_HDR2_CLK_GATE                        0x4c01
#define DOLBY_HDR2_MATRIXI_COEF00_01               0x4c02
#define DOLBY_HDR2_MATRIXI_COEF02_10               0x4c03
#define DOLBY_HDR2_MATRIXI_COEF11_12               0x4c04
#define DOLBY_HDR2_MATRIXI_COEF20_21               0x4c05
#define DOLBY_HDR2_MATRIXI_COEF22                  0x4c06
#define DOLBY_HDR2_MATRIXI_COEF30_31               0x4c07
#define DOLBY_HDR2_MATRIXI_COEF32_40               0x4c08
#define DOLBY_HDR2_MATRIXI_COEF41_42               0x4c09
#define DOLBY_HDR2_MATRIXI_OFFSET0_1               0x4c0a
#define DOLBY_HDR2_MATRIXI_OFFSET2               0x4c0b
#define DOLBY_HDR2_MATRIXI_PRE_OFFSET0_1           0x4c0c
//Bit 31:27        reserved
//Bit 26:16        reg_mtrxi_offst_inp0     // signed ,    RW, default = 0
//Bit 15:11        reserved
//Bit 10: 0        reg_mtrxi_offst_inp1     // signed ,    RW, default = -512
#define DOLBY_HDR2_MATRIXI_PRE_OFFSET2             0x4c0d
//Bit 31:11        reserved
//Bit 10: 0        reg_mtrxi_offst_inp2     // signed ,    RW, default = -512
#define DOLBY_HDR2_MATRIXO_COEF00_01               0x4c0e
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxo_coef00        // signed ,    RW, default = 218
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxo_coef01        // signed ,    RW, default = 732
#define DOLBY_HDR2_MATRIXO_COEF02_10               0x4c0f
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxo_coef02        // signed ,    RW, default = 74
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxo_coef10        // signed ,    RW, default = -117
#define DOLBY_HDR2_MATRIXO_COEF11_12               0x4c10
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxo_coef11        // signed ,    RW, default = -395
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxo_coef12        // signed ,    RW, default = 512
#define DOLBY_HDR2_MATRIXO_COEF20_21               0x4c11
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxo_coef20        // signed ,    RW, default = 512
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxo_coef21        // signed ,    RW, default = -465
#define DOLBY_HDR2_MATRIXO_COEF22                  0x4c12
//Bit 31:13        reserved
//Bit 12: 0        reg_mtrxo_coef22        // signed ,    RW, default = -47
#define DOLBY_HDR2_MATRIXO_COEF30_31               0x4c13
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxo_coef30        // signed ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxo_coef31        // signed ,    RW, default = 0
#define DOLBY_HDR2_MATRIXO_COEF32_40               0x4c14
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxo_coef32        // signed ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxo_coef40        // signed ,    RW, default = 0
#define DOLBY_HDR2_MATRIXO_COEF41_42               0x4c15
//Bit 31:29        reserved
//Bit 28:16        reg_mtrxo_coef41        // signed ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12: 0        reg_mtrxo_coef42        // signed ,    RW, default = 0
#define DOLBY_HDR2_MATRIXO_OFFSET0_1               0x4c16
#define DOLBY_HDR2_MATRIXO_OFFSET2                 0x4c17
#define DOLBY_HDR2_MATRIXO_PRE_OFFSET0_1           0x4c18
#define DOLBY_HDR2_MATRIXO_PRE_OFFSET2             0x4c19
#define DOLBY_HDR2_MATRIXI_CLIP                    0x4c1a
#define DOLBY_HDR2_MATRIXO_CLIP                    0x4c1b
#define DOLBY_HDR2_CGAIN_OFFT                      0x4c1c
#define DOLBY_HDR2_HIST_RD                         0x4c1d
#define DOLBY_HDR2_EOTF_LUT_ADDR_PORT              0x4c1e
#define DOLBY_HDR2_EOTF_LUT_DATA_PORT              0x4c1f
#define DOLBY_HDR2_OETF_LUT_ADDR_PORT              0x4c20
#define DOLBY_HDR2_OETF_LUT_DATA_PORT              0x4c21
#define DOLBY_HDR2_CGAIN_LUT_ADDR_PORT             0x4c22
#define DOLBY_HDR2_CGAIN_LUT_DATA_PORT             0x4c23
#define DOLBY_HDR2_CGAIN_COEF0                     0x4c24
#define DOLBY_HDR2_CGAIN_COEF1                     0x4c25
#define DOLBY_HDR2_OGAIN_LUT_ADDR_PORT             0x4c26
#define DOLBY_HDR2_OGAIN_LUT_DATA_PORT             0x4c27
#define DOLBY_HDR2_ADPS_CTRL                       0x4c28
#define DOLBY_HDR2_ADPS_ALPHA0                     0x4c29
#define DOLBY_HDR2_ADPS_ALPHA1                     0x4c2a
#define DOLBY_HDR2_ADPS_BETA0                      0x4c2b
#define DOLBY_HDR2_ADPS_BETA1                      0x4c2c
#define DOLBY_HDR2_ADPS_BETA2                      0x4c2d
#define DOLBY_HDR2_ADPS_COEF0                      0x4c2e
#define DOLBY_HDR2_ADPS_COEF1                      0x4c2f
#define DOLBY_HDR2_GMUT_CTRL                       0x4c30
#define DOLBY_HDR2_GMUT_COEF0                      0x4c31
//Bit 31:16        reg_gmut_coef01         // signed ,    RW, default = -150
//Bit 15: 0        reg_gmut_coef00         // signed ,    RW, default = 425
#define DOLBY_HDR2_GMUT_COEF1                      0x4c32
//Bit 31:16        reg_gmut_coef10         // signed ,    RW, default = -31
//Bit 15: 0        reg_gmut_coef02         // signed ,    RW, default = -18
#define DOLBY_HDR2_GMUT_COEF2                      0x4c33
//Bit 31:16        reg_gmut_coef12         // signed ,    RW, default = -2
//Bit 15: 0        reg_gmut_coef11         // signed ,    RW, default = 290
#define DOLBY_HDR2_GMUT_COEF3                      0x4c34
//Bit 31:16        reg_gmut_coef21         // signed ,    RW, default = -25
//Bit 15: 0        reg_gmut_coef20         // signed ,    RW, default = -5
#define DOLBY_HDR2_GMUT_COEF4                      0x4c35
//Bit 31:16        reserved
//Bit 15: 0        reg_gmut_coef_2_2         // signed ,    RW, default = 286
#define DOLBY_HDR2_PIPE_CTRL1                      0x4c36
//Bit 31:24        reg_vblank_num_oetf       // unsigned ,    RW, default = 4
//Bit 23:16        reg_hblank_num_oetf       // unsigned ,    RW, default = 4
//Bit 15: 8        reg_vblank_num_eotf       // unsigned ,    RW, default = 10
//Bit  7: 0        reg_hblank_num_eotf       // unsigned ,    RW, default = 10
#define DOLBY_HDR2_PIPE_CTRL2                      0x4c37
//Bit 31:24        reg_vblank_num_cgain      // unsigned ,    RW, default = 10
//Bit 23:16        reg_hblank_num_cgain      // unsigned ,    RW, default = 10
//Bit 15: 8        reg_vblank_num_gmut       // unsigned ,    RW, default = 17
//Bit  7: 0        reg_hblank_num_gmut       // unsigned ,    RW, default = 17
#define DOLBY_HDR2_PIPE_CTRL3                      0x4c38
//Bit 31:24        reg_vblank_num_adps       // unsigned ,    RW, default = 22
//Bit 23:16        reg_hblank_num_adps       // unsigned ,    RW, default = 22
//Bit 15: 8        reg_vblank_num_uv         // unsigned ,    RW, default = 4
//Bit  7: 0        reg_hblank_num_uv         // unsigned ,    RW, default = 4
#define DOLBY_HDR2_PROC_WIN1                       0x4c39
//Bit 31           reg_proc_win_gmut_en      // unsigned ,    RW, default = 0
//Bit 30           reg_proc_win_adps_en      // unsigned ,    RW, default = 0
//Bit 29           reg_proc_win_cgain_en     // unsigned ,    RW, default = 0
//Bit 28:16        reg_proc_x_ed             // unsigned ,    RW, default = 99
//Bit 15:13        reserved
//Bit 12: 0        reg_proc_x_st             // unsigned ,    RW, default = 0
#define DOLBY_HDR2_PROC_WIN2                       0x4c3a
//Bit 31:30        reserved
//Bit 29           reg_proc_win_aicr_en      // unsigned ,    RW, default = 1
//Bit 28:16        reg_proc_y_ed             // unsigned ,    RW, default = 99
//Bit 15:13        reserved
//Bit 12: 0        reg_proc_y_st             // unsigned ,    RW, default = 0
#define DOLBY_HDR2_MATRIXI_EN_CTRL                 0x4c3b
//Bit 31: 8        reserved
//Bit  7: 0        reg_matrixi_en_ctrl       // unsigned ,    RW, default = 0
#define DOLBY_HDR2_MATRIXO_EN_CTRL                 0x4c3c
//Bit 31: 8        reserved
//Bit  7: 0        reg_matrixo_en_ctrl       // unsigned ,    RW, default = 0
#define DOLBY_HDR2_HIST_CTRL                       0x4c3d
#define DOLBY_HDR2_HIST_H_START_END                0x4c3e
#define DOLBY_HDR2_HIST_V_START_END                0x4c3f
#define DOLBY_HDR2_OGAIN_LUT1_ADDR_PORT            0x4c40
#define DOLBY_HDR2_OGAIN_LUT1_DATA_PORT            0x4c41
#define DOLBY_HDR2_CGAIN_VIVID                     0x4c42
//Bit 31:30        reserved
//Bit 29:26        reg_omax_sync_gain_sft    // unsigned , RW, default = 0
//Bit 25:16        reg_omax_sync_gain        // unsigned , RW, default = 1
//Bit 15: 4        reserved
//Bit 3:2          reg_cgain_pos             // unsigned , RW, default = 0
//Bit 1            reg_ogain_inser           // unsigned , RW, default = 0
//Bit 0            reserved
#define DOLBY_HDR2_GMUT_COMP0                      0x4c43
//Bit 31           reg_hdr2_gm_comp_en       // unsigned ,RW, default = 1
//Bit 30:28        reserved
//Bit 27: 8        reg_hdr_comp_ofst_r       // unsigned ,RW, default = 85900
//Bit  7: 0        reserved
#define DOLBY_HDR2_GMUT_COMP1                      0x4c44
//Bit 31:28        reserved
//Bit 27: 8        reg_hdr_comp_ofst_g       // unsigned ,RW, default = 85900
//Bit  7: 0        reserved
#define DOLBY_HDR2_GMUT_COMP2                      0x4c45
//Bit 31:28        reserved
//Bit 27: 8        reg_hdr_comp_ofst_b       // unsigned ,RW, default = 85900
//Bit  7: 0        reserved
#define DOLBY_HDR2_GMUT_COMP3                      0x4c46
//Bit 31:28        reserved
//Bit 27: 8        reg_hdr_comp_min_r        // unsigned ,RW, default = 510025
//Bit  7: 0        reserved
#define DOLBY_HDR2_GMUT_COMP4                      0x4c47
//Bit 31:28        reserved
//Bit 27: 8        reg_hdr_comp_min_g        // unsigned ,RW, default = 472965
//Bit  7: 0        reserved
#define DOLBY_HDR2_GMUT_COMP5                      0x4c48
//Bit 31:28        reserved
//Bit 27: 8        reg_hdr_comp_min_b        // unsigned ,RW, default = 467019
//Bit  7: 0        reserved
#define DOLBY_HDR2_GMUT_COMP6                      0x4c49
//Bit 31:30        reserved
//Bit 29: 8        reg_hdr_comp_rat_r        // unsigned ,RW, default = 152108
//Bit  7: 0        reserved
#define DOLBY_HDR2_GMUT_COMP7                      0x4c4a
//Bit 31:30        reserved
//Bit 29: 8        reg_hdr_comp_rat_g        // unsigned ,RW, default = 472965
//Bit  7: 0        reserved
#define DOLBY_HDR2_GMUT_COMP8                      0x4c4b
//Bit 31:30        reserved
//Bit 29: 8        reg_hdr_comp_rat_b        // unsigned , RW, default = 467019
//Bit  7: 0        reserved
#define DOLBY_HDR2_HIST_SLC_X_ST_ED_0              0x4c4c
//Bit 31:29        reg_hist_slc_num          // unsigned ,    RW, default = 1
//Bit 28:16        reg_hist_slc0_x_st        // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12 :0        reg_hist_slc0_x_ed        // unsigned ,    RW, default = 239
#define DOLBY_HDR2_HIST_SLC_X_ST_ED_1              0x4c4d
//Bit 31:29        reserved
//Bit 28:16        reg_hist_slc1_x_st        // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12 :0        reg_hist_slc1_x_ed        // unsigned ,    RW, default = 239
#define DOLBY_HDR2_HIST_SLC_X_ST_ED_2              0x4c4e
//Bit 31:29        reserved
//Bit 28:16        reg_hist_slc2_x_st        // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12 :0        reg_hist_slc2_x_ed        // unsigned ,    RW, default = 239
#define DOLBY_HDR2_HIST_SLC_X_ST_ED_3              0x4c4f
//Bit 31:29        reserved
//Bit 28:16        reg_hist_slc3_x_st        // unsigned ,    RW, default = 0
//Bit 15:13        reserved
//Bit 12 :0        reg_hist_slc3_x_ed        // unsigned ,    RW, default = 239
#define DOLBY_HDR2_ADPSCL1_COEF0                   0x4c50
//Bit 31:28        reserved
//Bit 27:16        reg_adpscl_ys_coef1_1
// unsigned ,    RW, default = 1024
//coef to calculate the Ys, normalized to 2048 as "1", leave one bit margin;
//Bit 15:12        reserved
//Bit 11: 0        reg_adpscl_ys_coef1_0
// unsigned ,    RW, default = 1024
//coef to calculate the Ys, normalized to 2048 as "1", leave one bit margin;
#define DOLBY_HDR2_ADPSCL1_COEF1                   0x4c51
//Bit 31:16        reg_bypass_ootf2_gain
// unsigned ,    RW, default = 51200  reg_bypass_ootf2_gain
//Bit 15:12        reserved
//Bit 11: 0        reg_adpscl_ys_coef1_2
// unsigned ,    RW, default = 1024  coef to calculate the Ys,
//normalized to 2048 as "1", leave one bit margin;
#define DOLBY_HDR2_RGB_GM_CTRL                     0x4c52
//Bit 31: 3        reserved
//Bit  2           reg_adpscl_mode_gm        // unsigned ,    RW, default = 1
//Bit  1           reg_rgb_gm_mode           // unsigned ,    RW, default = 0
//Bit  0           reg_rgb_gm_en             // unsigned ,    RW, default = 0

//#define VD2_HDR2_ADPSCL1_COEF0                     0x6350
//#define VD2_HDR2_ADPSCL1_COEF1                     0x6351
//#define VD2_HDR2_RGB_GM_CTRL                       0x6352

#endif
