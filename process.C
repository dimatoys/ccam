#include "image.h"

#include <stdio.h>
#include <string.h>

void process(TImage* inpA, TImage* inpB) {
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

int main(int argn, char** argv) {
	if (argn >= 4) {
		process(argv[1], argv[2], argv[3]);
	} else {
		printf("Usage: %s imgA imgB out\n", argv[0]);
	}
	return 1;
}
