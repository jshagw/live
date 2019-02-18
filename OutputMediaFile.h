#pragma once
#ifndef __OUTPUT_MEDIA_FILE_H__
#define __OUTPUT_MEDIA_FILE_H__

#include "MediaFile.h"
#include "InputStream.h"
#include "OutputStream.h"

enum class OutputType
{
	RTMP,
	HLS
};

class OutputMediaFile :
	public MediaFile
{
public:
	OutputMediaFile(const char* url, MediaFile* inFile, OutputType type);
	virtual ~OutputMediaFile();

public:
	virtual EnumMediaError open();
	virtual EnumMediaError close();

public:
	EnumMediaError write(const AVPacket* const pkt, const InputStream* const ist);

protected:
	MediaFile* _inFile;
	OutputType _type;

private:
	
	Vector_OutputStream _streams;
};

typedef std::unique_ptr<OutputMediaFile> OutputMediaFilePtr;

#endif // !__OUTPUT_STREAM_H__
