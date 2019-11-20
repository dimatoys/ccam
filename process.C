#include "image.h"
#include "regression.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

void process(TImage* inpA, TImage* inpB) {
	uint32_t y = 150;
	uint8_t red[3] = {255, 40, 40};
	for (uint32_t x = 0; x < inpA->Width; ++x) {
		memcpy(inpA->Get(x,y), red, 3);
	}
}

void process(const char* inpAfile, const char* inpBfile, const char* outfile) {
	TImage* inpA = TImage::Load(inpAfile);
	TImage* inpB = TImage::Load(inpBfile);

	process(inpA, inpB);

	TImage out(inpA->Width + inpB->Width, inpA->Height, inpA->Depth);
	for (uint32_t y = 0; y < out.Height; ++y) {
		for (uint32_t x = 0; x < inpA->Width; ++x) {
			memcpy(out.Get(x, out.Height - y), inpA->Get(x,y), out.Depth);
		}
		for (uint32_t x = 0; x < inpB->Width; ++x) {
			memcpy(out.Get(x + inpA->Width,out.Height - y), inpB->Get(x,y), out.Depth);
		}
	}

	delete inpA;
	delete inpB;
	out.SaveJpg(outfile);
}

class TGradient {

	virtual void processSumIY(uint32_t x, uint32_t y, int* siy)=0;

public:
	uint32_t Width;
	uint32_t Height;
	uint32_t Depth;

	void countGradients(TImage* in, uint32_t freq) {
		Width = in->Width - freq;
		Height = in->Height;
		Depth = in->Depth;

		int half = freq / 2;
		int siy[Depth];
		int sumy[Depth];

		for (uint32_t y = 0; y < Height; ++y) {
			memset(siy, 0, Depth * sizeof(int));
			memset(sumy, 0, Depth * sizeof(int));
			for (uint32_t i = 0; i < freq; ++i) {
				auto pixel = in->Get(i, y);
				for (uint32_t c = 0; c < Depth; ++c) {
					sumy[c] += (int)pixel[c];
					siy[c] += ((int)i - half) * pixel[c];
				}
			}

			processSumIY(0, y, siy);

			uint32_t front = freq;
			for (uint32_t x = 1; x < Width; ++x) {
				auto frontY = in->Get(front, y);
				auto tailY = in->Get(front - freq, y);
				for (uint32_t c = 0; c < Depth; ++c) {
					sumy[c] -= tailY[c];
					siy[c] += half * ((int)frontY[c] + (int)tailY[c]) - sumy[c];
					sumy[c] += (int)frontY[c];
				}
				++front;
				processSumIY(x, y, siy);
			}
		}
	}
};

class TGradientScalar : public TGradient {

	virtual void processSumIY(uint32_t x, uint32_t y, int siy) = 0;

	void processSumIY(uint32_t x, uint32_t y, int* siy) {

		int siy_max = 0;
		for (uint32_t c = 0; c < Depth; ++c) {
			int siy_abs = abs(siy[c]);
			if (siy_abs > siy_max) {
				siy_max = siy_abs;
			}
		}

		processSumIY(x, y, siy_max > 50 ? 1 : 0);
	}
};

class TGradientPicture : public TGradientScalar {

	void processSumIY(uint32_t x, uint32_t y, int siy) {
		if (OutBuffer == NULL) {
			OutBuffer = new int[Width * Height];
			Min = siy;
			Max = siy;
		} else {
			if (siy < Min) {
				Min = siy;
			} else {
				if (siy > Max) {
					Max = siy;
				}
			}
		}
		OutBuffer[y * Width + x] = siy;
	}

public:
	int* OutBuffer;
	int  Min;
	int  Max;

	TGradientPicture() {
		OutBuffer = NULL;
	}

	~TGradientPicture() {
		if (OutBuffer != NULL) {
			delete OutBuffer;
		}
	}
};

void process1(const char* inFile, const char* outFile) {

	uint32_t freq = 5;

	TImage* in = TImage::Load(inFile);
	
	TGradientPicture gp;
	gp.countGradients(in, freq);

/*
	for (uint32_t x = 0; x < gp.Width; ++x) {
		printf("%u,%u,%d,%d,%d,%d,%d\n",
		       x,
		       x + in->Width,
		       gp.OutBuffer[136 * gp.Width + x],
		       gp.OutBuffer[137 * gp.Width + x],
		       gp.OutBuffer[138 * gp.Width + x],
		       gp.OutBuffer[139 * gp.Width + x],
		       gp.OutBuffer[140 * gp.Width + x]);
	}
*/
	auto ptr = gp.OutBuffer;
	TImage out(in->Width + gp.Width, in->Height, in->Depth);

	for (uint32_t y = 0; y < out.Height; ++y) {
		for (uint32_t x = 0; x < in->Width; ++x) {
			memcpy(out.Get(x, out.Height - y - 1), in->Get(x,y), out.Depth);
		}

		for (uint32_t x = 0; x < gp.Width; ++x) {
			double value = gp.Max > gp.Min ? 255 * (*ptr - gp.Min) / (gp.Max - gp.Min) : 0;
			++ptr;
			uint8_t* pixel = out.Get(x + in->Width, out.Height - y - 1);
			for (uint32_t c = 0; c < out.Depth; ++c) {
				*pixel++ = value;
			}
		}
	}

	delete in;
	out.SaveJpg(outFile);
}

int main(int argn, char** argv) {
	if (argn >= 3) {
		//process(argv[1], argv[2], argv[3]);
		process1(argv[1], argv[2]);
	} else {
		printf("Usage: %s imgA imgB out\n", argv[0]);
	}
	return 0;
}
