#include "MediaFile.h"



MediaFile::MediaFile(const char* url)
	: _url(url)
{
	_ctx = nullptr;
}


MediaFile::~MediaFile()
{
	
}
