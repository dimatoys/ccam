#include "camera.h"
#include "image.h"

#include <stdio.h>
#include <pigpio.h>

uint32_t width = 640;
uint32_t height = 480;
uint32_t depth = 3;
uint32_t fps = 30;
uint32_t frames = 15;

void runCameraA(int gpio, int level, uint32_t tick, void* userdata) {
	printf("GPIO %d became %u at %d\n", gpio, level, tick);
	if (level == 1) {
		TCameraMemory* mc = (TCameraMemory*)userdata;
		mc->Buffer->Reset();
		mc->Start();
		while(mc->Buffer->CurrentFrame < frames);
		mc->Stop();
		printf("%u frames made\n", mc->Buffer->CurrentFrame);

		char buffer[300];
		for (uint32_t i = 0; i < mc->Buffer->CurrentFrame; ++i) {
			TImage image(width, height, depth, mc->Buffer->GetFrame(i));
			sprintf(buffer, "../ccampic/%d.dump", i);
			image.Save(buffer);
			sprintf(buffer, "../ccampic/%d.jpg", i);
			image.SaveJpg(buffer);
		}
	}

}

void testCameraA() {
	gpioInitialise();
	TCameraMemory* mc = TCameraMemory::Init(width, height, depth, fps, frames);
	if (mc != NULL) {
		mc->WarmupDelay();
		int status = gpioSetAlertFuncEx(2, runCameraA, mc);
		printf("status=%d\n", status);
		gpioDelay(1000000000);
		mc->Release();
	} else {
		printf("Error setup camera\n");
	}
}

void testCameraB() {
	gpioInitialise();
	gpioSetMode(18, PI_OUTPUT);
	gpioWrite(18, 0);
	TCameraMemory* mc = TCameraMemory::Init(width, height, depth, fps, frames);
	if (mc != NULL) {
		mc->WarmupDelay();
		gpioWrite(18, 1);
		mc->Start();
		while(mc->Buffer->CurrentFrame < frames);
		mc->Stop();
		printf("%u frames made\n", mc->Buffer->CurrentFrame);
		fflush (stdout);
		mc->Release();
		char buffer[300];
		for (uint32_t i = 0; i < mc->Buffer->CurrentFrame; ++i) {
			TImage image(width, height, depth, mc->Buffer->GetFrame(i));
			sprintf(buffer, "../ccampic/%d.dump", i);
			image.Save(buffer);
			sprintf(buffer, "../ccampic/%d.jpg", i);
			image.SaveJpg(buffer);
		}
	}
}

int main(int argn, char** argv) {
	if (argn > 1) {
		switch(argv[1][0]) {
		case 'a':
			testCameraA();
			break;
		case 'b':
			testCameraB();
			break;
		}
	}
	return 0;
}
