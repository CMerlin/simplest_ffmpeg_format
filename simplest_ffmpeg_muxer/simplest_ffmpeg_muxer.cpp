/**
 * 最简单的基于FFmpeg的视音频复用器
 * Simplest FFmpeg Muxer
 *
 * 本程序可以将视频码流和音频码流打包到一种封装格式中。
 * 程序中将AAC编码的音频码流和H.264编码的视频码流打包成
 * MPEG2TS封装格式的文件。
 * 需要注意的是本程序并不改变视音频的编码格式。
 *
 * This software mux a video bitstream and a audio bitstream 
 * together into a file.
 * In this example, it mux a H.264 bitstream (in MPEG2TS) and 
 * a AAC bitstream file together into MP4 format file.
 *
 */

#include <stdio.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavformat/avformat.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#ifdef __cplusplus
};
#endif
#endif

/*
FIX: H.264 in some container format (FLV, MP4, MKV etc.) need 
"h264_mp4toannexb" bitstream filter (BSF)
  *Add SPS,PPS in front of IDR frame
  *Add start code ("0,0,0,1") in front of NALU
H.264 in some container (MPEG2TS) don't need this BSF.
*/
//'1': Use H.264 Bitstream Filter 
#define USE_H264BSF 0

/*
FIX:AAC in some container format (FLV, MP4, MKV etc.) need 
"aac_adtstoasc" bitstream filter (BSF)
*/
//'1': Use AAC Bitstream Filter 
#define USE_AACBSF 0

/*********************************************************************************************
* Description:几个常用结构体的定义
**********************************************************************************************/
typedef struct _AVCount{
	int AInCount;
	int VInCount;
	int AOutCount;
	int VOutCount;
} AVCount, *P_AVCount;

/**********************************************************************************************
* Description:打开媒体文件，输入的视频文件和音频文件
**********************************************************************************************/
int openInUrl(AVFormatContext **AVFContext, const char *url)
{
	int ret = 0;
	if ((ret = avformat_open_input(AVFContext, url, 0, 0)) < 0) /*打开输入的视频h264文件*/
	{
		printf( "[%s][Error]:Could not open url=%s. line:%d", __func__, url, __LINE__);
		return -1;
		//goto end;
	}
	if ((ret = avformat_find_stream_info(*AVFContext, 0)) < 0) 
	{
		printf( "[%s][Error]:Failed to retrieve input stream information line:%d", __func__, __LINE__);
		return -1;
		//goto end;
	}
	return 0;
}

/********************************************************************************************
 * Description:在输出流中增加视频通道
 *********************************************************************************************/
int addStreamV(AVFormatContext **PinAVFctx, AVFormatContext **PoutAVFctx, AVCount *AVCount)
{
	int i = 0, ret = 0;
	AVFormatContext *inAVFctx = *PinAVFctx, *outAVFctx = *PoutAVFctx;


	for (i = 0; i < inAVFctx->nb_streams; i++) /*对每一路视频流进行处理*/
	{
		//(根据输入流创建输出流)Create output AVStream according to input AVStream
		printf("[%s]:video codec_type=%d line:%d\n", __func__, (inAVFctx->streams[i]->codec->codec_type), __LINE__);
		if(inAVFctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) /*媒体数据的类型是视频*/
		{
			AVStream *in_stream = inAVFctx->streams[i]; /*第i路输入视频流*/
			AVStream *out_stream = avformat_new_stream(outAVFctx, in_stream->codec->codec); /*在输出文件中增加一个视频通道*/
			//videoindex_v=i;
			if (!out_stream) {
				printf( "[%s][Error]:Failed allocating output stream line:%d\n", __func__, __LINE__);
				ret = AVERROR_UNKNOWN;
				return -1;
				//goto end;
			}
			AVCount->VInCount = i;
			AVCount->VOutCount = out_stream->index;
			//videoindex_out=out_stream->index;
			//(复制AVCodecContext的设置)Copy the settings of AVCodecContext
			if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
				printf( "[%s][Error]:Failed to copy context from input to output stream codec context line:%d\n", __func__, __LINE__);
				return -1;
				//goto end;
			}
			out_stream->codec->codec_tag = 0;
			if (outAVFctx->oformat->flags & AVFMT_GLOBALHEADER)
				out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
			break;
		}
	}

	return 0;
}

/********************************************************************************************
 * Description:在输出流中增加音频通道
 *********************************************************************************************/
int addStreamA(AVFormatContext **PinAVFctx, AVFormatContext **PoutAVFctx, AVCount *AVCount)
{
	int i = 0, ret = 0;
	AVFormatContext *inAVFctx = *PinAVFctx, *outAVFctx = *PoutAVFctx;

	for (i = 0; i < inAVFctx->nb_streams; i++) {
		//(根据输入流创建输出流)Create output AVStream according to input AVStream
		printf("[%s]:audio codec_type=%d line:%d\n", __func__, (inAVFctx->streams[i]->codec->codec_type), __LINE__);
		if(inAVFctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
			AVStream *in_stream = inAVFctx->streams[i]; /*第i路输入音频流*/
			AVStream *out_stream = avformat_new_stream(outAVFctx, in_stream->codec->codec); /*在输出文件中增加一个音频通道*/
			//audioindex_a=i;
			if (!out_stream) {
				printf("[%s][Error]:Failed allocating output stream line:%d\n", __func__, __LINE__);
				ret = AVERROR_UNKNOWN;
				return -1;
				//goto end;
			}
			AVCount->AInCount = i;
			AVCount->AOutCount = out_stream->index;
			//audioindex_out=out_stream->index;
			//(复制AVCodecContext的设置)Copy the settings of AVCodecContext
			if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
				printf("[%s][Error]:Failed to copy context from input to output stream codec context line:%d\n", __func__, __LINE__);
				return -1;
				//goto end;
			}
			out_stream->codec->codec_tag = 0;
			if (outAVFctx->oformat->flags & AVFMT_GLOBALHEADER)
				out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

			break;
		}
	}
	return 0;
}

/*******************************************************************************************
 * Description:根据pts计算 dts和duration 视频
 ********************************************************************************************/
int calculatePKTDataV(AVStream **Pin_stream, AVPacket *pkt)
{
	static int frame_index = 0;
	AVStream *in_stream = *Pin_stream;
	//Write PTS
	AVRational time_base1=in_stream->time_base;
	//Duration between 2 frames (us)
	int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
	//printf("[%s][mDebug1]:frame_index=%d size=%d calc_duration=%lld pts=%lld dts=%lld duration=%lld line:%d\n", __func__, frame_index, (pkt.size), calc_duration,(pkt.pts), (pkt.dts), (pkt.duration), __LINE__);
	
	//Parameters
	pkt->pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
	pkt->dts=pkt->pts;
	pkt->duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
	frame_index++;
	return 0;
}

/*******************************************************************************************
 * Description:根据pts计算 dts和duration 音频
 ********************************************************************************************/
int	calculatePKTDataA(AVStream **Pin_stream, AVPacket *pkt)
{
	static int frame_index = 0;
	AVStream *in_stream = *Pin_stream;
	//Write PTS
	AVRational time_base1=in_stream->time_base;
	//Duration between 2 frames (us)
	int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
	//Parameters
	pkt->pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
	pkt->dts=pkt->pts;
	pkt->duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
	frame_index++;
	//aFrameCount++;
	//printf("[%s]:=%d aFrameCount line:%d\n", __func__, aFrameCount, __LINE__);
	return 0;
}


/*********************************************************************************************
 * Description:在输出流中增加视和音频通道
 *
 *********************************************************************************************/
int initOutStram(AVFormatContext **PinAVFctx, AVFormatContext **PoutAVFctx, AVCount *AVCount)
{
	int i = 0, ret = 0;
	AVFormatContext *inAVFctx = *PinAVFctx, *outAVFctx = *PoutAVFctx;

	printf("[%s]:init ifmt_ctx_v nb_streams=%d line:%d\n", __func__, (inAVFctx->nb_streams), __LINE__);
	for (i = 0; i < inAVFctx->nb_streams; i++) /*对每一路视频流进行处理*/
	{
		AVStream *in_stream = inAVFctx->streams[i]; /*第i路输入视频流*/
		AVStream *out_stream = avformat_new_stream(outAVFctx, in_stream->codec->codec); /*在输出文件中增加一个视频通道*/
		//memset(in_stream, 0, sizeof(AVStream));
		//memset(out_stream, 0, sizeof(out_stream));
		//videoindex_v=i;
		if (!out_stream) {
			printf( "[%s][Error]:Failed allocating output stream line:%d\n", __func__, __LINE__);
			ret = AVERROR_UNKNOWN;
			return ret;
			//goto end;
		}
		//videoindex_out=out_stream->index;
		//(复制AVCodecContext的设置)Copy the settings of AVCodecContext
		if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
			printf( "[%s][Error]:Failed to copy context from input to output stream codec context line:%d\n", __func__, __LINE__);
			return -1;
			//goto end;
		}
		out_stream->codec->codec_tag = 0;
		if (outAVFctx->oformat->flags & AVFMT_GLOBALHEADER){
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
			break;
		}
		//(根据输入流创建输出流)Create output AVStream according to input AVStream
		printf("[%s]:video codec_type=%d line:%d\n", __func__, (inAVFctx->streams[i]->codec->codec_type), __LINE__);
		if(inAVFctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
			AVCount->VInCount = i;
			AVCount->VOutCount = out_stream->index;
		}
		else if(inAVFctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
			AVCount->AInCount = i;
			AVCount->AOutCount = out_stream->index;
		}
	}
	return 0;
}

/*********************************************************************************************
 * Description：merlin 将AAC和H264封装成MP4文件.
 * *******************************************************************************************/
int main(int argc, char* argv[])
{
	AVCount AVCounter;
	int counterV = 0, counterA = 0;
	
	int vFrameCount = 0, aFrameCount = 0;
	AVOutputFormat *ofmt = NULL;
	//Input AVFormatContext and Output AVFormatContext
	AVFormatContext *ifmt_ctx_v = NULL, *ifmt_ctx_a = NULL,*ofmt_ctx = NULL;
	AVPacket pkt;
	int ret, i;
	int videoindex_v=-1,videoindex_out=-1;
	int audioindex_a=-1,audioindex_out=-1;
	int frame_index=0;
	int64_t cur_pts_v=0,cur_pts_a=0;

	//const char *in_filename_v = "cuc_ieschool.ts";//Input file URL
	const char *in_filename_v = "cuc_ieschool.h264";
	//const char *in_filename_a = "cuc_ieschool.mp3";
	//const char *in_filename_a = "gowest.m4a";
	const char *in_filename_a = "gowest.aac";
	//const char *in_filename_a = "huoyuanjia.mp3";

	const char *out_filename = "cuc_ieschool.mp4";//Output file URL
	av_register_all();

	/*打开所有输入文件*/
	openInUrl(&ifmt_ctx_v, in_filename_v);
	openInUrl(&ifmt_ctx_a, in_filename_a);

	/*调用调试函数向终端打印出一些媒体信息*/
	printf("[%s]:===========Input Information========== line:%d\n", __func__, __LINE__);
	//av_dump_format(ifmt_ctx_v, 0, in_filename_v, 0); /*打印输入的视频文件相关信息*/
	//av_dump_format(ifmt_ctx_a, 0, in_filename_a, 0); /*打印输入的音频文件的相关信息*/
	printf("[%s]:====================================== line:%d\n", __func__, __LINE__);
	//Output
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename); /*初始化输出流结构体*/
	if (!ofmt_ctx) {
		printf( "[%s][Error]:Could not create output context line:%d\n", __func__, __LINE__);
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	ofmt = ofmt_ctx->oformat; /*容器的输出格式，也就是输出文件的类型*/
	memset(&AVCounter,0, sizeof(AVCounter));
	addStreamV(&ifmt_ctx_v, &ofmt_ctx, &AVCounter);
	addStreamA(&ifmt_ctx_a, &ofmt_ctx, &AVCounter);
	videoindex_v = AVCounter.VInCount;
	audioindex_a = AVCounter.AInCount;
	videoindex_out = AVCounter.VOutCount;
	audioindex_out = AVCounter.AOutCount;

	printf("[%s]:==========Output Information========== line:%d\n", __func__, __LINE__);
	av_dump_format(ofmt_ctx, 0, out_filename, 1); /*打印输出文件的相关信息*/
	printf("[%s]:====================================== line:%d\n", __func__, __LINE__);
	//(打开输出文件)Open output file
	printf("[%s]:****** output_file name:%s ****** line:%d\n", __func__, out_filename, __LINE__);
	if (!(ofmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE) < 0) {
			printf( "[%s][Error]:Could not open output file '%s' line:%d\n", __func__, out_filename, __LINE__);
			goto end;
		}
	}
	//(写文件的头部)Write file header
	if (avformat_write_header(ofmt_ctx, NULL) < 0) {
		printf( "[%s][Error]:Error occurred when opening output file line:%d\n", __func__, __LINE__);
		goto end;
	}


	//FIX
#if USE_H264BSF
	AVBitStreamFilterContext* h264bsfc =  av_bitstream_filter_init("h264_mp4toannexb"); 
#endif
#if USE_AACBSF
	AVBitStreamFilterContext* aacbsfc =  av_bitstream_filter_init("aac_adtstoasc"); 
#endif

	printf("[%s]:****** write file video data ****** line:%d\n", __func__, __LINE__);
	while (1) {
		AVFormatContext *ifmt_ctx;
		int stream_index=0;
		AVStream *in_stream, *out_stream;

		//Get an AVPacket
		if(av_compare_ts(cur_pts_v,ifmt_ctx_v->streams[videoindex_v]->time_base,cur_pts_a,ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0)
		{
			counterV++;
			ifmt_ctx=ifmt_ctx_v;
			stream_index=videoindex_out;

			if(av_read_frame(ifmt_ctx, &pkt) >= 0) /*读取一个完整的视频帧*/
			{
				do{
					in_stream  = ifmt_ctx->streams[pkt.stream_index];
					out_stream = ofmt_ctx->streams[stream_index];

					if(pkt.stream_index==videoindex_v)
					{
						//FIX：No PTS (Example: Raw H.264)
						//Simple Write PTS
						if(pkt.pts==AV_NOPTS_VALUE){
							calculatePKTDataV(&in_stream, &pkt);
						}

						cur_pts_v=pkt.pts;
						break;
					}
				}while(av_read_frame(ifmt_ctx, &pkt) >= 0); /*读取完整的数据帧*/
			}else{
				break;
			}
		}else
		{
			counterA++;
			ifmt_ctx=ifmt_ctx_a;
			stream_index=audioindex_out;
			if(av_read_frame(ifmt_ctx, &pkt) >= 0) /*读取完整的数据帧*/
			{
				do{
					in_stream  = ifmt_ctx->streams[pkt.stream_index];
					out_stream = ofmt_ctx->streams[stream_index];

					if(pkt.stream_index==audioindex_a)
					{

						//FIX：No PTS
						//Simple Write PTS
						if(pkt.pts==AV_NOPTS_VALUE){
							calculatePKTDataA(&in_stream, &pkt);
						}
						cur_pts_a=pkt.pts;

						break;
					}
				}while(av_read_frame(ifmt_ctx, &pkt) >= 0); /*读取完整的数据帧*/
			}else{
				break;
			}

		}

		//FIX:Bitstream Filter
#if USE_H264BSF
		av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
#if USE_AACBSF
		av_bitstream_filter_filter(aacbsfc, out_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif


		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		pkt.stream_index=stream_index;

		//printf("[%s]:Write 1 Packet. size:%5d\tpts:%lld line:%d\n", __func__, pkt.size,pkt.pts, __LINE__);
		//Write
		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) /*将视频数据写入到文件中*/ 
		{
			printf( "[%s][Error]: muxing packet line:%d\n", __func__, __LINE__);
			break;
		}
		av_free_packet(&pkt);

	}
#if 1 /*输出的MP4文件的相关信息*/
	printf("[%s]:vFrameCount=%d aFrameCount=%d counterV=%d counterA=%d line:%d\n", __func__, vFrameCount, aFrameCount, counterV, counterA, __LINE__);
#endif
	//Write file trailer
	av_write_trailer(ofmt_ctx); /*写文件尾*/

#if USE_H264BSF
	av_bitstream_filter_close(h264bsfc);
#endif
#if USE_AACBSF
	av_bitstream_filter_close(aacbsfc);
#endif

end:
	avformat_close_input(&ifmt_ctx_v);
	avformat_close_input(&ifmt_ctx_a);
	/* close output */
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);
	if (ret < 0 && ret != AVERROR_EOF) {
		printf( "[%s][Error]:Error occurred. line:%d\n", __func__, __LINE__);
		return -1;
	}
	return 0;
}


