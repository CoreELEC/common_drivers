/* Display-management metadata, configuration and kernel-state layouts. */
#ifndef DOVI_DM_LAYOUT_H
#define DOVI_DM_LAYOUT_H
#if defined(_MSC_VER)
#define DM_OFFSETOF(t, m) ((unsigned long long)&(((t *)0)->m))
#else
#define DM_OFFSETOF(t, m) __builtin_offsetof(t,m)
#endif

typedef struct Trim_t Trim_t;
typedef struct TrimSet_t TrimSet_t;
typedef struct MdsExt_t MdsExt_t;
typedef struct DmCtrlFxp_t DmCtrlFxp_t;
typedef struct GrcSigEnvFxp_t GrcSigEnvFxp_t;
typedef struct SigEnvCom_t SigEnvCom_t;
typedef struct SrcSigEnvFxp_t SrcSigEnvFxp_t;
typedef struct TmCtrlFxp_t TmCtrlFxp_t;
typedef struct TgtSigEnvFxp_t TgtSigEnvFxp_t;
typedef struct MmgReq_t MmgReq_t;
typedef struct Mmg_t_ Mmg_t_;
typedef struct DmCfgFxp_t DmCfgFxp_t;
typedef struct CscCtrlFxp_t CscCtrlFxp_t;
typedef struct TcSigFxp_t TcSigFxp_t;
typedef struct TcCtrlFxp_t TcCtrlFxp_t;
typedef struct DmKsUsFxp_t DmKsUsFxp_t;
typedef struct DmKsDsFxp_t DmKsDsFxp_t;
typedef struct DmKsCtrl_t DmKsCtrl_t;
typedef struct DmKsFrmFmt_t DmKsFrmFmt_t;
typedef struct EotfParamFxp_t EotfParamFxp_t;
typedef struct DmKsIMapFxp_t DmKsIMapFxp_t;
typedef struct DmKsTMapFxp_t DmKsTMapFxp_t;
typedef struct OetfParamFxp_t OetfParamFxp_t;
typedef struct Dm3KsOMapFxp_t Dm3KsOMapFxp_t;
typedef struct DmKsFxp_t DmKsFxp_t;
typedef struct DmCtxtFxp_t DmCtxtFxp_t;

struct Trim_t {
    short TrimLevel;
    short TrimTypeDim;
    short TrimNumMax;
    short TrimNum;
    void *Trima;
};

struct TrimSet_t {
    short TrimSetNum;
    unsigned char padding_2[6];
    Trim_t TrimSets[2];
    unsigned short TrimaMem[187];
    unsigned char padding_19e[2];
};

struct MdsExt_t {
    unsigned char affected_dm_metadata_id;
    unsigned char scene_refresh_flag;
    short m33Yuv2RgbScale2P;
    short m33Yuv2Rgb[3][3];
    unsigned char padding_16[2];
    unsigned int v3Yuv2Rgb[3];
    short m33Rgb2WpLmsScale2P;
    short m33Rgb2WpLms[3][3];
    unsigned short signal_eotf;
    unsigned short signal_eotf_param0;
    unsigned short signal_eotf_param1;
    unsigned char padding_3e[2];
    unsigned int signal_eotf_param2;
    unsigned char signal_bit_depth;
    unsigned char signal_color_space;
    unsigned char signal_chroma_format;
    unsigned char signal_full_range_flag;
    unsigned short source_min_PQ;
    unsigned short source_max_PQ;
    unsigned short source_diagonal;
    unsigned char num_ext_blocks;
    unsigned char padding_4f[1];
    TrimSet_t trimSets;
    short lvl5AoiAvail;
    unsigned short active_area_left_offset;
    unsigned short active_area_right_offset;
    unsigned short active_area_top_offset;
    unsigned short active_area_bottom_offset;
    short lvl255RunModeAvail;
    unsigned char dm_run_mode;
    unsigned char dm_run_version;
    unsigned char dm_debug0;
    unsigned char dm_debug1;
    unsigned char dm_debug2;
    unsigned char dm_debug3;
    unsigned short min_PQ;
    unsigned short max_PQ;
    unsigned short mid_PQ;
};

struct DmCtrlFxp_t {
    int SecIn;
    int SeparatedAlpha;
    unsigned int StbMode;
    unsigned int PMin;
    unsigned int PMax;
    unsigned short PMinPq;
    unsigned short PMaxPq;
    unsigned int Platform;
    int DevId;
    int CFxp;
    int KFxp;
    int Lms2IptExt;
    short M33Lms2Ipt[3][3];
    unsigned char padding_3e[2];
    int M33Lms2IptScale2P;
    int Ipt2LmsExt;
    short M33Ipt2Lms[3][3];
    unsigned char padding_5a[2];
    int M33Ipt2LmsScale2P;
    int Prf;
    int MdsFollowed;
};

struct GrcSigEnvFxp_t {
    int RowNum;
    int ColNum;
    int RowPitch;
    int RowPitchC;
    unsigned int Clr;
    unsigned int Chrm;
    unsigned int Weav;
    unsigned int Dtp;
    unsigned int Loc;
    int Bdp;
    unsigned int Eotf;
    unsigned int Rng;
    unsigned int RgbDef;
    int WpExt;
    int V3Wp[3];
    short WpScale;
    unsigned char padding_46[2];
    unsigned int YuvXferSpec;
    int Yuv2RgbExt;
    short M33Yuv2Rgb[3][3];
    unsigned char padding_62[2];
    int M33Yuv2RgbScale2P;
    int Yuv2RgbOffExt;
    int V3Yuv2RgbOff[3];
    int V3Yuv2RgbOffInRgb[3];
    int Rgb2LmsRgbwExt;
    int V8Rgbw[8];
    int Rgb2LmsM33Ext;
    short M33Rgb2Lms[3][3];
    unsigned char padding_be[2];
    int M33Rgb2LmsScale2P;
    unsigned short Gamma;
    unsigned char padding_c6[2];
    unsigned int Min;
    unsigned int Max;
    unsigned short MinPq;
    unsigned short MaxPq;
    unsigned short A;
    unsigned short B;
    unsigned int G;
    unsigned short DiagSize;
    unsigned char padding_de[2];
    int CrossTalk;
};

struct SigEnvCom_t {
    int RowNum;
    int ColNum;
    int RowPitch;
    int RowPitchC;
    unsigned int Clr;
    unsigned int Chrm;
    unsigned int Weav;
    unsigned int Dtp;
    unsigned int Loc;
    int Bdp;
};

struct SrcSigEnvFxp_t {
    int RowNum;
    int ColNum;
    int RowPitch;
    int RowPitchC;
    unsigned int Clr;
    unsigned int Chrm;
    unsigned int Weav;
    unsigned int Dtp;
    unsigned int Loc;
    int Bdp;
    unsigned int Eotf;
    unsigned int Rng;
    unsigned int RgbDef;
    int WpExt;
    int V3Wp[3];
    short WpScale;
    unsigned char padding_46[2];
    unsigned int YuvXferSpec;
    int Yuv2RgbExt;
    short M33Yuv2Rgb[3][3];
    unsigned char padding_62[2];
    int M33Yuv2RgbScale2P;
    int Yuv2RgbOffExt;
    int V3Yuv2RgbOff[3];
    int V3Yuv2RgbOffInRgb[3];
    int Rgb2LmsRgbwExt;
    int V8Rgbw[8];
    int Rgb2LmsM33Ext;
    short M33Rgb2Lms[3][3];
    unsigned char padding_be[2];
    int M33Rgb2LmsScale2P;
    unsigned short Gamma;
    unsigned char padding_c6[2];
    unsigned int Min;
    unsigned int Max;
    unsigned short MinPq;
    unsigned short MaxPq;
    unsigned short A;
    unsigned short B;
    unsigned int G;
    unsigned short DiagSize;
    unsigned char padding_de[2];
    int CrossTalk;
};

struct TmCtrlFxp_t {
    int BwdInvertible;
    int Rolloff;
    int KeyWeight;
    int IntensityVectorWeight;
    int IntensityVectorWeightBwd;
    int ChrmVectorWeight;
    int ChrmVectorWeightBwd;
    int BpWeight;
    short TMinBias;
    short TMaxBias;
    short TMidBias;
    short DContrast;
    short DBrightness;
    unsigned char padding_2a[2];
    int l2off;
    int Default2[7];
    int ValueAdj2[7];
    int CodeBias2[7];
    int ValueAdj2SatGainBwd;
    int Default3[4];
    int CodeBias3[4];
};

struct TgtSigEnvFxp_t {
    int RowNum;
    int ColNum;
    int RowPitch;
    int RowPitchC;
    unsigned int Clr;
    unsigned int Chrm;
    unsigned int Weav;
    unsigned int Dtp;
    unsigned int Loc;
    int Bdp;
    unsigned int Eotf;
    unsigned int Rng;
    unsigned int RgbDef;
    int WpExt;
    int V3Wp[3];
    short WpScale;
    unsigned short Gamma;
    unsigned int YuvXferSpec;
    int Rgb2YuvExt;
    short M33Rgb2Yuv[3][3];
    unsigned char padding_62[2];
    int M33Rgb2YuvScale2P;
    int Rgb2YuvOffExt;
    int V3Rgb2YuvOff[3];
    int Lms2RgbRgbwExt;
    int V8Rgbw[8];
    int Lms2RgbM33Ext;
    short M33Lms2Rgb[3][3];
    unsigned char padding_b2[2];
    int M33Lms2RgbScale2P;
    unsigned int Min;
    unsigned int Max;
    unsigned short MinPq;
    unsigned short MaxPq;
    unsigned short DiagSize;
    unsigned char padding_c6[2];
    int CrossTalk;
};

struct MmgReq_t {
    unsigned int size;
    unsigned int align;
    int inDev;
    unsigned char padding_c[4];
    void *pCbParam;
    void *pCbFun;
};

struct Mmg_t_ {
    MmgReq_t mmgReqs[32];
    void *mmgBufs[32];
    int reqNum;
    unsigned int dmCtxtSize;
    unsigned int mdsExtSize;
    unsigned int gmLutSize;
    unsigned int pq2gLutSize;
    unsigned int dmKsSize;
};

struct DmCfgFxp_t {
    DmCtrlFxp_t dmCtrl;
    GrcSigEnvFxp_t grcSigEnv;
    SigEnvCom_t alpSigEnv;
    SrcSigEnvFxp_t srcSigEnv;
    TmCtrlFxp_t tmCtrl;
    TgtSigEnvFxp_t tgtSigEnv;
    unsigned char padding_3cc[4];
    Mmg_t_ *hMmg;
};

struct CscCtrlFxp_t {
    short sM33Rgb2Lms[3][3];
    unsigned char padding_12[2];
    int sM33Rgb2LmsScale2P;
    int sV3Wp[3];
    int sV3WpScale2P;
    short gM33Rgb2Lms[3][3];
    unsigned char padding_3a[2];
    int gM33Rgb2LmsScale2P;
    int gV3Wp[3];
    int gV3WpScale2P;
    short tM33Lms2Rgb[3][3];
    unsigned char padding_62[2];
    int tM33Lms2RgbScale2P;
    int tV3Wp[3];
    int tV3WpScale2P;
    short m33Lms2Ipt[3][3];
    unsigned char padding_8a[2];
    int m33Lms2IptScale2P;
    short m33Ipt2Lms[3][3];
    unsigned char padding_a2[2];
    int m33Ipt2LmsScale2P;
};

struct TcSigFxp_t {
    unsigned short minPq;
    unsigned short maxPq;
    unsigned short diagSize;
    short hlgA;
    unsigned int hlgGamma;
    unsigned int min;
    unsigned int rng;
    unsigned short crush;
    unsigned short clip;
    unsigned short mid;
    unsigned char padding_1a[2];
};

struct TcCtrlFxp_t {
    TcSigFxp_t tcSrcSig;
    TcSigFxp_t tcTgtSig;
    unsigned int tcLutDir;
    int bwdInvertible;
    int rolloff;
    int rolloffInv;
    int keyWeight;
    int intensityVectorWeight;
    int chrmVectorWeight;
    int bpWeight;
    int ratio;
    unsigned int gsMin;
    unsigned int gsMax;
    unsigned int gsGamma;
    short tMinBias;
    short tMaxBias;
    short tMidBias;
    short dContrast;
    short dBrightness;
    unsigned char padding_72[2];
    int level2[7];
    int level3[4];
    unsigned short tCrush;
    unsigned short tClip;
    unsigned short tMid;
    unsigned char padding_a6[2];
};

struct DmKsUsFxp_t {
    unsigned char chrmIn;
    unsigned char padding_1[1];
    short minUs;
    short maxUs;
    short filterUvRowUsHalfSize;
    short filterUvRowUsScale2P;
    short filterUvRowUs0_m[8];
    short filterUvRowUs1_m[8];
    short filterUvColUsHalfSize;
    short filterUvColUsScale2P;
    short filterUvColUs_m[8];
};

struct DmKsDsFxp_t {
    unsigned char chrmOut;
    unsigned char padding_1[1];
    short minDs;
    short maxDs;
    short filterUvColDsRadius;
    short filterUvColDsScale2P;
    short filterUvColDs_m[16];
};

struct DmKsCtrl_t {
    int mainIn;
    int secIn;
    int prf;
};

struct DmKsFrmFmt_t {
    int rowNum;
    int colNum;
    unsigned char dtp;
    unsigned char weav;
    unsigned char loc;
    unsigned char bdp;
    unsigned char chrm;
    unsigned char clr;
    unsigned char padding_e[2];
    int rowPitch;
    int colPitch;
    int rowPitchC;
};

struct EotfParamFxp_t {
    unsigned short rangeMin;
    unsigned short range;
    unsigned int rangeInv;
    unsigned short bdp;
    unsigned char eotf;
    unsigned char padding_b[1];
};

struct DmKsIMapFxp_t {
    unsigned char clr;
    unsigned char padding_1[1];
    short m33Yuv2RgbScale2P;
    short m33Yuv2Rgb[3][3];
    unsigned char padding_16[2];
    int v3Yuv2RgbOffInRgb[3];
    EotfParamFxp_t eotfParam;
    unsigned int g2L[256];
    short m33Rgb2LmsScale2P;
    short m33Rgb2Lms[3][3];
    short m33Lms2IptScale2P;
    short m33Lms2Ipt[3][3];
    int iptScale;
    int v3IptOff[3];
};

struct DmKsTMapFxp_t {
    short tmLutISizeM1;
    short tmLutSSizeM1;
    short tmLutI[512];
    short tmLutS[512];
    short smLutISizeM1;
    short smLutSSizeM1;
    short smLutI[512];
    short smLutS[512];
};

struct OetfParamFxp_t {
    unsigned int min;
    unsigned int max;
    unsigned short rangeMin;
    unsigned short range;
    unsigned int rangeOverOne;
    unsigned short bdp;
    unsigned char oetf;
    unsigned char padding_13[1];
};

struct Dm3KsOMapFxp_t {
    short m33Ipt2LmsScale2P;
    short m33Ipt2Lms[3][3];
    short m33Lms2RgbScale2P;
    short m33Lms2Rgb[3][3];
    OetfParamFxp_t oetfParam;
    unsigned char clr;
    unsigned char padding_3d[1];
    short m33Rgb2YuvScale2P;
    short m33Rgb2Yuv[3][3];
    unsigned char padding_52[2];
    int v3Rgb2YuvOff[3];
    int iptScale;
    int v3IptOff[3];
};

struct DmKsFxp_t {
    short rowPitchNum;
    unsigned char padding_2[6];
    void *frmBuf0;
    void *frmBuf1;
    void *frmBuf2;
    void *frmBuf0G;
    void *frmBuf1G;
    void *frmBuf2G;
    DmKsUsFxp_t ksUs;
    DmKsDsFxp_t ksDs;
    DmKsCtrl_t ksDmCtrl;
    DmKsFrmFmt_t ksFrmFmtI;
    DmKsIMapFxp_t ksIMap;
    DmKsTMapFxp_t ksTMap;
    DmKsUsFxp_t ksUsG;
    unsigned char padding_1576[2];
    DmKsFrmFmt_t ksFrmFmtA;
    unsigned int rangeInvA;
    DmKsFrmFmt_t ksFrmFmtG;
    DmKsIMapFxp_t ksIMapG;
    DmKsTMapFxp_t ksTMapG;
    Dm3KsOMapFxp_t ksOMap;
    DmKsFrmFmt_t ksFrmFmtO;
};

struct DmCtxtFxp_t {
    int dmCfgToBeValid;
    int dmCfgActValid;
    DmCfgFxp_t dmCfg[2];
    DmCfgFxp_t *pDmCfgAct;
    DmCfgFxp_t *pDmCfgToBe;
    int mdsExtFrcCmmt;
    int mdsExtActValid;
    MdsExt_t mdsExtAct;
    CscCtrlFxp_t cscCtrl;
    TcCtrlFxp_t tcCtrl;
    TcCtrlFxp_t tcCtrlUsed;
    TcCtrlFxp_t tc2Ctrl;
    TcCtrlFxp_t tc2CtrlUsed;
    unsigned int l2gLnLut[129];
    unsigned int l2gGmLut[129];
    DmKsFxp_t *hDmKs;
};

typedef char check_Trim_t_TrimLevel[(DM_OFFSETOF(Trim_t, TrimLevel) == 0) ? 1 : -1];
typedef char check_Trim_t_TrimTypeDim[(DM_OFFSETOF(Trim_t, TrimTypeDim) == 2) ? 1 : -1];
typedef char check_Trim_t_TrimNumMax[(DM_OFFSETOF(Trim_t, TrimNumMax) == 4) ? 1 : -1];
typedef char check_Trim_t_TrimNum[(DM_OFFSETOF(Trim_t, TrimNum) == 6) ? 1 : -1];
typedef char check_Trim_t_Trima[(DM_OFFSETOF(Trim_t, Trima) == 8) ? 1 : -1];
typedef char check_size_Trim_t[(sizeof(Trim_t) == 16) ? 1 : -1];
typedef char check_TrimSet_t_TrimSetNum[(DM_OFFSETOF(TrimSet_t, TrimSetNum) == 0) ? 1 : -1];
typedef char check_TrimSet_t_TrimSets[(DM_OFFSETOF(TrimSet_t, TrimSets) == 8) ? 1 : -1];
typedef char check_TrimSet_t_TrimaMem[(DM_OFFSETOF(TrimSet_t, TrimaMem) == 40) ? 1 : -1];
typedef char check_size_TrimSet_t[(sizeof(TrimSet_t) == 416) ? 1 : -1];
typedef char check_MdsExt_t_affected_dm_metadata_id
    [(DM_OFFSETOF(MdsExt_t, affected_dm_metadata_id) == 0) ? 1 : -1];
typedef char
    check_MdsExt_t_scene_refresh_flag[(DM_OFFSETOF(MdsExt_t, scene_refresh_flag) == 1) ? 1 : -1];
typedef char
    check_MdsExt_t_m33Yuv2RgbScale2P[(DM_OFFSETOF(MdsExt_t, m33Yuv2RgbScale2P) == 2) ? 1 : -1];
typedef char check_MdsExt_t_m33Yuv2Rgb[(DM_OFFSETOF(MdsExt_t, m33Yuv2Rgb) == 4) ? 1 : -1];
typedef char check_MdsExt_t_v3Yuv2Rgb[(DM_OFFSETOF(MdsExt_t, v3Yuv2Rgb) == 24) ? 1 : -1];
typedef char
    check_MdsExt_t_m33Rgb2WpLmsScale2P[(DM_OFFSETOF(MdsExt_t, m33Rgb2WpLmsScale2P) == 36) ? 1 : -1];
typedef char check_MdsExt_t_m33Rgb2WpLms[(DM_OFFSETOF(MdsExt_t, m33Rgb2WpLms) == 38) ? 1 : -1];
typedef char check_MdsExt_t_signal_eotf[(DM_OFFSETOF(MdsExt_t, signal_eotf) == 56) ? 1 : -1];
typedef char
    check_MdsExt_t_signal_eotf_param0[(DM_OFFSETOF(MdsExt_t, signal_eotf_param0) == 58) ? 1 : -1];
typedef char
    check_MdsExt_t_signal_eotf_param1[(DM_OFFSETOF(MdsExt_t, signal_eotf_param1) == 60) ? 1 : -1];
typedef char
    check_MdsExt_t_signal_eotf_param2[(DM_OFFSETOF(MdsExt_t, signal_eotf_param2) == 64) ? 1 : -1];
typedef char
    check_MdsExt_t_signal_bit_depth[(DM_OFFSETOF(MdsExt_t, signal_bit_depth) == 68) ? 1 : -1];
typedef char
    check_MdsExt_t_signal_color_space[(DM_OFFSETOF(MdsExt_t, signal_color_space) == 69) ? 1 : -1];
typedef char check_MdsExt_t_signal_chroma_format[(DM_OFFSETOF(MdsExt_t, signal_chroma_format) == 70)
                                                     ? 1
                                                     : -1];
typedef char check_MdsExt_t_signal_full_range_flag
    [(DM_OFFSETOF(MdsExt_t, signal_full_range_flag) == 71) ? 1 : -1];
typedef char check_MdsExt_t_source_min_PQ[(DM_OFFSETOF(MdsExt_t, source_min_PQ) == 72) ? 1 : -1];
typedef char check_MdsExt_t_source_max_PQ[(DM_OFFSETOF(MdsExt_t, source_max_PQ) == 74) ? 1 : -1];
typedef char
    check_MdsExt_t_source_diagonal[(DM_OFFSETOF(MdsExt_t, source_diagonal) == 76) ? 1 : -1];
typedef char check_MdsExt_t_num_ext_blocks[(DM_OFFSETOF(MdsExt_t, num_ext_blocks) == 78) ? 1 : -1];
typedef char check_MdsExt_t_trimSets[(DM_OFFSETOF(MdsExt_t, trimSets) == 80) ? 1 : -1];
typedef char check_MdsExt_t_lvl5AoiAvail[(DM_OFFSETOF(MdsExt_t, lvl5AoiAvail) == 496) ? 1 : -1];
typedef char check_MdsExt_t_active_area_left_offset
    [(DM_OFFSETOF(MdsExt_t, active_area_left_offset) == 498) ? 1 : -1];
typedef char check_MdsExt_t_active_area_right_offset
    [(DM_OFFSETOF(MdsExt_t, active_area_right_offset) == 500) ? 1 : -1];
typedef char check_MdsExt_t_active_area_top_offset
    [(DM_OFFSETOF(MdsExt_t, active_area_top_offset) == 502) ? 1 : -1];
typedef char check_MdsExt_t_active_area_bottom_offset
    [(DM_OFFSETOF(MdsExt_t, active_area_bottom_offset) == 504) ? 1 : -1];
typedef char
    check_MdsExt_t_lvl255RunModeAvail[(DM_OFFSETOF(MdsExt_t, lvl255RunModeAvail) == 506) ? 1 : -1];
typedef char check_MdsExt_t_dm_run_mode[(DM_OFFSETOF(MdsExt_t, dm_run_mode) == 508) ? 1 : -1];
typedef char check_MdsExt_t_dm_run_version[(DM_OFFSETOF(MdsExt_t, dm_run_version) == 509) ? 1 : -1];
typedef char check_MdsExt_t_dm_debug0[(DM_OFFSETOF(MdsExt_t, dm_debug0) == 510) ? 1 : -1];
typedef char check_MdsExt_t_dm_debug1[(DM_OFFSETOF(MdsExt_t, dm_debug1) == 511) ? 1 : -1];
typedef char check_MdsExt_t_dm_debug2[(DM_OFFSETOF(MdsExt_t, dm_debug2) == 512) ? 1 : -1];
typedef char check_MdsExt_t_dm_debug3[(DM_OFFSETOF(MdsExt_t, dm_debug3) == 513) ? 1 : -1];
typedef char check_MdsExt_t_min_PQ[(DM_OFFSETOF(MdsExt_t, min_PQ) == 514) ? 1 : -1];
typedef char check_MdsExt_t_max_PQ[(DM_OFFSETOF(MdsExt_t, max_PQ) == 516) ? 1 : -1];
typedef char check_MdsExt_t_mid_PQ[(DM_OFFSETOF(MdsExt_t, mid_PQ) == 518) ? 1 : -1];
typedef char check_size_MdsExt_t[(sizeof(MdsExt_t) == 520) ? 1 : -1];
typedef char check_DmCtrlFxp_t_SecIn[(DM_OFFSETOF(DmCtrlFxp_t, SecIn) == 0) ? 1 : -1];
typedef char
    check_DmCtrlFxp_t_SeparatedAlpha[(DM_OFFSETOF(DmCtrlFxp_t, SeparatedAlpha) == 4) ? 1 : -1];
typedef char check_DmCtrlFxp_t_StbMode[(DM_OFFSETOF(DmCtrlFxp_t, StbMode) == 8) ? 1 : -1];
typedef char check_DmCtrlFxp_t_PMin[(DM_OFFSETOF(DmCtrlFxp_t, PMin) == 12) ? 1 : -1];
typedef char check_DmCtrlFxp_t_PMax[(DM_OFFSETOF(DmCtrlFxp_t, PMax) == 16) ? 1 : -1];
typedef char check_DmCtrlFxp_t_PMinPq[(DM_OFFSETOF(DmCtrlFxp_t, PMinPq) == 20) ? 1 : -1];
typedef char check_DmCtrlFxp_t_PMaxPq[(DM_OFFSETOF(DmCtrlFxp_t, PMaxPq) == 22) ? 1 : -1];
typedef char check_DmCtrlFxp_t_Platform[(DM_OFFSETOF(DmCtrlFxp_t, Platform) == 24) ? 1 : -1];
typedef char check_DmCtrlFxp_t_DevId[(DM_OFFSETOF(DmCtrlFxp_t, DevId) == 28) ? 1 : -1];
typedef char check_DmCtrlFxp_t_CFxp[(DM_OFFSETOF(DmCtrlFxp_t, CFxp) == 32) ? 1 : -1];
typedef char check_DmCtrlFxp_t_KFxp[(DM_OFFSETOF(DmCtrlFxp_t, KFxp) == 36) ? 1 : -1];
typedef char check_DmCtrlFxp_t_Lms2IptExt[(DM_OFFSETOF(DmCtrlFxp_t, Lms2IptExt) == 40) ? 1 : -1];
typedef char check_DmCtrlFxp_t_M33Lms2Ipt[(DM_OFFSETOF(DmCtrlFxp_t, M33Lms2Ipt) == 44) ? 1 : -1];
typedef char check_DmCtrlFxp_t_M33Lms2IptScale2P[(DM_OFFSETOF(DmCtrlFxp_t, M33Lms2IptScale2P) == 64)
                                                     ? 1
                                                     : -1];
typedef char check_DmCtrlFxp_t_Ipt2LmsExt[(DM_OFFSETOF(DmCtrlFxp_t, Ipt2LmsExt) == 68) ? 1 : -1];
typedef char check_DmCtrlFxp_t_M33Ipt2Lms[(DM_OFFSETOF(DmCtrlFxp_t, M33Ipt2Lms) == 72) ? 1 : -1];
typedef char check_DmCtrlFxp_t_M33Ipt2LmsScale2P[(DM_OFFSETOF(DmCtrlFxp_t, M33Ipt2LmsScale2P) == 92)
                                                     ? 1
                                                     : -1];
typedef char check_DmCtrlFxp_t_Prf[(DM_OFFSETOF(DmCtrlFxp_t, Prf) == 96) ? 1 : -1];
typedef char check_DmCtrlFxp_t_MdsFollowed[(DM_OFFSETOF(DmCtrlFxp_t, MdsFollowed) == 100) ? 1 : -1];
typedef char check_size_DmCtrlFxp_t[(sizeof(DmCtrlFxp_t) == 104) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_RowNum[(DM_OFFSETOF(GrcSigEnvFxp_t, RowNum) == 0) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_ColNum[(DM_OFFSETOF(GrcSigEnvFxp_t, ColNum) == 4) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_RowPitch[(DM_OFFSETOF(GrcSigEnvFxp_t, RowPitch) == 8) ? 1 : -1];
typedef char
    check_GrcSigEnvFxp_t_RowPitchC[(DM_OFFSETOF(GrcSigEnvFxp_t, RowPitchC) == 12) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_Clr[(DM_OFFSETOF(GrcSigEnvFxp_t, Clr) == 16) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_Chrm[(DM_OFFSETOF(GrcSigEnvFxp_t, Chrm) == 20) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_Weav[(DM_OFFSETOF(GrcSigEnvFxp_t, Weav) == 24) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_Dtp[(DM_OFFSETOF(GrcSigEnvFxp_t, Dtp) == 28) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_Loc[(DM_OFFSETOF(GrcSigEnvFxp_t, Loc) == 32) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_Bdp[(DM_OFFSETOF(GrcSigEnvFxp_t, Bdp) == 36) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_Eotf[(DM_OFFSETOF(GrcSigEnvFxp_t, Eotf) == 40) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_Rng[(DM_OFFSETOF(GrcSigEnvFxp_t, Rng) == 44) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_RgbDef[(DM_OFFSETOF(GrcSigEnvFxp_t, RgbDef) == 48) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_WpExt[(DM_OFFSETOF(GrcSigEnvFxp_t, WpExt) == 52) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_V3Wp[(DM_OFFSETOF(GrcSigEnvFxp_t, V3Wp) == 56) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_WpScale[(DM_OFFSETOF(GrcSigEnvFxp_t, WpScale) == 68) ? 1 : -1];
typedef char
    check_GrcSigEnvFxp_t_YuvXferSpec[(DM_OFFSETOF(GrcSigEnvFxp_t, YuvXferSpec) == 72) ? 1 : -1];
typedef char
    check_GrcSigEnvFxp_t_Yuv2RgbExt[(DM_OFFSETOF(GrcSigEnvFxp_t, Yuv2RgbExt) == 76) ? 1 : -1];
typedef char
    check_GrcSigEnvFxp_t_M33Yuv2Rgb[(DM_OFFSETOF(GrcSigEnvFxp_t, M33Yuv2Rgb) == 80) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_M33Yuv2RgbScale2P
    [(DM_OFFSETOF(GrcSigEnvFxp_t, M33Yuv2RgbScale2P) == 100) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_Yuv2RgbOffExt[(DM_OFFSETOF(GrcSigEnvFxp_t, Yuv2RgbOffExt) == 104)
                                                    ? 1
                                                    : -1];
typedef char
    check_GrcSigEnvFxp_t_V3Yuv2RgbOff[(DM_OFFSETOF(GrcSigEnvFxp_t, V3Yuv2RgbOff) == 108) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_V3Yuv2RgbOffInRgb
    [(DM_OFFSETOF(GrcSigEnvFxp_t, V3Yuv2RgbOffInRgb) == 120) ? 1 : -1];
typedef char
    check_GrcSigEnvFxp_t_Rgb2LmsRgbwExt[(DM_OFFSETOF(GrcSigEnvFxp_t, Rgb2LmsRgbwExt) == 132) ? 1
                                                                                             : -1];
typedef char check_GrcSigEnvFxp_t_V8Rgbw[(DM_OFFSETOF(GrcSigEnvFxp_t, V8Rgbw) == 136) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_Rgb2LmsM33Ext[(DM_OFFSETOF(GrcSigEnvFxp_t, Rgb2LmsM33Ext) == 168)
                                                    ? 1
                                                    : -1];
typedef char
    check_GrcSigEnvFxp_t_M33Rgb2Lms[(DM_OFFSETOF(GrcSigEnvFxp_t, M33Rgb2Lms) == 172) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_M33Rgb2LmsScale2P
    [(DM_OFFSETOF(GrcSigEnvFxp_t, M33Rgb2LmsScale2P) == 192) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_Gamma[(DM_OFFSETOF(GrcSigEnvFxp_t, Gamma) == 196) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_Min[(DM_OFFSETOF(GrcSigEnvFxp_t, Min) == 200) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_Max[(DM_OFFSETOF(GrcSigEnvFxp_t, Max) == 204) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_MinPq[(DM_OFFSETOF(GrcSigEnvFxp_t, MinPq) == 208) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_MaxPq[(DM_OFFSETOF(GrcSigEnvFxp_t, MaxPq) == 210) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_A[(DM_OFFSETOF(GrcSigEnvFxp_t, A) == 212) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_B[(DM_OFFSETOF(GrcSigEnvFxp_t, B) == 214) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_G[(DM_OFFSETOF(GrcSigEnvFxp_t, G) == 216) ? 1 : -1];
typedef char check_GrcSigEnvFxp_t_DiagSize[(DM_OFFSETOF(GrcSigEnvFxp_t, DiagSize) == 220) ? 1 : -1];
typedef char
    check_GrcSigEnvFxp_t_CrossTalk[(DM_OFFSETOF(GrcSigEnvFxp_t, CrossTalk) == 224) ? 1 : -1];
typedef char check_size_GrcSigEnvFxp_t[(sizeof(GrcSigEnvFxp_t) == 228) ? 1 : -1];
typedef char check_SigEnvCom_t_RowNum[(DM_OFFSETOF(SigEnvCom_t, RowNum) == 0) ? 1 : -1];
typedef char check_SigEnvCom_t_ColNum[(DM_OFFSETOF(SigEnvCom_t, ColNum) == 4) ? 1 : -1];
typedef char check_SigEnvCom_t_RowPitch[(DM_OFFSETOF(SigEnvCom_t, RowPitch) == 8) ? 1 : -1];
typedef char check_SigEnvCom_t_RowPitchC[(DM_OFFSETOF(SigEnvCom_t, RowPitchC) == 12) ? 1 : -1];
typedef char check_SigEnvCom_t_Clr[(DM_OFFSETOF(SigEnvCom_t, Clr) == 16) ? 1 : -1];
typedef char check_SigEnvCom_t_Chrm[(DM_OFFSETOF(SigEnvCom_t, Chrm) == 20) ? 1 : -1];
typedef char check_SigEnvCom_t_Weav[(DM_OFFSETOF(SigEnvCom_t, Weav) == 24) ? 1 : -1];
typedef char check_SigEnvCom_t_Dtp[(DM_OFFSETOF(SigEnvCom_t, Dtp) == 28) ? 1 : -1];
typedef char check_SigEnvCom_t_Loc[(DM_OFFSETOF(SigEnvCom_t, Loc) == 32) ? 1 : -1];
typedef char check_SigEnvCom_t_Bdp[(DM_OFFSETOF(SigEnvCom_t, Bdp) == 36) ? 1 : -1];
typedef char check_size_SigEnvCom_t[(sizeof(SigEnvCom_t) == 40) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_RowNum[(DM_OFFSETOF(SrcSigEnvFxp_t, RowNum) == 0) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_ColNum[(DM_OFFSETOF(SrcSigEnvFxp_t, ColNum) == 4) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_RowPitch[(DM_OFFSETOF(SrcSigEnvFxp_t, RowPitch) == 8) ? 1 : -1];
typedef char
    check_SrcSigEnvFxp_t_RowPitchC[(DM_OFFSETOF(SrcSigEnvFxp_t, RowPitchC) == 12) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_Clr[(DM_OFFSETOF(SrcSigEnvFxp_t, Clr) == 16) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_Chrm[(DM_OFFSETOF(SrcSigEnvFxp_t, Chrm) == 20) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_Weav[(DM_OFFSETOF(SrcSigEnvFxp_t, Weav) == 24) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_Dtp[(DM_OFFSETOF(SrcSigEnvFxp_t, Dtp) == 28) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_Loc[(DM_OFFSETOF(SrcSigEnvFxp_t, Loc) == 32) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_Bdp[(DM_OFFSETOF(SrcSigEnvFxp_t, Bdp) == 36) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_Eotf[(DM_OFFSETOF(SrcSigEnvFxp_t, Eotf) == 40) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_Rng[(DM_OFFSETOF(SrcSigEnvFxp_t, Rng) == 44) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_RgbDef[(DM_OFFSETOF(SrcSigEnvFxp_t, RgbDef) == 48) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_WpExt[(DM_OFFSETOF(SrcSigEnvFxp_t, WpExt) == 52) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_V3Wp[(DM_OFFSETOF(SrcSigEnvFxp_t, V3Wp) == 56) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_WpScale[(DM_OFFSETOF(SrcSigEnvFxp_t, WpScale) == 68) ? 1 : -1];
typedef char
    check_SrcSigEnvFxp_t_YuvXferSpec[(DM_OFFSETOF(SrcSigEnvFxp_t, YuvXferSpec) == 72) ? 1 : -1];
typedef char
    check_SrcSigEnvFxp_t_Yuv2RgbExt[(DM_OFFSETOF(SrcSigEnvFxp_t, Yuv2RgbExt) == 76) ? 1 : -1];
typedef char
    check_SrcSigEnvFxp_t_M33Yuv2Rgb[(DM_OFFSETOF(SrcSigEnvFxp_t, M33Yuv2Rgb) == 80) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_M33Yuv2RgbScale2P
    [(DM_OFFSETOF(SrcSigEnvFxp_t, M33Yuv2RgbScale2P) == 100) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_Yuv2RgbOffExt[(DM_OFFSETOF(SrcSigEnvFxp_t, Yuv2RgbOffExt) == 104)
                                                    ? 1
                                                    : -1];
typedef char
    check_SrcSigEnvFxp_t_V3Yuv2RgbOff[(DM_OFFSETOF(SrcSigEnvFxp_t, V3Yuv2RgbOff) == 108) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_V3Yuv2RgbOffInRgb
    [(DM_OFFSETOF(SrcSigEnvFxp_t, V3Yuv2RgbOffInRgb) == 120) ? 1 : -1];
typedef char
    check_SrcSigEnvFxp_t_Rgb2LmsRgbwExt[(DM_OFFSETOF(SrcSigEnvFxp_t, Rgb2LmsRgbwExt) == 132) ? 1
                                                                                             : -1];
typedef char check_SrcSigEnvFxp_t_V8Rgbw[(DM_OFFSETOF(SrcSigEnvFxp_t, V8Rgbw) == 136) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_Rgb2LmsM33Ext[(DM_OFFSETOF(SrcSigEnvFxp_t, Rgb2LmsM33Ext) == 168)
                                                    ? 1
                                                    : -1];
typedef char
    check_SrcSigEnvFxp_t_M33Rgb2Lms[(DM_OFFSETOF(SrcSigEnvFxp_t, M33Rgb2Lms) == 172) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_M33Rgb2LmsScale2P
    [(DM_OFFSETOF(SrcSigEnvFxp_t, M33Rgb2LmsScale2P) == 192) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_Gamma[(DM_OFFSETOF(SrcSigEnvFxp_t, Gamma) == 196) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_Min[(DM_OFFSETOF(SrcSigEnvFxp_t, Min) == 200) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_Max[(DM_OFFSETOF(SrcSigEnvFxp_t, Max) == 204) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_MinPq[(DM_OFFSETOF(SrcSigEnvFxp_t, MinPq) == 208) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_MaxPq[(DM_OFFSETOF(SrcSigEnvFxp_t, MaxPq) == 210) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_A[(DM_OFFSETOF(SrcSigEnvFxp_t, A) == 212) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_B[(DM_OFFSETOF(SrcSigEnvFxp_t, B) == 214) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_G[(DM_OFFSETOF(SrcSigEnvFxp_t, G) == 216) ? 1 : -1];
typedef char check_SrcSigEnvFxp_t_DiagSize[(DM_OFFSETOF(SrcSigEnvFxp_t, DiagSize) == 220) ? 1 : -1];
typedef char
    check_SrcSigEnvFxp_t_CrossTalk[(DM_OFFSETOF(SrcSigEnvFxp_t, CrossTalk) == 224) ? 1 : -1];
typedef char check_size_SrcSigEnvFxp_t[(sizeof(SrcSigEnvFxp_t) == 228) ? 1 : -1];
typedef char
    check_TmCtrlFxp_t_BwdInvertible[(DM_OFFSETOF(TmCtrlFxp_t, BwdInvertible) == 0) ? 1 : -1];
typedef char check_TmCtrlFxp_t_Rolloff[(DM_OFFSETOF(TmCtrlFxp_t, Rolloff) == 4) ? 1 : -1];
typedef char check_TmCtrlFxp_t_KeyWeight[(DM_OFFSETOF(TmCtrlFxp_t, KeyWeight) == 8) ? 1 : -1];
typedef char check_TmCtrlFxp_t_IntensityVectorWeight
    [(DM_OFFSETOF(TmCtrlFxp_t, IntensityVectorWeight) == 12) ? 1 : -1];
typedef char check_TmCtrlFxp_t_IntensityVectorWeightBwd
    [(DM_OFFSETOF(TmCtrlFxp_t, IntensityVectorWeightBwd) == 16) ? 1 : -1];
typedef char
    check_TmCtrlFxp_t_ChrmVectorWeight[(DM_OFFSETOF(TmCtrlFxp_t, ChrmVectorWeight) == 20) ? 1 : -1];
typedef char check_TmCtrlFxp_t_ChrmVectorWeightBwd
    [(DM_OFFSETOF(TmCtrlFxp_t, ChrmVectorWeightBwd) == 24) ? 1 : -1];
typedef char check_TmCtrlFxp_t_BpWeight[(DM_OFFSETOF(TmCtrlFxp_t, BpWeight) == 28) ? 1 : -1];
typedef char check_TmCtrlFxp_t_TMinBias[(DM_OFFSETOF(TmCtrlFxp_t, TMinBias) == 32) ? 1 : -1];
typedef char check_TmCtrlFxp_t_TMaxBias[(DM_OFFSETOF(TmCtrlFxp_t, TMaxBias) == 34) ? 1 : -1];
typedef char check_TmCtrlFxp_t_TMidBias[(DM_OFFSETOF(TmCtrlFxp_t, TMidBias) == 36) ? 1 : -1];
typedef char check_TmCtrlFxp_t_DContrast[(DM_OFFSETOF(TmCtrlFxp_t, DContrast) == 38) ? 1 : -1];
typedef char check_TmCtrlFxp_t_DBrightness[(DM_OFFSETOF(TmCtrlFxp_t, DBrightness) == 40) ? 1 : -1];
typedef char check_TmCtrlFxp_t_l2off[(DM_OFFSETOF(TmCtrlFxp_t, l2off) == 44) ? 1 : -1];
typedef char check_TmCtrlFxp_t_Default2[(DM_OFFSETOF(TmCtrlFxp_t, Default2) == 48) ? 1 : -1];
typedef char check_TmCtrlFxp_t_ValueAdj2[(DM_OFFSETOF(TmCtrlFxp_t, ValueAdj2) == 76) ? 1 : -1];
typedef char check_TmCtrlFxp_t_CodeBias2[(DM_OFFSETOF(TmCtrlFxp_t, CodeBias2) == 104) ? 1 : -1];
typedef char check_TmCtrlFxp_t_ValueAdj2SatGainBwd
    [(DM_OFFSETOF(TmCtrlFxp_t, ValueAdj2SatGainBwd) == 132) ? 1 : -1];
typedef char check_TmCtrlFxp_t_Default3[(DM_OFFSETOF(TmCtrlFxp_t, Default3) == 136) ? 1 : -1];
typedef char check_TmCtrlFxp_t_CodeBias3[(DM_OFFSETOF(TmCtrlFxp_t, CodeBias3) == 152) ? 1 : -1];
typedef char check_size_TmCtrlFxp_t[(sizeof(TmCtrlFxp_t) == 168) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_RowNum[(DM_OFFSETOF(TgtSigEnvFxp_t, RowNum) == 0) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_ColNum[(DM_OFFSETOF(TgtSigEnvFxp_t, ColNum) == 4) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_RowPitch[(DM_OFFSETOF(TgtSigEnvFxp_t, RowPitch) == 8) ? 1 : -1];
typedef char
    check_TgtSigEnvFxp_t_RowPitchC[(DM_OFFSETOF(TgtSigEnvFxp_t, RowPitchC) == 12) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_Clr[(DM_OFFSETOF(TgtSigEnvFxp_t, Clr) == 16) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_Chrm[(DM_OFFSETOF(TgtSigEnvFxp_t, Chrm) == 20) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_Weav[(DM_OFFSETOF(TgtSigEnvFxp_t, Weav) == 24) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_Dtp[(DM_OFFSETOF(TgtSigEnvFxp_t, Dtp) == 28) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_Loc[(DM_OFFSETOF(TgtSigEnvFxp_t, Loc) == 32) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_Bdp[(DM_OFFSETOF(TgtSigEnvFxp_t, Bdp) == 36) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_Eotf[(DM_OFFSETOF(TgtSigEnvFxp_t, Eotf) == 40) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_Rng[(DM_OFFSETOF(TgtSigEnvFxp_t, Rng) == 44) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_RgbDef[(DM_OFFSETOF(TgtSigEnvFxp_t, RgbDef) == 48) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_WpExt[(DM_OFFSETOF(TgtSigEnvFxp_t, WpExt) == 52) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_V3Wp[(DM_OFFSETOF(TgtSigEnvFxp_t, V3Wp) == 56) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_WpScale[(DM_OFFSETOF(TgtSigEnvFxp_t, WpScale) == 68) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_Gamma[(DM_OFFSETOF(TgtSigEnvFxp_t, Gamma) == 70) ? 1 : -1];
typedef char
    check_TgtSigEnvFxp_t_YuvXferSpec[(DM_OFFSETOF(TgtSigEnvFxp_t, YuvXferSpec) == 72) ? 1 : -1];
typedef char
    check_TgtSigEnvFxp_t_Rgb2YuvExt[(DM_OFFSETOF(TgtSigEnvFxp_t, Rgb2YuvExt) == 76) ? 1 : -1];
typedef char
    check_TgtSigEnvFxp_t_M33Rgb2Yuv[(DM_OFFSETOF(TgtSigEnvFxp_t, M33Rgb2Yuv) == 80) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_M33Rgb2YuvScale2P
    [(DM_OFFSETOF(TgtSigEnvFxp_t, M33Rgb2YuvScale2P) == 100) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_Rgb2YuvOffExt[(DM_OFFSETOF(TgtSigEnvFxp_t, Rgb2YuvOffExt) == 104)
                                                    ? 1
                                                    : -1];
typedef char
    check_TgtSigEnvFxp_t_V3Rgb2YuvOff[(DM_OFFSETOF(TgtSigEnvFxp_t, V3Rgb2YuvOff) == 108) ? 1 : -1];
typedef char
    check_TgtSigEnvFxp_t_Lms2RgbRgbwExt[(DM_OFFSETOF(TgtSigEnvFxp_t, Lms2RgbRgbwExt) == 120) ? 1
                                                                                             : -1];
typedef char check_TgtSigEnvFxp_t_V8Rgbw[(DM_OFFSETOF(TgtSigEnvFxp_t, V8Rgbw) == 124) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_Lms2RgbM33Ext[(DM_OFFSETOF(TgtSigEnvFxp_t, Lms2RgbM33Ext) == 156)
                                                    ? 1
                                                    : -1];
typedef char
    check_TgtSigEnvFxp_t_M33Lms2Rgb[(DM_OFFSETOF(TgtSigEnvFxp_t, M33Lms2Rgb) == 160) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_M33Lms2RgbScale2P
    [(DM_OFFSETOF(TgtSigEnvFxp_t, M33Lms2RgbScale2P) == 180) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_Min[(DM_OFFSETOF(TgtSigEnvFxp_t, Min) == 184) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_Max[(DM_OFFSETOF(TgtSigEnvFxp_t, Max) == 188) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_MinPq[(DM_OFFSETOF(TgtSigEnvFxp_t, MinPq) == 192) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_MaxPq[(DM_OFFSETOF(TgtSigEnvFxp_t, MaxPq) == 194) ? 1 : -1];
typedef char check_TgtSigEnvFxp_t_DiagSize[(DM_OFFSETOF(TgtSigEnvFxp_t, DiagSize) == 196) ? 1 : -1];
typedef char
    check_TgtSigEnvFxp_t_CrossTalk[(DM_OFFSETOF(TgtSigEnvFxp_t, CrossTalk) == 200) ? 1 : -1];
typedef char check_size_TgtSigEnvFxp_t[(sizeof(TgtSigEnvFxp_t) == 204) ? 1 : -1];
typedef char check_MmgReq_t_size[(DM_OFFSETOF(MmgReq_t, size) == 0) ? 1 : -1];
typedef char check_MmgReq_t_align[(DM_OFFSETOF(MmgReq_t, align) == 4) ? 1 : -1];
typedef char check_MmgReq_t_inDev[(DM_OFFSETOF(MmgReq_t, inDev) == 8) ? 1 : -1];
typedef char check_MmgReq_t_pCbParam[(DM_OFFSETOF(MmgReq_t, pCbParam) == 16) ? 1 : -1];
typedef char check_MmgReq_t_pCbFun[(DM_OFFSETOF(MmgReq_t, pCbFun) == 24) ? 1 : -1];
typedef char check_size_MmgReq_t[(sizeof(MmgReq_t) == 32) ? 1 : -1];
typedef char check_Mmg_t__mmgReqs[(DM_OFFSETOF(Mmg_t_, mmgReqs) == 0) ? 1 : -1];
typedef char check_Mmg_t__mmgBufs[(DM_OFFSETOF(Mmg_t_, mmgBufs) == 1024) ? 1 : -1];
typedef char check_Mmg_t__reqNum[(DM_OFFSETOF(Mmg_t_, reqNum) == 1280) ? 1 : -1];
typedef char check_Mmg_t__dmCtxtSize[(DM_OFFSETOF(Mmg_t_, dmCtxtSize) == 1284) ? 1 : -1];
typedef char check_Mmg_t__mdsExtSize[(DM_OFFSETOF(Mmg_t_, mdsExtSize) == 1288) ? 1 : -1];
typedef char check_Mmg_t__gmLutSize[(DM_OFFSETOF(Mmg_t_, gmLutSize) == 1292) ? 1 : -1];
typedef char check_Mmg_t__pq2gLutSize[(DM_OFFSETOF(Mmg_t_, pq2gLutSize) == 1296) ? 1 : -1];
typedef char check_Mmg_t__dmKsSize[(DM_OFFSETOF(Mmg_t_, dmKsSize) == 1300) ? 1 : -1];
typedef char check_size_Mmg_t_[(sizeof(Mmg_t_) == 1304) ? 1 : -1];
typedef char check_DmCfgFxp_t_dmCtrl[(DM_OFFSETOF(DmCfgFxp_t, dmCtrl) == 0) ? 1 : -1];
typedef char check_DmCfgFxp_t_grcSigEnv[(DM_OFFSETOF(DmCfgFxp_t, grcSigEnv) == 104) ? 1 : -1];
typedef char check_DmCfgFxp_t_alpSigEnv[(DM_OFFSETOF(DmCfgFxp_t, alpSigEnv) == 332) ? 1 : -1];
typedef char check_DmCfgFxp_t_srcSigEnv[(DM_OFFSETOF(DmCfgFxp_t, srcSigEnv) == 372) ? 1 : -1];
typedef char check_DmCfgFxp_t_tmCtrl[(DM_OFFSETOF(DmCfgFxp_t, tmCtrl) == 600) ? 1 : -1];
typedef char check_DmCfgFxp_t_tgtSigEnv[(DM_OFFSETOF(DmCfgFxp_t, tgtSigEnv) == 768) ? 1 : -1];
typedef char check_DmCfgFxp_t_hMmg[(DM_OFFSETOF(DmCfgFxp_t, hMmg) == 976) ? 1 : -1];
typedef char check_size_DmCfgFxp_t[(sizeof(DmCfgFxp_t) == 984) ? 1 : -1];
typedef char check_CscCtrlFxp_t_sM33Rgb2Lms[(DM_OFFSETOF(CscCtrlFxp_t, sM33Rgb2Lms) == 0) ? 1 : -1];
typedef char check_CscCtrlFxp_t_sM33Rgb2LmsScale2P
    [(DM_OFFSETOF(CscCtrlFxp_t, sM33Rgb2LmsScale2P) == 20) ? 1 : -1];
typedef char check_CscCtrlFxp_t_sV3Wp[(DM_OFFSETOF(CscCtrlFxp_t, sV3Wp) == 24) ? 1 : -1];
typedef char
    check_CscCtrlFxp_t_sV3WpScale2P[(DM_OFFSETOF(CscCtrlFxp_t, sV3WpScale2P) == 36) ? 1 : -1];
typedef char
    check_CscCtrlFxp_t_gM33Rgb2Lms[(DM_OFFSETOF(CscCtrlFxp_t, gM33Rgb2Lms) == 40) ? 1 : -1];
typedef char check_CscCtrlFxp_t_gM33Rgb2LmsScale2P
    [(DM_OFFSETOF(CscCtrlFxp_t, gM33Rgb2LmsScale2P) == 60) ? 1 : -1];
typedef char check_CscCtrlFxp_t_gV3Wp[(DM_OFFSETOF(CscCtrlFxp_t, gV3Wp) == 64) ? 1 : -1];
typedef char
    check_CscCtrlFxp_t_gV3WpScale2P[(DM_OFFSETOF(CscCtrlFxp_t, gV3WpScale2P) == 76) ? 1 : -1];
typedef char
    check_CscCtrlFxp_t_tM33Lms2Rgb[(DM_OFFSETOF(CscCtrlFxp_t, tM33Lms2Rgb) == 80) ? 1 : -1];
typedef char check_CscCtrlFxp_t_tM33Lms2RgbScale2P
    [(DM_OFFSETOF(CscCtrlFxp_t, tM33Lms2RgbScale2P) == 100) ? 1 : -1];
typedef char check_CscCtrlFxp_t_tV3Wp[(DM_OFFSETOF(CscCtrlFxp_t, tV3Wp) == 104) ? 1 : -1];
typedef char
    check_CscCtrlFxp_t_tV3WpScale2P[(DM_OFFSETOF(CscCtrlFxp_t, tV3WpScale2P) == 116) ? 1 : -1];
typedef char check_CscCtrlFxp_t_m33Lms2Ipt[(DM_OFFSETOF(CscCtrlFxp_t, m33Lms2Ipt) == 120) ? 1 : -1];
typedef char check_CscCtrlFxp_t_m33Lms2IptScale2P
    [(DM_OFFSETOF(CscCtrlFxp_t, m33Lms2IptScale2P) == 140) ? 1 : -1];
typedef char check_CscCtrlFxp_t_m33Ipt2Lms[(DM_OFFSETOF(CscCtrlFxp_t, m33Ipt2Lms) == 144) ? 1 : -1];
typedef char check_CscCtrlFxp_t_m33Ipt2LmsScale2P
    [(DM_OFFSETOF(CscCtrlFxp_t, m33Ipt2LmsScale2P) == 164) ? 1 : -1];
typedef char check_size_CscCtrlFxp_t[(sizeof(CscCtrlFxp_t) == 168) ? 1 : -1];
typedef char check_TcSigFxp_t_minPq[(DM_OFFSETOF(TcSigFxp_t, minPq) == 0) ? 1 : -1];
typedef char check_TcSigFxp_t_maxPq[(DM_OFFSETOF(TcSigFxp_t, maxPq) == 2) ? 1 : -1];
typedef char check_TcSigFxp_t_diagSize[(DM_OFFSETOF(TcSigFxp_t, diagSize) == 4) ? 1 : -1];
typedef char check_TcSigFxp_t_hlgA[(DM_OFFSETOF(TcSigFxp_t, hlgA) == 6) ? 1 : -1];
typedef char check_TcSigFxp_t_hlgGamma[(DM_OFFSETOF(TcSigFxp_t, hlgGamma) == 8) ? 1 : -1];
typedef char check_TcSigFxp_t_min[(DM_OFFSETOF(TcSigFxp_t, min) == 12) ? 1 : -1];
typedef char check_TcSigFxp_t_rng[(DM_OFFSETOF(TcSigFxp_t, rng) == 16) ? 1 : -1];
typedef char check_TcSigFxp_t_crush[(DM_OFFSETOF(TcSigFxp_t, crush) == 20) ? 1 : -1];
typedef char check_TcSigFxp_t_clip[(DM_OFFSETOF(TcSigFxp_t, clip) == 22) ? 1 : -1];
typedef char check_TcSigFxp_t_mid[(DM_OFFSETOF(TcSigFxp_t, mid) == 24) ? 1 : -1];
typedef char check_size_TcSigFxp_t[(sizeof(TcSigFxp_t) == 28) ? 1 : -1];
typedef char check_TcCtrlFxp_t_tcSrcSig[(DM_OFFSETOF(TcCtrlFxp_t, tcSrcSig) == 0) ? 1 : -1];
typedef char check_TcCtrlFxp_t_tcTgtSig[(DM_OFFSETOF(TcCtrlFxp_t, tcTgtSig) == 28) ? 1 : -1];
typedef char check_TcCtrlFxp_t_tcLutDir[(DM_OFFSETOF(TcCtrlFxp_t, tcLutDir) == 56) ? 1 : -1];
typedef char
    check_TcCtrlFxp_t_bwdInvertible[(DM_OFFSETOF(TcCtrlFxp_t, bwdInvertible) == 60) ? 1 : -1];
typedef char check_TcCtrlFxp_t_rolloff[(DM_OFFSETOF(TcCtrlFxp_t, rolloff) == 64) ? 1 : -1];
typedef char check_TcCtrlFxp_t_rolloffInv[(DM_OFFSETOF(TcCtrlFxp_t, rolloffInv) == 68) ? 1 : -1];
typedef char check_TcCtrlFxp_t_keyWeight[(DM_OFFSETOF(TcCtrlFxp_t, keyWeight) == 72) ? 1 : -1];
typedef char check_TcCtrlFxp_t_intensityVectorWeight
    [(DM_OFFSETOF(TcCtrlFxp_t, intensityVectorWeight) == 76) ? 1 : -1];
typedef char
    check_TcCtrlFxp_t_chrmVectorWeight[(DM_OFFSETOF(TcCtrlFxp_t, chrmVectorWeight) == 80) ? 1 : -1];
typedef char check_TcCtrlFxp_t_bpWeight[(DM_OFFSETOF(TcCtrlFxp_t, bpWeight) == 84) ? 1 : -1];
typedef char check_TcCtrlFxp_t_ratio[(DM_OFFSETOF(TcCtrlFxp_t, ratio) == 88) ? 1 : -1];
typedef char check_TcCtrlFxp_t_gsMin[(DM_OFFSETOF(TcCtrlFxp_t, gsMin) == 92) ? 1 : -1];
typedef char check_TcCtrlFxp_t_gsMax[(DM_OFFSETOF(TcCtrlFxp_t, gsMax) == 96) ? 1 : -1];
typedef char check_TcCtrlFxp_t_gsGamma[(DM_OFFSETOF(TcCtrlFxp_t, gsGamma) == 100) ? 1 : -1];
typedef char check_TcCtrlFxp_t_tMinBias[(DM_OFFSETOF(TcCtrlFxp_t, tMinBias) == 104) ? 1 : -1];
typedef char check_TcCtrlFxp_t_tMaxBias[(DM_OFFSETOF(TcCtrlFxp_t, tMaxBias) == 106) ? 1 : -1];
typedef char check_TcCtrlFxp_t_tMidBias[(DM_OFFSETOF(TcCtrlFxp_t, tMidBias) == 108) ? 1 : -1];
typedef char check_TcCtrlFxp_t_dContrast[(DM_OFFSETOF(TcCtrlFxp_t, dContrast) == 110) ? 1 : -1];
typedef char check_TcCtrlFxp_t_dBrightness[(DM_OFFSETOF(TcCtrlFxp_t, dBrightness) == 112) ? 1 : -1];
typedef char check_TcCtrlFxp_t_level2[(DM_OFFSETOF(TcCtrlFxp_t, level2) == 116) ? 1 : -1];
typedef char check_TcCtrlFxp_t_level3[(DM_OFFSETOF(TcCtrlFxp_t, level3) == 144) ? 1 : -1];
typedef char check_TcCtrlFxp_t_tCrush[(DM_OFFSETOF(TcCtrlFxp_t, tCrush) == 160) ? 1 : -1];
typedef char check_TcCtrlFxp_t_tClip[(DM_OFFSETOF(TcCtrlFxp_t, tClip) == 162) ? 1 : -1];
typedef char check_TcCtrlFxp_t_tMid[(DM_OFFSETOF(TcCtrlFxp_t, tMid) == 164) ? 1 : -1];
typedef char check_size_TcCtrlFxp_t[(sizeof(TcCtrlFxp_t) == 168) ? 1 : -1];
typedef char check_DmKsUsFxp_t_chrmIn[(DM_OFFSETOF(DmKsUsFxp_t, chrmIn) == 0) ? 1 : -1];
typedef char check_DmKsUsFxp_t_minUs[(DM_OFFSETOF(DmKsUsFxp_t, minUs) == 2) ? 1 : -1];
typedef char check_DmKsUsFxp_t_maxUs[(DM_OFFSETOF(DmKsUsFxp_t, maxUs) == 4) ? 1 : -1];
typedef char check_DmKsUsFxp_t_filterUvRowUsHalfSize
    [(DM_OFFSETOF(DmKsUsFxp_t, filterUvRowUsHalfSize) == 6) ? 1 : -1];
typedef char check_DmKsUsFxp_t_filterUvRowUsScale2P
    [(DM_OFFSETOF(DmKsUsFxp_t, filterUvRowUsScale2P) == 8) ? 1 : -1];
typedef char
    check_DmKsUsFxp_t_filterUvRowUs0_m[(DM_OFFSETOF(DmKsUsFxp_t, filterUvRowUs0_m) == 10) ? 1 : -1];
typedef char
    check_DmKsUsFxp_t_filterUvRowUs1_m[(DM_OFFSETOF(DmKsUsFxp_t, filterUvRowUs1_m) == 26) ? 1 : -1];
typedef char check_DmKsUsFxp_t_filterUvColUsHalfSize
    [(DM_OFFSETOF(DmKsUsFxp_t, filterUvColUsHalfSize) == 42) ? 1 : -1];
typedef char check_DmKsUsFxp_t_filterUvColUsScale2P
    [(DM_OFFSETOF(DmKsUsFxp_t, filterUvColUsScale2P) == 44) ? 1 : -1];
typedef char
    check_DmKsUsFxp_t_filterUvColUs_m[(DM_OFFSETOF(DmKsUsFxp_t, filterUvColUs_m) == 46) ? 1 : -1];
typedef char check_size_DmKsUsFxp_t[(sizeof(DmKsUsFxp_t) == 62) ? 1 : -1];
typedef char check_DmKsDsFxp_t_chrmOut[(DM_OFFSETOF(DmKsDsFxp_t, chrmOut) == 0) ? 1 : -1];
typedef char check_DmKsDsFxp_t_minDs[(DM_OFFSETOF(DmKsDsFxp_t, minDs) == 2) ? 1 : -1];
typedef char check_DmKsDsFxp_t_maxDs[(DM_OFFSETOF(DmKsDsFxp_t, maxDs) == 4) ? 1 : -1];
typedef char check_DmKsDsFxp_t_filterUvColDsRadius
    [(DM_OFFSETOF(DmKsDsFxp_t, filterUvColDsRadius) == 6) ? 1 : -1];
typedef char check_DmKsDsFxp_t_filterUvColDsScale2P
    [(DM_OFFSETOF(DmKsDsFxp_t, filterUvColDsScale2P) == 8) ? 1 : -1];
typedef char
    check_DmKsDsFxp_t_filterUvColDs_m[(DM_OFFSETOF(DmKsDsFxp_t, filterUvColDs_m) == 10) ? 1 : -1];
typedef char check_size_DmKsDsFxp_t[(sizeof(DmKsDsFxp_t) == 42) ? 1 : -1];
typedef char check_DmKsCtrl_t_mainIn[(DM_OFFSETOF(DmKsCtrl_t, mainIn) == 0) ? 1 : -1];
typedef char check_DmKsCtrl_t_secIn[(DM_OFFSETOF(DmKsCtrl_t, secIn) == 4) ? 1 : -1];
typedef char check_DmKsCtrl_t_prf[(DM_OFFSETOF(DmKsCtrl_t, prf) == 8) ? 1 : -1];
typedef char check_size_DmKsCtrl_t[(sizeof(DmKsCtrl_t) == 12) ? 1 : -1];
typedef char check_DmKsFrmFmt_t_rowNum[(DM_OFFSETOF(DmKsFrmFmt_t, rowNum) == 0) ? 1 : -1];
typedef char check_DmKsFrmFmt_t_colNum[(DM_OFFSETOF(DmKsFrmFmt_t, colNum) == 4) ? 1 : -1];
typedef char check_DmKsFrmFmt_t_dtp[(DM_OFFSETOF(DmKsFrmFmt_t, dtp) == 8) ? 1 : -1];
typedef char check_DmKsFrmFmt_t_weav[(DM_OFFSETOF(DmKsFrmFmt_t, weav) == 9) ? 1 : -1];
typedef char check_DmKsFrmFmt_t_loc[(DM_OFFSETOF(DmKsFrmFmt_t, loc) == 10) ? 1 : -1];
typedef char check_DmKsFrmFmt_t_bdp[(DM_OFFSETOF(DmKsFrmFmt_t, bdp) == 11) ? 1 : -1];
typedef char check_DmKsFrmFmt_t_chrm[(DM_OFFSETOF(DmKsFrmFmt_t, chrm) == 12) ? 1 : -1];
typedef char check_DmKsFrmFmt_t_clr[(DM_OFFSETOF(DmKsFrmFmt_t, clr) == 13) ? 1 : -1];
typedef char check_DmKsFrmFmt_t_rowPitch[(DM_OFFSETOF(DmKsFrmFmt_t, rowPitch) == 16) ? 1 : -1];
typedef char check_DmKsFrmFmt_t_colPitch[(DM_OFFSETOF(DmKsFrmFmt_t, colPitch) == 20) ? 1 : -1];
typedef char check_DmKsFrmFmt_t_rowPitchC[(DM_OFFSETOF(DmKsFrmFmt_t, rowPitchC) == 24) ? 1 : -1];
typedef char check_size_DmKsFrmFmt_t[(sizeof(DmKsFrmFmt_t) == 28) ? 1 : -1];
typedef char check_EotfParamFxp_t_rangeMin[(DM_OFFSETOF(EotfParamFxp_t, rangeMin) == 0) ? 1 : -1];
typedef char check_EotfParamFxp_t_range[(DM_OFFSETOF(EotfParamFxp_t, range) == 2) ? 1 : -1];
typedef char check_EotfParamFxp_t_rangeInv[(DM_OFFSETOF(EotfParamFxp_t, rangeInv) == 4) ? 1 : -1];
typedef char check_EotfParamFxp_t_bdp[(DM_OFFSETOF(EotfParamFxp_t, bdp) == 8) ? 1 : -1];
typedef char check_EotfParamFxp_t_eotf[(DM_OFFSETOF(EotfParamFxp_t, eotf) == 10) ? 1 : -1];
typedef char check_size_EotfParamFxp_t[(sizeof(EotfParamFxp_t) == 12) ? 1 : -1];
typedef char check_DmKsIMapFxp_t_clr[(DM_OFFSETOF(DmKsIMapFxp_t, clr) == 0) ? 1 : -1];
typedef char check_DmKsIMapFxp_t_m33Yuv2RgbScale2P
    [(DM_OFFSETOF(DmKsIMapFxp_t, m33Yuv2RgbScale2P) == 2) ? 1 : -1];
typedef char check_DmKsIMapFxp_t_m33Yuv2Rgb[(DM_OFFSETOF(DmKsIMapFxp_t, m33Yuv2Rgb) == 4) ? 1 : -1];
typedef char check_DmKsIMapFxp_t_v3Yuv2RgbOffInRgb
    [(DM_OFFSETOF(DmKsIMapFxp_t, v3Yuv2RgbOffInRgb) == 24) ? 1 : -1];
typedef char check_DmKsIMapFxp_t_eotfParam[(DM_OFFSETOF(DmKsIMapFxp_t, eotfParam) == 36) ? 1 : -1];
typedef char check_DmKsIMapFxp_t_g2L[(DM_OFFSETOF(DmKsIMapFxp_t, g2L) == 48) ? 1 : -1];
typedef char check_DmKsIMapFxp_t_m33Rgb2LmsScale2P
    [(DM_OFFSETOF(DmKsIMapFxp_t, m33Rgb2LmsScale2P) == 1072) ? 1 : -1];
typedef char
    check_DmKsIMapFxp_t_m33Rgb2Lms[(DM_OFFSETOF(DmKsIMapFxp_t, m33Rgb2Lms) == 1074) ? 1 : -1];
typedef char check_DmKsIMapFxp_t_m33Lms2IptScale2P
    [(DM_OFFSETOF(DmKsIMapFxp_t, m33Lms2IptScale2P) == 1092) ? 1 : -1];
typedef char
    check_DmKsIMapFxp_t_m33Lms2Ipt[(DM_OFFSETOF(DmKsIMapFxp_t, m33Lms2Ipt) == 1094) ? 1 : -1];
typedef char check_DmKsIMapFxp_t_iptScale[(DM_OFFSETOF(DmKsIMapFxp_t, iptScale) == 1112) ? 1 : -1];
typedef char check_DmKsIMapFxp_t_v3IptOff[(DM_OFFSETOF(DmKsIMapFxp_t, v3IptOff) == 1116) ? 1 : -1];
typedef char check_size_DmKsIMapFxp_t[(sizeof(DmKsIMapFxp_t) == 1128) ? 1 : -1];
typedef char
    check_DmKsTMapFxp_t_tmLutISizeM1[(DM_OFFSETOF(DmKsTMapFxp_t, tmLutISizeM1) == 0) ? 1 : -1];
typedef char
    check_DmKsTMapFxp_t_tmLutSSizeM1[(DM_OFFSETOF(DmKsTMapFxp_t, tmLutSSizeM1) == 2) ? 1 : -1];
typedef char check_DmKsTMapFxp_t_tmLutI[(DM_OFFSETOF(DmKsTMapFxp_t, tmLutI) == 4) ? 1 : -1];
typedef char check_DmKsTMapFxp_t_tmLutS[(DM_OFFSETOF(DmKsTMapFxp_t, tmLutS) == 1028) ? 1 : -1];
typedef char
    check_DmKsTMapFxp_t_smLutISizeM1[(DM_OFFSETOF(DmKsTMapFxp_t, smLutISizeM1) == 2052) ? 1 : -1];
typedef char
    check_DmKsTMapFxp_t_smLutSSizeM1[(DM_OFFSETOF(DmKsTMapFxp_t, smLutSSizeM1) == 2054) ? 1 : -1];
typedef char check_DmKsTMapFxp_t_smLutI[(DM_OFFSETOF(DmKsTMapFxp_t, smLutI) == 2056) ? 1 : -1];
typedef char check_DmKsTMapFxp_t_smLutS[(DM_OFFSETOF(DmKsTMapFxp_t, smLutS) == 3080) ? 1 : -1];
typedef char check_size_DmKsTMapFxp_t[(sizeof(DmKsTMapFxp_t) == 4104) ? 1 : -1];
typedef char check_OetfParamFxp_t_min[(DM_OFFSETOF(OetfParamFxp_t, min) == 0) ? 1 : -1];
typedef char check_OetfParamFxp_t_max[(DM_OFFSETOF(OetfParamFxp_t, max) == 4) ? 1 : -1];
typedef char check_OetfParamFxp_t_rangeMin[(DM_OFFSETOF(OetfParamFxp_t, rangeMin) == 8) ? 1 : -1];
typedef char check_OetfParamFxp_t_range[(DM_OFFSETOF(OetfParamFxp_t, range) == 10) ? 1 : -1];
typedef char
    check_OetfParamFxp_t_rangeOverOne[(DM_OFFSETOF(OetfParamFxp_t, rangeOverOne) == 12) ? 1 : -1];
typedef char check_OetfParamFxp_t_bdp[(DM_OFFSETOF(OetfParamFxp_t, bdp) == 16) ? 1 : -1];
typedef char check_OetfParamFxp_t_oetf[(DM_OFFSETOF(OetfParamFxp_t, oetf) == 18) ? 1 : -1];
typedef char check_size_OetfParamFxp_t[(sizeof(OetfParamFxp_t) == 20) ? 1 : -1];
typedef char check_Dm3KsOMapFxp_t_m33Ipt2LmsScale2P
    [(DM_OFFSETOF(Dm3KsOMapFxp_t, m33Ipt2LmsScale2P) == 0) ? 1 : -1];
typedef char
    check_Dm3KsOMapFxp_t_m33Ipt2Lms[(DM_OFFSETOF(Dm3KsOMapFxp_t, m33Ipt2Lms) == 2) ? 1 : -1];
typedef char check_Dm3KsOMapFxp_t_m33Lms2RgbScale2P
    [(DM_OFFSETOF(Dm3KsOMapFxp_t, m33Lms2RgbScale2P) == 20) ? 1 : -1];
typedef char
    check_Dm3KsOMapFxp_t_m33Lms2Rgb[(DM_OFFSETOF(Dm3KsOMapFxp_t, m33Lms2Rgb) == 22) ? 1 : -1];
typedef char
    check_Dm3KsOMapFxp_t_oetfParam[(DM_OFFSETOF(Dm3KsOMapFxp_t, oetfParam) == 40) ? 1 : -1];
typedef char check_Dm3KsOMapFxp_t_clr[(DM_OFFSETOF(Dm3KsOMapFxp_t, clr) == 60) ? 1 : -1];
typedef char check_Dm3KsOMapFxp_t_m33Rgb2YuvScale2P
    [(DM_OFFSETOF(Dm3KsOMapFxp_t, m33Rgb2YuvScale2P) == 62) ? 1 : -1];
typedef char
    check_Dm3KsOMapFxp_t_m33Rgb2Yuv[(DM_OFFSETOF(Dm3KsOMapFxp_t, m33Rgb2Yuv) == 64) ? 1 : -1];
typedef char
    check_Dm3KsOMapFxp_t_v3Rgb2YuvOff[(DM_OFFSETOF(Dm3KsOMapFxp_t, v3Rgb2YuvOff) == 84) ? 1 : -1];
typedef char check_Dm3KsOMapFxp_t_iptScale[(DM_OFFSETOF(Dm3KsOMapFxp_t, iptScale) == 96) ? 1 : -1];
typedef char check_Dm3KsOMapFxp_t_v3IptOff[(DM_OFFSETOF(Dm3KsOMapFxp_t, v3IptOff) == 100) ? 1 : -1];
typedef char check_size_Dm3KsOMapFxp_t[(sizeof(Dm3KsOMapFxp_t) == 112) ? 1 : -1];
typedef char check_DmKsFxp_t_rowPitchNum[(DM_OFFSETOF(DmKsFxp_t, rowPitchNum) == 0) ? 1 : -1];
typedef char check_DmKsFxp_t_frmBuf0[(DM_OFFSETOF(DmKsFxp_t, frmBuf0) == 8) ? 1 : -1];
typedef char check_DmKsFxp_t_frmBuf1[(DM_OFFSETOF(DmKsFxp_t, frmBuf1) == 16) ? 1 : -1];
typedef char check_DmKsFxp_t_frmBuf2[(DM_OFFSETOF(DmKsFxp_t, frmBuf2) == 24) ? 1 : -1];
typedef char check_DmKsFxp_t_frmBuf0G[(DM_OFFSETOF(DmKsFxp_t, frmBuf0G) == 32) ? 1 : -1];
typedef char check_DmKsFxp_t_frmBuf1G[(DM_OFFSETOF(DmKsFxp_t, frmBuf1G) == 40) ? 1 : -1];
typedef char check_DmKsFxp_t_frmBuf2G[(DM_OFFSETOF(DmKsFxp_t, frmBuf2G) == 48) ? 1 : -1];
typedef char check_DmKsFxp_t_ksUs[(DM_OFFSETOF(DmKsFxp_t, ksUs) == 56) ? 1 : -1];
typedef char check_DmKsFxp_t_ksDs[(DM_OFFSETOF(DmKsFxp_t, ksDs) == 118) ? 1 : -1];
typedef char check_DmKsFxp_t_ksDmCtrl[(DM_OFFSETOF(DmKsFxp_t, ksDmCtrl) == 160) ? 1 : -1];
typedef char check_DmKsFxp_t_ksFrmFmtI[(DM_OFFSETOF(DmKsFxp_t, ksFrmFmtI) == 172) ? 1 : -1];
typedef char check_DmKsFxp_t_ksIMap[(DM_OFFSETOF(DmKsFxp_t, ksIMap) == 200) ? 1 : -1];
typedef char check_DmKsFxp_t_ksTMap[(DM_OFFSETOF(DmKsFxp_t, ksTMap) == 1328) ? 1 : -1];
typedef char check_DmKsFxp_t_ksUsG[(DM_OFFSETOF(DmKsFxp_t, ksUsG) == 5432) ? 1 : -1];
typedef char check_DmKsFxp_t_ksFrmFmtA[(DM_OFFSETOF(DmKsFxp_t, ksFrmFmtA) == 5496) ? 1 : -1];
typedef char check_DmKsFxp_t_rangeInvA[(DM_OFFSETOF(DmKsFxp_t, rangeInvA) == 5524) ? 1 : -1];
typedef char check_DmKsFxp_t_ksFrmFmtG[(DM_OFFSETOF(DmKsFxp_t, ksFrmFmtG) == 5528) ? 1 : -1];
typedef char check_DmKsFxp_t_ksIMapG[(DM_OFFSETOF(DmKsFxp_t, ksIMapG) == 5556) ? 1 : -1];
typedef char check_DmKsFxp_t_ksTMapG[(DM_OFFSETOF(DmKsFxp_t, ksTMapG) == 6684) ? 1 : -1];
typedef char check_DmKsFxp_t_ksOMap[(DM_OFFSETOF(DmKsFxp_t, ksOMap) == 10788) ? 1 : -1];
typedef char check_DmKsFxp_t_ksFrmFmtO[(DM_OFFSETOF(DmKsFxp_t, ksFrmFmtO) == 10900) ? 1 : -1];
typedef char check_size_DmKsFxp_t[(sizeof(DmKsFxp_t) == 10928) ? 1 : -1];
typedef char
    check_DmCtxtFxp_t_dmCfgToBeValid[(DM_OFFSETOF(DmCtxtFxp_t, dmCfgToBeValid) == 0) ? 1 : -1];
typedef char
    check_DmCtxtFxp_t_dmCfgActValid[(DM_OFFSETOF(DmCtxtFxp_t, dmCfgActValid) == 4) ? 1 : -1];
typedef char check_DmCtxtFxp_t_dmCfg[(DM_OFFSETOF(DmCtxtFxp_t, dmCfg) == 8) ? 1 : -1];
typedef char check_DmCtxtFxp_t_pDmCfgAct[(DM_OFFSETOF(DmCtxtFxp_t, pDmCfgAct) == 1976) ? 1 : -1];
typedef char check_DmCtxtFxp_t_pDmCfgToBe[(DM_OFFSETOF(DmCtxtFxp_t, pDmCfgToBe) == 1984) ? 1 : -1];
typedef char
    check_DmCtxtFxp_t_mdsExtFrcCmmt[(DM_OFFSETOF(DmCtxtFxp_t, mdsExtFrcCmmt) == 1992) ? 1 : -1];
typedef char
    check_DmCtxtFxp_t_mdsExtActValid[(DM_OFFSETOF(DmCtxtFxp_t, mdsExtActValid) == 1996) ? 1 : -1];
typedef char check_DmCtxtFxp_t_mdsExtAct[(DM_OFFSETOF(DmCtxtFxp_t, mdsExtAct) == 2000) ? 1 : -1];
typedef char check_DmCtxtFxp_t_cscCtrl[(DM_OFFSETOF(DmCtxtFxp_t, cscCtrl) == 2520) ? 1 : -1];
typedef char check_DmCtxtFxp_t_tcCtrl[(DM_OFFSETOF(DmCtxtFxp_t, tcCtrl) == 2688) ? 1 : -1];
typedef char check_DmCtxtFxp_t_tcCtrlUsed[(DM_OFFSETOF(DmCtxtFxp_t, tcCtrlUsed) == 2856) ? 1 : -1];
typedef char check_DmCtxtFxp_t_tc2Ctrl[(DM_OFFSETOF(DmCtxtFxp_t, tc2Ctrl) == 3024) ? 1 : -1];
typedef char
    check_DmCtxtFxp_t_tc2CtrlUsed[(DM_OFFSETOF(DmCtxtFxp_t, tc2CtrlUsed) == 3192) ? 1 : -1];
typedef char check_DmCtxtFxp_t_l2gLnLut[(DM_OFFSETOF(DmCtxtFxp_t, l2gLnLut) == 3360) ? 1 : -1];
typedef char check_DmCtxtFxp_t_l2gGmLut[(DM_OFFSETOF(DmCtxtFxp_t, l2gGmLut) == 3876) ? 1 : -1];
typedef char check_DmCtxtFxp_t_hDmKs[(DM_OFFSETOF(DmCtxtFxp_t, hDmKs) == 4392) ? 1 : -1];
typedef char check_size_DmCtxtFxp_t[(sizeof(DmCtxtFxp_t) == 4400) ? 1 : -1];

#define CPlatformCpu (0u)
#define CPlatformCuda (1u)
#define CPlatformOpenCl (2u)
#define CPlatformNum (3u)
#define CClrYuv (0u)
#define CClrRgb (1u)
#define CClrRgba (2u)
#define CClrIpt (3u)
#define CClrGrey (4u)
#define CClrNum (5u)
#define CChrm420 (0u)
#define CChrm422 (1u)
#define CChrm444 (2u)
#define CChrmNum (3u)
#define CDtpU16 (0u)
#define CDtpU8 (1u)
#define CDtpF32 (2u)
#define CDtpNum (3u)
#define CLocHost (0u)
#define CLocDev (1u)
#define CLocNum (2u)
#define CWeavPlnr (0u)
#define CWeavIntl (1u)
#define CWeavUyVy (2u)
#define CWeavNum (3u)
#define CEotfBt1886 (0u)
#define CEotfPq (1u)
#define CEotfPower (2u)
#define CEotfHlg (3u)
#define CEotfNum (4u)
#define CRngNarrow (0u)
#define CRngFull (1u)
#define CRngSdi (2u)
#define CRngNum (3u)
#define CRgbDefP3d65 (0u)
#define CRgbDefDci (1u)
#define CRgbDefR709 (2u)
#define CRgbDefR2020 (3u)
#define CRgbDefAces (4u)
#define CRgbDefAlexa (5u)
#define CRgbDefR601 (6u)
#define CRgbDefNum (7u)
#define CYuvXferSpecP3d65 (0u)
#define CYuvXferSpecDci (1u)
#define CYuvXferSpecR709 (2u)
#define CYuvXferSpecR2020 (3u)
#define CYuvXferSpecAces (4u)
#define CYuvXferSpecAlexa (5u)
#define CYuvXferSpecR601 (6u)
#define CYuvXferSpecNum (7u)
#define CStbModeGpri (0u)
#define CStbModeVpri (1u)
#define CStbModeNum (2u)
#define TrimTypeTMaxPq2 (0u)
#define TrimTypeSlope (1u)
#define TrimTypeOffset (2u)
#define TrimTypePower (3u)
#define TrimTypeChromaWeight (4u)
#define TrimTypeSatGain (5u)
#define TrimTypeMsWeight (6u)
#define TrimTypeNum2 (6u)
#define CTcLutDirFwd (0u)
#define CTcLutDirBwd (1u)
#define CTcLutDirPass (2u)
#define CTcLutDirNum (3u)
#define DM_NORMAL_TC (0u)
#define DM_INVERSE_TC (1u)
#define DM_PASSTHROUGH_TC (2u)
#define DM_SDRX_TC (3u)
#define MDS_CHG_NONE (0u)
#define MDS_CHG_TC (1u)
#define MDS_CHG_CFG (2u)
#define DM_METADATA_COMPARE_BYTES 520
#define DM_METADATA_CONFIG_COMPARE_BYTES 514

#endif
