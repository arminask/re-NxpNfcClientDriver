#pragma once
#include "phNciNfc.h"
#include "phNciNfc_Common.h"
#include "phNciNfc_Sequence.h"

// Private C types restored from exact-match PDB; see NXP-NCI-TYPES.json.

typedef struct phNciNfc_RfConfigPropInfo {
    uint32_t SystemConfigParams : 1;
    uint32_t SwpConfigParams : 1;
    uint32_t NxpExtnRfDiscConfig : 1;
    uint32_t PropConfig : 1;
} phNciNfc_RfConfigPropInfo_t;

typedef enum phNciNfc_SysClkSrc {
    phNciNfc_InvalidClkSrc = 0,
    phNciNfc_Xtal = 1,
    phNciNfc_Pll = 2,
    phNciNfc_PadDirect = 3
} phNciNfc_SysClkSrc_t;

typedef enum phNciNfc_SysClkFreq {
    phNciNfc_13MHz = 0,
    phNciNfc_19_2MHz = 1,
    phNciNfc_24MHz = 2,
    phNciNfc_26MHz = 3,
    phNciNfc_38_4MHz = 4,
    phNciNfc_52MHz = 5,
    phNciNfc_InvalidClkFreq = 6
} phNciNfc_SysClkFreq_t;

typedef enum phNciNfc_eSeIntfSel {
    phNciNfc_eSeSwp2Intf = 0,
    phNciNfc_eSeDwpIntf = 2
} phNciNfc_eSeIntfSel_t;

typedef struct phNciNfc_SysConfigParams {
    union {
        struct {
            uint32_t HostIfCharCfg : 1;
            uint32_t SysNtfCfg : 1;
            uint32_t ClkReqCfg : 1;
            uint32_t ClkSelCfg : 1;
            uint32_t ClkTimeOutCfg : 1;
            uint32_t IrqPolarityCfg : 1;
            uint32_t VbatMonitorEnCfg : 1;
            uint32_t VenCfg : 1;
            uint32_t PowByFieldEnCfg : 1;
            uint32_t ToBeforeStandByCfg : 1;
            uint32_t PllXtalCfg : 1;
            uint32_t SysESeIntfSelCfg : 1;
            uint32_t FelicaTSNCfg : 1;
            uint32_t RfReg : 1;
        } Config;
        uint32_t EnableConfig;
    } SystemConfig;
    uint16_t wHostIfCharCfg;
    struct {
        uint32_t Info_Txldo_OverCurrent : 1;
        uint32_t Info_Pmuvcc : 1;
        uint32_t Info_Mem_Violation : 1;
        uint32_t Info_Temp_OverHeat : 1;
        uint32_t Info_Svdd_OverCurr : 1;
        uint32_t Rfu : 3;
    } SystemNtfCfg;
    uint8_t bClkReqCfg;
    uint8_t bClkTimeOutCfg;
    struct {
        phNciNfc_SysClkSrc_t ClkSrc;
        phNciNfc_SysClkFreq_t ClkIn;
    } ClkSelCfg;
    uint8_t bIrqPolarityCfg;
    uint8_t bVbatMonitorEnCfg;
    struct {
        uint32_t VenValue;
        uint32_t VenPullDown;
    } VenCfg;
    uint8_t bPwrByFieldEnCfg;
    uint16_t bToBeforeStandByCfg;
    union {
        struct {
            uint8_t bKickTime;
            uint8_t bStartTimeLsb;
            uint8_t bStartTimeMsb;
            uint8_t bStopTime;
        } XtalCfg;
        struct {
            uint8_t bLockTimePll1And2;
            uint8_t bLockTimePll2Only;
            uint8_t bLockLoopIterations;
            uint8_t bDelayBtwDisAndEn;
        } PllCfg;
    } PllXtalCfg;
    phNciNfc_eSeIntfSel_t eSysESeIntfSel;
    uint8_t bFelicaTSN;
    uint8_t aRfRegCfg[6];
} phNciNfc_SysConfigParams_t;

typedef struct phNciNfc_SwpConfig {
    union {
        struct {
            uint32_t SwpBitRateInt1 : 1;
        } Config;
        uint32_t EnableConfig;
    } SwpConfig;
    union {
        struct {
            uint32_t SwpInt1Cfg : 1;
            uint32_t SwpInt2Cfg : 1;
            uint32_t SwpSvddOn : 1;
        } Config2;
        uint32_t EnableConfig2;
    } SwpConfig2;
    uint8_t bSwpBitRateInt1;
    uint8_t bSwpInt1Cfg;
    uint8_t bSwpInt2Cfg;
    uint8_t bSwpSvddOn;
} phNciNfc_SwpConfig_t;

typedef struct phNciNfc_NxpExtnRfDiscConfig {
    union {
        struct {
            uint32_t SetTagDetector : 1;
            uint32_t SetTagDetectorThresh : 1;
            uint32_t SetTagDetectorPeriod : 1;
            uint32_t SetTagDetectorFallBackCnt : 1;
            uint32_t SetPollProfileSelCfg : 1;
            uint32_t Set15693BailOut : 1;
            uint32_t SetHidBailOut : 1;
            uint32_t SetEpcGenBailOut : 1;
            uint32_t SetKovioBailOut : 1;
            uint32_t GtNfcAACfg : 1;
            uint32_t GtNfcAPCfg : 1;
            uint32_t GtNfcBCfg : 1;
            uint32_t GtNfcFCfg : 1;
            uint32_t GtNfc15693Cfg : 1;
        } Config;
        uint32_t EnableConfig;
    } NxpExtnRfDiscConf;
    union {
        struct {
            uint32_t MifKey0Cfg : 1;
            uint32_t MifKey1Cfg : 1;
            uint32_t MifKey2Cfg : 1;
            uint32_t MifKey3Cfg : 1;
            uint32_t MifKey4Cfg : 1;
            uint32_t MifKey5Cfg : 1;
            uint32_t MifKey6Cfg : 1;
            uint32_t MifKey7Cfg : 1;
            uint32_t MifKey8Cfg : 1;
            uint32_t MifKey9Cfg : 1;
            uint32_t MifKey10Cfg : 1;
            uint32_t MifKey11Cfg : 1;
            uint32_t MifKey12Cfg : 1;
            uint32_t MifKey13Cfg : 1;
            uint32_t MifKey14Cfg : 1;
            uint32_t MifKey15Cfg : 1;
        } Config;
        uint32_t ConfigKey;
    } ConfigMifKeys;
    uint8_t bTagDetector;
    uint8_t bTagDetectorThresh;
    uint16_t wTagDetectorPeriod;
    uint8_t bTagDetectorFallBackCnt;
    uint8_t bPollProfileSel;
    uint8_t bP15693BailOut;
    uint8_t bPHidBailOut;
    uint8_t bEpcGenBailOut;
    uint8_t bKovioBailOut;
    uint8_t bGtNfcAACfg;
    uint8_t bGtNfcAPCfg;
    uint8_t bGtNfcBCfg;
    uint8_t bGtNfcFCfg;
    uint8_t bGtNfc15693Cfg;
    uint8_t aMifKey0Cfg[6];
    uint8_t aMifKey1Cfg[6];
    uint8_t aMifKey2Cfg[6];
    uint8_t aMifKey3Cfg[6];
    uint8_t aMifKey4Cfg[6];
    uint8_t aMifKey5Cfg[6];
    uint8_t aMifKey6Cfg[6];
    uint8_t aMifKey7Cfg[6];
    uint8_t aMifKey8Cfg[6];
    uint8_t aMifKey9Cfg[6];
    uint8_t aMifKey10Cfg[6];
    uint8_t aMifKey11Cfg[6];
    uint8_t aMifKey12Cfg[6];
    uint8_t aMifKey13Cfg[6];
    uint8_t aMifKey14Cfg[6];
    uint8_t aMifKey15Cfg[6];
} phNciNfc_NxpExtnRfDiscConfig_t;

typedef struct phNciNfc_PropDiscParams {
    union {
        struct {
            uint32_t SetFelicaReqCode : 1;
            uint32_t SetFelicaSysCode : 1;
            uint32_t SetToRfOffCfg : 1;
            uint32_t SetLstnProfileSelCfg : 1;
        } Config;
        uint32_t EnableConfig;
    } PropConfig;
    uint8_t bFelicaReqCode;
    uint16_t wFelicaSysCode;
    uint8_t bToRfOff;
    uint8_t bLstnProfileSel;
} phNciNfc_PropDiscParams_t;

typedef struct phNciNfc_RfDiscConfigPropParams {
    phNciNfc_RfConfigPropInfo_t tConfigInfo;
    phNciNfc_SysConfigParams_t tSysConfigParams;
    phNciNfc_SwpConfig_t tSwpConfigParams;
    phNciNfc_NxpExtnRfDiscConfig_t tNxpExtnRfDiscParams;
    phNciNfc_PropDiscParams_t tPropDiscParams;
} phNciNfc_RfDiscConfigPropParams_t;

#ifdef __cplusplus
extern "C" {
#endif
#include <phNciNfc_TlvUtils.h>
NFCSTATUS phNciNfc_SetConfigRaw(void*,uint8_t*,uint16_t,void (*)(void*,NFCSTATUS,void*),void*);
NFCSTATUS phNciNfc_GetConfigRaw(void*,uint8_t*,uint16_t,void (*)(void*,NFCSTATUS,void*),void*);
NFCSTATUS phNciNfc_GetConfigProp(void*);
NFCSTATUS phNciNfc_GetConfigRawRsp(void*,NFCSTATUS);
NFCSTATUS phNciNfc_CompleteGetConfigRaw(void*,NFCSTATUS);
NFCSTATUS phNciNfc_SetConfigProp(void*);
NFCSTATUS phNciNfc_SetConfigPropRsp(void*,NFCSTATUS);
NFCSTATUS phNciNfc_CompleteSetConfigProp(void*,NFCSTATUS);
NFCSTATUS phNciNfc_ValidateSetConfPropParams(phNciNfc_RfDiscConfigPropParams_t*,uint16_t*,uint8_t*);
NFCSTATUS phNciNfc_BuildSetConfPropPayload(phNciNfc_RfDiscConfigPropParams_t*,uint8_t*,uint16_t);
NFCSTATUS phNciNfc_ValidateSysConfigParams(phNciNfc_SysConfigParams_t*,uint16_t*,uint8_t*);
NFCSTATUS phNciNfc_ValidateSwpConfigParams(phNciNfc_SwpConfig_t*,uint16_t*,uint8_t*);
NFCSTATUS phNciNfc_ValidateNxpExtnRfDiscConfigParams(phNciNfc_NxpExtnRfDiscConfig_t*,uint16_t*,uint8_t*);
NFCSTATUS phNciNfc_ValidatePropConfigParams(phNciNfc_PropDiscParams_t*,uint16_t*,uint8_t*);
uint8_t phNciNfc_BuildSysConfigParams(phNciNfc_TlvUtilInfo_t*,phNciNfc_SysConfigParams_t*);
uint8_t phNciNfc_BuildSwpConfigParams(phNciNfc_TlvUtilInfo_t*,phNciNfc_SwpConfig_t*);
uint8_t phNciNfc_BuildNxpRfDiscConfigParams(phNciNfc_TlvUtilInfo_t*,phNciNfc_NxpExtnRfDiscConfig_t*);
uint8_t phNciNfc_BuildConfigPropParams(phNciNfc_TlvUtilInfo_t*,phNciNfc_PropDiscParams_t*);
NFCSTATUS phNciNfc_BuildSetConfPropCmdPayload(phNciNfc_RfDiscConfigPropParams_t*,phNciNfc_TlvUtilInfo_t*);
NFCSTATUS phNciNfc_ClearConfigParams(void*);
NFCSTATUS phNciNfc_GetRfPropParams(phNciNfc_RfDiscConfigPropParams_t*,uint8_t*,uint8_t*,uint8_t*);
NFCSTATUS phNciNfc_StoreRfPropParams(uint8_t*,uint16_t,phNciNfc_RfDiscConfigPropParams_t*);
NFCSTATUS phNciNfc_UpdateReqParamIDs(void*,uint8_t*,uint8_t,uint8_t);
NFCSTATUS phNciNfc_ProcessRfPropParams(void*,uint16_t,uint8_t*,uint8_t);
void phNciNfc_CmpParamsSetConfigPropOpt(phNciNfc_RfDiscConfigPropParams_t*,phNciNfc_RfDiscConfigPropParams_t*);
NFCSTATUS phNciNfc_GetConfigPropRsp(void*,NFCSTATUS);
NFCSTATUS phNciNfc_CompleteGetConfigProp(void*,NFCSTATUS);
NFCSTATUS phNciNfc_GetConfigPropOpt(void*);
NFCSTATUS phNciNfc_GetConfigPropOptRsp(void*,NFCSTATUS);
NFCSTATUS phNciNfc_SetConfigPropRfParameters(void*,phNciNfc_RfDiscConfigPropParams_t*,void (*)(void*,NFCSTATUS,void*),void*);
#ifdef __cplusplus
}
#endif
