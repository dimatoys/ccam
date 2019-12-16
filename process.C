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
		OutBuffer = new T[Width * Height];
		Min = std::numeric_limits<T>::max();
		Max = std::numeric_limits<T>::min();
	}

	TPicture(uint32_t width, uint32_t height, T fill) {
		Width = width;
		Height = height;
		OutBuffer = new T[Width * Height];
		for (int i = 0; i < Width * Height; ++i) {
			OutBuffer[i] = fill;
		}
		Min = fill;
		Max = fill;
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

	void Draw(TImage* out, uint32_t x0, uint32_t y0) {
		for (uint32_t y = 0; y < Height; ++y) {
			for (uint32_t x = 0; x < Width; ++x) {
				uint8_t value = (uint8_t)((long)OutBuffer[x + y * Width] - Min) * 255 / (Max-(long)Min);
				auto pixel = out->Get(x0 + x, y0 + y);
				for (uint32_t c = 0; c < 3; ++c) {
					*pixel++ = value;
				}
			}
		}
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

	void countGradients(TImage* in, uint32_t freq) {
		int half = freq / 2;
		int siy[in->Depth];
		int sumy[in->Depth];
		uint32_t width = in->Width - freq;

		TDot* dots = new TDot[width * in->Height / 2];
		uint16_t ystart[in->Height];
		uint32_t current = 0;
		dots[0].X0 = NO_VALUE;

		BorderInit(width, in->Height);
		for (uint32_t y = 0; y < in->Height; ++y) {
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
			processSumIY(0, y, in->Depth, dots, &current, siy);

			uint32_t front = freq;
			for (uint32_t x = 1; x < width; ++x) {
				auto frontY = in->Get(front, y);
				auto tailY = in->Get(front - freq, y);
				for (uint32_t c = 0; c < in->Depth; ++c) {
					sumy[c] -= tailY[c];
					siy[c] += half * ((int)frontY[c] + (int)tailY[c]) - sumy[c];
					sumy[c] += (int)frontY[c];
				}
				++front;
				processSumIY(x, y, in->Depth, dots, &current, siy);
			}
			if (dots[current].X0 != NO_VALUE) {
				dots[current++].X1 = width;
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

		GBorderInit(width, in->Height);
		uint32_t cnt = 0;
		for (int y0 = in->Height - 1; y0 >= 0; --y0) {
			while(current > ystart[y0]) {
				--current;
				if (dots[current].Count > 20) {
					++cnt;
					auto c = current;
					auto y = y0;
					while(1) {
						GBorderDrawHLine(dots[c].X0, dots[c].X1, y);
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
		delete dots;
	}

	void processSumIY(uint16_t  x,
	                  uint16_t  y,
	                  uint8_t   depth,
	                  TDot*     dots,
	                  uint32_t* current,
	                  int*      siy) {

		for (uint8_t c = 0; c < depth; ++c) {
			int siy_abs = abs(siy[c]);
			if (siy_abs > MinValue) {
				BorderSet(x, y);
				if (dots[*current].X0 == NO_VALUE) {
					dots[*current].X0 = x;
					dots[*current].Count = 1;

				}
				return;
			}
		}

		if (dots[*current].X0 != NO_VALUE) {
			dots[(*current)++].X1 = x;
			dots[*current].X0 = NO_VALUE;
			
		}
	}

	TPicture<uint8_t>* Border;
	void BorderInit(uint32_t width, uint32_t height) {
		Border = new TPicture<uint8_t>(width, height, 0);
	}

	void BorderSet(uint16_t  x,
	               uint16_t  y) {
		Border->Set(x,y, 0xFF);
	}

	TPicture<uint8_t>* GBorder;
	void GBorderInit(uint32_t width, uint32_t height) {
		GBorder = new TPicture<uint8_t>(width, height, 0);
	}

	void GBorderDrawHLine(uint16_t x0, uint16_t x1, uint16_t y) {
		for (uint16_t x = x0; x < x1; ++x) {
			GBorder->Set(x, y, 1);
		}
		GBorder->Set((x0 + x1) / 2, y, 2);
	}

	~TGradient() {
		delete Border;
		delete GBorder;
	}
};

struct TLine {
	uint16_t X0; // start of border
	uint16_t X1; // start of shape
	uint16_t X2; // end of shape
	uint16_t Y;
	uint8_t  Color[3];
};

struct TGradient2 {

	const uint16_t NO_VALUE = std::numeric_limits<uint16_t>::max();
	int      MinValue;
	uint16_t MinArea;

	uint32_t Width;

	TGradient2(int minValue, uint16_t minArea) {
		MinValue = minValue;
		MinArea = minArea;
	}

	void countGradients(TImage* in, uint32_t freq) {
		int half = freq / 2;
		int siy[in->Depth];
		int sumy[in->Depth];
		Width = in->Width - freq;

		BorderInit(Width, in->Height);
		for (uint32_t y = 0; y < in->Height; ++y) {
			memset(siy, 0, in->Depth * sizeof(int));
			memset(sumy, 0, in->Depth * sizeof(int));
			for (uint32_t i = 0; i < freq; ++i) {
				auto pixel = in->Get(i, y);
				for (uint32_t c = 0; c < in->Depth; ++c) {
					sumy[c] += (int)pixel[c];
					siy[c] += ((int)i - half) * pixel[c];
				}
			}

			processSumIY(0, y, in->Depth, siy);

			uint32_t front = freq;
			for (uint32_t x = 1; x < Width; ++x) {
				auto frontY = in->Get(front, y);
				auto tailY = in->Get(front - freq, y);
				for (uint32_t c = 0; c < in->Depth; ++c) {
					sumy[c] -= tailY[c];
					siy[c] += half * ((int)frontY[c] + (int)tailY[c]) - sumy[c];
					sumy[c] += (int)frontY[c];
				}
				++front;
				processSumIY(x, y, in->Depth, siy);
			}
		}

		//printf("current=%u\n", current);
	}

	TImage* Image;
	TLine* Lines;
	uint32_t Lptr;
	uint32_t Sum[3];

	void countLines(TImage* in, uint32_t freq) {
		Image = in;
		Lines = new TLine[in->Width * in->Height];
		Lptr = 0;
		Lines[Lptr].X0 = 0;
		Lines[Lptr].X1 = NO_VALUE;
		countGradients(in, freq);
	}

	void processSumIY(uint16_t  x,
	                  uint16_t  y,
	                  uint8_t   depth,
	                  int*      siy) {

		TLine* line = &Lines[Lptr];

		if ((line->X1 == NO_VALUE) || (x - line->X1 >= MinArea)) {
			for (uint8_t c = 0; c < depth; ++c) {
				int siy_abs = abs(siy[c]);
				if (siy_abs > MinValue) {
					BorderSet(x, y);
					if (line->X1 == NO_VALUE) {
						if (x == Width - 1) {
							line->X0 = 0;
						}
					} else {
						line->X2 = x - 1;
						for(uint8_t i = 0; i < depth; ++i) {
							line->Color[i] = (uint8_t)(Sum[i] / (x - line->X1));
						}
						line = &Lines[++Lptr];
						line->X0 = x;
						line->X1 = NO_VALUE;
					}
					return;
				}
			}
		}
		//printf("[%u]X1=%hu\n", Lptr, line->X1);

		auto pixel = Image->Get(x, y);
		if (line->X1 == NO_VALUE) {
			line->X1 = x;
			line->Y = y;
			for (uint8_t c = 0; c < depth; ++c) {
				Sum[c] = pixel[c];
			}
		} else {
			for (uint8_t c = 0; c < depth; ++c) {
				Sum[c] += pixel[c];
			}
		}

		if (x == Width - 1) {
			line->X2 = x;
			for(uint8_t i = 0; i < depth; ++i) {
				line->Color[i] = (uint8_t)(Sum[i] / (x + 1 - line->X1));
			}
			line = &Lines[++Lptr];
			line->X0 = 0;
			line->X1 = NO_VALUE;
		}
	}

	TPicture<uint8_t>* Border;
	void BorderInit(uint32_t width, uint32_t height) {
		Border = new TPicture<uint8_t>(width, height, 0);
	}

	void BorderSet(uint16_t  x,
	               uint16_t  y) {
		Border->Set(x,y, 0xFF);
	}

	void DrawShapes(TImage* out, uint32_t x0, uint32_t y0) {
		printf("lines=%u\n", Lptr);
		uint8_t border[3] = {255, 255, 255};
		for (uint32_t i = 0; i < Lptr; ++i) {
			TLine& line = Lines[i];
			//printf("%hu [%hu,%hu,%hu] (%hhu,%hhu,%hhu)\n", line.Y, line.X0, line.X1, line.X2, line.Color[0], line.Color[1], line.Color[2]);
			for (uint32_t x = line.X0; x < line.X1; ++x) {
				memcpy(out->Get(x + x0, line.Y + y0), border, 3);
			}
			for (uint32_t x = line.X1; x <= line.X2; ++x) {
				memcpy(out->Get(x + x0, line.Y + y0), line.Color, 3);
			}
		}
	}
};

void process1(const char* inFile1, const char* inFile2, const char* outFile) {

	uint32_t freq = 5;

	TImage* in = TImage::Load(inFile1);

	TGradient gp(50);
	gp.countGradients(in, freq);

	TImage out(in->Width + gp.Border->Width + gp.GBorder->Width, in->Height * 2, in->Depth);

	for (uint32_t y = 0; y < in->Height; ++y) {
		for (uint32_t x = 0; x < in->Width; ++x) {
			memcpy(out.Get(x, y), in->Get(x,y), out.Depth);
		}
	}

	gp.Border->Draw(&out, in->Width, 0);
	gp.GBorder->Draw(&out, in->Width + gp.Border->Width, 0);

	delete in;

	in = TImage::Load(inFile2);
	gp.countGradients(in, freq);

	for (uint32_t y = 0; y < in->Height; ++y) {
		for (uint32_t x = 0; x < in->Width; ++x) {
			memcpy(out.Get(x, y + in->Height), in->Get(x,y), out.Depth);
		}
	}

	gp.Border->Draw(&out, in->Width, in->Height);
	gp.GBorder->Draw(&out, in->Width + gp.Border->Width, in->Height);

	delete in;

	out.Flip();
	out.SaveJpg(outFile);
}

void process2(const char* inFile1, const char* inFile2, const char* outFile) {

	uint32_t freq = 5;

	TImage* in = TImage::Load(inFile1);

	TGradient2 gp(80, 10);
	gp.countLines(in, freq);

	TImage out(in->Width + gp.Border->Width + gp.Width, in->Height * 2, in->Depth);

	for (uint32_t y = 0; y < in->Height; ++y) {
		for (uint32_t x = 0; x < in->Width; ++x) {
			memcpy(out.Get(x, y), in->Get(x,y), out.Depth);
		}
	}

	gp.Border->Draw(&out, in->Width, 0);
	gp.DrawShapes(&out, in->Width + gp.Border->Width, 0);

	delete in;

	in = TImage::Load(inFile2);
	gp.countLines(in, freq);

	for (uint32_t y = 0; y < in->Height; ++y) {
		for (uint32_t x = 0; x < in->Width; ++x) {
			memcpy(out.Get(x, y + in->Height), in->Get(x,y), out.Depth);
		}
	}

	gp.Border->Draw(&out, in->Width, in->Height);
	gp.DrawShapes(&out, in->Width + gp.Border->Width, in->Height);

	delete in;

	out.Flip();
	out.SaveJpg(outFile);
}

struct Cell {
	uint16_t X;
	uint32_t Sum1[3];
	uint32_t Sum2[3];
};

struct TGradient3 {

	const uint16_t NO_VALUE = std::numeric_limits<uint16_t>::max();
	int      MinValue;
	uint16_t EvalArea;

	uint32_t Width;

	TGradient3(int minValue, uint16_t evalArea) {
		MinValue = minValue;
		EvalArea = evalArea;
	}

	void countGradients(TImage* in, uint32_t freq) {
		int half = freq / 2;
		int siy[in->Depth];
		int sumy[in->Depth];
		Width = in->Width - freq;

		BorderInit(Width, in->Height);
		for (uint32_t y = 0; y < in->Height; ++y) {
			memset(siy, 0, in->Depth * sizeof(int));
			memset(sumy, 0, in->Depth * sizeof(int));
			for (uint32_t i = 0; i < freq; ++i) {
				auto pixel = in->Get(i, y);
				for (uint32_t c = 0; c < in->Depth; ++c) {
					sumy[c] += (int)pixel[c];
					siy[c] += ((int)i - half) * pixel[c];
				}
			}

			processSumIY(0, y, in->Depth, siy);

			uint32_t front = freq;
			for (uint32_t x = 1; x < Width; ++x) {
				auto frontY = in->Get(front, y);
				auto tailY = in->Get(front - freq, y);
				for (uint32_t c = 0; c < in->Depth; ++c) {
					sumy[c] -= tailY[c];
					siy[c] += half * ((int)frontY[c] + (int)tailY[c]) - sumy[c];
					sumy[c] += (int)frontY[c];
				}
				++front;
				processSumIY(x, y, in->Depth, siy);
			}
		}

		//printf("current=%u\n", current);
	}

	uint16_t XCells;
	Cell*    Cells;
	TImage*  Image;
	int      CMax;
	uint32_t SumC[3];

	void countCells(TImage* in, uint32_t freq) {
		Image = in;
		XCells = (in->Width - freq) / EvalArea;
		Cells = new Cell[XCells * in->Height];
		
		countGradients(in, freq);
	}

	void processSumIY(uint16_t  x,
	                  uint16_t  y,
	                  uint8_t   depth,
	                  int*      siy) {

		if (x >= EvalArea * XCells) {
			return;
		}

		Cell& cell = Cells[y * XCells + x / EvalArea];
		if (x % EvalArea == 0) {
			cell.X = NO_VALUE;
			CMax = 0;
			memset(SumC, 0, 3 * sizeof(uint32_t));
			memset(cell.Sum2, 0, 3 * sizeof(uint32_t));
		}

		uint8_t* pixel = Image->Get(x, y);

		bool hasMax = false;
		for (uint8_t c = 0; c < depth; ++c) {
			int siy_abs = abs(siy[c]);
			if (siy_abs > MinValue) {
				BorderSet(x, y);
				if (cell.X == NO_VALUE || siy_abs > CMax) {
					CMax = siy_abs;
					hasMax = true;
				}
			}
		}

		if (hasMax) {
			cell.X = x;
			memcpy(cell.Sum1, SumC, 3 * sizeof(uint32_t));
			memset(cell.Sum2, 0, 3 * sizeof(uint32_t));
		} else {
			for (uint8_t c = 0; c < depth; ++c) {
				cell.Sum2[c] += pixel[c];
			}
		}

		for (uint8_t c = 0; c < depth; ++c) {
			SumC[c] += pixel[c];
		}
	}

	TPicture<uint8_t>* Border;
	void BorderInit(uint32_t width, uint32_t height) {
		Border = new TPicture<uint8_t>(width, height, 0);
	}

	void BorderSet(uint16_t  x,
	               uint16_t  y) {
		Border->Set(x,y, 0xFF);
	}

	void DrawShapes(TImage* out, uint32_t x0, uint32_t y0) {
		uint8_t border[3] = {255, 255, 255};
		Cell* cell = Cells;
		for (uint16_t y = 0; y < Image->Height; ++y) {
			for (uint16_t i = 0; i < XCells; ++i, ++cell) {
				auto x = i * EvalArea;
				uint8_t* pixel = out->Get(x0 + x, y0 + y);
				uint16_t p2;
				if (cell->X != NO_VALUE) {
					auto p1 = cell->X - x;
					if (p1 > 0) {
						uint8_t color1[3];
						color1[0] = cell->Sum1[0] / p1;
						color1[1] = cell->Sum1[1] / p1;
						color1[2] = cell->Sum1[2] / p1;
						for (auto j = 0; j < p1; ++j) {
							memcpy(pixel, color1, 3);
							pixel += 3;
						}
					}
					memcpy(pixel, border, 3);
					pixel += 3;
					p2 = x + EvalArea - cell->X - 1;
				} else {
					p2 = EvalArea;
				}
				if (p2 > 0) {
					uint8_t color2[3];
					color2[0] = cell->Sum2[0] / p2;
					color2[1] = cell->Sum2[1] / p2;
					color2[2] = cell->Sum2[2] / p2;
					for (auto j = 0; j < p2; ++j) {
						memcpy(pixel, color2, 3);
						pixel += 3;
					}
				}
			}
		}
	}
};

void process3(const char* inFile1, const char* inFile2, const char* outFile) {

	uint32_t freq = 5;

	TImage* in = TImage::Load(inFile1);

	TGradient3 gp(50, 30);
	gp.countCells(in, freq);

	TImage out(in->Width + gp.Border->Width + gp.Width, in->Height * 2, in->Depth);

	for (uint32_t y = 0; y < in->Height; ++y) {
		for (uint32_t x = 0; x < in->Width; ++x) {
			memcpy(out.Get(x, y), in->Get(x,y), out.Depth);
		}
	}

	gp.Border->Draw(&out, in->Width, 0);
	gp.DrawShapes(&out, in->Width + gp.Border->Width, 0);

	delete in;

	in = TImage::Load(inFile2);
	gp.countCells(in, freq);

	for (uint32_t y = 0; y < in->Height; ++y) {
		for (uint32_t x = 0; x < in->Width; ++x) {
			memcpy(out.Get(x, y + in->Height), in->Get(x,y), out.Depth);
		}
	}

	gp.Border->Draw(&out, in->Width, in->Height);
	gp.DrawShapes(&out, in->Width + gp.Border->Width, in->Height);

	delete in;

	out.Flip();
	out.SaveJpg(outFile);
}

struct XLine {
	uint16_t X;
	uint8_t  Color[3];
	
};

struct YLine {
	uint8_t  Color[3];
	uint16_t XIdx;
};

struct TGradient4 {

	const uint16_t NO_VALUE = std::numeric_limits<uint16_t>::max();
	int      MinValue;
	uint16_t EvalArea;
	int      MinDiff;

	uint32_t Width;
	uint16_t XMin;
	uint16_t XMax;

	TGradient4(int minValue, uint16_t evalArea, int minDiff) {
		MinValue = minValue;
		EvalArea = evalArea;
		MinDiff = minDiff;
	}

	void countGradients(TImage* in, uint32_t freq) {
		int half = freq / 2;
		int siy[in->Depth];
		int sumy[in->Depth];
		Width = in->Width - freq;
		XMin = freq / 2;
		XMax = XMin + Width - 1;

		BorderInit(Width, in->Height);
		for (uint32_t y = 0; y < in->Height; ++y) {
			memset(siy, 0, in->Depth * sizeof(int));
			memset(sumy, 0, in->Depth * sizeof(int));
			for (uint32_t i = 0; i < freq; ++i) {
				auto pixel = in->Get(i, y);
				for (uint32_t c = 0; c < in->Depth; ++c) {
					sumy[c] += (int)pixel[c];
					siy[c] += ((int)i - half) * pixel[c];
				}
			}

			processSumIY(XMin, y, in->Depth, siy);

			uint32_t front = freq;
			for (uint32_t x = 1; x < Width; ++x) {
				auto frontY = in->Get(front, y);
				auto tailY = in->Get(front - freq, y);
				for (uint32_t c = 0; c < in->Depth; ++c) {
					sumy[c] -= tailY[c];
					siy[c] += half * ((int)frontY[c] + (int)tailY[c]) - sumy[c];
					sumy[c] += (int)frontY[c];
				}
				++front;
				processSumIY(x + XMin, y, in->Depth, siy);
			}
		}

		//printf("current=%u\n", current);
	}

	TImage* Image;
	XLine*   XLines;
	uint16_t XIdx;
	YLine*   YLines;

	int CMax;
	uint32_t Sum1[3];
	uint32_t Sum2[3];
	uint32_t Count1;
	uint32_t Count2;

	void countCells(TImage* in, uint32_t freq) {
		Image = in;
		XLines = new XLine[in->Height * (in->Width - freq) / EvalArea];
		YLines = new YLine[in->Height];

		CMax = 0;
		XIdx = 0;
		memset(Sum1, 0, 3 * sizeof(uint32_t));
		Count1 = 0;

		countGradients(in, freq);
	}

	void processSumIY(uint16_t  x,
	                  uint16_t  y,
	                  uint8_t   depth,
	                  int*      siy) {

		bool hasMax = false;
		for (uint8_t c = 0; c < depth; ++c) {
			int siy_abs = abs(siy[c]);
			if (siy_abs > MinValue) {
				BorderSet(x, y);
				if (siy_abs > CMax) {
					CMax = siy_abs;
					hasMax = true;
				}
			}
		}

		uint8_t* pixel = Image->Get(x, y);

		if (hasMax) {
			XLines[XIdx].X = x;
			if (Count1 > 0) {
				for (uint8_t c = 0; c < depth; ++c) {
					XLines[XIdx].Color[c] = Sum1[c] / Count1;
				}
			}
			memset(Sum2, 0, 3 * sizeof(uint32_t));
			Count2 = 0;
		} else {
			if (CMax > 0) {
				for (uint8_t c = 0; c < depth; ++c) {
					Sum2[c] += pixel[c];
				}
				++Count2;
			}
		}
		for (uint8_t c = 0; c < depth; ++c) {
			Sum1[c] += pixel[c];
		}
		++Count1;

		if ((CMax > 0) && (/*x % EvalArea == EvalArea - 1*/ Count2 >= EvalArea)) {
			for (uint8_t c = 0; c < depth; ++c) {
				if (abs(XLines[XIdx].Color[c] - (int)Sum2[c] / Count2) > MinDiff) {
					++XIdx;
					break;
				}
			}
			CMax = 0;
			memcpy(Sum1, Sum2, 3 * sizeof(uint32_t));
			Count1 = Count2;
		}

		if (x == XMax) {
			if (CMax > 0) {
				++XIdx;
				CMax = 0;
				if (Count2 > 0) {
					for (uint8_t c = 0; c < depth; ++c) {
						YLines[y].Color[c] = Sum2[c] / Count2;
					}
				}
			} else {
				if (Count1 > 0) {
					for (uint8_t c = 0; c < depth; ++c) {
						YLines[y].Color[c] = Sum1[c] / Count1;
					}
				}
			}

			memset(Sum1, 0, 3 * sizeof(uint32_t));
			Count1 = 0;
			YLines[y].XIdx = XIdx;

		}
	}

	TPicture<uint8_t>* Border;
	void BorderInit(uint32_t width, uint32_t height) {
		Border = new TPicture<uint8_t>(width, height, 0);
	}

	void BorderSet(uint16_t  x,
	               uint16_t  y) {
		Border->Set(x - XMin,y, 0xFF);
	}

	void DrawShapes(TImage* out, uint32_t x0, uint32_t y0) {
		
		printf("lines=%hu\n", XIdx);
		
		//uint8_t black[3] = {0, 0, 0};
		uint8_t border[3] = {255, 255, 255};
		uint16_t XIdx = 0;
		for (uint16_t y = 0; y < Image->Height; ++y) {
			uint16_t x = 0;
			printf("%3hu:", Image->Height - y - 1);
			for (;XIdx < YLines[y].XIdx; ++XIdx) {
				while (x < XLines[XIdx].X) {
					memcpy(out->Get(x0 + x++, y0 + y), XLines[XIdx].Color, 3);
					//memcpy(out->Get(x0 + x++, y0 + y), black, 3);
				}
				printf(" [%3hhu,%3hhu,%3hhu] %3hu", XLines[XIdx].Color[0], XLines[XIdx].Color[1], XLines[XIdx].Color[2], XLines[XIdx].X);
				memcpy(out->Get(x0 + x++, y0 + y), border, 3);
			}
			printf(" [%3hhu,%3hhu,%3hhu]\n", YLines[y].Color[0], YLines[y].Color[1], YLines[y].Color[2]);
			while (x < Width) {
				memcpy(out->Get(x0 + x++, y0 + y), YLines[y].Color, 3);
				//memcpy(out->Get(x0 + x++, y0 + y), black, 3);
			}
		}
	}
};

void process4(const char* inFile1, const char* inFile2, const char* outFile) {

	uint32_t freq = 5;

	TImage* in = TImage::Load(inFile1);

	TGradient4 gp(50, 10, 30);
	gp.countCells(in, freq);

	TImage out(in->Width + gp.Border->Width + in->Width, in->Height * 2, in->Depth);

	for (uint32_t y = 0; y < in->Height; ++y) {
		for (uint32_t x = 0; x < in->Width; ++x) {
			memcpy(out.Get(x, y), in->Get(x,y), out.Depth);
		}
	}

	gp.Border->Draw(&out, in->Width, 0);
	gp.DrawShapes(&out, in->Width + gp.Border->Width, 0);

	delete in;

	in = TImage::Load(inFile2);
	gp.countCells(in, freq);

	for (uint32_t y = 0; y < in->Height; ++y) {
		for (uint32_t x = 0; x < in->Width; ++x) {
			memcpy(out.Get(x, y + in->Height), in->Get(x,y), out.Depth);
		}
	}

	gp.Border->Draw(&out, in->Width, in->Height);
	gp.DrawShapes(&out, in->Width + gp.Border->Width, in->Height);

	delete in;

	out.Flip();
	out.SaveJpg(outFile);
}

int main(int argn, char** argv) {
	if (argn >= 3) {
		//process(argv[1], argv[2], argv[3]);
		//process1(argv[1], argv[2], argv[3]);
		//process2(argv[1], argv[2], argv[3]);
		//process3(argv[1], argv[2], argv[3]);
		process4(argv[1], argv[2], argv[3]);
	} else {
		printf("Usage: %s imgA imgB out\n", argv[0]);
	}
	return 0;
}
