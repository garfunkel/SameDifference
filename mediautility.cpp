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

char *MediaUtility::getError(const int errNum) {
	return av_err2str(errNum);
}

MediaUtility::MediaUtility(const char *path)
{
	this->path = strdup(path);
	position = 0.0;
	avFormatContext = NULL;
	avCodecContext = NULL;
	avVideoStreamIndex = -1;
	mediaType = MEDIA_TYPE_UNKNOWN;
	fingerprint = NULL;
}

MediaUtility::~MediaUtility()
{
	avcodec_close(avCodecContext);
	avformat_close_input(&avFormatContext);

	if (fingerprint) {
		delete fingerprint;

		fingerprint = NULL;
	}
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

	if (!(swsContext9x8 = sws_getContext(avCodecContext->width,
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

	if (!(swsContext8x9 = sws_getContext(avCodecContext->width,
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
	int ret = 0;

	if (duration < 0) {
		seek(0.0);

		return AVERROR_INVALIDDATA;
	}

	double pos = 0;
	fingerprint = (uint8_t *)calloc(FINGERPRINT_SIZE, 1);

	for (int i = 0; i < 10; i++) {
		if (i == 9)
			pos = duration;

		else
			pos = duration / 9 * i;

		if ((ret = seek(pos)) < 0)
			break;

		AVFrame *frame = readFrame();

		if (!frame) {
			ret = AVERROR_INVALIDDATA;

			break;
		}

		computeFrameFingerprint(frame, fingerprint + (128 * i), GREY_FRAME_TYPE_9x8);
		computeFrameFingerprint(frame, fingerprint + (128 * i) + 64, GREY_FRAME_TYPE_8x9);

		av_frame_free(&frame);
	}

	if (ret < 0) {
		delete fingerprint;

		fingerprint = NULL;
	}

	seek(0.0);

	return ret;
}

int MediaUtility::computeFrameFingerprint(const AVFrame *frame, uint8_t *frameFingerprint, const GREY_FRAME_TYPE frameType)
{
	int numBytes = 0;
	AVFrame *greyFrame = av_frame_alloc();
	uint8_t *buffer = NULL;
	int xStart = 0;
	int yStart = 0;
	int ret = 0;
	uint8_t pixel = 0;
	uint8_t comparisonPixel = 0;

	switch (frameType) {
		case GREY_FRAME_TYPE_9x8:
			numBytes = av_image_get_buffer_size(AV_PIX_FMT_GRAY8, 9, 8, 1);
			buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
			greyFrame->width = 9;
			greyFrame->height = 8;
			xStart = 1;

			break;

		case GREY_FRAME_TYPE_8x9:
			numBytes = av_image_get_buffer_size(AV_PIX_FMT_GRAY8, 8, 9, 1);
			buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
			greyFrame->width = 8;
			greyFrame->height = 9;
			yStart = 1;

			break;
	}

	if ((ret = av_image_fill_arrays(greyFrame->data, greyFrame->linesize, buffer, AV_PIX_FMT_GRAY8, greyFrame->width, greyFrame->height, 1)) < 0)
		return ret;

	if ((ret = sws_scale(swsContext9x8,
						 (uint8_t const *const *)frame->data,
						 frame->linesize,
						 0,
						 avCodecContext->height,
						 greyFrame->data,
						 greyFrame->linesize)) < 0)
		return ret;

	for (int y = yStart; y < greyFrame->height; y++) {
		for (int x = xStart; x < greyFrame->width; x++) {
			pixel = *(greyFrame->data[0] + (y * greyFrame->width) + x);
			comparisonPixel = *(greyFrame->data[0] + ((y - yStart) * greyFrame->width) + (x - xStart));

			if (pixel >= comparisonPixel) {
				memset(frameFingerprint + ((y - yStart) * (greyFrame->width - xStart) + (x - xStart)), 1, 1);
			}
		}
	}

	return 128;
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
	position = seconds;
	AVRational timeBase = avFormatContext->streams[avVideoStreamIndex]->time_base;
	int64_t timestamp = seconds * ((double)timeBase.den / timeBase.num);

	avcodec_flush_buffers(avCodecContext);

	return av_seek_frame(avFormatContext, avVideoStreamIndex, timestamp, AVSEEK_FLAG_BACKWARD);
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
			if (avPacket.stream_index == avVideoStreamIndex) {
				break;
			}

			av_packet_unref(&avPacket);
		}

		// av_read_frame() returned an error. Send NULL packet to signal end of stream.
		if (ret < 0) {
			if ((avcodec_send_packet(avCodecContext, NULL)) < 0) {
				break;
			}
		}

		else
			if ((avcodec_send_packet(avCodecContext, &avPacket)) < 0) {
				break;
			}
	}

	av_packet_unref(&avPacket);
	av_frame_free(&nextAvFrame);

	// Doesn't look like there was a frame to decode... return NULL.
	if (av_frame_get_best_effort_timestamp(avFrame) == AV_NOPTS_VALUE) {
		av_frame_free(&avFrame);
	}

	return avFrame;
}
