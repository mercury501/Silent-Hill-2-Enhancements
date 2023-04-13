#pragma once

extern "C"
{
#include "libavcodec\avcodec.h"
#include "libavfilter\avfilter.h"
#include "libavformat\avformat.h"
#include "libavutil\avutil.h"
#include "libavutil\channel_layout.h"
#include "libavutil\frame.h"
#include "libavutil\mem.h"
#include "libavutil\dict.h"
#include "libavutil\time.h"
#include "libswresample\swresample.h"
#include "libswscale\swscale.h"
#include "libavutil\imgutils.h"

#ifndef FFMPEG_LIBS
#define FFMPEG_LIBS
#pragma comment(lib, "libavcodec.a")
#pragma comment(lib, "libavfilter.a")
#pragma comment(lib, "libavformat.a")
#pragma comment(lib, "libavutil.a")
#pragma comment(lib, "libswresample.a")
#pragma comment(lib, "libswscale.a")
#endif
}

#include <vector>

class FFmpeg
{
public:
	int open(const char* name, int Lang_id);
	int open_flc(const char* name);
	void close();

	int decode_video(AVCodecContext* avctx, AVFrame* frame, AVPacket* pkt);
	int decode_audio(AVCodecContext* avctx, AVFrame* frame, AVPacket* pkt);

	void (*process_video)(AVFrame* frame) = nullptr;
	void (*process_audio)(AVFrame* frame) = nullptr;

	int packet_queue();
	int lang_to_astream(int lang_id);

	AVFormatContext* ctx;
	int video_stream, audio_stream;
	const AVCodec* vcodec, * acodec;
	std::vector<int> audio_streams;
	AVStream *astream, *vstream;
	AVCodecContext *actx, *vctx;
	AVCodecParserContext *aparse, *vparse;
	AVFrame *vframe, *vframeRGB,
		*aframe;

	SwsContext* sws;
	SwrContext* swr;
	uint8_t* vbuffer = nullptr;
	size_t vnumBytes = 0;
};