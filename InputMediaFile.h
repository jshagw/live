#pragma once
#ifndef __INPUT_MEDIA_FILE_H__
#define __INPUT_MEDIA_FILE_H__

#include "MediaFile.h"
#include "InputStream.h"

class InputMediaFile : public MediaFile
{
public:
	InputMediaFile(const char* url);
	virtual ~InputMediaFile();

public:
	virtual EnumMediaError open();
	virtual EnumMediaError close();

public:
	EnumMediaError read(AVPacketPtr& packet, InputStreamPtr& ist);

private:
	int64_t _ts_offset;		// timestamp offset
	int64_t _last_ts;

private:	
	Vector_InputStream _streams;
};

typedef std::unique_ptr<InputMediaFile> InputMediaFilePtr;

#endif // !__INPUT_STREAM_H__
