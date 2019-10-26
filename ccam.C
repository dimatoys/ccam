#include <stdio.h>
#include <string.h>

// https://www.airspayce.com/mikem/bcm2835/
//#include "bcm_host.h"
//#include "interface/vcos/vcos.h"

#include <bcm2835.h>

#include "interface/mmal/mmal.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
//#include "interface/mmal/util/mmal_connection.h"

#include "jpeg.h"

#define MMAL_CAMERA_VIDEO_PORT 1

// Camera
// https://github.com/cedricve/raspicam/blob/master/src/private/private_impl.cpp
// https://github.com/tasanakorn/rpi-mmal-demo/blob/develop/buffer_demo.c

// DMA SPI
// https://github.com/Wallacoloo/Raspberry-Pi-DMA-Example/blob/master/dma-gpio.c
// http://www.airspayce.com/mikem/bcm2835/
// https://www.airspayce.com/mikem/bcm2835/spi_8c-example.html
// https://www.mbtechworks.com/hardware/raspberry-pi-UART-SPI-I2C.html

// https://www.raspberrypi.org/app/uploads/2012/02/BCM2835-ARM-Peripherals.pdf


// modes: https://picamera.readthedocs.io/en/release-1.12/fov.html

class FramesBuffer {
	int      FrameSize;
	uint8_t* Buffer;
	int      Frames;
public:
	int      CurrentFrame;

	FramesBuffer(int frame_size, int frames) {
		FrameSize = frame_size;
		Frames = frames;
		Buffer = new uint8_t[FrameSize * Frames];
		CurrentFrame = 0;
	}
		
	bool HasMoreSpace() {
		return CurrentFrame < Frames;
	}
		
	bool AddFrame(const void* frame) {
		if (CurrentFrame < Frames) {
			memcpy(Buffer + CurrentFrame++ * FrameSize, frame, FrameSize);
		}
		return HasMoreSpace();
	}
		
	uint8_t* GetFrame(int frame) {
		return Buffer + frame * FrameSize;
	}
		
	~FramesBuffer() {
		delete Buffer;
	}
};

uint8_t countSum(uint8_t* data, uint32_t length) {
	uint8_t sum = 0;
	for (uint32_t i = 0; i < length; ++i) {
		sum += data[i];
	}
	return sum;
}

FramesBuffer* fbuffer = NULL;
MMAL_COMPONENT_T* camera_component = NULL;

void start(MMAL_COMPONENT_T* camera) {
	printf("%llu: START recording\n", bcm2835_st_read());
	MMAL_PORT_T* video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
	MMAL_STATUS_T status = mmal_port_parameter_set_boolean(video_port, MMAL_PARAMETER_CAPTURE, 1);
	if (status != MMAL_SUCCESS) {
        printf("Failed to start capture: %u\n", status);
    }
}

void stop(MMAL_COMPONENT_T* camera) {
	printf("%llu: STOP recording\n", bcm2835_st_read());
	MMAL_PORT_T* video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
	MMAL_STATUS_T status = mmal_port_parameter_set_boolean(video_port, MMAL_PARAMETER_CAPTURE, 0);
	if (status != MMAL_SUCCESS) {
        printf("Failed to stop capture: %u\n", status);
    }
}

void dumpBuffer(MMAL_BUFFER_HEADER_T *buffer) {
	uint8_t sum = countSum(buffer->data + buffer->offset, buffer->length);
	printf("BUFFER: %X data=%X length=%u pts=%llu sum=%u\n",
				buffer,
				buffer->data,
				buffer->length,
				buffer->pts,
				(uint32_t)sum);
}

static void video_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
	// https://github.com/raspberrypi/userland/blob/master/interface/mmal/mmal_buffer.h
	// http://www.jvcref.com/files/PI/documentation/html/struct_m_m_a_l___b_u_f_f_e_r___h_e_a_d_e_r___t.html
	//printf("%llu: CALLBACK port=%s \n", bcm2835_st_read(),port->is_enabled ? "ENABLES" : "DISABLED");
	//dumpBuffer(buffer);
	bool hasMoreSpace = fbuffer->AddFrame(buffer->data + buffer->offset);
	mmal_buffer_header_release(buffer);
	if (!hasMoreSpace) {
		stop(camera_component);
		return;
	}
	if (port->is_enabled) {
		MMAL_STATUS_T status;
		MMAL_BUFFER_HEADER_T* new_buffer = mmal_queue_get ( ((MMAL_POOL_T*)(port->userdata))->queue );
		if ( new_buffer ) {
			dumpBuffer(new_buffer);
			status = mmal_port_send_buffer ( port, new_buffer );
		}
		if ( !new_buffer || status != MMAL_SUCCESS ) {
			printf ( "Unable to return a buffer to the encoder port" );
		}
	}
}

MMAL_COMPONENT_T* setup(uint32_t width, uint32_t height, uint32_t frames) {
	MMAL_COMPONENT_T* camera;
    MMAL_STATUS_T status = mmal_component_create ( MMAL_COMPONENT_DEFAULT_CAMERA, &camera );

    if ( status != MMAL_SUCCESS ) {
		// http://www.jvcref.com/files/PI/documentation/html/group___mmal_types.html#ga5cf856e743410d3a43dd395209cb61ab
        printf( "Failed to create camera component (%u)\n", status );
        return NULL;
    }

    if ( !camera->output_num ) {
        printf( "Camera doesn't have output ports\n" );
        mmal_component_destroy ( camera );
        return NULL;
    }

    MMAL_PORT_T* video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];

	// https://raw.githubusercontent.com/raspberrypi/userland/master/interface/mmal/mmal_parameters_camera.h
	MMAL_PARAMETER_CAMERA_CONFIG_T cam_config;
    cam_config.hdr.id=MMAL_PARAMETER_CAMERA_CONFIG;
    cam_config.hdr.size=sizeof ( cam_config );
    cam_config.max_stills_w = width;/**< Max size of stills capture */
    cam_config.max_stills_h = height;
    cam_config.stills_yuv422 = 0;/**< Allow YUV422 stills capture */
    cam_config.one_shot_stills = 0;
    cam_config.max_preview_video_w = width;/**< Max size of the preview or video capture frames */
    cam_config.max_preview_video_h = height;
    cam_config.num_preview_video_frames = 3;
    cam_config.stills_capture_circular_buffer_height = 0;
    cam_config.fast_preview_resume = 0;
    cam_config.use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC;
    
	mmal_port_parameter_set ( camera->control, &cam_config.hdr );
	
	// http://www.jvcref.com/files/PI/documentation/html/struct_m_m_a_l___e_s___f_o_r_m_a_t___t.html
	MMAL_ES_FORMAT_T* format = video_port->format;

    format->encoding = MMAL_ENCODING_RGB24;
    //format->encoding_variant = MMAL_ENCODING_I420;

    format->es->video.width = width;
    format->es->video.height = height;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = width;
    format->es->video.crop.height = height;
	//format->es->video.par.num
	//format->es->video.par.den
    format->es->video.frame_rate.num = frames;
    format->es->video.frame_rate.den = 1;


    video_port->buffer_size = video_port->buffer_size_recommended;
    video_port->buffer_num = 2;

    status = mmal_port_format_commit(video_port);

    printf(" camera video buffer_size = %d\n", video_port->buffer_size);
    printf(" camera video buffer_num = %d\n", video_port->buffer_num);
    if (status != MMAL_SUCCESS) {
        printf("Error: unable to commit camera video port format (%u)\n", status);
		mmal_component_destroy ( camera );
        return NULL;
    }

    // crate pool form camera video port
    MMAL_POOL_T* video_port_pool = (MMAL_POOL_T *)mmal_port_pool_create(video_port, video_port->buffer_num, video_port->buffer_size);
    video_port->userdata = (struct MMAL_PORT_USERDATA_T *)video_port_pool;

    status = mmal_port_enable(video_port, video_buffer_callback);
    if (status != MMAL_SUCCESS) {
        printf("Error: unable to enable camera video port (%u)\n", status);
		mmal_component_destroy ( camera );
        return NULL;
    }

    status = mmal_component_enable(camera);
	if ( status ) {
        printf( "camera component couldn't be enabled: %u\n", status );
        mmal_component_destroy ( camera );
        return NULL;
    }
    
    int num = mmal_queue_length(video_port_pool->queue);
    printf("qnum=%d\n", num);
    for (int q = 0; q < num; q++) {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(video_port_pool->queue);

        if (!buffer)
            printf("Unable to get a required buffer %d from pool queue\n", q);

        if (mmal_port_send_buffer(video_port, buffer) != MMAL_SUCCESS)
                printf("Unable to send a buffer to encoder output port (%d)\n", q);
    }

	return camera;
}

int main(int argn, char** argv) {
	printf("Setup\n");
	uint32_t width = 640;
	uint32_t height = 480;
	uint32_t depth = 3;
	uint32_t fps = 30;
	bcm2835_init();
	camera_component = setup(width, height, fps);
	if (camera_component != NULL) {
		fbuffer = new FramesBuffer(camera_component->output[MMAL_CAMERA_VIDEO_PORT]->buffer_size, 150);
		// bcm2835_delay(2000);
		start(camera_component);
		// delay 
		// start_recording
		bcm2835_delay(10000);
		stop(camera_component);
		mmal_component_destroy ( camera_component );
		
		printf("%d frames recorded\n", fbuffer->CurrentFrame);
		/*
		char filename[100];
		for (int i = 0; i < fbuffer->CurrentFrame; ++i) {
			sprintf(filename, "../ccampic/%03d.jpg", i);
			write_jpeg_file(filename, fbuffer->GetFrame(i), width, height, depth);
		}
		*/
	} else {
		printf("Error setup camera\n");
	}
	return 0;
}
