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

AVFormatContext* g_context = 0;
AVFormatContext* g_outputContext = 0;
int64_t g_timestamp = 0;

void Init()
{
	//av_register_all();
	//avfilter_register_all();
	//avformat_network_init();
	av_log_set_level(AV_LOG_INFO);
}

void remove_avoptions(AVDictionary **a, AVDictionary *b)
{
	AVDictionaryEntry *t = NULL;

	while ((t = av_dict_get(b, "", t, AV_DICT_IGNORE_SUFFIX))) {
		av_dict_set(a, t->key, NULL, AV_DICT_MATCH_CASE);
	}
}

int OpenInput(const char *fileName)
{
	//AVFormatContext *ic = nullptr;

	g_context = avformat_alloc_context();
	//if ( nullptr == ic )
	//{
	//	return -1;
	//}

	//AVDictionary *format_opts = nullptr;
	//av_dict_set(&format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);

	g_context->flags |= AVFMT_FLAG_NONBLOCK;

	int ret = avformat_open_input(&g_context, fileName, nullptr, nullptr);
	if (ret < 0)
	{
		cout << "avformat_open_input failed" << endl;
		return  ret;
	}

	//av_dict_set(&format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);
	//remove_avoptions(&format_opts, nullptr);

	ret = avformat_find_stream_info(g_context, nullptr);
	if (ret < 0)
	{
		cout << "avformat_find_stream_info failed" << endl;
		return  ret;
	}

	g_timestamp = 0;
	if (g_context->start_time != AV_NOPTS_VALUE)
		g_timestamp += g_context->start_time;

/*	for (unsigned int i = 0; i < g_context->nb_streams; i++)
	{
		auto codecpar = g_context->streams[i]->codecpar;
		
		auto codec = avcodec_find_decoder(codecpar->codec_id);	
		if (nullptr == codec)
		{
			cout << "avcodec_find_decoder " << avcodec_get_name(codecpar->codec_id) << " failed" << endl;
			return -1;
		}

		cout << "avcodec_find_decoder " << avcodec_get_name(codecpar->codec_id) << endl;

		auto codecContext = avcodec_alloc_context3(codec);
		if (nullptr == codecContext)
		{
			cout << "avcodec_alloc_context3 failed" << endl;
			return -2;
		}
		cout << "avcodec_alloc_context3 " << endl;

		ret = avcodec_open2(codecContext, codec, nullptr);
		if ( ret < 0 )
		{
			cout << "avcodec_open2 failed" << endl;
			return -3;
		}
		cout << "avcodec_open2 " << endl;
	}
	*/
	return ret;
}

void CloseInput()
{
	avformat_close_input(&g_context);
}

int OpenOutput(const char *fileName)
{
	char strError[AV_ERROR_MAX_STRING_SIZE] = { 0 };
	int ret = 0;
	ret = avformat_alloc_output_context2(&g_outputContext, nullptr, "flv", fileName);
	if (ret < 0)
	{
		cout << "avformat_alloc_output_context2 failed" << endl;
		goto Error;
	}
	cout << "avformat_alloc_output_context2 " << endl;

	for (unsigned int i = 0; i < g_context->nb_streams; i++)
	{
		AVCodecContext* avcodecctx = avcodec_alloc_context3(nullptr);
		AVStream * stream = avformat_new_stream(g_outputContext, nullptr);

		//avcodecctx->codec_type = g_context->streams[i]->codecpar->codec_type;
	
		ret = avcodec_parameters_to_context(avcodecctx, g_context->streams[i]->codecpar);

		avcodecctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		ret = avcodec_parameters_from_context(stream->codecpar, avcodecctx);
		//avcodec_parameters_copy(stream->codecpar, g_context->streams[i]->codecpar);

		//ret = avcodec_copy_context(stream->codec, g_context->streams[i]->codec);
		//if (nullptr == stream)
		//{
		//	cout << "avcodec_parameters_copy failed" << endl;
		//	goto Error;
		//}
		cout << "avformat_new_stream " << endl;
	
		ret = avformat_transfer_internal_stream_timing_info(g_outputContext->oformat, stream, g_context->streams[i], AVFMT_TBCF_AUTO);

		avcodec_free_context(&avcodecctx);
	}

	ret = avio_open2(&g_outputContext->pb, fileName, AVIO_FLAG_WRITE, nullptr, nullptr);
	if (ret < 0)
	{
		cout << "avio_open2 failed" << endl;
		goto Error;
	}
	cout << "avio_open2 " << endl;

	ret = avformat_write_header(g_outputContext, nullptr);
	if (ret < 0)
	{
		cout << "avformat_write_header failed: " << av_make_error_string( strError, sizeof(strError), ret ) << endl;
		goto Error;
	}

	return ret;
Error:
	if (g_outputContext)
	{
		for (unsigned int i = 0; i < g_outputContext->nb_streams; i++)
		{
			//avcodec_close(g_outputContext->streams[i]->codec);
		}

		if (g_outputContext->pb)
		{
			avio_closep(&g_outputContext->pb);
		}

		avformat_free_context(g_outputContext);
	}

	return ret;
}

void CloseOutput()
{
	if (g_outputContext)
	{
		for (unsigned int i = 0; i < g_outputContext->nb_streams; i++)
		{
			//avcodec_close(g_outputContext->streams[i]->codec);
		}

		if (g_outputContext->pb)
		{
			avio_closep(&g_outputContext->pb);
		}

		avformat_free_context(g_outputContext);
	}
}

#include "Transcode.h"

int main(int argc, char** argv)
{
	const char* fileInput = "rtsp://admin:admin12345@192.168.5.24/h264/ch1/sub/av_stream";
	const char* fileOutput = "rtmp://localhost/live/24";

	Init();

	Transcode tr;

	auto ret = tr.start(fileInput, fileOutput, OutputType::RTMP);
	if (!MEDIA_SUCCESS(ret))
	{
		return -1;
	}

	do
	{
		ret = tr.heartbeat();
		if (!MEDIA_SUCCESS(ret))
		{
			this_thread::sleep_for(chrono::microseconds(5));
		}
	} while (true);

/*	if (OpenInput(fileInput) < 0)
	{
		cout << "Open file Input failed!" << endl;
		this_thread::sleep_for(chrono::seconds(100));
		return 0;
	}

	if (OpenOutput(fileOutput) < 0)
	{
		cout << "Open file Output failed!" << endl;
		this_thread::sleep_for(chrono::seconds(100));
		return 0;
	}

	auto timebase = av_q2d(g_context->streams[0]->time_base);

	AVPacket pkt;

	bool bFirst = true;

	while (true)
	{
		if ( 0 == av_read_frame(g_context, &pkt) )
		{
			AVStream* st = g_context->streams[pkt.stream_index];

			//cout << "read frame" << endl;
			int ret = av_interleaved_write_frame(g_outputContext, &pkt);
			//int ret = av_write_frame(g_outputContext, &packet);
			if (ret < 0)
			{
				cout << "send failed" << endl;
			}

			av_packet_unref(&pkt);
		}
		else
		{
			cout << "no frame!" << endl;
			this_thread::sleep_for(chrono::microseconds(5));
		}
	}

	CloseInput();
	CloseOutput();
	cout << "Transcode file end!" << endl;
	this_thread::sleep_for(chrono::seconds(100));
*/	
	return 0;
}
