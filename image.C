#include "image.h"
#include "jpeg.h"

#include <stdio.h>
#include <string.h>

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

void TImage::Flip() {
	auto halfy = Height / 2;
	auto line_size = Width * Depth;
	uint8_t buffer[line_size];
	for (uint32_t y = 0; y < halfy; ++ y) {
		auto line1 = Get(0, y);
		auto line2 = Get(0, Height - 1 - y); 
		memcpy(buffer, line1, line_size);
		memcpy(line1, line2, line_size);
		memcpy(line2, buffer, line_size);
	}
}
