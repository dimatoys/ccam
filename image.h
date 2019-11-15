#include <stdint.h>

class TImage {

public:

	uint32_t Width;
	uint32_t Height;
	uint32_t Depth;
	uint8_t* Data;

	bool NeedDestroy;

	TImage(uint32_t width,
	       uint32_t height,
	       uint32_t depth,
	       void*    data) {
	    Width = width;
	    Height = height;
	    Depth = depth;
	    Data = (uint8_t*)data;
	    NeedDestroy = false;
	}

	TImage(uint32_t width,
	       uint32_t height,
	       uint32_t depth) {
	    Width = width;
	    Height = height;
	    Depth = depth;
	    Data = new uint8_t[width * height * depth];
	    NeedDestroy = true;
	}

	~TImage() {
		if (NeedDestroy) {
			delete Data;
		}
	}

	void Save(const char* fileName);
	static TImage* Load(const char* fileName);

	void SaveJpg(const char* fileName);

	uint8_t* Get(uint32_t x, uint32_t y) {
		return Data + (y * Width + x) * Depth;
	}

	uint32_t GetSize() {
		return Width * Height * Depth;
	}
};
