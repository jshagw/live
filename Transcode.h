#pragma once
#ifndef __TRANSCODE_H__
#define __TRANSCODE_H__

#include <mutex>
#include <vector>

#include "InputMediaFile.h"
#include "OutputMediaFile.h"

class Transcode
{
public:
	Transcode();
	~Transcode();

public:
	EnumMediaError start(const char* inURL, const char* outURL, OutputType type);
	EnumMediaError stop();
	EnumMediaError add(const char* outURL, OutputType type);
	EnumMediaError heartbeat();

protected:
	// 转码是一对多的关系
	InputMediaFilePtr				_inFile;
	std::vector<OutputMediaFilePtr> _outFiles;
	std::mutex						_mutex;
};

#endif // !__TRANSCODE_H__
