extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/avutil.h>
	#include <libavutil/imgutils.h>
	#include <libswscale/swscale.h>
}

#include "mediautility.h"

const int MediaUtility::FRAME_FINGERPRINT_SIZE = 64;
const int MediaUtility::TWO_WAY_FRAME_FINGERPRINT_SIZE = MediaUtility::FRAME_FINGERPRINT_SIZE * 2;
const int MediaUtility::NUM_FINGERPRINT_FRAMES = 10;
const int MediaUtility::FINGERPRINT_SIZE = MediaUtility::TWO_WAY_FRAME_FINGERPRINT_SIZE * MediaUtility::NUM_FINGERPRINT_FRAMES;
const int MediaUtility::BUFFER_SIZE_GREY_FRAME_9x8 = av_image_get_buffer_size(AV_PIX_FMT_GRAY8, 9, 8, 1) * sizeof(uint8_t);
const int MediaUtility::BUFFER_SIZE_GREY_FRAME_8x9 = av_image_get_buffer_size(AV_PIX_FMT_GRAY8, 8, 9, 1) * sizeof(uint8_t);

MediaUtility::MediaUtility(const char *path)
{
	this->path = strdup(path);
	position = 0.0;
	avFormatContext = NULL;
	avCodecContext = NULL;
	avVideoStreamIndex = -1;
	mediaType = MEDIA_TYPE_UNKNOWN;
	fingerprint = NULL;
	swsContext9x8 = NULL;
	swsContext8x9 = NULL;
}

MediaUtility::~MediaUtility()
{
	avcodec_free_context(&avCodecContext);
	avformat_close_input(&avFormatContext);

	free(fingerprint);
	free(path);

	fingerprint = NULL;
	path = NULL;
}

const char *MediaUtility::getError(const int errNum) {

	av_strerror(errNum, error, AV_ERROR_MAX_STRING_SIZE);

	return error;
}

int MediaUtility::open() {
	int ret = 0;
	AVCodec *avCodec = NULL;
	avFormatContext = avformat_alloc_context();

	if ((ret = avformat_open_input(&avFormatContext, path, NULL, NULL)) != 0) {
		return ret;
	}

	if ((ret = avformat_find_stream_info(avFormatContext, NULL)) < 0) {
		return ret;
	}

	if ((ret = av_find_best_stream(avFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &avCodec, 0)) < 0) {
		return ret;
	}

	avVideoStreamIndex = ret;
	avCodecContext = avcodec_alloc_context3(avCodec);

	if ((ret = avcodec_parameters_to_context(avCodecContext, avFormatContext->streams[avVideoStreamIndex]->codecpar)) < 0) {
		return ret;
	}

	if ((ret = avcodec_open2(avCodecContext, avCodec, NULL)) < 0) {
		return ret;
	}

	AVFrame *frame = NULL;

	if ((frame = readFrame())) {
		// If we can read a frame and there is no duration, this is likely an image.
		if (avFormatContext->duration == AV_NOPTS_VALUE) {
			mediaType = MEDIA_TYPE_IMAGE;
		// If we can read a frame, and there is a valid duration, this is likely a video.
		} else {
			mediaType = MEDIA_TYPE_VIDEO;
		}

		av_frame_free(&frame);

		seek(0.0);

		computeFingerprint();
	}

	return ret;
}

double MediaUtility::getDuration() const {
	if (avFormatContext->duration == AV_NOPTS_VALUE) {
		return 0;
	}

	return double(avFormatContext->duration) / AV_TIME_BASE;
}

int MediaUtility::getWidth() const
{
	return avCodecContext->width;
}

int MediaUtility::getHeight() const
{
	return avCodecContext->height;
}

const char *MediaUtility::getCodec() const
{
	return avCodecContext->codec->name;
}

const char *MediaUtility::getContainer() const
{
	return avFormatContext->iformat->long_name;
}

int MediaUtility::computeFingerprint()
{
	/*
	 * We take 10 frames from the file, 1 at the start, 8 at evenly spaced intervals,
	 * and one at the end of the file.
	 */
	double duration = getDuration();

	if (duration < 0)
		return AVERROR_INVALIDDATA;

	int ret = 0;
	double pos = 0;
	AVFrame *frame = NULL;
	fingerprint = (uint8_t *)calloc(FINGERPRINT_SIZE, 1);

	if (!swsContext9x8 && !(swsContext9x8 = sws_getContext(avCodecContext->width,
														   avCodecContext->height,
														   avCodecContext->pix_fmt,
														   9,
														   8,
														   AV_PIX_FMT_GRAY8,
														   0,
														   NULL,
														   NULL,
														   NULL)))
		return AVERROR_INVALIDDATA;

	if (!swsContext8x9 && !(swsContext8x9 = sws_getContext(avCodecContext->width,
														   avCodecContext->height,
														   avCodecContext->pix_fmt,
														   8,
														   9,
														   AV_PIX_FMT_GRAY8,
														   0,
														   NULL,
														   NULL,
														   NULL)))
		return AVERROR_INVALIDDATA;

	int numFrames = NUM_FINGERPRINT_FRAMES;

	// If this is an image or a really short video, just get one frame.
	if (mediaType == MEDIA_TYPE_IMAGE || duration == 0.0)
		numFrames = 1;

	for (int i = 0; i < numFrames; i++) {
		i == NUM_FINGERPRINT_FRAMES - 1 ? pos = duration : pos = duration / (NUM_FINGERPRINT_FRAMES - 1) * i;

		if ((ret = seek(pos)) < 0)
			break;

		if (!(frame = readFrame())) {
			ret = AVERROR_INVALIDDATA;

			break;
		}

		computeFrameFingerprint(frame, fingerprint + (128 * i), GREY_FRAME_TYPE_9x8);
		computeFrameFingerprint(frame, fingerprint + (128 * i) + 64, GREY_FRAME_TYPE_8x9);

		av_frame_free(&frame);
	}

	if (ret < 0) {
		free(fingerprint);

		fingerprint = NULL;
	}

	seek(0.0);

	return ret;
}

int MediaUtility::computeFrameFingerprint(const AVFrame *frame, uint8_t *frameFingerprint, const GREY_FRAME_TYPE frameType) const
{
	AVFrame *greyFrame = av_frame_alloc();
	uint8_t *buffer = NULL;
	int xStart = 0;
	int yStart = 0;
	int ret = 0;
	uint8_t pixel = 0;
	uint8_t comparisonPixel = 0;
	SwsContext *swsContext = NULL;

	switch (frameType) {
		case GREY_FRAME_TYPE_9x8:
			buffer = (uint8_t *)av_malloc(BUFFER_SIZE_GREY_FRAME_9x8);
			greyFrame->width = 9;
			greyFrame->height = 8;
			xStart = 1;
			swsContext = swsContext9x8;

			break;

		case GREY_FRAME_TYPE_8x9:
			buffer = (uint8_t *)av_malloc(BUFFER_SIZE_GREY_FRAME_8x9);
			greyFrame->width = 8;
			greyFrame->height = 9;
			yStart = 1;
			swsContext = swsContext8x9;

			break;
	}

	if ((ret = av_image_fill_arrays(greyFrame->data,
									greyFrame->linesize,
									buffer,
									AV_PIX_FMT_GRAY8,
									greyFrame->width,
									greyFrame->height,
									1)) < 0) {
		av_frame_free(&greyFrame);
		av_freep(&buffer);

		return ret;
	}

	if ((ret = sws_scale(swsContext,
						 (uint8_t const *const *)frame->data,
						 frame->linesize,
						 0,
						 avCodecContext->height,
						 greyFrame->data,
						 greyFrame->linesize)) < 0) {
		av_frame_free(&greyFrame);
		av_freep(&buffer);

		return ret;
	}

	for (int y = yStart; y < greyFrame->height; y++) {
		for (int x = xStart; x < greyFrame->width; x++) {
			pixel = *(greyFrame->data[0] + (y * greyFrame->width) + x);
			comparisonPixel = *(greyFrame->data[0] + ((y - yStart) * greyFrame->width) + (x - xStart));

			if (pixel >= comparisonPixel)
				memset(frameFingerprint + ((y - yStart) * (greyFrame->width - xStart) + (x - xStart)), 1, 1);
		}
	}

	av_frame_free(&greyFrame);
	av_freep(&buffer);

	return FRAME_FINGERPRINT_SIZE;
}

void MediaUtility::save(AVFrame *frame, int index)
{
	SwsContext *swsContext = sws_getContext(avCodecContext->width,
				 avCodecContext->height,
				 avCodecContext->pix_fmt,
				 64,
				 64,
				 AV_PIX_FMT_GRAY8,
				 0,
				 NULL,
				 NULL,
				 NULL
				 );

	AVFrame *greyFrame = av_frame_alloc();

	int numBytes = av_image_get_buffer_size(AV_PIX_FMT_GRAY8, 64, 64, 1);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	av_image_fill_arrays(greyFrame->data, greyFrame->linesize, buffer, AV_PIX_FMT_GRAY8, 64, 64, 1);

	sws_scale(swsContext, (uint8_t const * const *)frame->data,
		  frame->linesize, 0, avCodecContext->height,
		  greyFrame->data, greyFrame->linesize);

	FILE *pFile;
	char szFilename[32];

	// Open file
	sprintf(szFilename, "/home/simon/frame%d.ppm", index);
	pFile=fopen(szFilename, "wb");

	if(pFile==NULL)
		return;

	// Write header
	fprintf(pFile, "P5\n%d %d\n255\n", 64, 64);

	// Write pixel data
	fwrite(greyFrame->data[0], 64, 64, pFile);

	// Close file
	fclose(pFile);
}

int MediaUtility::seek(const double seconds)
{
	AVRational timeBase = avFormatContext->streams[avVideoStreamIndex]->time_base;
	int64_t timestamp = seconds * ((double)timeBase.den / timeBase.num);
	int ret = 0;

	avcodec_flush_buffers(avCodecContext);

	if ((ret = av_seek_frame(avFormatContext, avVideoStreamIndex, timestamp, AVSEEK_FLAG_BACKWARD)) >= 0)
		position = seconds;

	return ret;
}

AVFrame *MediaUtility::readFrame()
{
	AVPacket avPacket;
	AVFrame *avFrame = av_frame_alloc();
	AVFrame *nextAvFrame = av_frame_alloc();
	AVFrame *tmpFrame = NULL;
	int ret = 0;
	double newPosition = 0;

	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;

	// Keep trying to receive a packet until EOF (or unrecoverable error).
	while ((ret = avcodec_receive_frame(avCodecContext, nextAvFrame)) >= 0 || ret == AVERROR(EAGAIN)) {
		// We received a valid frame.
		if (ret >= 0) {
			// Swap our next/current frames.
			tmpFrame = avFrame;
			avFrame = nextAvFrame;
			nextAvFrame = tmpFrame;
			tmpFrame = NULL;
			newPosition = av_frame_get_best_effort_timestamp(avFrame) * av_q2d(avFormatContext->streams[avVideoStreamIndex]->time_base);

			// This frame is >= our seek position, so this is the frame we want to return.
			if (newPosition >= position) {
				position = newPosition;

				break;
			}

			// Try to receive more frames before moving on.
			continue;
		}

		// Read packets from the format context until we get a video packet (or error).
		while ((ret = av_read_frame(avFormatContext, &avPacket)) >= 0) {
			// We found a video packet.
			if (avPacket.stream_index == avVideoStreamIndex)
				break;

			av_packet_unref(&avPacket);
		}

		// av_read_frame() returned an error. Send NULL packet to signal end of stream.
		if (ret < 0) {
			if ((avcodec_send_packet(avCodecContext, NULL)) < 0)
				break;
		}

		else
			if ((avcodec_send_packet(avCodecContext, &avPacket)) < 0)
				break;
	}

	av_packet_unref(&avPacket);
	av_frame_free(&nextAvFrame);

	// Doesn't look like there was a frame to decode... return NULL.
	if (av_frame_get_best_effort_timestamp(avFrame) == AV_NOPTS_VALUE)
		av_frame_free(&avFrame);

	return avFrame;
}
