#include <stdio.h>
#include <memory>
#include <string>
#include <thread>
#include <iostream>

//#include "ScopeGuard.h"

extern "C"
{
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libavutil/avutil.h"
}

using namespace std;

void Init()
{
	//av_register_all();
	//avfilter_register_all();
	//avformat_network_init();
	av_log_set_level(AV_LOG_INFO);
}

#include "Transcode.h"

int main(int argc, char** argv)
{
	const char* fileInput = "rtsp://admin:admin12345@192.168.5.24/h264/ch1/sub/av_stream";
	const char* fileOutput = "rtmp://localhost/live/24";
	OutputType type = OutputType::RTMP;

	Init();

	Transcode tr;

	auto ret = tr.start(fileInput, fileOutput, type);
	if (!MEDIA_SUCCESS(ret))
	{
		return -1;
	}

	if (argc > 1 && strcmp(argv[1], "-hls") == 0)
	{
		fileOutput = "/opt/ts/hls/24/test.m3u8";
		type = OutputType::HLS;

		ret = tr.add(fileOutput, type);
		if (!MEDIA_SUCCESS(ret))
		{
			return -1;
		}
	}

	do
	{
		ret = tr.heartbeat();
		if (!MEDIA_SUCCESS(ret))
		{
			this_thread::sleep_for(chrono::microseconds(5));
		}
	} while (true);

	return 0;
}
