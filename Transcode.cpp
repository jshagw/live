#include "Transcode.h"



Transcode::Transcode()
{
}


Transcode::~Transcode()
{
	stop();
}

EnumMediaError Transcode::start(const char* inURL, const char* outURL, OutputType type)
{
	_inFile = std::make_unique<InputMediaFile>(inURL);
	auto ret = _inFile->open();
	if ( !MEDIA_SUCCESS(ret) )
	{
		return ret;
	}

	return add(outURL, type);
}

EnumMediaError Transcode::stop()
{
	{
		std::lock_guard<std::mutex> guard(_mutex);
		_outFiles.clear();
	}

	_inFile = nullptr;

	return EnumMediaError::OK;
}

EnumMediaError Transcode::add(const char* outURL, OutputType type)
{
	auto outFile = std::make_unique<OutputMediaFile>(outURL, _inFile.get(), type);
	auto ret = outFile->open();
	if (!MEDIA_SUCCESS(ret))
	{
		return ret;
	}

	std::lock_guard<std::mutex> guard(_mutex);
	_outFiles.push_back(std::move(outFile));

	return EnumMediaError::OK;
}

EnumMediaError Transcode::heartbeat()
{
	AVPacketPtr pkt;
	InputStreamPtr ist;
	auto ret = _inFile->read(pkt, ist);

	std::lock_guard<std::mutex> guard(_mutex);
	for (auto& file : _outFiles)
	{
		file->write(pkt.get(), ist.get());
	}

	return ret;
}
