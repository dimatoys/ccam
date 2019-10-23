#include <stdio.h>

//#include "bcm_host.h"
//#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
//#include "interface/mmal/util/mmal_connection.h"

#define MMAL_CAMERA_VIDEO_PORT 1

// Camera
// http://robotblogging.blogspot.com/2013/10/an-efficient-and-simple-c-api-for.html
// https://github.com/cedricve/raspicam/blob/master/src/private/private_impl.cpp
// https://github.com/tasanakorn/rpi-mmal-demo/blob/develop/buffer_demo.c

// DMA SPI
// https://github.com/Wallacoloo/Raspberry-Pi-DMA-Example/blob/master/dma-gpio.c
// http://www.airspayce.com/mikem/bcm2835/
// https://www.airspayce.com/mikem/bcm2835/spi_8c-example.html
// https://www.mbtechworks.com/hardware/raspberry-pi-UART-SPI-I2C.html


// modes: https://picamera.readthedocs.io/en/release-1.12/fov.html

static void video_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
	// https://github.com/raspberrypi/userland/blob/master/interface/mmal/mmal_buffer.h
	// http://www.jvcref.com/files/PI/documentation/html/struct_m_m_a_l___b_u_f_f_e_r___h_e_a_d_e_r___t.html
	mmal_buffer_header_release(buffer);
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
    //cam_config.max_stills_w = state->width;
    //cam_config.max_stills_h = state->height;
    //cam_config.stills_yuv422 = 0;
    //cam_config.one_shot_stills = 0;
    cam_config.max_preview_video_w = width;
    cam_config.max_preview_video_h = height;
    cam_config.num_preview_video_frames = 3;
    //cam_config.stills_capture_circular_buffer_height = 0;
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

	return camera;
}

void start(MMAL_COMPONENT_T* camera) {
	MMAL_PORT_T* video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
	MMAL_STATUS_T status = mmal_port_parameter_set_boolean(video_port, MMAL_PARAMETER_CAPTURE, 1);
	if (status != MMAL_SUCCESS) {
        printf("Failed to start capture: %u\n", status);
    }
}

void stop(MMAL_COMPONENT_T* camera) {
	MMAL_PORT_T* video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
	MMAL_STATUS_T status = mmal_port_parameter_set_boolean(video_port, MMAL_PARAMETER_CAPTURE, 0);
	if (status != MMAL_SUCCESS) {
        printf("Failed to stop capture: %u\n", status);
    }
}

int main(int argn, char** argv) {
	printf("Setup\n");
	MMAL_COMPONENT_T* camera = setup(640, 480, 30);
	if (camera != NULL) {
		
		mmal_component_destroy ( camera );
	} else {
		printf("Error setup camera\n");
	}
	return 0;
}
