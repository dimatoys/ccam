#pragma once

#include <stdio.h>

class IWritableImage {
public:
	virtual void SetDimensions(int width, int height, int depth) = 0;
	virtual void Write(const unsigned char* buffer, int size) = 0;
};

int write_jpeg_file(const char *filename, void* raw_image, int width, int height, int bytes_per_pixel);
int write_fliped_jpeg_file(const char *filename, void* raw_image, int width, int height, int bytes_per_pixel);

int read_jpeg_file (const char * filename, IWritableImage* image);
