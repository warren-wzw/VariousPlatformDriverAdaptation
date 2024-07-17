#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "mpi_sys.h"
#include "hi_common.h"
#include "hi_type.h"
#include "hifb.h"
#include "sample_comm.h"
#include "getFrame.h"
#define HEIGHT 1920
#define WIDE   1080
 
HI_S32 SAMPLE_VIO_WZW(HI_U32 u32VoIntfType);
 
/******************************************************************************
* function    : main()
* Description : main
******************************************************************************/
int main(int argc, char *argv[])
{
    HI_S32 s32Ret = HI_FAILURE;
    HI_U32 u32VoIntfType = 0;
    HI_U32  u32ChipId;

    HI_MPI_SYS_GetChipId(&u32ChipId);
    if (HI3516C_V500 == u32ChipId)
    {
        u32VoIntfType = 1;
    }
    else
    {
        u32VoIntfType = 0;
    }
 
    s32Ret=SAMPLE_VIO_WZW(u32VoIntfType); 

    if (HI_SUCCESS == s32Ret)
    {
        SAMPLE_PRT("sample_vio exit success!\n");
    }
    else
    {
        SAMPLE_PRT("sample_vio exit abnormally!\n");
    }

    return s32Ret;
}

HI_S32 SAMPLE_VIO_WZW(HI_U32 u32VoIntfType)  
 {
    printf("Hello wzw ,the programm will start in 1 second\n");
    usleep(1000000);
    HI_S32             s32Ret = HI_SUCCESS;

    HI_S32             s32ViCnt       = 1;
    VI_DEV             ViDev          = 0;          // VI device number which in [0, VI_MAX_DEV_NUM)
    VI_PIPE            ViPipe         = 0;
    VI_CHN             ViChn          = 0;
    HI_S32             s32WorkSnsId   = 0;
    SAMPLE_VI_CONFIG_S stViConfig;

    SIZE_S             stSize;
    VB_CONFIG_S        stVbConf;
    PIC_SIZE_E         enPicSize;
    HI_U32             u32BlkSize;
    VO_CHN             VoChn          = 0;
    SAMPLE_VO_CONFIG_S stVoConfig;

    WDR_MODE_E         enWDRMode      = WDR_MODE_NONE;
    DYNAMIC_RANGE_E    enDynamicRange = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E     enPixFormat    = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VIDEO_FORMAT_E     enVideoFormat  = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E    enCompressMode = COMPRESS_MODE_NONE;
    VI_VPSS_MODE_E     enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;               //set by wzw
//
    VPSS_GRP           VpssGrp        = 0;
    VPSS_GRP_ATTR_S    stVpssGrpAttr;
    VPSS_CHN           VpssChn        = VPSS_CHN0;
    HI_BOOL            abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};
    VPSS_CHN_ATTR_S    astVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];
//
    /*********************************config vi************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    stViConfig.s32WorkingViNum                                   = s32ViCnt;
    stViConfig.as32WorkingViId[0]                                = 0;
    stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.MipiDev         = ViDev;
    stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.s32BusId        = 0;
    stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType       =SONY_IMX214_MIPI ;   //set by myself
    stViConfig.astViInfo[s32WorkSnsId].stDevInfo.ViDev           = ViDev;       //
    stViConfig.astViInfo[s32WorkSnsId].stDevInfo.enWDRMode       = enWDRMode;    //WDR work mode??which in frame-mode??line-mode??non-wdr
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.enMastPipeMode = enMastPipeMode;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[0]       = ViPipe;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[1]       = -1;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[2]       = -1;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[3]       = -1;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.ViChn           = ViChn;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enPixFormat     = enPixFormat;   //video layer input pixel form??YVU420 ??YVU422??YUV400 
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enDynamicRange  = enDynamicRange; //target image dynamic range 
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enVideoFormat   = enVideoFormat;//target image video data type
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enCompressMode  = enCompressMode;
    SAMPLE_PRT("config vi finish!\n");

    /*get picture size--PIC_1080P*/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get picture size by sensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);   //get image size to set VB
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get picture size failed!\n");
        return s32Ret;
    }
        SAMPLE_PRT("get picture size  finish!\n");

    /*config vb*/
    memset_s(&stVbConf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt              = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt   = 10;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 4;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        return s32Ret;
    }
        SAMPLE_PRT("config vb finish!\n");

    /*start vi   main to set*/
     s32Ret = SAMPLE_COMM_VI_StartVi_wzw(&stViConfig);   //add by wzw
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT;
    }
        SAMPLE_PRT("\n------------------start vi finish!-----------------------\n");
/**********************************************************************************************/
    /*config vpss*/
    
    memset_s(&stVpssGrpAttr, sizeof(VPSS_GRP_ATTR_S), 0, sizeof(VPSS_GRP_ATTR_S));
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate    = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate    = -1;
    stVpssGrpAttr.enDynamicRange                 = DYNAMIC_RANGE_SDR8;
    stVpssGrpAttr.enPixelFormat                  = enPixFormat;      //INPUT pixel form
    stVpssGrpAttr.u32MaxW                        = HEIGHT;                //input width
    stVpssGrpAttr.u32MaxH                        = WIDE;               //input height
    stVpssGrpAttr.bNrEn                          = HI_TRUE;               //Nr switch
    stVpssGrpAttr.stNrAttr.enCompressMode        = COMPRESS_MODE_FRAME;    
    stVpssGrpAttr.stNrAttr.enNrMotionMode        = NR_MOTION_MODE_NORMAL;
    stVpssGrpAttr.stNrAttr.enNrType            = VPSS_NR_TYPE_VIDEO;

    astVpssChnAttr[VpssChn].u32Width                    = HEIGHT;          //target image  width
    astVpssChnAttr[VpssChn].u32Height                   = WIDE;          //target image height
    astVpssChnAttr[VpssChn].enChnMode                   = VPSS_CHN_MODE_USER;      //channnel mode
    astVpssChnAttr[VpssChn].enCompressMode              = enCompressMode;           //tar mode
    astVpssChnAttr[VpssChn].enDynamicRange              = enDynamicRange;         //dynamic range
    astVpssChnAttr[VpssChn].enVideoFormat               = enVideoFormat;      //video mode VIDEO_FORMAT_LINEAR
    astVpssChnAttr[VpssChn].enPixelFormat               = enPixFormat;           //pixel mode
    astVpssChnAttr[VpssChn].stFrameRate.s32SrcFrameRate = 30;               //source framrate 
    astVpssChnAttr[VpssChn].stFrameRate.s32DstFrameRate = 30;                   //destination framerate
    astVpssChnAttr[VpssChn].u32Depth                    = 1;              //user could get the num of frames in (0,8)
    astVpssChnAttr[VpssChn].bMirror                     = HI_FALSE;                   //,irror
    astVpssChnAttr[VpssChn].bFlip                       = HI_FALSE;              //vertical turnover
    astVpssChnAttr[VpssChn].stAspectRatio.enMode        = ASPECT_RATIO_NONE;
        SAMPLE_PRT("config vpss finish!\n");

    /*start vpss*/
    abChnEnable[0] = HI_TRUE;
    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, abChnEnable, &stVpssGrpAttr, astVpssChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vpss group failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT1;
    }
        SAMPLE_PRT("start vpss finish!\n");

    /*vpss bind vi*/
    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ViPipe, ViChn, VpssGrp);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vpss bind vi failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT2;
    }
    HI_MPI_VPSS_SetChnRotation(ViPipe, ViChn, ROTATION_270); //rotate 270 to correct 
/****************************************venc*******************************************/
/*     VENC_CHN        VencChn[2]    = {0,1};
    HI_U32          u32Profile[2] = {3,0};
    PAYLOAD_TYPE_E  enPayLoad[2]  = {PT_H264};
    VENC_GOP_MODE_E enGopMode     = VENC_GOPMODE_NORMALP;
    VENC_GOP_ATTR_S stGopAttr;
    SAMPLE_RC_E     enRcMode;
    HI_BOOL         bRcnRefShareBuf = HI_TRUE;
    PIC_SIZE_E      enSize[2]     = {PIC_1080P}; 

    enRcMode                        =SAMPLE_RC_CBR;
     s32Ret = SAMPLE_COMM_VENC_GetGopAttr(enGopMode,&stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Venc Get GopAttr for %#x!\n", s32Ret);   
    }

   //encode h.264 
    s32Ret = SAMPLE_COMM_VENC_Start(VencChn[0], enPayLoad[0],enSize[0], enRcMode,u32Profile[0],bRcnRefShareBuf,&stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Venc Start failed for %#x!\n", s32Ret);
      
    }

    s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn,VencChn[0]);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Venc Get GopAttr failed for %#x!\n", s32Ret);
        
    }



     //stream save process
    s32Ret = SAMPLE_COMM_VENC_StartGetStream_Svc_t(1);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        
    }
    getchar(); */
/***********************************************************************************/
    /*config vo*/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    stVoConfig.enDstDynamicRange = enDynamicRange;
    stVoConfig.enVoIntfType = VO_INTF_MIPI;
    stVoConfig.enIntfSync = VO_OUTPUT_USER;
    stVoConfig.enPicSize = enPicSize;

    /*start vo*/
    SAMPLE_PRT("/n----------------------begin to start vo--------------------------------------\n");
    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT3;
    }
    /*vpss bind vo*/
    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(VpssGrp, VpssChn, stVoConfig.VoDev, VoChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vo bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT4;
    }
    printf("---------------press Enter key to exit!---------------\n");
   /*  while(1){
        Get_Frame_VPSS_VO(VpssGrp,VpssChn);
    } */
    if(getchar()){
            goto EXIT4;
        }
     
EXIT4:
    SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp, VpssChn, stVoConfig.VoDev, VoChn);
    SAMPLE_COMM_VO_StopVO(&stVoConfig);
EXIT3:
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe, ViChn, VpssGrp);
EXIT2:
    SAMPLE_COMM_VPSS_Stop(VpssGrp, abChnEnable);
EXIT1:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
EXIT:
    SAMPLE_COMM_SYS_Exit();
    return s32Ret;

 }

 
 
 
 
