#include "OutputMediaFile.h"


OutputMediaFile::OutputMediaFile(const char* url, MediaFile* inStream, OutputType type)
	: MediaFile(url)
	, _inFile(inStream)
	, _type(type)
{
}


OutputMediaFile::~OutputMediaFile()
{
	close();
}

EnumMediaError OutputMediaFile::open()
{
	close();

	const char* format_name = "flv";

	int ret = avformat_alloc_output_context2(&_ctx, nullptr, format_name, _url.c_str());
	if (ret < 0)
	{
		return EnumMediaError::OUT_MEMORY;
	}

	// 根据输入流创建输出流
	const AVFormatContext* inCtx = _inFile->getAVFormatContext();
	for (unsigned int i = 0; i < inCtx->nb_streams; i++)
	{
		auto ost = std::make_shared<OutputStream>();
		//ost->enc_ctx = avcodec_alloc_context3(nullptr);

		auto inStream = inCtx->streams[i];
		auto outStream = avformat_new_stream(_ctx, nullptr);

		//ret = avcodec_parameters_to_context(ost->enc_ctx, inStream->codecpar);

		//ost->enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		//ret = avcodec_parameters_from_context(outStream->codecpar, ost->enc_ctx);
		avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);

		ret = avformat_transfer_internal_stream_timing_info(_ctx->oformat, outStream, inStream, AVFMT_TBCF_AUTO);

		// copy timebase while removing common factors
		if (outStream->time_base.num <= 0 || outStream->time_base.den <= 0)
		{
			outStream->time_base = av_add_q(av_stream_get_codec_timebase(outStream), { 0, 1 });
		}

		// copy estimated duration as a hint to the muxer
		if (outStream->duration <= 0 && inStream->duration > 0)
		{
			outStream->duration = av_rescale_q(inStream->duration, inStream->time_base, outStream->time_base);
		}

		// copy disposition
		outStream->disposition = inStream->disposition;

		if (inStream->nb_side_data) {
			for (decltype(inStream->nb_side_data) i = 0; i < inStream->nb_side_data; i++) {
				const auto sd_src = &inStream->side_data[i];

				auto dst_data = av_stream_new_side_data(outStream, sd_src->type, sd_src->size);
				if (!dst_data)
				{
					return EnumMediaError::OUT_MEMORY;
				}

				memcpy(dst_data, sd_src->data, sd_src->size);
			}
		}

		auto par_dst = outStream->codecpar;
		switch (par_dst->codec_type)
		{
		case AVMEDIA_TYPE_AUDIO:
			if ((par_dst->block_align == 1 || par_dst->block_align == 1152 || par_dst->block_align == 576)
				&& par_dst->codec_id == AV_CODEC_ID_MP3)
			{
				par_dst->block_align = 0;
			}
			if (par_dst->codec_id == AV_CODEC_ID_AC3)
			{
				par_dst->block_align = 0;
			}
			break;
		case AVMEDIA_TYPE_VIDEO:
			if (inStream->sample_aspect_ratio.num)
			{
				outStream->sample_aspect_ratio = par_dst->sample_aspect_ratio = inStream->sample_aspect_ratio;
			}			
			outStream->avg_frame_rate = inStream->avg_frame_rate;
			outStream->r_frame_rate = inStream->r_frame_rate;
			break;
		default:
			break;
		}

		ost->mux_timebase = inStream->time_base;

		_streams.push_back(ost);
	}

	ret = avio_open2(&_ctx->pb, _url.c_str(), AVIO_FLAG_WRITE | AVIO_FLAG_NONBLOCK, nullptr, nullptr);
	if (ret < 0)
	{
		return EnumMediaError::OPEN_FAILED;
	}

	ret = avformat_write_header(_ctx, nullptr);
	if (ret < 0)
	{
		return EnumMediaError::WRITE_HEADER_FAILED;
	}
	
	return EnumMediaError::OK;
}

EnumMediaError OutputMediaFile::close()
{
	if (_ctx)
	{
		avio_closep(&_ctx->pb);
		avformat_free_context(_ctx);
		_ctx = nullptr;
	}

	_streams.clear();

	return EnumMediaError::OK;
}

EnumMediaError OutputMediaFile::write(const AVPacket* const pkt, const InputStream* const ist)
{
	auto ost = _streams[pkt->stream_index];
	auto inStream = _inFile->getAVFormatContext()->streams[pkt->stream_index];
	auto outStream = _ctx->streams[pkt->stream_index];

	//int64_t start_time = 0;
	//auto ost_tb_start_time = av_rescale_q(start_time, AV_TIME_BASE_Q, ost->mux_timebase);
	AVPacket opkt = { 0 };

	av_init_packet(&opkt);

	av_packet_copy_props(&opkt, pkt);

	/* force the input stream PTS */
	if (pkt->pts != AV_NOPTS_VALUE)
		opkt.pts = av_rescale_q(pkt->pts, inStream->time_base, ost->mux_timebase)/* - ost_tb_start_time*/;
	else
		opkt.pts = AV_NOPTS_VALUE;

	if (pkt->dts == AV_NOPTS_VALUE)
		opkt.dts = av_rescale_q(ist->dts, AV_TIME_BASE_Q, ost->mux_timebase);
	else
		opkt.dts = av_rescale_q(pkt->dts, inStream->time_base, ost->mux_timebase);
	//opkt.dts -= ost_tb_start_time;

	if (outStream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && pkt->dts != AV_NOPTS_VALUE) {
		int duration = av_get_audio_frame_duration(ist->dec_ctx, pkt->size);
		if (!duration)
			duration = ist->dec_ctx->frame_size;
		opkt.dts = opkt.pts = av_rescale_delta(inStream->time_base, pkt->dts,
			{1, ist->dec_ctx->sample_rate}, duration, &ost->filter_in_rescale_delta_last,
			ost->mux_timebase)/* - ost_tb_start_time*/;
	}

	opkt.duration = av_rescale_q(pkt->duration, inStream->time_base, ost->mux_timebase);

	opkt.flags = pkt->flags;

	if (pkt->buf) {
		opkt.buf = av_buffer_ref(pkt->buf);
		if (!opkt.buf)
		{
			return EnumMediaError::OUT_MEMORY;
		}
	}
	opkt.data = pkt->data;
	opkt.size = pkt->size;

	opkt.stream_index = pkt->stream_index;

	// timestamp必须重新计算一下
	av_packet_rescale_ts(&opkt, ost->mux_timebase, outStream->time_base);

	auto err = av_interleaved_write_frame(_ctx, &opkt);
	if (err < 0)
	{
		print_error(err);

		// todo 处理断线重连
		if (EPIPE == AVUNERROR(err))
		{
			return EnumMediaError::DISCONNECT;
		}

		return EnumMediaError::FAILED;
	}

	return EnumMediaError::OK;
}
