/*************************************************************************
	> File Name: demo_G711.c
	> Author: znr1995
	> Description: Demo of read from AI and code the stream with G711 
	> Created Time: 2017年08月02日 星期三 09时02分33秒
 ************************************************************************/

#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include "acodec.h"
#include "hi_comm_aio.h"
#include "mpi_ai.h"
#include "hi_comm_aenc.h"
#include "mpi_aenc.h"
#include "hi_comm_sys.h"
#include "mpi_sys.h"
#include "hi_comm_vb.h"
#include "mpi_vb.h"

#define ACODEC_FILE "/dev/acodec"
#define PTNUMPERFRM 160
typedef struct st_Aenc2File
{
    HI_BOOL     bStart;
    AENC_CHN    aeChn;
    char*       filename;
    pthread_t   stAePid;
}AENC2FILE_S;


 AENC2FILE_S g_thread;

typedef struct _wav_pcm_hdr
{
    char            riff[4];
    int             size_8;
    char            wave[4];
    char            fmt[4];
    int             fmt_size;
    short int       format_tag;
    short int       channels;
    int             samples_per_sec;
    int             avg_bytes_per_sec;
    short int       block_align;
    short int       bit_per_sample;
    char            data[4];
    int             data_size;
}wave_pcm_hdr;

wave_pcm_hdr default_wav_hdr =
{
    {'R', 'I', 'F', 'F'},
    0,
    {'W', 'A', 'V', 'E'},
    {'f', 'm', 't', ' '},
    16,
    6,
    1,
    8000,
    8000,
    1,
    8,
    {'d', 'a', 't', 'a'},
    0
};

/************************************************************************
*function: Init system 
************************************************************************/
HI_S32 SYS_Init(VB_CONF_S* pstVbConf)
{
   	HI_S32 s32ret;
	MPP_SYS_CONF_S stSysConf = {0};

	HI_MPI_SYS_Exit();
	HI_MPI_VB_Exit();

	if(NULL == pstVbConf)
	{
		printf("stVbConf is null,it is invaild\n");
		return HI_FAILURE;
	}

	s32ret = HI_MPI_VB_SetConf(pstVbConf);
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_VB_SetConf failed\n");
		return HI_FAILURE;
	}

	s32ret = HI_MPI_VB_Init();
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_VB_Init failed\n");
		return HI_FAILURE;
	}

	stSysConf.u32AlignWidth = 64;
	s32ret = HI_MPI_SYS_SetConf(&stSysConf);
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_SYS_SetConf failed\n");
		return HI_FAILURE;
	}

	s32ret = HI_MPI_SYS_Init();
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_SYS_Init failed\n");
		return HI_FAILURE;
	}

	return HI_SUCCESS; 
}

/************************************************************************
*function: config inner codec
************************************************************************/
HI_S32 AUDIO_ConfigCodec()
{
    int fd = -1;
    HI_S32 s32ret = HI_SUCCESS;
    ACODEC_FS_E i2s_fs_sel = ACODEC_FS_8000;
    int iAcodecInputVol = 0;

    fd = open(ACODEC_FILE, O_RDWR);
    if(fd < 0)
    {
        printf("%s: can't open Acodec %s \n",__FUNCTION__, ACODEC_FILE);
        return HI_FAILURE;
    }

    if(ioctl(fd, ACODEC_SOFT_RESET_CTRL))
    {
        printf("Reset audio code error\n");
    }

    if(ioctl(fd, ACODEC_SET_I2S1_FS, &i2s_fs_sel))
    {
        printf("set acodec sample rate failed\n");
        return HI_FAILURE;
    }

    iAcodecInputVol = 30;
    if(ioctl(fd, ACODEC_SET_INPUT_VOL, &iAcodecInputVol))
    {
        printf("set acodec micin volume failed\n");
        return HI_FAILURE;
    }

    close(fd);
    return s32ret;
}

/************************************************************************
*function: config and start aidev and aiChn
************************************************************************/
HI_S32 AUDIO_StartAi(AUDIO_DEV aiDevId, AI_CHN aiChn, AIO_ATTR_S *pstAioAttr)
{
	HI_S32 s32ret;
	s32ret = HI_MPI_AI_SetPubAttr(aiDevId, pstAioAttr);
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_AI_SetPubAttr %d failed with 0x%x\n",aiDevId, s32ret );
		return s32ret;
	}

	s32ret = HI_MPI_AI_Enable(aiDevId);
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_AI_Enable %d failed whith 0x%x\n",aiDevId, s32ret );
		return s32ret;
	}

	s32ret = HI_MPI_AI_EnableChn(aiDevId, aiChn);
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_AI_EnableChn %d:%d failed with 0x%x\n",aiDevId,aiChn,s32ret);
		return s32ret;
	}

	return HI_SUCCESS;
}

/************************************************************************
*function:close aiChn and aiDev  
************************************************************************/
HI_S32 AUDIO_StopAi(AUDIO_DEV aiDevId, AI_CHN aiChn)
{
	HI_S32 s32ret;

	s32ret = HI_MPI_AI_DisableChn(aiDevId,aiChn);
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_AI_DisableChn %d:%d failed with 0x%x\n", aiDevId, aiChn, s32ret);
		return s32ret;
	}

	s32ret = HI_MPI_AI_Disable(aiDevId);
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_AI_Disable %d failed with 0x%x\n", aiDevId,s32ret);
		return s32ret;
	}

	return HI_SUCCESS;
}

/************************************************************************
*function: config and start aenc Chn
************************************************************************/
HI_S32 AUDIO_StartAenc(AENC_CHN aeChn)
{
	HI_S32 s32ret;
	AENC_CHN_ATTR_S stAencAttr;
	AENC_ATTR_G711_S stAencG711;

	stAencAttr.enType			= PT_G711A;
	stAencAttr.u32PtNumPerFrm	= PTNUMPERFRM;
	stAencAttr.u32BufSize		= 30;
	stAencAttr.pValue			= &stAencG711;

	s32ret = HI_MPI_AENC_CreateChn(aeChn,&stAencAttr);
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_AENC_CreateChn %d failed with 0x%x\n",aeChn, s32ret);
		return s32ret;
	}

	return HI_SUCCESS;
}

/************************************************************************
*function:close aenc Chn 
************************************************************************/
HI_S32 AUDIO_StopAenc(AENC_CHN aeChn)
{
	HI_S32 s32ret;
	s32ret = HI_MPI_AENC_DestroyChn(aeChn);
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_AENC_DestroyChn %d failed with 0x%x\n",aeChn, s32ret);
		return s32ret;
	}
	return HI_SUCCESS;
}

/************************************************************************
*function: bind aiChn to aenc Chn
************************************************************************/
HI_S32 AUDIO_BindAiAenc(AUDIO_DEV aiDevId, AI_CHN aiChn, AENC_CHN aeChn)
{
	MPP_CHN_S stSrcChn,stDestChn;
	stSrcChn.enModId	= HI_ID_AI;
	stSrcChn.s32ChnId	= aiChn;
	stSrcChn.s32DevId	= aiDevId;
	stDestChn.enModId	= HI_ID_AENC;
	stDestChn.s32DevId	= 0;
	stDestChn.s32ChnId	= aeChn;

	return HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
}

/************************************************************************
function: unbind aichn to aenc chn
************************************************************************/
HI_S32 AUDIO_UnbindAiAenc(AUDIO_DEV aiDevId, AI_CHN aiChn, AENC_CHN aeChn)
{
	MPP_CHN_S stSrcChn,stDestChn;
	stSrcChn.enModId	= HI_ID_AI;
	stSrcChn.s32ChnId	= aiChn;
	stSrcChn.s32DevId	= aiDevId;
	stDestChn.enModId	= HI_ID_AENC;
	stDestChn.s32DevId	= 0;
	stDestChn.s32ChnId	= aeChn;

	return HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
}


/************************************************************************
*function: get Stream and save file
************************************************************************/
HI_S32 AUDIO_Aenc2File(void* ptr)
{
    wave_pcm_hdr wav_hdr = default_wav_hdr;
    AENC2FILE_S* pstAe = (AENC2FILE_S*)ptr;
    AENC_CHN aeChn = pstAe->aeChn;
    char* filename = pstAe->filename;
    HI_S32 s32ret;
    AUDIO_STREAM_S stStrFrm;
    FILE* pfd = NULL;
    pfd = fopen(filename, "wt+");
    fwrite(&wav_hdr,sizeof(wave_pcm_hdr), 1, pfd );
    while(pstAe->bStart)
    {
         s32ret = HI_MPI_AENC_GetStream(aeChn, &stStrFrm, -1);
         if(HI_SUCCESS != s32ret)
         {
             printf("HI_MPI_GetStream failed with %x \n",s32ret);
             return HI_FAILURE;
         }   

         //do save file and check frame index
         printf("stream index: %x \n",stStrFrm.u32Seq);
         fwrite(stStrFrm.pStream, 1, stStrFrm.u32Len, pfd );    //第二个参数，是由于压缩编码位数为8bit,
        wav_hdr.data_size += stStrFrm.u32Len;
        s32ret = HI_MPI_AENC_ReleaseStream(aeChn, &stStrFrm);
        if(HI_SUCCESS != s32ret)
        {
            printf("HI_MPI_ReleaseStream failed with %x \n",s32ret);
            return HI_FAILURE;
        }
    }
    
    wav_hdr.size_8 += wav_hdr.data_size + (sizeof(wav_hdr) - 8);	
	/* 将修正过的数据写回文件头部,音频文件为wav格式 */
	fseek(pfd, 4, 0);
	fwrite(&wav_hdr.size_8, sizeof(wav_hdr.size_8), 1, pfd);
	fseek(pfd, 40, 0); 
	fwrite(&wav_hdr.data_size, sizeof(wav_hdr.data_size), 1, pfd); //写入data_size的值
    printf("thread stop\n");
    fclose(pfd);
    printf("save file success\n");
    return HI_SUCCESS;    
}

/************************************************************************
function: create the thread to get frame from aenc
************************************************************************/
HI_S32 AUDIO_CreatTrdAencFile(AENC_CHN aeChn, char* filename)
{
    AENC2FILE_S* pstAe = NULL;
    pstAe = &g_thread;
    pstAe->aeChn = aeChn;
    pstAe->filename = filename;
    pstAe->bStart = HI_TRUE;
    pthread_create(&pstAe->stAePid, 0, AUDIO_Aenc2File, pstAe);
    return HI_SUCCESS;
}



/************************************************************************
*function: ai -> aenc -> file
************************************************************************/
HI_S32 AUDIO_code2file()
{
    HI_S32 s32ret;
    AUDIO_DEV aiDev = 0;
    AI_CHN aiChn = 0;
    AENC_CHN aeChn = 0;
    HI_S32 s32AiChnCnt;
    HI_S32 s32AencChnCnt;
    FILE* pfd = NULL;
    char* filename = "./save.wav";
    AIO_ATTR_S stAioAttr;

    stAioAttr.enSamplerate  = AUDIO_SAMPLE_RATE_8000;
    stAioAttr.enBitwidth    = AUDIO_BIT_WIDTH_16;
    stAioAttr.enWorkmode    = AIO_MODE_I2S_MASTER;
    stAioAttr.enSoundmode   = AUDIO_SOUND_MODE_MONO;
    stAioAttr.u32EXFlag     = 0;
    stAioAttr.u32FrmNum     = 30;
    stAioAttr.u32PtNumPerFrm= 160;    //80,160,240,320,480
    stAioAttr.u32ChnCnt     = 1;
    stAioAttr.u32ClkSel     = 0;


    /************************************************************************
        step 0: Init sys
    ************************************************************************/
    VB_CONF_S stVbConf;
    memset(&stVbConf, 0, sizeof(VB_CONF_S));    
    s32ret = SYS_Init(&stVbConf);
    if(HI_SUCCESS != s32ret)
    {
        printf("SYS init failed\n");
        return HI_FAILURE;
    }   
    /************************************************************************
        step 1: config audio codec
    ************************************************************************/
    s32ret = AUDIO_ConfigCodec();
    if(HI_SUCCESS != s32ret)
    {
        printf("config Acodec failed\n");
        return HI_FAILURE;
    }   
    /************************************************************************
        step 2: start ai dev and ai chn
    ************************************************************************/
    s32ret = AUDIO_StartAi(aiDev, aiChn, &stAioAttr);
    if(HI_SUCCESS != s32ret)
    {
        printf("start ai failed\n");
        return HI_FAILURE;
    }   
    /************************************************************************
        step 3: start aenc chn
    ************************************************************************/
    s32ret = AUDIO_StartAenc(aeChn);
    if(HI_SUCCESS != s32ret)
    {
        printf("start aeChn failed\n");
        return HI_FAILURE;
    }   
    /************************************************************************
        step 4: bind ai chn and aenc chn
    ************************************************************************/
    s32ret = AUDIO_BindAiAenc(aiDev, aiChn, aeChn);
    if(HI_SUCCESS  != s32ret)
    {
        printf("bind ai to aenc failed\n");
        return HI_FAILURE;
    }   
    /************************************************************************
        step 5:get frame from aenc chn 
    ************************************************************************/   
    s32ret = AUDIO_CreatTrdAencFile(aeChn, filename);
    if(HI_SUCCESS != s32ret)
    {
        printf("create Thread to get frame failed\n");
        goto EXIT;
    }
    else
    {
        printf("create Thread success, and audio will input [ %s ] file\n", filename);
        printf("enter another to exit\n");
        getchar();
        g_thread.bStart = HI_FALSE;
        sleep(1);   //wait thread stop 
        goto EXIT;
    }
    
EXIT:
    /************************************************************************
        step 6:unbind ai chn and aenc chn
    ************************************************************************/   
    s32ret =  AUDIO_UnbindAiAenc(aiDev, aiChn, aeChn);
    if(HI_SUCCESS != s32ret)
    {
        printf("unbind ai chn and aechn failed with error code %x \n",s32ret);
        return HI_FAILURE;
    }
    /************************************************************************
        step 7:destroy aenc chn
    ************************************************************************/   
    s32ret = AUDIO_StopAenc(aeChn);
    if(HI_SUCCESS != s32ret)
    {
        printf("destory aenc chn failed\n");
    }
    /************************************************************************
        step 8:destroy ai chn and close ai dev
    ************************************************************************/   
    s32ret = AUDIO_StopAi(aiDev, aiChn);
    if(HI_SUCCESS != s32ret)
    {
        printf("destory ai dev ai chn failed\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;


}


int main()
{
    AUDIO_code2file();
    return 0;
}