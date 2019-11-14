#include <stdint.h>
#include <string.h>

class TCamera {
protected:
	uint32_t Width;
	uint32_t Height;
	uint32_t Depth;
	uint32_t Fps;

	TCamera(uint32_t width,
	        uint32_t height,
	        uint32_t depth,
	        uint32_t fps) {
		Width = width;
		Height = height;
		Depth = depth;
		Fps = fps;
	}

public:

	static TCamera* Instance;
	static void*    Camera;

	static void Init(uint32_t width,
	                 uint32_t height,
	                 uint32_t depth,
	                 uint32_t fps);

	static void Release();

	static void WarmupDelay();

	virtual void Write(uint8_t *data, uint32_t length, int64_t  pts) = 0;
	virtual ~TCamera(){};

	void Start();
	void Stop();
};

class FramesBuffer {
	uint32_t FrameSize;
	uint8_t* Buffer;
	uint32_t Frames;
public:
	uint32_t CurrentFrame;

	FramesBuffer(uint32_t frame_size, uint32_t frames) {
		FrameSize = frame_size;
		Frames = frames;
		Buffer = new uint8_t[FrameSize * Frames];
		CurrentFrame = 0;
	}

	void Reset() {
		CurrentFrame = 0;
	}

	bool HasMoreSpace() {
		return CurrentFrame < Frames;
	}
		
	bool AddFrame(const void* frame) {
		if (CurrentFrame < Frames) {
			memcpy(Buffer + CurrentFrame++ * FrameSize, frame, FrameSize);
		}
		return HasMoreSpace();
	}
		
	uint8_t* GetFrame(int frame) {
		return Buffer + frame * FrameSize;
	}
		
	~FramesBuffer() {
		delete Buffer;
	}
};

class TCameraMemory : public TCamera {

	TCameraMemory(uint32_t width,
	              uint32_t height,
	              uint32_t depth,
	              uint32_t fps) :
		TCamera(width, height, depth, fps) {}

public:
	FramesBuffer* Buffer;

	static TCameraMemory* Init(uint32_t width,
	                           uint32_t height,
	                           uint32_t depth,
	                           uint32_t fps,
	                           uint32_t frames);

	void Write(uint8_t *data, uint32_t length, int64_t  pts);
};
