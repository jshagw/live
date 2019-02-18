#include "InputMediaFile.h"

extern "C"
{
#include "libavutil/avutil.h"
}

static constexpr int64_t dts_delta_threshold = 10;

InputMediaFile::InputMediaFile(const char* url)
	: MediaFile(url)
{
	_ts_offset = 0;
	_last_ts = AV_NOPTS_VALUE;
}


InputMediaFile::~InputMediaFile()
{
	close();
}

EnumMediaError InputMediaFile::open()
{
	_ctx = avformat_alloc_context();
	if (nullptr == _ctx)
	{
		return EnumMediaError::OUT_MEMORY;
	}

	_ctx->flags |= AVFMT_FLAG_NONBLOCK;

	int ret = avformat_open_input(&_ctx, _url.c_str(), nullptr, nullptr);
	if (ret < 0)
	{
		return  EnumMediaError::OPEN_FAILED;
	}

	ret = avformat_find_stream_info(_ctx, nullptr);
	if (ret < 0)
	{
		return EnumMediaError::FIND_STREAM_INFO_FAILED;
	}

	for (decltype(_ctx->nb_streams) i = 0; i < _ctx->nb_streams; i++)
	{
		auto st = _ctx->streams[i];
		auto par = st->codecpar;
		auto ist = std::make_shared<InputStream>();
		
		ist->dec_ctx = avcodec_alloc_context3(nullptr);
		if (nullptr == ist->dec_ctx)
		{
			return EnumMediaError::OUT_MEMORY;
		}

		ret = avcodec_parameters_to_context(ist->dec_ctx, par);
		if (ret < 0)
		{
			return EnumMediaError::FAILED;
		}

		switch (par->codec_type)
		{
		case AVMEDIA_TYPE_VIDEO:
			{
				// avformat_find_stream_info() doesn't set this for us anymore.
				ist->dec_ctx->framerate = st->avg_frame_rate;
			}
			break;
		case AVMEDIA_TYPE_AUDIO:
			{
				ist->dec_ctx->channel_layout = av_get_default_channel_layout(ist->dec_ctx->channels);
			}
			break;
		case AVMEDIA_TYPE_DATA:
		case AVMEDIA_TYPE_SUBTITLE:
		case AVMEDIA_TYPE_ATTACHMENT:
		case AVMEDIA_TYPE_UNKNOWN:
			break;
		default:
			return EnumMediaError::FAILED;
		}

		ret = avcodec_parameters_from_context(par, ist->dec_ctx);
		if (ret < 0) {
			return EnumMediaError::FAILED;
		}

		_streams.push_back(ist);
	}

	return EnumMediaError::OK;
}

EnumMediaError InputMediaFile::close()
{
	avformat_close_input(&_ctx);
	_streams.clear();

	return EnumMediaError::OK;
}

EnumMediaError InputMediaFile::read(AVPacketPtr& packet, InputStreamPtr& ist)
{
	packet = AVPacketPtr(
		av_packet_alloc()
		, [](AVPacket* p) { av_packet_free(&p); }
	);

	auto pkt = packet.get();

	auto ret = av_read_frame(_ctx, pkt);
	if (ret < 0)
	{
		if (AVERROR(EAGAIN) == ret )
		{
			return EnumMediaError::AGAIN;
		}
		else if (AVERROR(EPIPE) == ret)
		{
			// todo 处理断线重连
			return EnumMediaError::DISCONNECT;
		}
	}

	if ( pkt->stream_index < 0 || (unsigned int)pkt->stream_index >= _ctx->nb_streams )
	{
		return EnumMediaError::BAD_INDEX;
	}

	ist = _streams[pkt->stream_index];
	auto st = _ctx->streams[pkt->stream_index];

	if (!ist->wrap_correction_done && _ctx->start_time != AV_NOPTS_VALUE && st->pts_wrap_bits < 64)
	{
		// Correcting starttime based on the enabled streams
		// FIXME this ideally should be done before the first use of starttime but we do not know which are the enabled streams at that point.
		//       so we instead do it here as part of discontinuity handling
		if (ist->next_dts == AV_NOPTS_VALUE
			&& _ts_offset == -_ctx->start_time
			&& (_ctx->iformat->flags & AVFMT_TS_DISCONT))
		{
			int64_t new_start_time = INT64_MAX;

			for (decltype(_ctx->nb_streams) i = 0; i < _ctx->nb_streams; i++) {
				auto tmpStream = _ctx->streams[i];
				if (tmpStream->discard == AVDISCARD_ALL || tmpStream->start_time == AV_NOPTS_VALUE)
				{
					continue;
				}

				int64_t tmp_start_time = av_rescale_q(tmpStream->start_time, tmpStream->time_base, AV_TIME_BASE_Q);
				new_start_time = FFMIN(new_start_time, tmp_start_time);
			}

			if (new_start_time > _ctx->start_time) {
				_ts_offset = -new_start_time;
			}
		}

		auto stime = av_rescale_q(_ctx->start_time, AV_TIME_BASE_Q, st->time_base);
		auto stime2 = stime + (1LL << st->pts_wrap_bits);

		ist->wrap_correction_done = 1;

		if (stime2 > stime && pkt->dts != AV_NOPTS_VALUE && pkt->dts > stime + (1LL << (st->pts_wrap_bits - 1)))
		{
			pkt->dts -= 1ULL << st->pts_wrap_bits;
			ist->wrap_correction_done = 0;
		}

		if (stime2 > stime && pkt->pts != AV_NOPTS_VALUE && pkt->pts > stime + (1LL << (st->pts_wrap_bits - 1)))
		{
			pkt->pts -= 1ULL << st->pts_wrap_bits;
			ist->wrap_correction_done = 0;
		}
	}

	/* add the stream-global side data to the first packet */
	if (!ist->recv_first_packet) 
	{
		for (int i = 0; i < st->nb_side_data; i++)
		{
			auto src_sd = &st->side_data[i];

			if (src_sd->type == AV_PKT_DATA_DISPLAYMATRIX)
				continue;

			if (av_packet_get_side_data(pkt, src_sd->type, NULL))
				continue;

			auto dst_data = av_packet_new_side_data(pkt, src_sd->type, src_sd->size);
			if (nullptr == dst_data)
			{
				return EnumMediaError::OUT_MEMORY;
			}

			memcpy(dst_data, src_sd->data, src_sd->size);
		}

		ist->recv_first_packet = true;
	}

	if (pkt->dts != AV_NOPTS_VALUE)
	{
		pkt->dts += av_rescale_q(_ts_offset, AV_TIME_BASE_Q, st->time_base);
	}

	if (pkt->pts != AV_NOPTS_VALUE)
	{
		pkt->pts += av_rescale_q(_ts_offset, AV_TIME_BASE_Q, st->time_base);
	}

	if (pkt->dts != AV_NOPTS_VALUE)
	{
		_last_ts = av_rescale_q(pkt->dts, st->time_base, AV_TIME_BASE_Q);
	}

	if (!ist->saw_first_ts)
	{
		ist->dts = st->avg_frame_rate.num ? -ist->dec_ctx->has_b_frames * AV_TIME_BASE / av_q2d(st->avg_frame_rate) : 0;
		ist->pts = 0;

		if (pkt->pts != AV_NOPTS_VALUE) {
			ist->dts += av_rescale_q(pkt->pts, st->time_base, AV_TIME_BASE_Q);
			ist->pts = ist->dts; //unused but better to set it to a value thats not totally wrong
		}

		ist->saw_first_ts = true;
	}

	if (ist->next_dts == AV_NOPTS_VALUE)
	{
		ist->next_dts = ist->dts;
	}

	if (ist->next_pts == AV_NOPTS_VALUE)
	{
		ist->next_pts = ist->pts;
	}

	if (pkt->dts != AV_NOPTS_VALUE) {
		ist->next_dts = ist->dts = av_rescale_q(pkt->dts, st->time_base, AV_TIME_BASE_Q);
		ist->next_pts = ist->pts = ist->dts;
	}

	/* handle stream copy */
	// do copying
	{
		ist->dts = ist->next_dts;
		switch (ist->dec_ctx->codec_type)
		{
		case AVMEDIA_TYPE_AUDIO:
			if (ist->dec_ctx->sample_rate)
			{
				ist->next_dts += ((int64_t)AV_TIME_BASE * ist->dec_ctx->frame_size) /
					ist->dec_ctx->sample_rate;
			}
			else
			{
				ist->next_dts += av_rescale_q(pkt->duration, st->time_base, AV_TIME_BASE_Q);
			}
			break;
		case AVMEDIA_TYPE_VIDEO:
			if (pkt->duration)
			{
				ist->next_dts += av_rescale_q(pkt->duration, st->time_base, AV_TIME_BASE_Q);
			}
			else if (ist->dec_ctx->framerate.num != 0)
			{
				auto parser = av_stream_get_parser(st);
				auto ticks = parser ? parser->repeat_pict + 1 : ist->dec_ctx->ticks_per_frame;

				ist->next_dts += ((int64_t)AV_TIME_BASE *
					ist->dec_ctx->framerate.den * ticks) /
					ist->dec_ctx->framerate.num / ist->dec_ctx->ticks_per_frame;
			}
			break;
		default:
			break;
		}

		ist->pts = ist->dts;
		ist->next_pts = ist->next_dts;
	}

	return EnumMediaError::OK;
}
