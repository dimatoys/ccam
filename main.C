#include "camera.h"

#include <stdio.h>

void testCameraA() {
}

void testCameraB() {
	TCameraMemory* mc = TCameraMemory::Init(640, 480, 3, 30, 150);
	if (mc != NULL) {
		mc->WarmupDelay();
		mc->Start();
		while(mc->Buffer->CurrentFrame < 15);
		mc->Stop();
		printf("%u frames made", mc->Buffer->CurrentFrame);
		mc->Release();
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
