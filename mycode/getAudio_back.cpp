#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>

#include "myhead.h"
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
    1,
    1,
    16000,
    32000,
    2,
    16,
    {'d', 'a', 't', 'a'},
    0
};
int main()
{
	HI_S32 s32ret;
	AUDIO_DEV aiDev = AUDIO_IN_DEV;
	AI_CHN aiChn = 0;
	FILE *pfd = NULL;
	AIO_ATTR_S stAioAttr;
	AI_CHN_PARAM_S stAiChnParam;
	wave_pcm_hdr wav_hdr = default_wav_hdr;
	
	stAioAttr.enSamplerate = AUDIO_SAMPLE_RATE_16000;
	stAioAttr.enBitwidth = 1;
	stAioAttr.enWorkmode = AIO_MODE_I2S_MASTER;
	stAioAttr.enSoundmode = AUDIO_SOUND_MODE_MONO;
	stAioAttr.u32EXFlag = 0;
	stAioAttr.u32FrmNum = 30;		//AI DEV need to save at least 30 frm
	stAioAttr.u32PtNumPerFrm = 160; //must >=160
	stAioAttr.u32ChnCnt = 1;
	stAioAttr.u32ClkSel = 0;

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
	AUDIO_FRAME_S stAudioFrm;

	pfd = fopen("./audio.wav", "w+");
	fwrite(&wav_hdr, sizeof(wav_hdr) ,1, pfd); //添加wav音频头，使用采样率为16000
	int i;
	for (i = 0; i < 100; i++)
	{
		//对每一帧，获取后必须释放，否则会无法销毁通道
		s32ret = HI_MPI_AI_GetFrame(aiDev, aiChn, &stAudioFrm, NULL, -1);
		if (HI_SUCCESS != s32ret)
		{
			printf("Audio get Frame failed %x\n", s32ret);
			return HI_FAILURE;
		}
		
		fwrite(stAudioFrm.pVirAddr[0], 1, stAudioFrm.u32Len, pfd);
		wav_hdr.data_size += stAudioFrm.u32Len;
		s32ret = HI_MPI_AI_ReleaseFrame(aiDev, aiChn, &stAudioFrm, NULL);
		if (HI_SUCCESS != s32ret)
		{
			printf("AUDIO_ReleaseFrame failed\n");
			return HI_FAILURE;
		}
	}
		/* 修正wav文件头数据的大小 */
	wav_hdr.size_8 += wav_hdr.data_size + (sizeof(wav_hdr) - 8);
	
	/* 将修正过的数据写回文件头部,音频文件为wav格式 */
	fseek(pfd, 4, 0);
	fwrite(&wav_hdr.size_8, sizeof(wav_hdr.size_8), 1, pfd); //写入size_8的值
	fseek(pfd, 40, 0); //将文件指针偏移到存储data_size值的位置
	fwrite(&wav_hdr.data_size, sizeof(wav_hdr.data_size), 1, pfd); //写入data_size的值
	fclose(pfd);
	
	//step3: stop Ai
	s32ret = AUDIO_StopAi(aiDev, aiChn);
	if (HI_SUCCESS != s32ret)
	{
		printf("AUDIO_StopAi failed\n");
		return HI_FAILURE;
	}

	printf("The End!\n");
	return 0;
}
