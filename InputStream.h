#pragma once
#ifndef __INPUT_STREAM_H__
#define __INPUT_STREAM_H__

extern "C"
{
#include "libavcodec/avcodec.h"
}

class InputStream
{
public:
	InputStream()
	{
		dec_ctx = nullptr;
		next_dts = dts = AV_NOPTS_VALUE;
		next_pts = pts = AV_NOPTS_VALUE;
		wrap_correction_done = 0;
		recv_first_packet = false;
		saw_first_ts = false;
	}

	~InputStream()
	{
		avcodec_free_context(&dec_ctx);
	}

public:
	AVCodecContext *dec_ctx;

	int64_t       next_dts;
	int64_t       dts;       ///< dts of the last packet read for this stream (in AV_TIME_BASE units)

	int64_t       next_pts;  ///< synthetic pts for the next decode frame (in AV_TIME_BASE units)
	int64_t       pts;       ///< current pts of the decoded frame  (in AV_TIME_BASE units)
	int           wrap_correction_done;

	bool		  recv_first_packet;
	bool		  saw_first_ts;
};

#include <memory>
#include <vector>
typedef std::shared_ptr<InputStream> InputStreamPtr;
typedef std::vector<InputStreamPtr> Vector_InputStream;

#endif // !__INPUT_STREAM_H__

