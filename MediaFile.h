#pragma once
#ifndef __MEDIA_FILE_H__
#define __MEDIA_FILE_H__

#include <string>
#include <memory>

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

typedef std::shared_ptr<AVPacket> AVPacketPtr;

enum class EnumMediaError
{
	OK = 0,
	FAILED = -1,
	AGAIN = -2,
	DISCONNECT = -3,
	OUT_MEMORY = -101,
	OPEN_FAILED	= -201,
	WRITE_HEADER_FAILED = -202,
	FIND_STREAM_INFO_FAILED = -203,
	BAD_INDEX = 301,
};

#define MEDIA_SUCCESS(r) (EnumMediaError::OK == r)

#ifdef _WINDOWS
#undef AV_TIME_BASE_Q
#define AV_TIME_BASE_Q          {1, AV_TIME_BASE}
#endif // WINDOWS


class MediaFile
{
public:
	MediaFile(const char* url);
	virtual ~MediaFile();

public:
	virtual EnumMediaError open() = 0;
	virtual EnumMediaError close() = 0;

public:
	const AVFormatContext* getAVFormatContext(void) const { return _ctx; }

protected:
	std::string			_url;	// Á÷µÄurl
	AVFormatContext*	_ctx;
};

extern "C"
{
#include "libavutil/error.h"
}

inline void print_error(int err)
{
	char str[AV_ERROR_MAX_STRING_SIZE]{ 0 };
	printf("%d, %s\n", AVUNERROR(err), av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, err));
}

#endif // !__STREAM_H__
