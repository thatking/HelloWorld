#include "acodec.h"
#include "sample_comm.h"

#define ACODEC_FILE "/dev/acodec"
#define AUDIO_IN_DEV 0


#define NEEDCLEAR 0
#define ADDWAV 1


typedef struct _wav_pcm_hdr
{
    char            riff[4];
    int             size_8;
    char            wave[4];
    char            fmt[4];
    int             fmt_size;
    HI_S16 	       format_tag;
    HI_S16 			channels;
    int             samples_per_sec;
    int             avg_bytes_per_sec;
    HI_S16 			block_align;
    HI_S16 			bit_per_sample;
    char            data[4];
    int             data_size;
}wave_pcm_hdr;

typedef struct st_Thread
{	
	pthread_t pid;
	HI_BOOL bStart;
	AI_CHN aiChn;
	AUDIO_DEV aiDev;
	HI_CHAR *filename;
}THREAD_S;


/*************************************************
*function: debug 
*************************************************/
HI_VOID myDEBUG(const char* function,const int linenum, char* out)
{
	printf("[%15s : %5d] : %s\n",function, linenum, out);
}


/*************************************************
*function: SYS init
*************************************************/
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

/*************************************************
*function: Create Ai Chn
*************************************************/
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

/*************************************************
*function: Destory Ai Chn
*************************************************/
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

/*************************************************
*function: Create Ao Chn
*************************************************/
HI_S32 AUDIO_StartAo(AUDIO_DEV aoDevId, AO_CHN aoChn, AIO_ATTR_S* pstAioAttr)
{
	HI_S32 s32ret;

	s32ret = HI_MPI_AO_SetPubAttr(aoDevId, pstAioAttr);
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_AO_SetPubAttr %d with 0x%x\n",aoDevId, s32ret);
		return s32ret;
	}

	s32ret = HI_MPI_AO_Enable(aoDevId);
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_AO_Enable %d failed with 0x%x\n",aoDevId, s32ret );
		return s32ret;
	}

	s32ret = HI_MPI_AO_EnableChn(aoDevId, aoChn);
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_AO_EnableChn %d:%d failed with 0x%x\n",aoDevId, aoChn, s32ret );
		return s32ret;
	}

	return HI_SUCCESS;
}

/*************************************************
*function: Destory Ao Chn
*************************************************/
HI_S32 AUDIO_StopAo(AUDIO_DEV aoDevId, AO_CHN aoChn)
{
	HI_S32 s32ret;

	s32ret = HI_MPI_AO_DisableChn(aoDevId,aoChn);
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_AO_DisableChn %d:%d failed with 0x%x\n", aoDevId,aoChn, s32ret);
		return s32ret;
	}

	s32ret = HI_MPI_AO_Disable(aoDevId);
	if(HI_SUCCESS != s32ret)
	{
		printf("HI_MPI_AO_Diable %d failed with 0x%x\n", aoDevId, s32ret);
		return s32ret;
	}

	return HI_SUCCESS;
}

/*************************************************
*function: bind ai and ao
*************************************************/
HI_S32 AUDIO_BindAiAo(AUDIO_DEV aiDevId, AI_CHN aiChn, AUDIO_DEV aoDevId, AO_CHN aoChn)
{
	MPP_CHN_S stSrcChn,stDestChn;
	stSrcChn.enModId 	= HI_ID_AI;
	stSrcChn.s32DevId	= aiDevId;
	stSrcChn.s32ChnId	= aiChn;
	stDestChn.enModId 	= HI_ID_AO;
	stDestChn.s32ChnId	= aoChn;
	stDestChn.s32DevId	= aoDevId;

	return HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
}

/*************************************************
*function: UnBind Ai and ao
*************************************************/
HI_S32 AUDIO_UnbindAiAo(AUDIO_DEV aiDevId, AI_CHN aiChn, AUDIO_DEV aoDevId, AO_CHN aoChn)
{
	MPP_CHN_S stSrcChn,stDestChn;
	stSrcChn.enModId 	= HI_ID_AI;
	stSrcChn.s32DevId	= aiDevId;
	stSrcChn.s32ChnId	= aiChn;
	stDestChn.enModId 	= HI_ID_AO;
	stDestChn.s32ChnId	= aoChn;
	stDestChn.s32DevId	= aoDevId;

	return HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
}

/*************************************************
*function: create Aenc Chn
*************************************************/
HI_S32 AUDIO_StartAenc(AENC_CHN aeChn)
{
	HI_S32 s32ret;
	AENC_CHN_ATTR_S stAencAttr;
	AENC_ATTR_G711_S stAencG711;

	stAencAttr.enType			= PT_G711A;
	stAencAttr.u32PtNumPerFrm	= 0;
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

/*************************************************
*function: Destory Aenc Chn	
*************************************************/
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

/*************************************************
*function: bind ai to aenc
*************************************************/
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

/*************************************************
*function: unBind ai and aenc
*************************************************/
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

/*************************************************
*function: read from ai and send to ao
*************************************************/
HI_S32 AUDIO_AIAO(HI_VOID)
{
	HI_S32 s32ret;	
	AUDIO_DEV aiDevId = 0;
	AUDIO_DEV aoDevId = 0;
	AI_CHN aiChn = 0;
	AO_CHN aoChn = 0;
	AIO_ATTR_S stAioAttr;

	stAioAttr.enSamplerate	= AUDIO_SAMPLE_RATE_16000;
	stAioAttr.enBitwidth	= 1;
	stAioAttr.enWorkmode	= AIO_MODE_I2S_MASTER;
	stAioAttr.enSoundmode	= AUDIO_SOUND_MODE_MONO;
	stAioAttr.u32EXFlag		= 0;
	stAioAttr.u32FrmNum		= 30;
	stAioAttr.u32PtNumPerFrm= 160;
	stAioAttr.u32ChnCnt 	= 1;
	stAioAttr.u32ClkSel		= 0;
	
	//step0:init vb 
	VB_CONF_S stVbConf;
	memset(&stVbConf, 0, sizeof(VB_CONF_S));
	s32ret = SYS_Init(&stVbConf);
	if(HI_SUCCESS != s32ret)
	{
		printf("SYS_Init failed\n");
		return HI_FAILURE;
	}
	//step1:conf aduio codec
	//SAMPLE_INNER_CODEC_CfgAudio(AUDIO_SAMPLE_RATE_16000);
	//step2:start Ai
	s32ret =AUDIO_StartAi(aiDevId, aiChn, &stAioAttr);
	if(HI_SUCCESS != s32ret)
	{
		printf("AUDIO_StartAi failed\n");
		return HI_FAILURE;
	}

	//step3:start ao
	s32ret = AUDIO_StartAo(aoDevId, aoChn, &stAioAttr);
	if(HI_SUCCESS != s32ret)
	{
		printf("AUDIO_StartAo failed\n");
		return HI_FAILURE;
	}

	//step4:bind ai -> ao
	s32ret = AUDIO_BindAiAo(aiDevId, aiChn, aoDevId, aoChn);
	if(HI_SUCCESS != s32ret)
	{
		printf("AUDIO_BindAiAo failed\n");
		return HI_FAILURE;
	}

	printf("now ,ai iuput voice,ao output sound,enter 3 to exit\n");
	getchar();
	getchar();
	getchar();

	//now relase dev
	s32ret = AUDIO_UnbindAiAo(aiDevId, aiChn, aoDevId, aoChn);
	if(HI_SUCCESS != s32ret)
	{
		printf("AUDIO_UnbindAiAo failed\n");
		return HI_FAILURE;
	}

	s32ret = AUDIO_StopAi(aiDevId, aiChn);
	if(HI_SUCCESS != s32ret)
	{
		printf("AUDIO_StopAi failed\n");
		return HI_FAILURE;
	}

	s32ret = AUDIO_StopAo(aoDevId, aoChn);
	if(HI_SUCCESS != s32ret)
	{
		printf("AUDIO_StopAo failed\n");
		return HI_FAILURE;
	}

	return HI_SUCCESS;

}

/*************************************************
*function:  get Audio Frame 
*************************************************/
HI_S32 getAudioFrame(AUDIO_DEV aiDev, AI_CHN aiChn)
{
	HI_S32 s32ret;
	HI_S32 aiFd;
	AUDIO_FRAME_S stFrame;
	AEC_FRAME_S stAecFrm;
	fd_set read_fds;
	struct timeval timeoutlVal;
	AI_CHN_PARAM_S stAiChnParam;
	FILE* pfd = NULL;
   
    pfd = fopen("./audio","w+");
	
	s32ret = HI_MPI_AI_GetChnParam(aiDev, aiChn, &stAiChnParam);
	if(HI_SUCCESS != s32ret)
	{
		printf("Get ai Chn param failed\n");
		return s32ret;
	}

	s32ret = HI_MPI_AI_SetChnParam(aiDev, aiChn, &stAiChnParam);
	if(HI_SUCCESS != s32ret)
	{
		printf("Set ai Chn param failed\n");
		return s32ret;
	}

	FD_ZERO(&read_fds);
	aiFd = HI_MPI_AI_GetFd(aiDev, aiChn);
	if(aiFd <=0)
	{
		printf("get ai Fd failed \n");
		return aiFd;
	}	
	FD_SET(aiFd, &read_fds);

		timeoutlVal.tv_sec = 3;
		timeoutlVal.tv_usec = 0;

		FD_ZERO(&read_fds);
		FD_SET(aiFd, &read_fds);

		s32ret = select(aiFd + 1, &read_fds, NULL, NULL, &timeoutlVal);
		if(s32ret < 0)
		{
			printf("s32ret < 0\n");
			return 0;
		}
		else if( 0 == s32ret)
		{
			printf("get ai frame select time out\n");
		}


		if(FD_ISSET(aiFd, &read_fds))
		{
			memset(&stAecFrm, 0, sizeof(AEC_FRAME_S));
			s32ret = HI_MPI_AI_GetFrame(aiDev, aiChn, &stFrame, &stAecFrm, HI_FALSE);
			if(HI_SUCCESS != s32ret)
			{
				printf("get Frame failed,error code %x\n",s32ret);
			//	return s32ret;
			//	continue;
			}
    		fwrite(stFrame.pVirAddr[0], 1, stFrame.u32Len, pfd);
		}

	fclose(pfd); 
	s32ret = HI_MPI_AI_ReleaseFrame(aiDev, aiChn, &stFrame, &stAecFrm);
	if(HI_SUCCESS != s32ret)
	{
		printf("frame relase false\n");
		return s32ret;
	}

	return 0;
}

/*************************************************
*function: 获取平均能量
*************************************************/
float getAvePowerValue(void* start, int len)
{
	float tot = 0.0;
	len = len / 2;
    HI_S16* num = (HI_S16*)start;
    int i=0;
    for( i=0; i< len; i++)
    {
        num[i] = (~num[i]) + 1;
        tot += num[i] * num[i];
    }
    return (float)tot/len;
}


/*************************************************
*function: 计算平均过零率
*************************************************/
float getZeroCrossRate(void* start, int len)
{
	int tot = 0;
	len = len / 2 ;
    HI_S16* num = (HI_S16*)start;
	HI_S16 x = num[0];
	HI_S16 y ;
    HI_S16 i;
    for(i=1; i < len; i++)
    {
        y = num[i];
		x = x & 0x8000;
		y = y & 0x8000;
        tot += ( (x ^ y ) == 0 ) ? 0 : 1 ;
        x = y;
    }
    
    return (float)tot/len;
}
