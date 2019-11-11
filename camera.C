#include <stdio.h>

#include "interface/mmal/mmal.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"

#include <pigpio.h>

#include "camera.h"

#define MMAL_CAMERA_VIDEO_PORT 1

TCamera* TCamera::Instance = NULL;
void* TCamera::Camera = NULL;

void video_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
	//printf("%llu: CALLBACK port=%s \n", bcm2835_st_read(),port->is_enabled ? "ENABLES" : "DISABLED");
	if (TCamera::Instance != NULL) {
		TCamera::Instance->Write(buffer->data + buffer->offset, buffer->length, buffer->pts);
	}

	mmal_buffer_header_release(buffer);
	if (port->is_enabled) {
		MMAL_STATUS_T status;
		MMAL_BUFFER_HEADER_T* new_buffer = mmal_queue_get ( ((MMAL_POOL_T*)(port->userdata))->queue );
		if ( new_buffer ) {
			status = mmal_port_send_buffer ( port, new_buffer );
		}
		if ( !new_buffer || status != MMAL_SUCCESS ) {
			printf ( "Unable to return a buffer to the encoder port" );
		}
	}
}

MMAL_COMPONENT_T* setup(uint32_t width, uint32_t height, uint32_t fps) {
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
    format->es->video.frame_rate.num = fps;
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

void TCamera::Init(uint32_t width,
	               uint32_t height,
	               uint32_t depth,
	               uint32_t fps) {
		if (Instance != NULL) {
			Instance->Release();
		}
		TCamera::Camera = setup(width, height, fps);
}

void TCamera::Start() {
	if (Instance != NULL) {
		MMAL_PORT_T* video_port = ((MMAL_COMPONENT_T*)Instance->Camera)->output[MMAL_CAMERA_VIDEO_PORT];
		MMAL_STATUS_T status = mmal_port_parameter_set_boolean(video_port, MMAL_PARAMETER_CAPTURE, 1);
		if (status != MMAL_SUCCESS) {
			printf("Failed to start capture: %u\n", status);
		}
	}
}

void TCamera::Stop() {
	if (Instance != NULL) {
		MMAL_PORT_T* video_port = ((MMAL_COMPONENT_T*)Instance->Camera)->output[MMAL_CAMERA_VIDEO_PORT];
		MMAL_STATUS_T status = mmal_port_parameter_set_boolean(video_port, MMAL_PARAMETER_CAPTURE, 0);
		if (status != MMAL_SUCCESS) {
			printf("Failed to stop capture: %u\n", status);
		}
	}
}

void TCamera::Release() {
	if (Instance != NULL) {
		mmal_component_destroy ((MMAL_COMPONENT_T*)Camera);
		delete Instance;
		Instance = NULL;
	}
}

void TCamera::WarmupDelay() {
	gpioDelay(500000);
}

TCameraMemory* TCameraMemory::Init(uint32_t width,
	                               uint32_t height,
	                               uint32_t depth,
	                               uint32_t fps,
	                               uint32_t frames) {
		if (Instance != NULL) {
			Instance->Release();
		}
		TCamera::Camera = setup(width, height, fps);
		if (TCamera::Camera != NULL) {
			TCameraMemory* instance = new TCameraMemory(width, height, depth, fps);
			Instance = instance;
			instance->Buffer = new FramesBuffer(width * height * depth, frames);
			return instance;
		} else {
			return NULL;
		}
}

void TCameraMemory::Write(uint8_t *data, uint32_t length, int64_t  pts) {
	if (!Buffer->AddFrame(data)) {
		Stop();
	}
}

