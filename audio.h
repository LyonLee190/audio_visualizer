#ifndef AUDIO_H
#define AUDIO_H

#include <fftw3.h>
#include <iostream>
#include <stdlib.h>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswresample/swresample.h>
}

using namespace std;

class audio{
public:
	/*
		file_dir: path to the audio file
	*/
	audio(const char *file_dir) {
		this->file_dir = file_dir;
	}

	/*
		aCodecCtx:	audio codec context
		PCM:		PCM data extracted form the audio file
		PCM_len:	length of the PCM data
	*/
	void decode(AVCodecContext *aCodecCtx, float **PCM, size_t *PCM_len);

private:
	const char *file_dir;

	// # of samples of audio carried per second, measured in Hz
	int out_sample_rate = 44100;
};

/*
	Notice that avcodec_decode_audio4() method is deprecated
	Deprecated List:
		https://libav.org/documentation/doxygen/master/deprecated.html
*/
void audio::decode(AVCodecContext *aCodecCtx, float **PCM, size_t *PCM_len) {
	cout << "Decoding audio..." << endl;

	// // registe all available file formats & codecs
	// av_register_all();

	// read the audio file header & store its format info
	AVFormatContext *pFormatCtx = avformat_alloc_context();
	if(avformat_open_input(&pFormatCtx, file_dir, NULL, NULL) != 0) {
		cout << "Failed to open the audio file" << endl;
		exit(1);
	}
	if(avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		cout << "Failed to retrieve stream info" << endl;
		exit(1);
	}
	// dump file info onto standard error
	av_dump_format(pFormatCtx, 0, file_dir, 0);

	// walk through the AVFormatContext structure until find an audio stream
	unsigned int audioStream = 0;
	for(; audioStream < pFormatCtx->nb_streams; audioStream++) {
		// if(pFormatCtx->streams[audioStream]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
		if(pFormatCtx->streams[audioStream]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			break;
		}
	}
	if (audioStream == pFormatCtx->nb_streams) {
		cout << "Failed to find an audio stream" << endl;
		exit(1);
	}

	// get the codec context for the audio stream,
	// which contains the info about the codec that the stream is using
	// aCodecCtx = pFormatCtx->streams[audioStream]->codec;
	aCodecCtx = avcodec_alloc_context3(NULL);
	avcodec_parameters_to_context(aCodecCtx, pFormatCtx->streams[audioStream]->codecpar);
	// find the decoder for the audio stream
	AVCodec *aCodec = avcodec_find_decoder(aCodecCtx->codec_id);
	if(!aCodec) {
		cout << "Unsupported codec" << endl;
		exit(1);
	}

	if (avcodec_open2(aCodecCtx, aCodec, NULL) < 0) {
		cout << "Failed to open codec" << endl;
		exit(1);
	}

	// prepare re-sampler
	SwrContext *swr = swr_alloc_set_opts(
			NULL,						// allocate a new context
			AV_CH_LAYOUT_MONO,			// out_ch_layout
			AV_SAMPLE_FMT_FLT,			// out_sample_fmt
			out_sample_rate,			// out_sample_rate
			aCodecCtx->channel_layout,	// in_ch_layout
			aCodecCtx->sample_fmt,		// in_sample_fmt
			aCodecCtx->sample_rate,		// in_sample_rate
			0,							// log_offset
			NULL);						// log_ctx

	swr_init(swr);
	if (!swr_is_initialized(swr)) {
		cout << "Failed to initialize the re-sampler" << endl;
		exit(1);
	}

	// packet contains bits of data that are decoded into raw frames
	AVPacket packet;
	av_init_packet(&packet);
	// the decoded data would be written into frame
	AVFrame *frame = av_frame_alloc();
	if (!frame) {
		cout << "Failed to allocate frame" << endl;
		exit(1);
	}

	*PCM = NULL;
	*PCM_len = 0;
	// iterate through frames
	while (av_read_frame(pFormatCtx, &packet) >= 0) {
		// skip non-audio packets
		if (packet.stream_index != (int) audioStream) {continue;}

		// decode one frame
		int got_frame = 0;
		if (avcodec_decode_audio4(aCodecCtx, frame, &got_frame, &packet) < 0) {
			cout << "Failed to decode the frame" << endl;
			exit(1);
		}
		if (!got_frame) {continue;}

		// re-sample frame
		float* buffer;
		av_samples_alloc(
			(uint8_t **) &buffer,	// audio_data
			NULL,					// line size
			1,						// nb_channels
			frame->nb_samples,		// nb_samples
			AV_SAMPLE_FMT_FLT,		// sample_fmt
			0);						// align

		int frame_count = swr_convert(
			swr,							// struct SwrContext *s
			(uint8_t **) &buffer,			// uint8_t *out_arg[SWR_CH_MAX]
			frame->nb_samples,				// int out_count
			(const uint8_t **) frame->data,	// uint8_t *in_arg [SWR_CH_MAX]
			frame->nb_samples);				// int in_count
		if (frame_count < 0) {
			cout << "Failed to re-sample the date" << endl;
			exit(1);
		}

		// append re-sampled frames to data
		*PCM = (float *) realloc(*PCM, (*PCM_len + frame->nb_samples) * sizeof(float));
		memcpy(*PCM + *PCM_len, buffer, frame_count * sizeof(float));
		*PCM_len += frame_count;
	}

	cout << "Length of the decoded audio data: " << *PCM_len << endl;

	// free spaces
	av_frame_free(&frame);
	// av_free_packet(&packet);
	av_packet_unref(&packet);
	swr_free(&swr);
	avformat_free_context(pFormatCtx);

	cout << "Done!" << endl;
}

#endif
