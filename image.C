#include "image.h"
#include "jpeg.h"

#include <stdio.h>

void TImage::Save(const char* fileName) {
	FILE *outfile = fopen( fileName, "wb");
	if (outfile) {
		fwrite(&Width, 1, sizeof(Width), outfile);
		fwrite(&Height, 1, sizeof(Height), outfile);
		fwrite(&Depth, 1, sizeof(Depth), outfile);
		fwrite(Data, 1, Width * Height * Depth, outfile);
		fclose( outfile );
	}
}

TImage* TImage::Load(const char* fileName) {
		FILE* file = fopen(fileName, "rb");
		if (file != NULL) {
			uint32_t width;
			uint32_t height;
			uint32_t depth;
			fread(&width, 1, sizeof(uint32_t), file);
			fread(&height, 1, sizeof(uint32_t), file);
			fread(&depth, 1, sizeof(uint32_t), file);

			printf("LoadDump: %d x %d : %d\n", width, height, depth);

			TImage* img = new TImage(width, height, depth);
			fread(img->Data, 1, img->GetSize(), file);
			fclose(file);
			return img;
		} else {
			printf("Cannot open %s\n", fileName);
			return NULL;
		}
}

void TImage::SaveJpg(const char* fileName) {
	write_jpeg_file(fileName, Data, Width, Height, Depth);
}
