#pragma once
#ifndef __OUTPUT_STREAM_H__
#define __OUTPUT_STREAM_H__

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavutil/fifo.h"
}

class OutputStream
{
public:
	OutputStream()
	{
		//enc_ctx = nullptr;
		filter_in_rescale_delta_last = AV_NOPTS_VALUE;
	}

	~OutputStream()
	{
		//avcodec_free_context(&enc_ctx);
	}

public:
	//AVCodecContext *enc_ctx;

	AVRational		mux_timebase; // the timebase of the packets sent to the muxer

	int64_t			filter_in_rescale_delta_last;
};

#include <memory>
#include <vector>
typedef std::shared_ptr<OutputStream> OutputStreamPtr;
typedef std::vector<OutputStreamPtr> Vector_OutputStream;

#endif // !__OUTPUT_STREAM_H__

