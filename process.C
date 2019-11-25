#include "image.h"
#include "regression.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <limits>

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

template<typename T>
class TPicture {

public:
	uint32_t Width;
	uint32_t Height;


	T* OutBuffer;
	T  Min;
	T  Max;

	TPicture(uint32_t width, uint32_t height) {
		Width = width;
		Height = height;
		OutBuffer = new int[Width * Height];
		Min = std::numeric_limits<T>::max();
		Max = std::numeric_limits<T>::min();
	}

	~TPicture() {
		delete OutBuffer;
	}

	void Set(uint32_t x, uint32_t y, T siy) {
		if (siy < Min) {
			Min = siy;
		}
		if (siy > Max) {
			Max = siy;
		}
		OutBuffer[y * Width + x] = siy;
	}

};

struct TDot {
	uint16_t X0;
	uint16_t X1;
	uint32_t Up;
	uint16_t Count;
};

struct TGradient {

	int MinValue;
	const uint16_t NO_VALUE = std::numeric_limits<uint16_t>::max();

	TGradient(int minValue) {
		MinValue = minValue;
	}

	TPicture<int>* countGradients(TImage* in, uint32_t freq) {
		int half = freq / 2;
		int siy[in->Depth];
		int sumy[in->Depth];

		TPicture<int>* out = new TPicture<int>(in->Width - freq, in->Height);
		TDot* dots = new TDot[out->Width * out->Height / 2];
		uint16_t ystart[out->Height];
		uint32_t current = 0;
		dots[0].X0 = NO_VALUE;

		for (uint32_t y = 0; y < in->Height; ++y) {
			fflush(stdout);
			memset(siy, 0, in->Depth * sizeof(int));
			memset(sumy, 0, in->Depth * sizeof(int));
			for (uint32_t i = 0; i < freq; ++i) {
				auto pixel = in->Get(i, y);
				for (uint32_t c = 0; c < in->Depth; ++c) {
					sumy[c] += (int)pixel[c];
					siy[c] += ((int)i - half) * pixel[c];
				}
			}

			ystart[y] = current;
			processSumIY(out, 0, y, in->Depth, dots, &current, siy);

			uint32_t front = freq;
			for (uint32_t x = 1; x < out->Width; ++x) {
				auto frontY = in->Get(front, y);
				auto tailY = in->Get(front - freq, y);
				for (uint32_t c = 0; c < in->Depth; ++c) {
					sumy[c] -= tailY[c];
					siy[c] += half * ((int)frontY[c] + (int)tailY[c]) - sumy[c];
					sumy[c] += (int)frontY[c];
				}
				++front;
				processSumIY(out, x, y, in->Depth, dots, &current, siy);
			}
			if (dots[current].X0 != NO_VALUE) {
				dots[current++].X1 = out->Width;
				dots[current].X0 = NO_VALUE;
			}
			if (y > 0) {
				auto upper = ystart[y - 1];
				auto start = ystart[y];
				for (auto i = start; i < current; ++i) {
					while(upper < start) {
						if (dots[upper].X1 + 1 >= dots[i].X0) {
							if (dots[upper].X0 <= dots[i].X1 + 1) {
								dots[i].Up = upper;
								dots[i].Count = dots[upper].Count + 1;
							}
							break;
						} else {
							++upper;
						}
					}
				}
			}
		}

		printf("current=%u\n", current);

		TPicture<int>* out2 = new TPicture<int>(in->Width - freq, in->Height);
		memset(out2->OutBuffer, 0, out2->Width * sizeof(out2->OutBuffer));
		uint32_t cnt = 0;
		for (int y0 = out2->Height - 1; y0 >= 0; --y0) {
			while(current > ystart[y0]) {
				--current;
				if (dots[current].Count > 20) {
					++cnt;
					auto c = current;
					auto y = y0;
					while(1) {
						for (uint16_t x = dots[c].X0; x < dots[c].X1; ++x) {
							out2->Set(x, y, 1);
						}
						out2->Set((dots[c].X0 + dots[c].X1) / 2, y, 2);
						auto count = dots[c].Count;
						dots[c].Count = 0;
						if (count <= 1) {
							break;
						}
						--y;
						c = dots[c].Up;
					}
				}
			}
		}
		printf("filered: %u\n", cnt);
		out2->Min = 0;
		delete dots;
		return out2;
	}

	void processSumIY(TPicture<int>* out,
	                  uint16_t  x,
	                  uint16_t  y,
	                  uint8_t   depth,
	                  TDot*     dots,
	                  uint32_t* current,
	                  int*      siy) {

		for (uint8_t c = 0; c < depth; ++c) {
			int siy_abs = abs(siy[c]);
			if (siy_abs > MinValue) {
				out->Set(x, y, 1);
				if (dots[*current].X0 == NO_VALUE) {
					dots[*current].X0 = x;
					dots[*current].Count = 1;

				}
				return;
			}
		}

		out->Set(x, y, 0);
		if (dots[*current].X0 != NO_VALUE) {
			dots[(*current)++].X1 = x;
			dots[*current].X0 = NO_VALUE;
			
		}
	}
};

void process1(const char* inFile, const char* outFile) {

	uint32_t freq = 5;

	TImage* in = TImage::Load(inFile);

	TGradient gp(50);
	TPicture<int>* outp = gp.countGradients(in, freq);

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
	auto ptr = outp->OutBuffer;
	TImage out(in->Width + outp->Width, in->Height, in->Depth);

	for (uint32_t y = 0; y < out.Height; ++y) {
		for (uint32_t x = 0; x < in->Width; ++x) {
			memcpy(out.Get(x, out.Height - y - 1), in->Get(x,y), out.Depth);
		}

		for (uint32_t x = 0; x < outp->Width; ++x) {
			double value = outp->Max > outp->Min ? 255 * (*ptr - outp->Min) / (outp->Max - outp->Min) : 0;
			++ptr;
			uint8_t* pixel = out.Get(x + in->Width, out.Height - y - 1);
			for (uint32_t c = 0; c < out.Depth; ++c) {
				*pixel++ = value;
			}
		}
	}

	delete in;
	delete outp;
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
