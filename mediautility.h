#ifndef MEDIAUTILITY_H
#define MEDIAUTILITY_H

#include <cstdint>

extern "C" {
	#include <libavutil/error.h>
}

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;

enum MEDIA_TYPE {
	MEDIA_TYPE_UNKNOWN,
	MEDIA_TYPE_VIDEO,
	MEDIA_TYPE_IMAGE
};

class MediaUtility
{
	public:
		static const int FINGERPRINT_SIZE;

		MediaUtility(const char *path);
		~MediaUtility();

		const char *getError(const int errNum);
		int open();
		double getDuration() const;
		int getHeight() const;
		int getWidth() const;
		const char *getCodec() const;
		const char *getContainer() const;
		const uint8_t *getFingerprint() const { return fingerprint; }
		MEDIA_TYPE getMediaType() const { return mediaType; }

	private:
		static const int FRAME_FINGERPRINT_SIZE;
		static const int TWO_WAY_FRAME_FINGERPRINT_SIZE;
		static const int NUM_FINGERPRINT_FRAMES;
		static const int BUFFER_SIZE_GREY_FRAME_9x8;
		static const int BUFFER_SIZE_GREY_FRAME_8x9;

		enum GREY_FRAME_TYPE {
			GREY_FRAME_TYPE_9x8,
			GREY_FRAME_TYPE_8x9
		};

		char *path;
		char error[AV_ERROR_MAX_STRING_SIZE];
		double position;
		uint8_t *fingerprint;
		MEDIA_TYPE mediaType;
		AVFormatContext *avFormatContext;
		AVCodecContext *avCodecContext;
		SwsContext *swsContext9x8;
		SwsContext *swsContext8x9;
		int avVideoStreamIndex;

		int computeFingerprint();
		int seek(const double seconds);
		AVFrame *readFrame();
		int computeFrameFingerprint(const AVFrame *frame, uint8_t *frameFingerprint, const GREY_FRAME_TYPE) const;
		void save(AVFrame *frame, int index);
};

#endif // MEDIAUTILITY_H
