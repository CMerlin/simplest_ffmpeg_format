/**
 * 最简单的基于FFmpeg的视音频分离器
 * Simplest FFmpeg Demuxer
 *
 * 本程序可以将封装格式中的视频码流数据和音频码流数据分离出来。
 * 在该例子中， 将MPEG2TS的文件分离得到H.264视频码流文件和AAC
 * 音频码流文件。
 *
 * This software split a media file (in Container such as 
 * MKV, FLV, AVI...) to video and audio bitstream.
 * In this example, it demux a MPEG2TS file to H.264 bitstream
 * and AAC bitstream.
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

/**********************************************************************************
* Description:鎵撳嵃AVPacket鐩稿叧淇℃伅
* Inpurt pkt:AVPacket缁撴瀯浣�
***********************************************************************************/
void printAVPacketInfo(struct AVPacket *pkt)
{
	static int count = 0;

#if 0
	/*鍙墦鍗板嚑涓暟鎹寘*/
	count++;
	if((60 > count) || (90 < count)) {
		return;
	}
	if(0 == (count%2))
		return;
#endif
	//pkt.stream_index; //鍙互鍒ゆ柇鍑烘暟鎹被鍨嬫槸瑙嗛杩樻槸闊抽
	printf("[%s]:%d type=%d size:%d duration:%lld flag:%d pts:%lld dts:%lld line:%d\n", __func__, count, (pkt->stream_index), (pkt->size), (pkt->duration), (pkt->flags), (pkt->pts), (pkt->dts), __LINE__);

	return ;
}

/************************************************************************
* Description:璁＄畻闊宠棰戞枃浠剁殑闀垮害锛屽崟浣嶆槸绉�
* Return:瑙嗛鏂囦欢鐨勯暱搴�(鍗曚綅鏄)
***********************************************************************/
double getFileLen(AVStream *st)
{
	double ret = 0;
	ret = st->duration * av_q2d(st->time_base); /* 璁＄畻瑙嗛闀垮害鐨勬柟娉� */
	return ret;
}

/************************************************************************
* Desription:璁＄畻鏌愭、锛屽湪鏁翠釜瑙嗛涓殑鏃堕棿浣嶇疆
* Return锛氫綅缃�(鍗曚綅鏄)
************************************************************************/
double curPosition(double pts, AVStream *st)
{
	double ret = 0;
	ret = pts * av_q2d(st->time_base); /* 鐜板湪鍙互鏍规嵁pts鏉ヨ绠椾竴妗㈠湪鏁翠釜瑙嗛涓殑鏃堕棿浣嶇疆 */
	return ret;
}

/************************************************************************
* Description:鎶婅棰戣烦杞埌绗琋绉�
* Input pos:闇�瑕佽烦杞殑浣嶇疆锛堝崟浣嶆槸绉掞級
* Input stream_index锛氶�氶亾鍙凤紝鍏跺疄涔熷氨鏄椂闂寸殑绫诲瀷锛岃棰戞垨鏄煶棰�
* Reurun:0-鎴愬姛 -1-锛氬け璐�
*************************************************************************/
int seekFrame(long long pos, int stream_index, AVFormatContext *fmtctx)
{
	int ret = 0;
	int64_t timestamp = pos * AV_TIME_BASE;
	ret = av_seek_frame(fmtctx, stream_index, timestamp, AVSEEK_FLAG_BACKWARD);
	return ret;
}

int main(int argc, char* argv[])
{
	AVOutputFormat *ofmt_a = NULL,*ofmt_v = NULL;
	//（Input AVFormatContext and Output AVFormatContext）
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx_a = NULL, *ofmt_ctx_v = NULL;
	AVPacket pkt;
	int ret, i;
	int videoindex=-1,audioindex=-1;
	int frame_index=0;

	/*杈撳叆鍜岃緭鍑烘枃浠剁殑鍚嶇О*/
	const char *in_filename  = "cuc_ieschool.ts";//Input file URL
	//char *in_filename  = "cuc_ieschool.mkv";
	const char *out_filename_v = "cuc_ieschool.h264";//Output file URL
	//char *out_filename_a = "cuc_ieschool.mp3";
	const char *out_filename_a = "cuc_ieschool.aac";
	printf("[%s]:inFile=%s outFile=%s-%s line:%d\n", __func__, in_filename, out_filename_v, out_filename_a, __LINE__);

	av_register_all();
	//Input
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		printf("[%s][Error]:Could not open input file. line:%d\n", __func__, __LINE__);
		goto end;
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		printf("[%s][Error]:Failed to retrieve input stream information line:%d\n", __func__, __LINE__);
		goto end;
	}

	//Output
	avformat_alloc_output_context2(&ofmt_ctx_v, NULL, NULL, out_filename_v);
	if (!ofmt_ctx_v) {
		printf("[%s][Error]:Could not create output context line:%d\n", __func__, __LINE__);
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	ofmt_v = ofmt_ctx_v->oformat;

	avformat_alloc_output_context2(&ofmt_ctx_a, NULL, NULL, out_filename_a);
	if (!ofmt_ctx_a) {
		printf("[%s][Error]:Could not create output context line:%d\n", __func__, __LINE__);
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	ofmt_a = ofmt_ctx_a->oformat;

	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
			//Create output AVStream according to input AVStream
			AVFormatContext *ofmt_ctx;
			AVStream *in_stream = ifmt_ctx->streams[i];
			AVStream *out_stream = NULL;

			printf("[%s]:index=%d fileLen=%lf line:%d\n", __func__, i, getFileLen(in_stream), __LINE__);
			if(ifmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
				videoindex=i;
				out_stream=avformat_new_stream(ofmt_ctx_v, in_stream->codec->codec);
				ofmt_ctx=ofmt_ctx_v;
			}else if(ifmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
				audioindex=i;
				out_stream=avformat_new_stream(ofmt_ctx_a, in_stream->codec->codec);
				ofmt_ctx=ofmt_ctx_a;
			}else{
				break;
			}
			
			if (!out_stream) {
				printf( "[%s][Error]:Failed allocating output stream line:%d\n", __func__, __LINE__);
				ret = AVERROR_UNKNOWN;
				goto end;
			}
			//Copy the settings of AVCodecContext
			if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
				printf("[%s][Error]:Failed to copy context from input to output stream codec context line:%d\n", __func__, __LINE__);
				goto end;
			}
			out_stream->codec->codec_tag = 0;

			if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	printf("[%s]:nb_streams=%d videoindex=%d audioindex=%d line:%d\n", __func__, (ifmt_ctx->nb_streams), videoindex, audioindex,  __LINE__);
	//Dump Format------------------
	printf("\n==============Input Video=============\n");
	av_dump_format(ifmt_ctx, 0, in_filename, 0);
	printf("\n==============Output Video============\n");
	av_dump_format(ofmt_ctx_v, 0, out_filename_v, 1);
	printf("\n==============Output Audio============\n");
	av_dump_format(ofmt_ctx_a, 0, out_filename_a, 1);
	printf("\n======================================\n");
	//Open output file
	if (!(ofmt_v->flags & AVFMT_NOFILE)) {
		if (avio_open(&ofmt_ctx_v->pb, out_filename_v, AVIO_FLAG_WRITE) < 0) {
			printf("[%s][Error]:Could not open output file '%s' line:%d\n", __func__, out_filename_v, __LINE__);
			goto end;
		}
	}

	if (!(ofmt_a->flags & AVFMT_NOFILE)) {
		if (avio_open(&ofmt_ctx_a->pb, out_filename_a, AVIO_FLAG_WRITE) < 0) {
			printf("[%s][Error]:Could not open output file '%s' line:%d\n", __func__, out_filename_a, __LINE__);
			goto end;
		}
	}

	//Write file header
	if (avformat_write_header(ofmt_ctx_v, NULL) < 0) {
		printf("[%s][Error]:Error occurred when opening video output file line:%d\n", __func__, __LINE__);
		goto end;
	}
	if (avformat_write_header(ofmt_ctx_a, NULL) < 0) {
		printf("[%s][Error]:Error occurred when opening audio output file line:%d\n", __func__, __LINE__);
		goto end;
	}
	
#if USE_H264BSF
	AVBitStreamFilterContext* h264bsfc =  av_bitstream_filter_init("h264_mp4toannexb"); 
#endif

	while (1) {
		AVFormatContext *ofmt_ctx;
		AVStream *in_stream, *out_stream;
		//Get an AVPacket
		if (av_read_frame(ifmt_ctx, &pkt) < 0)
			break;
		in_stream  = ifmt_ctx->streams[pkt.stream_index];

		
		if(pkt.stream_index==videoindex){
			//printf("[%s]:curPos=%lf line:%d\n", __func__, (	curPosition((pkt.pts), (in_stream))), __LINE__);
			/*杈撳嚭瑙嗛鏂囦欢*/
			out_stream = ofmt_ctx_v->streams[0];
			ofmt_ctx=ofmt_ctx_v;
			//seekFrame(20, videoindex, ofmt_ctx);
			//printf("Write Video Packet. size:%d\tpts:%lld\n",pkt.size,pkt.pts);
#if USE_H264BSF
			av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
		}else if(pkt.stream_index==audioindex){
			/*杈撳嚭闊抽鏂囦欢*/
			out_stream = ofmt_ctx_a->streams[0];
			ofmt_ctx=ofmt_ctx_a;
			//printf("Write Audio Packet. size:%d\tpts:%lld\n",pkt.size,pkt.pts);
		}else{
			continue;
		}

		//printAVPacketInfo(&pkt); /* 杈撳嚭pkg鐩稿叧鐨勪俊鎭� */
		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		pkt.stream_index=0;
		//printAVPacketInfo(&pkt);
		//Write
		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
			printf("[%s][Error]:Error muxing packet line:%d\n", __func__, __LINE__);
			break;
		}
		//printf("Write %8d frames to output file\n",frame_index);
		av_free_packet(&pkt);
		frame_index++;
	}

#if USE_H264BSF
	av_bitstream_filter_close(h264bsfc);  
#endif

	//Write file trailer
	av_write_trailer(ofmt_ctx_a);
	av_write_trailer(ofmt_ctx_v);
end:
	avformat_close_input(&ifmt_ctx);
	/* close output */
	if (ofmt_ctx_a && !(ofmt_a->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx_a->pb);

	if (ofmt_ctx_v && !(ofmt_v->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx_v->pb);

	avformat_free_context(ofmt_ctx_a);
	avformat_free_context(ofmt_ctx_v);


	if (ret < 0 && ret != AVERROR_EOF) {
		printf("[%s][Error]:Error occurred. line:%d\n", __func__, __LINE__);
		return -1;
	}
	return 0;
}


