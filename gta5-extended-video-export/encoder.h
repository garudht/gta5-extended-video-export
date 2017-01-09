#pragma once

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")

#include <Windows.h>
#include <mfidl.h>
#include <mutex>
#include <future>
#include <vector>
#include <valarray>
#include "SafeQueue.h"
#include <d3d11.h>
#include <wrl.h>

using namespace Microsoft::WRL;

extern "C" {
#include <libavcodec\avcodec.h>
#include <libavformat\avformat.h>
#include <libavutil\imgutils.h>
#include <libswresample\swresample.h>
}

//template<typename T>
//using std::shared_ptr = std::shared_ptr<T, std::function<void(T*)>>;

namespace Encoder {
	class Session {
	public:
		AVOutputFormat *oformat;
		AVFormatContext *fmtContext;
		
		AVCodec *videoCodec = NULL;
		AVCodecContext *videoCodecContext = NULL;
		AVFrame *inputFrame = NULL;
		AVFrame *outputFrame = NULL;
		AVStream *videoStream = NULL;
		SwsContext *pSwsContext = NULL;
		AVDictionary *videoOptions = NULL;
		uint64_t videoPTS = 0;
		uint64_t motionBlurPTS = 0;

		AVCodec *audioCodec = NULL;
		AVCodecContext *audioCodecContext = NULL;
		AVFrame *inputAudioFrame = NULL;
		AVFrame *outputAudioFrame = NULL;
		AVStream *audioStream = NULL;
		SwrContext* pSwrContext = NULL;
		AVDictionary *audioOptions = NULL;
		AVAudioFifo *audioSampleBuffer = NULL;
		uint64_t audioPTS = 0;


		bool isVideoFinished = false;
		bool isAudioFinished = false;
		bool isSessionFinished = false;
		bool isCapturing = false;
		bool isBeingDeleted = false;

		std::mutex mxVideoContext;
		std::mutex mxAudioContext;
		std::mutex mxFormatContext;
		std::condition_variable cvVideoContext;
		std::condition_variable cvAudioContext;
		std::condition_variable cvFormatContext;

		std::mutex mxEndSession;
		std::condition_variable cvEndSession;

		struct frameQueueItem {
			frameQueueItem():
				data(nullptr)
			{
				
			}
			frameQueueItem(std::shared_ptr<std::valarray<uint8_t>> bytes) :
				data(bytes)
			{}

			std::shared_ptr<std::valarray<uint8_t>> data;
		};

		struct exr_queue_item {
			exr_queue_item() :
				cRGB(nullptr),
				pRGBData(nullptr),
				cDepth(nullptr),
				pDepthData(nullptr),
				isEndOfStream(true)
			{ }

			exr_queue_item(ComPtr<ID3D11Texture2D> cRGB, void *pRGBData, ComPtr<ID3D11Texture2D> cDepth, void *pDepthData, ComPtr<ID3D11Texture2D> cStencil, D3D11_MAPPED_SUBRESOURCE mStencilData) :
				cRGB(cRGB),
				pRGBData(pRGBData),
				cDepth(cDepth),
				pDepthData(pDepthData),
				cStencil(cStencil),
				mStencilData(mStencilData)
			{ }

			bool isEndOfStream = false;

			ComPtr<ID3D11Texture2D> cRGB;
			void* pRGBData;
			ComPtr<ID3D11Texture2D> cDepth;
			void* pDepthData;
			ComPtr<ID3D11Texture2D> cStencil;
			D3D11_MAPPED_SUBRESOURCE mStencilData;
			//void* pStencilData;
		};

		SafeQueue<frameQueueItem> videoFrameQueue;
		SafeQueue<exr_queue_item> exrImageQueue;

		bool isVideoContextCreated = false;
		bool isAudioContextCreated = false;
		bool isFormatContextCreated = false;
		bool isEncodingThreadFinished = false;
		std::condition_variable cvEncodingThreadFinished;
		std::mutex mxEncodingThread;
		std::thread thread_video_encoder;
		std::valarray<uint16_t> motionBlurAccBuffer;
		std::valarray<uint16_t> motionBlurTempBuffer;
		std::valarray<uint8_t> motionBlurDestBuffer;

		bool isEXREncodingThreadFinished = false;
		std::condition_variable cvEXREncodingThreadFinished;
		std::mutex mxEXREncodingThread;
		std::thread thread_exr_encoder;


		//std::condition_variable cvFormatContext;

		std::mutex mxFinish;
		std::mutex mxWriteFrame;

		UINT width;
		UINT height;
		UINT framerate;
		uint32_t motionBlurSamples;
		UINT audioBlockAlign;
		AVPixelFormat outputPixelFormat;
		AVPixelFormat inputPixelFormat;
		AVSampleFormat inputAudioSampleFormat;
		AVSampleFormat outputAudioSampleFormat;
		UINT inputAudioSampleRate;
		UINT inputAudioChannels;
		UINT outputAudioSampleRate;
		UINT outputAudioChannels;
		char filename[MAX_PATH];
		std::string exrOutputPath;
		uint64_t exrPTS=0;
		//LPWSTR *outputDir;
		//LPWSTR *outputFile;
		//FILE *file;

		Session();

		~Session();

		HRESULT createVideoContext(UINT width, UINT height, std::string inputPixelFormatString, UINT fps_num, UINT fps_den, uint8_t motionBlurSamples, std::string outputPixelFormatString, std::string vcodec, std::string preset);
		HRESULT createAudioContext(uint32_t inputChannels, uint32_t inputSampleRate, uint32_t inputBitsPerSample, AVSampleFormat inputSampleFormat, uint32_t inputAlignment, uint32_t outputSampleRate, std::string outputSampleFormatString, std::string acodec, std::string preset);
		HRESULT createFormatContext(LPCSTR filename, std::string exrOutputPath);

		HRESULT enqueueVideoFrame(BYTE * pData, int length);
		HRESULT enqueueEXRImage(ComPtr<ID3D11DeviceContext> pDeviceContext, ComPtr<ID3D11Texture2D> cRGB, ComPtr<ID3D11Texture2D> cDepth, ComPtr<ID3D11Texture2D> cStencil);

		void videoEncodingThread();
		void exrEncodingThread();

		HRESULT writeVideoFrame(BYTE *pData, int length, LONGLONG sampleTime);
		HRESULT writeAudioFrame(BYTE *pData, int length, LONGLONG sampleTime);

		HRESULT finishVideo();
		HRESULT finishAudio();

		HRESULT endSession();

	private:
		HRESULT createVideoFrames(uint32_t srcWidth, uint32_t srcHeight, AVPixelFormat srcFmt, uint32_t dstWidth, uint32_t dstHeight, AVPixelFormat dstFmt);
		HRESULT createAudioFrames(uint32_t inputChannels, AVSampleFormat inputSampleFmt, uint32_t inputSampleRate, uint32_t outputChannels, AVSampleFormat outputSampleFmt, uint32_t outputSampleRate);
	};
}