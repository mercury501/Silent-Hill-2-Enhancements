/**
* Copyright (C) 2023 Gemini
*
* This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
* authors be held liable for any damages arising from the use of this software.
* Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
* applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
*      original  software. If you use this  software  in a product, an  acknowledgment in the product
*      documentation would be appreciated but is not required.
*   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
*      being the original software.
*   3. This notice may not be removed or altered from any source distribution.
*/

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