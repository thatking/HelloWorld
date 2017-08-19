#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pocketsphinx.h>
#include "myhead.h"
//一帧是20ms
#define MINLASTPOWER 			50					//当平均功率连续多少帧高于标准值时，判断为语音
#define MINLASTCROSSRATE 		50					//当平均过零率连续多少帧高于标准值时，判断为语音
#define MINVOICELEN  			25					//最小语音长度 0.2s
#define MAXVOICELEN 			25					//最长语音识别长度 0.5s
#define MAXNOVOICELEN 			10					//多长语音中间空隙时长
#define RATEOFAVEPOWER 			2					//判断语音能量是背景噪音能量的多少倍
#define RATEOFAVECROSSZERORATE	1.3					//过零率是背景噪音的多少倍
#define RATEOFSTEPLEN			0.001				//每一步变化长度
#define NEEDAUTOADAPTION		0					//是否开启自适应门限
#define P 						6  					//random > P
#define MAXSLIENCELEN 			100				//退出唤醒时间

typedef struct speech_s	
{
	ps_decoder_t 	*ps;
	cmd_ln_t 		*config;
	FILE			*pfd;
	wave_pcm_hdr    *pwav_hdr;
	const char* 	res;
	HI_BOOL			hasRes;
	HI_BOOL			isUsing;
}SPHINX_S;

wave_pcm_hdr default_wav_hdr =
{
    {'R', 'I', 'F', 'F'},
    0,
    {'W', 'A', 'V', 'E'},
    {'f', 'm', 't', ' '},
    16,
    1,
    1,
    16000,
    32000,
    2,
    16,
    {'d', 'a', 't', 'a'},
    0
};

HI_BOOL gloab_beSpheechUp = HI_FALSE;
HI_BOOL gloab_bFileWR = HI_FALSE;
HI_BOOL gloab_bVoice = HI_FALSE;    //标记语音段与非语音段，如果非语音段，如果识别100%转移，语音段，以一定的概率转移
HI_BOOL gloab_bWeakUp = HI_FALSE;
float gloab_favePower,gloab_fcrossZeroRate, gloab_fbaseAvePower, gloab_fbaseCrossZeroRate;
HI_S32 gloab_ivoicelastLast,  gloab_iavePowLast,  gloab_icrossZeroRateLast,  gloab_inoVoiceLast;
       gloab_ivoicelastLast = 0; gloab_iavePowLast = 0; gloab_icrossZeroRateLast = 0;  gloab_inoVoiceLast = 0;
HI_S8 gloab_istat = 0;
HI_S32 gloab_iSlienceLen = 0;
SPHINX_S	sphinxParm;
HI_S16	buf[1024];
HI_CHAR timeStr[50];
pthread_t sphinxThread;
HI_BOOL gloab_breadyRecing = HI_FALSE;
FILE *pfdinit,*pofd;

/*************************************************
*function: output format time string
*************************************************/
const char* outputTime() 
{ 
	time_t rawtime; 
	struct tm * timeinfo; 
	rawtime = time ( NULL ); 
	timeinfo = localtime ( &rawtime );
	strftime(timeStr,50,"%H:%M:%S",timeinfo);  
	return  timeStr; 
} 


/*************************************************
*function: save file
*************************************************/
HI_S32 saveFile(FILE* pfd, void* ptr, HI_U32 u32Len, wave_pcm_hdr* pwav_hdr)
{
	//会有隐患，因为bool值的改变与判断不是原子性的
	while(gloab_bFileWR)
	{
		usleep(50);
	}
	gloab_bFileWR = HI_TRUE;
	HI_U32 len;
	int i;
	len = fwrite(ptr, 1, u32Len, pfd);
	pwav_hdr->data_size = pwav_hdr->data_size +  u32Len;
	if(len != u32Len)
	{
		printf("fwrite error len : %d  u32Len: %d\n",len, u32Len);	
	}
	gloab_bFileWR = HI_FALSE;
	return len;
}

/*************************************************
*function: clear file
*************************************************/
HI_S32 clearFile(FILE* pfd, wave_pcm_hdr* pwav_hdr)
{
	while(gloab_bFileWR)
	{
		usleep(50);
	}
	gloab_bFileWR = HI_TRUE;
	//myDEBUG(__FUNCTION__, __LINE__, "clear file");
	ftruncate(fileno(pfd), 0);
	fseek(pfd, 0, SEEK_SET);
	ftell(pfd);
	pwav_hdr->data_size = 0;
	gloab_bFileWR = HI_FALSE;
	return HI_SUCCESS;
}

/***************************************************
*function: 判断识别结果，决定是否唤醒
***************************************************/
HI_BOOL bWeakUp(const char* str)
{
	static int stat = 1;
	int len =strlen(str);
	switch(len)
	{
		case 2:
		//hi
		stat = 2;
		gloab_bVoice = HI_TRUE;
		break;
		case 5:
		//hello
		stat = 2;
		gloab_bVoice = HI_TRUE;
		break;
		case 6:
		//danale
		if(!gloab_bVoice)
			{
				gloab_bVoice = HI_TRUE;
				stat = 1;
				return HI_TRUE;
			}
			else
			{

				int r =stat * ( rand()%10 );  //如果前面一个是hi/hello，成功概率翻倍，否则，不翻倍
				if(r > P)
				{
					stat = 1;
					gloab_bVoice = HI_TRUE;
					return HI_TRUE;
				}
			}
		break;
		case 8:case 12: case 13: //hi danale | hello danale | danale danale
		stat = 1;
		gloab_bVoice = HI_TRUE;
		return HI_TRUE;   //weak up
		break;
		default:
		stat = 1;
		gloab_bVoice = HI_TRUE;
		break;
	}
	return HI_FALSE;
	

}

/***************************************************
*function: 识别语音
***************************************************/
void* getRecognitionResult(void* ptr)
{	
	//将此线程与主线程分离，便于结束线程释放资源
	pthread_detach(pthread_self());
	SPHINX_S* pSphinx = (SPHINX_S*)ptr;
	pSphinx->isUsing = HI_TRUE;
	HI_S32  score,rv;
	while(gloab_bFileWR)
	{
		usleep(50);
	}
	gloab_bFileWR = HI_TRUE;
	//开始解码话语
	rv = ps_start_utt(pSphinx->ps);
	size_t nsamp;
	ps_process_raw(pSphinx->ps, pSphinx->pwav_hdr, sizeof(wave_pcm_hdr), FALSE, FALSE);
	if(ftell(pSphinx->pfd) < MAXVOICELEN * 640)
		fseek(pSphinx->pfd, 0, SEEK_SET);
	else
		fseek(pSphinx->pfd, -MAXVOICELEN * 640, SEEK_CUR);		//如果文件长度大于0.5S，取最后的0.5s
	HI_S32 tot = 0;   //最大可读 长度为 512*2*16 = 16K -》 0.5s
	while((!feof(pSphinx->pfd)) && tot < 16)
	{
		++tot;
		nsamp = fread(buf, 2, 512, pSphinx->pfd);
		nsamp = ps_process_raw(pSphinx->ps, buf, nsamp, FALSE, FALSE);
	}
	//结束解码
	rv = ps_end_utt(pSphinx->ps);
	gloab_bFileWR = HI_FALSE;
	pSphinx->res = ps_get_hyp(pSphinx->ps, &score);
	if(bWeakUp(pSphinx->res))
	{
		gloab_bWeakUp = HI_TRUE;
		printf("\n\nhas been weak \n\n\n");
	}
	fprintf(pofd,"%20s : %s %d\n", outputTime(), pSphinx->res,score);
	pSphinx->hasRes = HI_TRUE;
	pSphinx->isUsing = HI_FALSE;
	return NULL;
}


/*************************************************
*function: 获取当前avePower，corssZeroRate与标准值的对比情况,并且更新门限
*************************************************/
HI_S32 getStat(float avePower, float crossZeroRate)
{
	if(avePower >= gloab_favePower)
	{
		if(crossZeroRate >= gloab_fcrossZeroRate)
		{
			//双双达标
			return 3;
		}
		else
		{
			//平均能量达标
			return 2;
		}
	}
	else
	{
		if(crossZeroRate >= gloab_fcrossZeroRate)
		{
			//平均过零率达标
			return 1;
		}
	}
	if(NEEDAUTOADAPTION)
	{
		if(avePower > gloab_fbaseAvePower)
		{
			gloab_fbaseAvePower += RATEOFSTEPLEN * gloab_fbaseAvePower;
		}
		else
		{
			gloab_fbaseAvePower -= RATEOFSTEPLEN * gloab_fbaseAvePower;  //上升慢，下降快？测试而已，没有理论依据
		}
		gloab_favePower = RATEOFAVEPOWER * gloab_fbaseAvePower;
		if(crossZeroRate > gloab_fcrossZeroRate)
		{
			gloab_fbaseCrossZeroRate += RATEOFSTEPLEN * gloab_fbaseCrossZeroRate;
		}
		else
		{
			gloab_fbaseCrossZeroRate -= RATEOFSTEPLEN * gloab_fbaseCrossZeroRate;
		}
		gloab_fcrossZeroRate = RATEOFAVEPOWER * gloab_fbaseCrossZeroRate;
	}
	return 0;
}


/*************************************************
*function: 用双门限法判断疑似语音段
*************************************************/
HI_S32 doubleThreshold(float avePower, float crossZeroRate, void* ptr, HI_U32 u32Len, FILE* pfd, wave_pcm_hdr* pwav_hdr,AUDIO_FRAME_S* pAudioFrm)
{
	//后面改状态值为枚举
	switch(gloab_istat)
	{
		case 0:
			//开始非语音
			//myDEBUG(__FUNCTION__, __LINE__, "enter stat 0");
			if(gloab_bWeakUp)
			{
				gloab_iSlienceLen++;
				if(gloab_iSlienceLen > MAXSLIENCELEN)
				{
					gloab_bWeakUp = HI_FALSE;
					gloab_iSlienceLen=0;
					printf("\n\nhas been sleep \n\n\n");
				}
			}
			gloab_bVoice = HI_FALSE;   //非语音状态
			gloab_ivoicelastLast = gloab_icrossZeroRateLast = gloab_iavePowLast = gloab_inoVoiceLast =  0;
			switch(getStat(avePower, crossZeroRate))
			{
				case 1:
					gloab_icrossZeroRateLast++;
					clearFile(pfd, pwav_hdr);
					saveFile(pfd, ptr, u32Len, pwav_hdr);
					gloab_istat = 1;
				break;
				case 2:
					gloab_icrossZeroRateLast++;			
					clearFile(pfd, pwav_hdr);
					saveFile(pfd, ptr, u32Len, pwav_hdr);
					gloab_istat = 2;
				break;
				case 3:
					clearFile(pfd, pwav_hdr);
					saveFile(pfd, ptr, u32Len, pwav_hdr);
					gloab_istat = 3;
				break;
				default:
				case 0:
				break;
			}
		break;
		case 1:
			//平均过零率达标
			//myDEBUG(__FUNCTION__, __LINE__, "enter stat 1");
			if(gloab_bWeakUp)
			{
				gloab_iSlienceLen++;
				if(gloab_iSlienceLen > MAXSLIENCELEN)
				{
					gloab_bWeakUp = HI_FALSE;
					gloab_iSlienceLen=0;
					printf("\n\n has been sleep \n\n\n");
				}
			}
			switch(getStat(avePower, crossZeroRate))
			{
				case 1:
					gloab_icrossZeroRateLast++;
					saveFile(pfd, ptr, u32Len, pwav_hdr);
					if(gloab_icrossZeroRateLast  >= MINLASTPOWER && !gloab_bWeakUp)  //并且没有被唤醒情况下才进入3状态
					{	
						//达到一定时间长度,判断为语音
						gloab_ivoicelastLast = gloab_icrossZeroRateLast = gloab_iavePowLast = gloab_inoVoiceLast = 0;
						gloab_istat = 3;
					}
				break;
				case 2:
					gloab_iavePowLast = gloab_icrossZeroRateLast = 0;
					gloab_icrossZeroRateLast++;
					#if NEEDCLEAR
					//要不要清空文件？？
						clearFile(pfd, pwav_hdr);
					#endif
					saveFile(pfd, ptr, u32Len, pwav_hdr);
					gloab_istat = 2;
				break;
				case 3:
					gloab_ivoicelastLast = gloab_icrossZeroRateLast = gloab_iavePowLast = gloab_inoVoiceLast = 0;
					saveFile(pfd, ptr, u32Len, pwav_hdr);
					gloab_ivoicelastLast = MINLASTCROSSRATE / 3;
					gloab_istat = 3;
				break;
				default:
				case 0:
					clearFile(pfd, pwav_hdr);
					gloab_istat = 0;
				break;
			}

			
		break;
		case 2:
			//平均能量达标
			if(gloab_bWeakUp)
			{
				gloab_iSlienceLen++;
				if(gloab_iSlienceLen > MAXSLIENCELEN)
				{
					gloab_bWeakUp = HI_FALSE;
					gloab_iSlienceLen=0;
					printf("\n\nhas been sleep \n\n\n");
				}
			}
			//	myDEBUG(__FUNCTION__, __LINE__, "enter stat 2");
			switch(getStat(avePower, crossZeroRate))
			{
				case 1:
					gloab_icrossZeroRateLast = gloab_iavePowLast = 0;
					gloab_icrossZeroRateLast++;
					#if NEEDCLEAR
						clearFile(pfd, pwav_hdr);
					#endif
					saveFile(pfd, ptr, u32Len, pwav_hdr);
					gloab_istat = 1;
				break;
				case 2:
					gloab_iavePowLast++;
					saveFile(pfd, ptr, u32Len, pwav_hdr);
					if(gloab_iavePowLast >= MINLASTPOWER && !gloab_bWeakUp)
					{
						gloab_ivoicelastLast = gloab_icrossZeroRateLast = gloab_iavePowLast = gloab_inoVoiceLast = 0;
						gloab_ivoicelastLast = MINLASTPOWER / 3;
						gloab_istat = 3;
					}
				break;
				case 3:
					gloab_ivoicelastLast = gloab_icrossZeroRateLast = gloab_iavePowLast = gloab_inoVoiceLast = 0;
					saveFile(pfd, ptr, u32Len, pwav_hdr);
					gloab_istat = 3;
				break;
				default:
				case 0:
					clearFile(pfd, pwav_hdr);
					gloab_istat = 0;
				break;
			}
		
		break;
		case 3:
		//语音段，俩项指标都达标
			
			myDEBUG(__FUNCTION__, __LINE__, "enter stat 3");
			
			//唤醒时候才进行转发
			if(gloab_bWeakUp)
			{
				HI_S32 s32 = HI_MPI_AO_SendFrame(0,0,pAudioFrm,-1);
				if(s32 != HI_SUCCESS)
				{
					printf("send frame false %x\n",s32);
				}
			}
			switch(getStat(avePower, crossZeroRate))
			{
				case 3:
				if(gloab_bWeakUp)
				{
					gloab_iSlienceLen=0;
				}
				else
				{
					gloab_inoVoiceLast = 0;
				}
				saveFile(pfd, ptr, u32Len, pwav_hdr);
				break;
				case 1:
				case 2:
					if(gloab_bWeakUp)
					{
						gloab_istat = 1;
					}
					else
					{
						gloab_ivoicelastLast++;
						gloab_inoVoiceLast++;
						saveFile(pfd, ptr, u32Len, pwav_hdr);
					}
				break;
				default:
				case 0:
				if(gloab_bWeakUp)
				{
					gloab_istat = 0;
				}
				else
				{
					saveFile(pfd, ptr, u32Len, pwav_hdr);	
					gloab_inoVoiceLast++;
					if(gloab_inoVoiceLast >= MAXNOVOICELEN && !gloab_bWeakUp)
					{
						//沉默语音段时间过长，认为结束
						if(gloab_ivoicelastLast >= MINVOICELEN)
						{
							//大于最小的语音长度，不丢弃
							while(sphinxParm.isUsing)
							{
								//如果这个线程还在跑，那么等待
								usleep(100);
							}
							myDEBUG(__FUNCTION__, __LINE__, "start recognize");
							pwav_hdr->size_8 += pwav_hdr->data_size + (sizeof(wave_pcm_hdr) - 8);
							fprintf(pofd,"%20s : %s\n",outputTime(), "start resconize");
							pthread_create(&sphinxThread, 0, getRecognitionResult, &sphinxParm);
							//处理完成
							gloab_istat = 0;
						}	
						else
						{	
							//语音持续时间太短，不认为是人在说话
							clearFile(pfd, pwav_hdr);
							myDEBUG(__FUNCTION__, __LINE__, "the speech is too short and clear file");
							gloab_istat = 0;
						}
					}
				}
					
				break;
			}
			
		break;
		default:
			printf("default gloab_istat \n");
		break;			
	}
	return HI_SUCCESS;
}


/*******************************************************
*function:假设前一秒是用来确定噪声的门限
********************************************************/
HI_S32 initAveParam(void* ptr, HI_U32 u32Len)
{
	static HI_S16 count = 0;
	gloab_favePower += getAvePowerValue(ptr, u32Len);
	gloab_fcrossZeroRate += getZeroCrossRate(ptr, u32Len);
	fwrite(ptr, 1, u32Len, pfdinit);
	count++;
    if(count >= 150)
	{
		gloab_breadyRecing = HI_TRUE;
		gloab_fbaseAvePower = (float) gloab_favePower / count;
		gloab_favePower = RATEOFAVEPOWER * gloab_fbaseAvePower;
		gloab_fbaseCrossZeroRate = (float) gloab_fcrossZeroRate / count;
		gloab_fcrossZeroRate = RATEOFAVECROSSZERORATE * gloab_fbaseCrossZeroRate;
		printf("%10f  %10f\n",gloab_favePower, gloab_fcrossZeroRate);
		printf("ready to recognized\n");
	}
	return HI_SUCCESS;
}


/************************************************************
*function: get Frame from ai's api
**************************************************************/
void* getFrm(void* ptr)
{

	myDEBUG(__FUNCTION__, __LINE__, "enter geFr,");
	HI_S32 s32ret;
	THREAD_S* pthread = (void*)ptr;
	AUDIO_DEV aiDev = pthread->aiDev;
	AI_CHN aiChn = pthread->aiChn; 
	AUDIO_FRAME_S stAudioFrm;
	wave_pcm_hdr wav_hdr = default_wav_hdr;
	FILE* pfd = NULL;
	float favePower,fcrossZeroRate;

	pfd = fopen("./audio.wav", "w+");
    if(NULL == pfd)
    {
        printf("failed to open audio.wav\n");
        return;
    }
	pofd = fopen("./recResult","at+");
    if(NULL == pofd)
    {
        printf("failed to open recResult\n");
    }
	sphinxParm.pfd =pfd;
	sphinxParm.pwav_hdr = &wav_hdr;
	
	
	while(pthread->bStart)
	{
		//对每一帧，获取后必须释放，否则会无法销毁通道
		s32ret = HI_MPI_AI_GetFrame(aiDev, aiChn, &stAudioFrm, NULL, -1);
		if (HI_SUCCESS != s32ret)
		{
			printf("Audio get Frame failed %x\n", s32ret);
			return NULL;
		}
		favePower = getAvePowerValue(stAudioFrm.pVirAddr[0], stAudioFrm.u32Len);
		fcrossZeroRate = getZeroCrossRate(stAudioFrm.pVirAddr[0], stAudioFrm.u32Len);
		//init param or start recognize 
		if(gloab_breadyRecing)
			doubleThreshold(favePower, fcrossZeroRate, stAudioFrm.pVirAddr[0], stAudioFrm.u32Len, pfd, &wav_hdr,&stAudioFrm);
		else
			initAveParam(stAudioFrm.pVirAddr[0], stAudioFrm.u32Len);
		//fwrite(stAudioFrm.pVirAddr[0], 1, stAudioFrm.u32Len, pfd);
		s32ret = HI_MPI_AI_ReleaseFrame(aiDev, aiChn, &stAudioFrm, NULL);
		if (HI_SUCCESS != s32ret)
		{
			printf("AUDIO_ReleaseFrame failed\n");
			return NULL;
		}
	}
	
	fclose(pofd);
	fclose(pfd);
	return NULL;
}



int main()
{
	HI_S32 s32ret;
	AUDIO_DEV aiDev = AUDIO_IN_DEV;
	AUDIO_DEV aoDev = AUDIO_IN_DEV;
	AI_CHN aiChn = 0;
	AO_CHN aoChn = 0;
	AIO_ATTR_S stAioAttr;
	AI_CHN_PARAM_S stAiChnParam;
	THREAD_S g_thread;
	gloab_recBuf[0]='\0';
	pfdinit = fopen("./init.wav","w+");
    if(NULL == pfdinit)
    {
        printf("pfdinit open faile\n");
        return HI_SUCCESS;
    }
	stAioAttr.enSamplerate = AUDIO_SAMPLE_RATE_16000;
	stAioAttr.enBitwidth = 1;
	stAioAttr.enWorkmode = AIO_MODE_I2S_SLAVE;
	stAioAttr.enSoundmode = AUDIO_SOUND_MODE_MONO;
	stAioAttr.u32EXFlag = 0;
	stAioAttr.u32FrmNum = 30;		//AI DEV need to save at least 30 frm
	stAioAttr.u32PtNumPerFrm = 320; //must >=160
	stAioAttr.u32ChnCnt = 1;
	stAioAttr.u32ClkSel = 0;
	//step-1: init config for pocketSphinx
	sphinxParm.config = cmd_ln_init(NULL, ps_args(), TRUE,
									"-hmm",  "./xtrain/model_parameters/danale.cd_ptm_1000",
									"-lm",	 "./xtrain/etc/danale.lm.DMP",
									"-dict", "./xtrain/etc/danale.dic",
									NULL);
	//sphinxParm.config = cmd_ln_init(NULL, ps_args(), TRUE,
	//								"-hmm",  "./xtrain/model_parameters/danalecn.cd_ptm_1000",
	//								"-lm",	 "./xtrain/etc/danalecn.lm.DMP",
	//								"-dict", "./xtrain/etc/danalecn.dic",
	//								NULL);
	sphinxParm.ps = ps_init(sphinxParm.config);
	sphinxParm.hasRes = HI_FALSE;
	sphinxParm.isUsing = HI_FALSE;
	
	//step0: init vb
	VB_CONF_S stVbConf;
	memset(&stVbConf, 0, sizeof(VB_CONF_S));
	
	s32ret = SYS_Init(&stVbConf);
	if (HI_SUCCESS != s32ret)
	{
		printf("SYS_Init failed\n");
		return HI_FAILURE;
	}
	
	//setp1: start Ai and config ai chn
	s32ret = AUDIO_StartAi(aiDev, aiChn, &stAioAttr);
	if (HI_SUCCESS != s32ret)
	{
		printf("AUDIO_StartAi failed\n");
		return HI_FAILURE;
	}
	
	s32ret = AUDIO_StartAo(aoDev, aoChn, &stAioAttr);
	if (HI_SUCCESS != s32ret)
	{
		printf("AUDIO_StartAo failed\n");
		return HI_FAILURE;
	}
		

	s32ret = HI_MPI_AI_GetChnParam(aiDev, aiChn, &stAiChnParam);
	if (HI_SUCCESS != s32ret)
	{
		printf("Get ai Chn param failed\n");
		return s32ret;
	}
	
	stAiChnParam.u32UsrFrmDepth = 30;   //this is very important,default value is 0, and wil getFrame will not work
	
	s32ret = HI_MPI_AI_SetChnParam(aiDev, aiChn, &stAiChnParam);
	if (HI_SUCCESS != s32ret)
	{
		printf("Set ai Chn param failed\n");
		return s32ret;
	}


	//step2: get Frame
	g_thread.aiDev = aiDev;
	g_thread.aiChn = aiChn;
	g_thread.bStart = HI_TRUE;
	pthread_create(&g_thread.pid, 0, getFrm, (void*)&g_thread);
	getchar();
	g_thread.bStart =HI_FALSE;
	pthread_join(g_thread.pid,NULL);	
	fclose(pfdinit);
	
	
	//step3: stop Ai
	s32ret = AUDIO_StopAi(aiDev, aiChn);
	if (HI_SUCCESS != s32ret)
	{
		printf("AUDIO_StopAi failed\n");
		return HI_FAILURE;
	}

	s32ret = AUDIO_StopAo(aoDev, aoChn);
	if (HI_SUCCESS != s32ret)
	{
		printf("AUDIO_StopAo failed\n");
		return HI_FAILURE;
	}
	ps_free(sphinxParm.ps);
	cmd_ln_free_r(sphinxParm.config);
	printf("The End!\n");
	return 0;
}
