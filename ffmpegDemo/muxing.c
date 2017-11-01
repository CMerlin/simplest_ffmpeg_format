/*
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * libavformat API example.
 *
 * Output a media file in any supported libavformat format. The default
 * codecs are used.
 * @example muxing.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#define STREAM_DURATION   10.0 /* 控制录制的文件的长度 */
//#define STREAM_DURATION   10.0

#define STREAM_FRAME_RATE 25 /* 25 images/s */
//#define STREAM_PIX_FMT    AV_PIX_FMT_YUV422P; //AV_PIX_FMT_YUV420P /* default pix_fmt */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */


#define SCALE_FLAGS SWS_BICUBIC

#if 1 /*自定义的结构体*/
/*视频属性信息*/
typedef struct _CodeVAttr{
	/*分辨率*/
	int width;
	int height;
	int bit_rate; /*码率*/
	int frame_rate; /*帧率 帧/秒*/
	int codec_id; /*编码类型*/
} CODEV_ATTR, *P_CODEV_ATTR; 
/*音频属性信息*/
typedef struct _CodeAAttr{
	/*分辨率*/
	int width;
	int height;
	int bit_rate; /*码率*/
	int sample_rate; /*采样率*/
	int codec_id; /*编码类型*/
} CODEA_ATTR, *P_CODEA_ATTR; 

#endif

// a wrapper around a single output AVStream
typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc;

    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;

    AVFrame *frame;
    AVFrame *tmp_frame;

    float t, tincr, tincr2;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
} OutputStream;

/*******************************************************************************
* Description:打印AVPacket相关的信息
******************************************************************************/
static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
#if 0 /*只显示音频数据 OR 只显示视频数据*/
	if(1 == pkt->stream_index){
		return;
	}
#endif
    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
}

/*****************************************************************************************
* Description:将数据写如何到文件中
* Input fmt_ctx:输出文件
* Input:pkt-已经经过编码的数据
* Return:
******************************************************************************************/
static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
    /* rescale output packet timestamp values from codec to stream timebase */
    av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;
    /* Write the compressed frame to the media file. */
    //log_packet(fmt_ctx, pkt);
    return av_interleaved_write_frame(fmt_ctx, pkt);
}

/*************************************************************
* Description:音视频编码器相关设置信息
**************************************************************/
int initCodeAttr(CODEV_ATTR *videoAttr, CODEA_ATTR *audioAttr){
	/*视频的*/
	videoAttr->frame_rate = STREAM_FRAME_RATE;
	videoAttr->bit_rate = 400000;
	videoAttr->width    = 352;
    videoAttr->height   = 288;
	videoAttr->frame_rate = 25;
	//videoAttr->width    = 1920; //720;
    //videoAttr->height   =1080; //480;
    //videoAttr->width    = 720;
    //videoAttr->height   = 480;
	/*音频的*/
	audioAttr->bit_rate	= 64000;
	audioAttr->sample_rate = 44100;
	
	return 0;
}


/*******************************************************************************************
* Description:设置视频编码器的相关信息
* Input c:设置后的编码参数
* Output:pattr-编码信息
* Return:0-成功
********************************************************************************************/
int setCodeAttrV(AVCodecContext *c, CODEV_ATTR* pattr)
{
	     //c->codec_id = codec_id;
		 c->codec_id = pattr->codec_id;

        //c->bit_rate = 400000;
        c->bit_rate = (pattr->bit_rate);
        /* Resolution must be a multiple of two. */
        //c->width    = 352;
        //c->height   = 288;
        c->width    = (pattr->width);
        c->height   = (pattr->height);
        /* timebase: This is the fundamental unit of time (in seconds) in terms
         * of which frame timestamps are represented. For fixed-fps content,
         * timebase should be 1/framerate and timestamp increments should be
         * identical to 1. */
        //ost->st->time_base = (AVRational){ 1, STREAM_FRAME_RATE };
        //c->time_base       = ost->st->time_base;
         c->time_base       = (AVRational){ 1, (pattr->frame_rate) };

        c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
        c->pix_fmt       = STREAM_PIX_FMT;
        if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
            /* just for testing, we also add B-frames */
            c->max_b_frames = 2;
        }
        if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
            /* Needed to avoid using macroblocks in which some coeffs overflow.
             * This does not happen with normal video, it just happens here as
             * the motion of the chroma plane does not match the luma plane. */
            c->mb_decision = 2;
        }
	return 0;
}

/*******************************************************************************************
* Description:设置音频编码器的相关信息
* Input c:设置后的编码参数
* Output： pattr-编码信息 codec-编码器的一些信息
* Return:0-成功
********************************************************************************************/
int setCodeAttrA(AVCodecContext *c, CODEA_ATTR* pattr, AVCodec *codec)
{
#if 1
		int i = 0;
		c->sample_fmt  = codec->sample_fmts ?
				codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
			//c->bit_rate	 = 64000;
			//c->sample_rate = 44100;
			c->bit_rate    = pattr->bit_rate;
			c->sample_rate = pattr->sample_rate;
			if (codec->supported_samplerates) {
				c->sample_rate = codec->supported_samplerates[0];
				for (i = 0; codec->supported_samplerates[i]; i++) {
					if (codec->supported_samplerates[i] == 44100)
						//c->sample_rate = 44100;
						c->sample_rate = (pattr->sample_rate);
				}
			}
			c->channels 	   = av_get_channel_layout_nb_channels(c->channel_layout);
			c->channel_layout = AV_CH_LAYOUT_STEREO;
			if (codec->channel_layouts) {
				c->channel_layout = codec->channel_layouts[0];
				for (i = 0; codec->channel_layouts[i]; i++) {
					if (codec->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
						c->channel_layout = AV_CH_LAYOUT_STEREO;
				}
			}
			c->channels 	   = av_get_channel_layout_nb_channels(c->channel_layout);
			//ost->st->time_base = (AVRational){ 1, c->sample_rate };
		return 0;
#endif

#if 0
	int i = 0;
	c->sample_fmt  = (*codec)->sample_fmts ?
            (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        //c->bit_rate    = 64000;
        //c->sample_rate = 44100;
        c->bit_rate    = pattr->bit_rate;
        c->sample_rate = pattr->sample_rate;
        if ((*codec)->supported_samplerates) {
            c->sample_rate = (*codec)->supported_samplerates[0];
            for (i = 0; (*codec)->supported_samplerates[i]; i++) {
                if ((*codec)->supported_samplerates[i] == 44100)
                    //c->sample_rate = 44100;
					c->sample_rate = (pattr->sample_rate);
            }
        }
        c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
        c->channel_layout = AV_CH_LAYOUT_STEREO;
        if ((*codec)->channel_layouts) {
            c->channel_layout = (*codec)->channel_layouts[0];
            for (i = 0; (*codec)->channel_layouts[i]; i++) {
                if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                    c->channel_layout = AV_CH_LAYOUT_STEREO;
            }
        }
        c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
        //ost->st->time_base = (AVRational){ 1, c->sample_rate };
	return 0;
#endif
}

/*******************************************************************************************
* Description:创建流通道
* Input:oc
* Output:st
* Return:0-成功 0<-失败
******************************************************************************************/
int add_stream_to_file(AVFormatContext *oc, AVStream **st){
#if 1
	/* 创建流通道 */
    *st = avformat_new_stream(oc, NULL);
    if (!(*st)) {
        fprintf(stderr, "Could not allocate stream\n");
		return -1;
    }
    (*st)->id = oc->nb_streams-1;
#endif
	return 0;
}

int setCodeAttr(AVCodecContext *enc, OutputStream *ost, AVCodec **codec, CODEV_ATTR *videoAttr, CODEA_ATTR *audioAttr)
{
#if 0
	CODEV_ATTR videoAttr = {0};
	CODEA_ATTR audioAttr = {0};
	initCodeAttr(&videoAttr, &audioAttr);
#endif
	AVCodecContext *c;
	//c = ost->enc;
	c = enc;

	switch ((*codec)->type) {
	case AVMEDIA_TYPE_VIDEO:
		{
			ost->st->time_base = (AVRational){ 1, (videoAttr->frame_rate) };
			setCodeAttrV(c, videoAttr);
			break;
		}
	case AVMEDIA_TYPE_AUDIO:
		{
			ost->st->time_base = (AVRational){ 1, (audioAttr->sample_rate) };
			setCodeAttrA(c, audioAttr, *codec);
			break;
		}
	default:
		break;
	}

	return 0;
}

/***************************************************************************************
* Description:添加一个输出流
* 
****************************************************************************************/
/* Add an output stream. */
static void add_stream(OutputStream *ost, AVFormatContext *oc,
                       AVCodec **codec,
                       enum AVCodecID codec_id)
{
    AVCodecContext *c;
    int i;

	printf("[%s]:codec_id=%d line:%d\n", __func__, codec_id, __LINE__);
    /* 查找编码器 */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        exit(1);
    }

#if 0
	/* 创建流通道 */
    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }
    ost->st->id = oc->nb_streams-1;
#endif
	//add_stream_to_file(oc, &(ost->st));
	/* 解码网络流 */
	c = avcodec_alloc_context3(*codec);
    if (!c) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        exit(1);
    }
	ost->enc = c;
#if 0
	CODEV_ATTR videoAttr = {0};
	CODEA_ATTR audioAttr = {0};
	initCodeAttr(&videoAttr, &audioAttr);
	switch ((*codec)->type) {
	case AVMEDIA_TYPE_VIDEO:
	{
		ost->st->time_base = (AVRational){ 1, (videoAttr.frame_rate) };
		setCodeAttrV(c, &videoAttr);
		break;
	}
	case AVMEDIA_TYPE_AUDIO:
	{
		ost->st->time_base = (AVRational){ 1, (audioAttr.sample_rate) };
		setCodeAttrA(c, &audioAttr, *codec);
		break;
	}
	default:
		break;
	}
#endif
#if 0
	/*****************************************************************
	* Description:检查编码类型，根据类型设置相关的参数
	******************************************************************/
    switch ((*codec)->type) {
    case AVMEDIA_TYPE_AUDIO:
        c->sample_fmt  = (*codec)->sample_fmts ?
            (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        c->bit_rate    = 64000;
        c->sample_rate = 44100;
        if ((*codec)->supported_samplerates) {
            c->sample_rate = (*codec)->supported_samplerates[0];
            for (i = 0; (*codec)->supported_samplerates[i]; i++) {
                if ((*codec)->supported_samplerates[i] == 44100)
                    c->sample_rate = 44100;
            }
        }
        c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
        c->channel_layout = AV_CH_LAYOUT_STEREO;
        if ((*codec)->channel_layouts) {
            c->channel_layout = (*codec)->channel_layouts[0];
            for (i = 0; (*codec)->channel_layouts[i]; i++) {
                if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                    c->channel_layout = AV_CH_LAYOUT_STEREO;
            }
        }
        c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
        ost->st->time_base = (AVRational){ 1, c->sample_rate };
        break;

    case AVMEDIA_TYPE_VIDEO:
        c->codec_id = codec_id;

        c->bit_rate = 400000;
        /* Resolution must be a multiple of two. */
        c->width    = 352;
        c->height   = 288;
        /* timebase: This is the fundamental unit of time (in seconds) in terms
         * of which frame timestamps are represented. For fixed-fps content,
         * timebase should be 1/framerate and timestamp increments should be
         * identical to 1. */
        ost->st->time_base = (AVRational){ 1, STREAM_FRAME_RATE };
        c->time_base       = ost->st->time_base;

        c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
        c->pix_fmt       = STREAM_PIX_FMT;
        if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
            /* just for testing, we also add B-frames */
            c->max_b_frames = 2;
        }
        if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
            /* Needed to avoid using macroblocks in which some coeffs overflow.
             * This does not happen with normal video, it just happens here as
             * the motion of the chroma plane does not match the luma plane. */
            c->mb_decision = 2;
        }
    break;

    default:
        break;
    }
#endif
    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

/**************************************************************/
/* audio output */

static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
                                  uint64_t channel_layout,
                                  int sample_rate, int nb_samples)
{
    AVFrame *frame = av_frame_alloc();
    int ret;

    if (!frame) {
        fprintf(stderr, "Error allocating an audio frame\n");
        exit(1);
    }

    frame->format = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if (nb_samples) {
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            fprintf(stderr, "Error allocating an audio buffer\n");
            exit(1);
        }
    }

    return frame;
}

/*****************************************************************************************
 * Description:打开视频编码器
 **********************************************************************************************/
static void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
	AVCodecContext *c;
	int nb_samples;
    int ret;
    AVDictionary *opt = NULL;

    c = ost->enc;

    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
        exit(1);
    }

    /* init signal generator */
    ost->t     = 0;
    ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
    /* increment frequency by 110 Hz per second */
    ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

    if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
        nb_samples = 10000;
    else
        nb_samples = c->frame_size;

    ost->frame     = alloc_audio_frame(c->sample_fmt, c->channel_layout,
                                       c->sample_rate, nb_samples);
    ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
                                       c->sample_rate, nb_samples);

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }

    /* create resampler context */
        ost->swr_ctx = swr_alloc();
        if (!ost->swr_ctx) {
            fprintf(stderr, "Could not allocate resampler context\n");
            exit(1);
        }

        /* set options */
        av_opt_set_int       (ost->swr_ctx, "in_channel_count",   c->channels,       0);
        av_opt_set_int       (ost->swr_ctx, "in_sample_rate",     c->sample_rate,    0);
        av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
        av_opt_set_int       (ost->swr_ctx, "out_channel_count",  c->channels,       0);
        av_opt_set_int       (ost->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
        av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);

        /* initialize the resampling context */
        if ((ret = swr_init(ost->swr_ctx)) < 0) {
            fprintf(stderr, "Failed to initialize the resampling context\n");
            exit(1);
        }
}

/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
 * 'nb_channels' channels. */
static AVFrame *get_audio_frame(OutputStream *ost)
{
    AVFrame *frame = ost->tmp_frame;
    int j, i, v;
    int16_t *q = (int16_t*)frame->data[0];

    /* check if we want to generate more frames */
    if (av_compare_ts(ost->next_pts, ost->enc->time_base, STREAM_DURATION, (AVRational){ 1, 1 }) >= 0){
		//printf("[%s]:dont't new data frames! line:%d\n", __func__, __LINE__);
		return NULL;
	}

    for (j = 0; j <frame->nb_samples; j++) {
        v = (int)(sin(ost->t) * 10000);
        for (i = 0; i < ost->enc->channels; i++)
            *q++ = v;
        ost->t     += ost->tincr;
        ost->tincr += ost->tincr2;
    }

    frame->pts = ost->next_pts;
    ost->next_pts  += frame->nb_samples;

    return frame;
}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
/****************************************************************************
* Description:将媒体数据写入到录制文件中
* Return：1-成功 0-获取YUV数据失败或是编码YUV数据失败
******************************************************************************/
static int write_audio_frame(AVFormatContext *oc, OutputStream *ost)
{
    AVCodecContext *c;
    AVPacket pkt = { 0 }; // data and size must be 0;
    AVFrame *frame;
    int ret;
    int got_packet;
    int dst_nb_samples;

    av_init_packet(&pkt);
    c = ost->enc;

    frame = get_audio_frame(ost);

    if (frame) {
        /* convert samples from native format to destination codec format, using the resampler */
            /* compute destination number of samples */
            dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
                                            c->sample_rate, c->sample_rate, AV_ROUND_UP);
            av_assert0(dst_nb_samples == frame->nb_samples);

        /* when we pass a frame to the encoder, it may keep a reference to it
         * internally;
         * make sure we do not overwrite it here
         */
        ret = av_frame_make_writable(ost->frame);
        if (ret < 0){
			printf("[%s][Error]:line:%d\n", __func__, __LINE__);
			exit(1);
		}

        /* convert to destination format */
        ret = swr_convert(ost->swr_ctx,
                          ost->frame->data, dst_nb_samples,
                          (const uint8_t **)frame->data, frame->nb_samples);
        if (ret < 0) {
			printf("Error while converting\n");
            fprintf(stderr, "Error while converting\n");
            exit(1);
        }
        frame = ost->frame;

        frame->pts = av_rescale_q(ost->samples_count, (AVRational){1, c->sample_rate}, c->time_base);
        ost->samples_count += dst_nb_samples;
    }
#if 0  //下面两句我加的。编码前一定要给frame时间戳
	frame->pts = lastpts;
	lastpts = frame->pts + frame->nb_samples;
#endif
    ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
    if (ret < 0) {
		printf("[Error]:Error encoding audio frame: %s\n", av_err2str(ret));
        fprintf(stderr, "Error encoding audio frame: %s\n", av_err2str(ret));
        exit(1);
    }

    if (got_packet) {
        ret = write_frame(oc, &c->time_base, ost->st, &pkt);
        if (ret < 0) {
			printf("Error while writing audio frame: %s\n", av_err2str(ret));
            fprintf(stderr, "Error while writing audio frame: %s\n", av_err2str(ret));
            exit(1);
        }
    }

    return (frame || got_packet) ? 0 : 1;
}

/**************************************************************/
/* video output */

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
    AVFrame *picture;
    int ret;

    picture = av_frame_alloc();
    if (!picture)
        return NULL;

    picture->format = pix_fmt;
    picture->width  = width;
    picture->height = height;

    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(picture, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate frame data.\n");
        exit(1);
    }

    return picture;
}

/*****************************************************************************************
* Description:打开视频编码器
**********************************************************************************************/
static void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
    int ret;
    AVCodecContext *c = ost->enc;
    AVDictionary *opt = NULL;

    av_dict_copy(&opt, opt_arg, 0);

    /* 打开视频编码器 */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "[Error]:Could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }

	printf("[%s]:YUV420P=%d-%d line:%d\n", __func__, (c->width), (c->height), __LINE__); /* 需要创建的YUV图像的大小 */
    /* allocate and init a re-usable frame */
    ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!ost->frame) {
        fprintf(stderr, "[Error]:Could not allocate video frame\n");
        exit(1);
    }

    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    ost->tmp_frame = NULL;
    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
        if (!ost->tmp_frame) {
            fprintf(stderr, "[Error]:Could not allocate temporary picture\n");
            exit(1);
        }
    }

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "[Error]:Could not copy the stream parameters\n");
        exit(1);
    }
}

/************************************************************************
* Description:生成一帧数YUV数据
* Input:width height-生成的YUV数据的大小
* Output:pict-生长的YUV的数据
**************************************************************************/
static void fill_yuv_image(AVFrame *pict, int frame_index,
                           int width, int height)
{
    int x, y, i;

    i = frame_index;

    /* Y */
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;

    /* Cb and Cr */
    for (y = 0; y < height / 2; y++) {
        for (x = 0; x < width / 2; x++) {
            pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
            pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
        }
    }
}

/*******************************************************************************
 * Description:将其其它类型的YUV数据转化成YUV420的数据
 * Input:pix_fmt-源YUV数据的类型
* Input:width hight-转化后YUV数据的大小
* Return：0-成功 -1-失败
*******************************************************************************/
static int conversionYUV420(OutputStream *ost, enum AVPixelFormat pix_fmt, int width, int height){
#if 1
	/* as we only generate a YUV420P picture, we must convert it
	 * to the codec pixel format if needed */
        if (!ost->sws_ctx) {
			printf("[%s]:null line:%d\n", __func__, __LINE__);
            ost->sws_ctx = sws_getContext(width, height,AV_PIX_FMT_YUV420P, width, height, pix_fmt, SCALE_FLAGS, NULL, NULL, NULL);
            if (!ost->sws_ctx) {
                fprintf(stderr, "[Error]:Could not initialize the conversion context\n");
                exit(1);
            }
        }
        fill_yuv_image(ost->tmp_frame, ost->next_pts, width, height);
        sws_scale(ost->sws_ctx, (const uint8_t * const *)ost->tmp_frame->data, ost->tmp_frame->linesize, 0, height, ost->frame->data, ost->frame->linesize);
#endif
	return 0;
}

/*********************************************************************************
* Description:从yuv输入流中获取一帧YUV数据
* Input ost：YUV流
* Return：NULL != 获取到的YUV数据
**********************************************************************************/
static AVFrame *get_video_frame(OutputStream *ost)
{
    AVCodecContext *c = ost->enc;
	
    /* 检查是否到最大录制时长了，到了的话就结束录制 */
    if (av_compare_ts(ost->next_pts, c->time_base, STREAM_DURATION, (AVRational){ 1, 1 }) >= 0){
		//printf("[%s]:dont't new data frames! line:%d\n", __func__, __LINE__);
		return NULL;
	}

    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if (av_frame_make_writable(ost->frame) < 0){
		printf("[%s][Error]:line:%d\n", __func__, __LINE__);
		 exit(1);
	} 

	/* 生成特定格式的YUV数据给编码器 */
    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
		conversionYUV420(ost, (c->pix_fmt), (c->width), (c->height));
#if 0
	/* as we only generate a YUV420P picture, we must convert it
         * to the codec pixel format if needed */
        if (!ost->sws_ctx) {
            ost->sws_ctx = sws_getContext(c->width, c->height,
                                          AV_PIX_FMT_YUV420P,
                                          c->width, c->height,
                                          c->pix_fmt,
                                          SCALE_FLAGS, NULL, NULL, NULL);
            if (!ost->sws_ctx) {
				printf("[Error]:Could not initialize the conversion context\n");
                fprintf(stderr, "[Error]:Could not initialize the conversion context\n");
                exit(1);
            }
        }
        fill_yuv_image(ost->tmp_frame, ost->next_pts, c->width, c->height);
        sws_scale(ost->sws_ctx,
                  (const uint8_t * const *)ost->tmp_frame->data, ost->tmp_frame->linesize,
                  0, c->height, ost->frame->data, ost->frame->linesize);
#endif
    } else {
        fill_yuv_image(ost->frame, ost->next_pts, c->width, c->height);
    }

    ost->frame->pts = ost->next_pts++;

    return ost->frame;
}

/*
 * encode one video frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */

/*************************************************************************************************
* Description:
* Input oc:输出的设备的文件描述符（可以简单的看成一个视频文件描述符）
* Input ost:YUV数据流文件(可以看成流文件描述符)
* Return：1-成功 0-获取YUV数据失败或是编码YUV数据失败
***************************************************************************************************/
static int write_video_frame(AVFormatContext *oc, OutputStream *ost)
{
	int ret;
	AVCodecContext *c;
	AVFrame *frame;
	int got_packet = 0;
	AVPacket pkt = { 0 };

	c = ost->enc; /* 编码器 */
	frame = get_video_frame(ost); 	/*从流中获取一帧YUV数据*/

	/* 将YUV数据进行编码操作 */
	av_init_packet(&pkt); /*初始化pkt*/
	ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
	if (ret < 0) {
		fprintf(stderr, "[Error]:encoding video frame: %s\n", av_err2str(ret));
		exit(1);
	}
	/* 将编码号的数据写入到输出文件中 */
	if (got_packet) {
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
		if (ret < 0) {
			fprintf(stderr, "[Error]:while writing video frame: %s\n", av_err2str(ret));
			exit(1);
		}
	}
	return (frame || got_packet) ? 0 : 1;
}

static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}

#if 1
/************************************************************************************
 * description:打开输出文件
 * Output oc:相当于文件描述符
 * Input filePath:输出文件的路径
 *************************************************************************************/
int openOutFile(AVFormatContext **oc, const char *filePath)
{
	int ret = 0;
	/* 创建输出媒体的 context */
	avformat_alloc_output_context2(oc, NULL, NULL, filePath);
	if (!*oc) {
		printf("[%s]:Could not deduce output format from file extension: using MPEG. line:%d\n", __func__, __LINE__);
		avformat_alloc_output_context2(oc, NULL, "mpeg", filePath); /* 如果不能识别输出文件的格式，输出文件的格式设置成mpeg */
	}
	if (!*oc){
		return -1;
	}
	/* 打开输出文件 */
	if (!((*oc)->oformat->flags & AVFMT_NOFILE)) {
		ret = avio_open(&(*oc)->pb, filePath, AVIO_FLAG_WRITE);
		if(0 > ret)
			printf("[%s][Error]:Could not open output file:%s info:%s line:%d\n", __func__, filePath, av_err2str(ret), __LINE__);
		return ret;
	}
#if 0
	if (!(fmt->flags & AVFMT_NOFILE)) {
			ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
			if (ret < 0) {
				fprintf(stderr, "Could not open '%s': %s\n", filename,
						av_err2str(ret));
				return 1;
			}
		}
#endif
	
	return 0;
}

#endif

/*************************************************************************************
* Description:初始化视频编码器
* Output:
***********************************************************************************/
int initCodeVide(AVCodecContext **enc, AVFormatContext *oc,
                       AVCodec **codec, CODEV_ATTR *pattr, OutputStream *ost){
#if 1
	AVCodecContext *c;
    int i;
	
    /* 查找编码器 */
	printf("[%s]:codec_id=%d line:%d\n", __func__, (pattr->codec_id), __LINE__);
    *codec = avcodec_find_encoder(pattr->codec_id);
    if (!(*codec)) {
        fprintf(stderr, "[%s][Error]:Could not find encoder for '%s' line:%d\n", __func__, avcodec_get_name(pattr->codec_id), __LINE__);
        return -1;
    }
	//add_stream_to_file(oc, &(ost->st));
	/* 解码网络流 */
	c = avcodec_alloc_context3(*codec);
	 if (!c) {
	//(*enc) = avcodec_alloc_context3(*codec);
    //if (!(*enc)) {
        fprintf(stderr, "Could not alloc an encoding context\n");
		return -1;
    }
	 ost->enc = c;
	//(enc) = c;
	//(*enc) = c;
	/*设置编码器的属性*/
	if(AVMEDIA_TYPE_VIDEO != ((*codec)->type)){
		return -1; /* 如果不是视频编码器，不进行设置*/
	}
	//printf("[%s]:line:%d\n", __func__, __LINE__);
	setCodeAttrV(c, pattr);
	/* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER){
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	//printf("[%s]:line:%d\n", __func__, __LINE__);
#endif

#if 0
/* 解码网络流 */
	c = avcodec_alloc_context3(*codec);
    if (!c) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        exit(1);
    }
	ost->enc = c;
#endif

#if 0
	/* 添加音视频流，并默认的格式进行编码 */
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
		//fmt->video_codec = AV_CODEC_ID_H264; /*H264编码方式*/
        add_stream(&video_st, oc, &video_codec, fmt->video_codec);
        have_video = 1;
        encode_video = 1;
    }

	/* 打开音视频编码器，并设置缓存空间 */
    if (have_video)
        open_video(oc, video_codec, &video_st, opt);
#endif
	return 0;
}

/*************************************************************************************
 * Description:初始化音频编码器
 ***********************************************************************************/
int initCodeAudio(AVCodecContext **enc, AVFormatContext *oc,
		AVCodec **codec, CODEA_ATTR *pattr, OutputStream *ost){
#if 1
	AVCodecContext *c;
	int i;

	/* 查找编码器 */
	*codec = avcodec_find_encoder(pattr->codec_id);
	if (!(*codec)) {
		fprintf(stderr, "[%s][Error]:Could not find encoder for '%s'\n", __func__, avcodec_get_name(pattr->codec_id));
		return -1;
	}
	//add_stream_to_file(oc, &(ost->st));
	/* 解码网络流 */
	c = avcodec_alloc_context3(*codec);
	 if (!c) {
	//(*enc) = avcodec_alloc_context3(*codec);
    //if (!(*enc)) {
        fprintf(stderr, "Could not alloc an encoding context\n");
		return -1;
    }
	ost->enc = c;
	//(enc) = &c;
	//(*enc) = c;
	/*设置编码器的属性*/
	if(AVMEDIA_TYPE_AUDIO != ((*codec)->type)){
		return -1; /* 如果不是视频编码器，不进行设置*/
	}
	setCodeAttrA(c, pattr, (*codec));
	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER){
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
#endif

#if 0
if (fmt->audio_codec != AV_CODEC_ID_NONE) {
	   add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
	   have_audio = 1;
	   encode_audio = 1;
   }

#endif
	return 0;
}

/*************************************************************************************
*
***********************************************************************************/



/**************************************************************/
/* media file output */
int main(int argc, char **argv)
{
    OutputStream video_st = { 0 }, audio_st = { 0 };
    const char *filename;
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVCodec *audio_codec, *video_codec;
    int ret = 0, i = 0;
    int have_video = 0, have_audio = 0;
    int encode_video = 0, encode_audio = 0;
    AVDictionary *opt = NULL;
    CODEV_ATTR videoAttr = {0};
	CODEA_ATTR audioAttr = {0};

	initCodeAttr(&videoAttr, &audioAttr);
    /* Initialize libavcodec, and register all codecs and formats. */
    av_register_all();

    if (argc < 2) {
#if 0
        printf("[%s]:usage: %s output_file\n"
               "API example program to output a media file with libavformat.\n"
               "This program generates a synthetic audio and video stream, encodes and\n"
               "muxes them into a file named output_file.\n"
               "The output format is automatically guessed according to the file extension.\n"
               "Raw images can also be output by using '%%d' in the filename.\n"
               " line:%d\n", __func__, argv[0], __LINE__);
#endif
		return 1;
    }

    filename = argv[1];
#if 0
    for (i = 2; i+1 < argc; i+=2) {
        if (!strcmp(argv[i], "-flags") || !strcmp(argv[i], "-fflags"))
            av_dict_set(&opt, argv[i]+1, argv[i+1], 0);
    }
#endif

#if 0
    /* 创建输出媒体的 context */
    avformat_alloc_output_context2(&oc, NULL, NULL, filename);
    if (!oc) {
        printf("[%s]:Could not deduce output format from file extension: using MPEG. line:%d\n", __func__, __LINE__);
        avformat_alloc_output_context2(&oc, NULL, "mpeg", filename); /* 如果不能识别输出文件的格式，输出文件的格式设置成mpeg */
    }
    if (!oc)
        return 1;
#endif
	/* 输出文件相关的信息 */
	if(0 > openOutFile(&oc, filename)){
		return 1;
	}
    fmt = oc->oformat;
	videoAttr.codec_id = fmt->video_codec;
	audioAttr.codec_id = fmt->audio_codec;

#if 1
	/* 添加音视频流，并默认的格式进行编码 */
    /* Add the audio and video streams using the default format codecs
     * and initialize the codecs. */
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
		//fmt->video_codec = AV_CODEC_ID_H264; /*H264编码方式*/
		add_stream_to_file(oc, &(video_st.st));
        add_stream(&video_st, oc, &video_codec, fmt->video_codec);
        have_video = 1;
        encode_video = 1;
    }
    if (fmt->audio_codec != AV_CODEC_ID_NONE) {
		add_stream_to_file(oc, &(audio_st.st));
        add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
        have_audio = 1;
        encode_audio = 1;
    }
	setCodeAttr((video_st.enc), &video_st, (&video_codec), &videoAttr,  &audioAttr);
	setCodeAttr((audio_st.enc), &audio_st, (&audio_codec), &videoAttr, &audioAttr);
#endif

#if 0
	/* 音视频编码相关的信息 */
	initCodeAttr(&videoAttr, &audioAttr);
	videoAttr.codec_id = fmt->video_codec;
	audioAttr.codec_id = fmt->audio_codec;
	/*编码器初始化*/
	add_stream_to_file(oc, &(video_st.st));
	initCodeVide(&(video_st.enc), oc, &(video_codec), &videoAttr, &video_st);
	printf("[%s]:line:%d\n", __func__, __LINE__);
	//video_st.st->time_base = video_st.enc->time_base; /*时间戳*/
	video_st.st->time_base = (AVRational){ 1, (videoAttr.frame_rate) };
	//printf("[%s]:line:%d\n", __func__, __LINE__);
	

	add_stream_to_file(oc, &(audio_st.st));
	initCodeAudio(&(audio_st.enc), oc, &(audio_codec), &audioAttr, &audio_st);
	//audio_st.st->time_base = audio_st.enc->time_base; /*时间戳*/
	audio_st.st->time_base = (AVRational){ 1, (audioAttr.sample_rate) };
	printf("[%s]:line:%d\n", __func__, __LINE__);
	/*打开编码器*/
	setCodeAttr(&video_st, (&video_codec));
	setCodeAttr(&audio_st, (&audio_codec));
#endif

	/* 打开音视频编码器，并设置缓存空间 */
    /* Now that all the parameters are set, we can open the audio and
     * video codecs and allocate the necessary encode buffers. */
    if (have_video)
        open_video(oc, video_codec, &video_st, opt);

    if (have_audio)
        open_audio(oc, audio_codec, &audio_st, opt);


    av_dump_format(oc, 0, filename, 1);
	printf("[%s]:line:%d\n", __func__, __LINE__);
#if 0
	/* 打开输出文件 */
    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open '%s': %s\n", filename,
                    av_err2str(ret));
            return 1;
        }
    }
#endif

	/* 写输出文件的头 */
    ret = avformat_write_header(oc, &opt);
    if (ret < 0) {
        printf("[%s]:Error occurred when opening output file: %s line:%d\n", __func__, av_err2str(ret), __LINE__);
        return 1;
    }

	printf("[%s]:recording begin! line:%d\n", __func__, __LINE__);
	/* 当写视频和音频数据都失败的时候，停止录制 */
	while (encode_video || encode_audio) {
        /* select the stream to encode */
        if (encode_video &&
            (!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base,
                                            audio_st.next_pts, audio_st.enc->time_base) <= 0)) {
            encode_video = !write_video_frame(oc, &video_st); /* 写视频数据 */
        } else {
            encode_audio = !write_audio_frame(oc, &audio_st); /* 写音频数据 */
        }
		//printf("[%s]audio=%d video=%d line:%d\n", __func__, encode_audio, encode_video, __LINE__);
    }
	printf("[%s]:recording over! line:%d\n", __func__, __LINE__);
	
	/* 写文件的尾 */
    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    av_write_trailer(oc);

    /* Close each codec. */
    if (have_video)
        close_stream(oc, &video_st);
    if (have_audio)
        close_stream(oc, &audio_st);

    if (!(fmt->flags & AVFMT_NOFILE))
        /* Close the output file. */
        avio_closep(&oc->pb);

    /* free the stream */
    avformat_free_context(oc);

    return 0;
}
