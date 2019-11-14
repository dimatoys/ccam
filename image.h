#include <stdint.h>

class TImage {

public:

	uint32_t Width;
	uint32_t Height;
	uint32_t Depth;
	uint8_t* Data;

	TImage(uint32_t width,
	       uint32_t height,
	       uint32_t depth,
	       void*    data) {
	    Width = width;
	    Height = height;
	    Depth = depth;
	    Data = (uint8_t*)data;
	}

	void Save(const char* fileName);
	static TImage* Load(const char* fileName);

	void SaveJpg(const char* fileName);
};
