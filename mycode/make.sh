arm-hisiv600-linux-gcc getAudio.c -o getAudio -Wall -g  -I/home/znr1995/3516CV500/mpp/sample/audio/../common -I/home/znr1995/3516CV500/mpp/include -I/home/znr1995/3516CV500/mpp/component/acodec -I/home/znr1995/3516CV500/osal/include -I/home/znr1995/3516CV500/drv/extdrv/tlv320aic31 -Dhi3516cv300 -DHICHIP=0x3516C300 -DSENSOR_TYPE=SONY_IMX290_MIPI_1080P_30FPS -I/sphinxEM/3516CV500/include -L/sphinxEM/3516CV500/lib -DHI_RELEASE -DHI_XXXX -lpocketsphinx -lsphinxad -lsphinxbase  -lpthread -lm -ldl -DISP_V2 -DHI_ACODEC_TYPE_INNER -mcpu=arm926ej-s -mno-unaligned-access -fno-aggressive-loop-optimizations -ffunction-sections -fdata-sections -ldl -lpthread -lm  /home/znr1995/3516CV500/mpp/sample/audio/../common/sample_comm_audio.o  /home/znr1995/3516CV500/mpp/sample/audio/../common/sample_comm_sys.o  /home/znr1995/3516CV500/mpp/lib/libmpi.a /home/znr1995/3516CV500/mpp/lib/libive.a /home/znr1995/3516CV500/mpp/lib/libmd.a /home/znr1995/3516CV500/mpp/lib/libVoiceEngine.a /home/znr1995/3516CV500/mpp/lib/libupvqe.a /home/znr1995/3516CV500/mpp/lib/libdnvqe.a /home/znr1995/3516CV500/mpp/lib/lib_hiae.a /home/znr1995/3516CV500/mpp/lib/libisp.a /home/znr1995/3516CV500/mpp/lib/libsns_imx290.a /home/znr1995/3516CV500/mpp/lib/lib_hiae.a /home/znr1995/3516CV500/mpp/lib/lib_hiawb.a /home/znr1995/3516CV500/mpp/lib/lib_hiaf.a /home/znr1995/3516CV500/mpp/lib/lib_hidefog.a
cp ./getAudio /sphinxEM/
