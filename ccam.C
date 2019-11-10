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
			//dumpBuffer(new_buffer);
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

void testCamera() {
	printf("Setup\n");
	uint32_t width = 640;
	uint32_t height = 480;
	uint32_t depth = 3;
	uint32_t fps = 30;
	bcm2835_init();
	camera_component = setup(width, height, fps);
	if (camera_component != NULL) {
		fbuffer = new FramesBuffer(camera_component->output[MMAL_CAMERA_VIDEO_PORT]->buffer_size, 150);
		bcm2835_delay(500);
		start(camera_component);
		// delay 
		// start_recording
		bcm2835_delay(500);
		stop(camera_component);
		mmal_component_destroy ( camera_component );
		
		printf("%d frames recorded\n", fbuffer->CurrentFrame);

		char filename[100];
		for (int i = 0; i < fbuffer->CurrentFrame; ++i) {
			sprintf(filename, "../ccampic/%03d.jpg", i);
			write_jpeg_file(filename, fbuffer->GetFrame(i), width, height, depth);
		}
	} else {
		printf("Error setup camera\n");
	}
}

// exa: https://raspberrypi.stackexchange.com/questions/76109/raspberry-as-an-i2c-slave
// doc: http://abyz.me.uk/rpi/pigpio/cif.html#bscXfer
// src: https://github.com/joan2937/pigpio/blob/master/pigpio.c

/*
pi@raspberrypi:~ $ pinout
.-------------------------.
| 55GoooGooGooooGoGooo J8 |
| 3oooGooo3oooGooooooG   |c
---+       +---+ PiZero W|s
 sd|       |SoC|   V1.1  |i
---+|hdmi| +---+  usb pwr |
`---|    |--------| |-| |-'

Revision           : 9000c1
SoC                : BCM2835
RAM                : 512Mb
Storage            : MicroSD
USB ports          : 1 (excluding power)
Ethernet ports     : 0
Wi-fi              : True
Bluetooth          : True
Camera ports (CSI) : 1
Display ports (DSI): 0

J8:
   3V3  (1) (2)  5V
 GPIO2  (3) (4)  5V
 GPIO3  (5) (6)  GND
 GPIO4  (7) (8)  GPIO14
   GND  (9) (10) GPIO15
GPIO17 (11) (12) GPIO18
GPIO27 (13) (14) GND
GPIO22 (15) (16) GPIO23
   3V3 (17) (18) GPIO24
GPIO10 (19) (20) GND
 GPIO9 (21) (22) GPIO25
GPIO11 (23) (24) GPIO8
   GND (25) (26) GPIO7
 GPIO0 (27) (28) GPIO1
 GPIO5 (29) (30) GND
 GPIO6 (31) (32) GPIO12
GPIO13 (33) (34) GND
GPIO19 (35) (36) GPIO16
GPIO26 (37) (38) GPIO20
   GND (39) (40) GPIO21

*/

void testMasterSPI() {
    // If you call this, it will not actually access the GPIO
    // Use for testing
     //        bcm2835_set_debug(1);
     
/*
    uint32_t *bcm2835_peripherals = (uint32_t *)MAP_FAILED;
	if ((memfd = open("/dev/mem", O_RDWR | O_SYNC) ) < 0) 
	{
	  fprintf(stderr, "bcm2835_init: Unable to open /dev/mem: %s\n",
		  strerror(errno)) ;
	  goto exit;
	}
      
      // Base of the peripherals block is mapped to VM
      bcm2835_peripherals = mapmem("gpio", bcm2835_peripherals_size, memfd, (off_t)bcm2835_peripherals_base);
      if (bcm2835_peripherals == MAP_FAILED) goto exit;
      
      // Now compute the base addresses of various peripherals, 
      // which are at fixed offsets within the mapped peripherals block
      // Caution: bcm2835_peripherals is uint32_t*, so divide offsets by 4

      bcm2835_spi0 = bcm2835_peripherals + BCM2835_SPI0_BASE/4;
*/
     
     
    if (!bcm2835_init())
    {
      printf("bcm2835_init failed. Are you running as root??\n");
      return;
    }
    /*

	volatile uint32_t* paddr;

    if (bcm2835_spi0 == MAP_FAILED)
      return 0; // bcm2835_init() failed, or not root
    
    // Set the SPI0 pins to the Alt 0 function to enable SPI0 access on them
    bcm2835_gpio_fsel(RPI_GPIO_P1_26, BCM2835_GPIO_FSEL_ALT0); // CE1
    bcm2835_gpio_fsel(RPI_GPIO_P1_24, BCM2835_GPIO_FSEL_ALT0); // CE0
    bcm2835_gpio_fsel(RPI_GPIO_P1_21, BCM2835_GPIO_FSEL_ALT0); // MISO
    bcm2835_gpio_fsel(RPI_GPIO_P1_19, BCM2835_GPIO_FSEL_ALT0); // MOSI
    bcm2835_gpio_fsel(RPI_GPIO_P1_23, BCM2835_GPIO_FSEL_ALT0); // CLK
    
    // Set the SPI CS register to the some sensible defaults
    paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    bcm2835_peri_write(paddr, 0); // All 0s
    
    // Clear TX and RX fifos
    bcm2835_peri_write_nb(paddr, BCM2835_SPI0_CS_CLEAR);

    */
    bcm2835_spi_begin();

    //ignored
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      // The default

	/*
	volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    // Mask in the CPO and CPHA bits of CS
    bcm2835_peri_set_bits(paddr, mode << 2, BCM2835_SPI0_CS_CPOL | BCM2835_SPI0_CS_CPHA);
	*/

    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   // The default
    
    /*
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CLK/4;
    bcm2835_peri_write(paddr, divider);
    */
    
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_65536); // The default
    
    /*
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    // Mask in the CS bits of CS
    bcm2835_peri_set_bits(paddr, cs, BCM2835_SPI0_CS_CS);
    */
    
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      // The default
    
    /*
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    uint8_t shift = 21 + cs;
    // Mask in the appropriate CSPOLn bit
    bcm2835_peri_set_bits(paddr, active << shift, 1 << shift);
    */
    
    
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);      // the default
    
	char* tbuf = (char*)"0123456789";
	char rbuf[20];

	//bcm2835_spi_transfernb(tbuf,rbuf,10);
	
	
	/*
	volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    volatile uint32_t* fifo = bcm2835_spi0 + BCM2835_SPI0_FIFO/4;
    uint32_t ret;

    // This is Polled transfer as per section 10.6.1
    // BUG ALERT: what happens if we get interupted in this section, and someone else
    // accesses a different peripheral? 
    // Clear TX and RX fifos
    //
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    // Set TA = 1
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

    // Maybe wait for TXD
    while (!(bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_TXD));

    // Write to FIFO, no barrier
    bcm2835_peri_write_nb(fifo, value);

    // Wait for DONE to be set
    while (!(bcm2835_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE));

    // Read any byte that was sent back by the slave while we sere sending to it
    ret = bcm2835_peri_read_nb(fifo);

    // Set TA = 0, and also set the barrier
    bcm2835_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);

    return ret; 
	*/
	
	
	uint8_t v = bcm2835_spi_transfer(0x34);
	printf("v=%X\n", (uint32_t)v);
	v = bcm2835_spi_transfer(0x35);
	printf("v=%X\n", (uint32_t)v);
	v = bcm2835_spi_transfer(0x36);
	printf("v=%X\n", (uint32_t)v);

    bcm2835_spi_end();
    bcm2835_close();
}

#include <pigpio.h>

void testMasterSPI2() {
	int status = gpioInitialise();
	if (status < 0){
	    printf("Error initialize %d\n", status);
	    return;
	}

	unsigned speed = 1000000;
	int h = spiOpen(0, speed, 0);

	if (h < 0){
		printf("error spi open %d\n", h);
	    return;
	}

	char tbuf[20];
	tbuf[0] = 0x33;
	tbuf[1] = 0x34;
	tbuf[2] = 0x35;
	char rbuf[16384];
	rbuf[0] = 0x12;
	rbuf[1] = 0x13;
	rbuf[2] = 0x14;
	status = spiXfer(h, tbuf, rbuf, 3);
	printf("status=%d r=%X\n", status, (uint32_t)rbuf[0]);

	spiClose(h);

   gpioTerminate();

}

/*

IT    invert transmit status flags
HC    enable host control

TF    enable test FIFO
IR    invert receive status flags
RE *s enable receive
TE *s enable transmit

BK    abort operation and clear FIFOs
EC    send control register as first I2C byte
ES    send status register as first I2C byte
PL    set SPI polarity high

PH	  set SPI phase high
I2 *  enable I2C mode
SP  s enable SPI mode
EN *s enable BSC peripheral

*/

/*

SSSSS	number of bytes successfully copied to transmit FIFO
RRRRR	number of bytes in receieve FIFO
TTTTT	number of bytes in transmit FIFO
RB	receive busy
TE	transmit FIFO empty
RF	receive FIFO full
TF	transmit FIFO full
RE	receive FIFO empty
TB	transmit busy

S S S S |  S R R R | R R T T | T T T RB | TE TF RE TB


STATUS0: 10406
STATUS: 406
STATUS: C04
STATUS: 406
RX: 00
STATUS: C04
STATUS: 406
RX: 00
* 
--------------------------
STATUS0: 110406
STATUS: 406
STATUS: C04
STATUS: 406
RX: 00
STATUS: 1C04
STATUS: 406
RX: 00 00 00
STATUS: 1C04
STATUS: 406
RX: 00 00 00
*
*/

/*
18 (MOSI),
19 (SCLK) white
20 (MISO)
21 (CE)   yellow
GRN       black
*/

void testSlaveSPI() {
	bsc_xfer_t xfer;
    gpioInitialise();
	xfer.control = 0x303;
    int status = bscXfer(&xfer);
    if (status >= 0) {
		printf("STATUS0: %0X\n", status);
		int last_status = status;
        xfer.rxCnt = 0;
		xfer.txCnt = 5;
		memcpy(xfer.txBuf, "abcde", 5);
        while(1) {
			status = bscXfer(&xfer);
			if (last_status != status) {
				printf("STATUS: %0X\n", status);
				last_status = status;
			}
            if (xfer.rxCnt > 0) {
				printf("RX:");
				for (int i = 0; i < xfer.rxCnt; ++i) {
					printf(" %02X", (uint32_t)xfer.rxBuf[i]);
				}
				printf("\n");
            }
       }
    } else {
		printf("cannot initialize SPI %d\n", status);
	}
}
//--------------------------------------------------------------------------------------------

// 2 - SDA
// 3 - SCL

void testMasterIIC() {
	if (!bcm2835_init())
    {
      printf("bcm2835_init failed. Are you running as root??\n");
      return;
    }
      
    if (!bcm2835_i2c_begin())
    {
        printf("bcm2835_i2c_begin failed. Are you running as root??\n");
        return;
    }
          
    bcm2835_i2c_setSlaveAddress(0x13);
    bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_148);

	uint8_t data = bcm2835_i2c_write("0123456789012345", 16);
    printf("Write Result = %X\n", (uint32_t)data);
	bcm2835_delay(5);

	data = bcm2835_i2c_write("abcdefghijklmnop", 16);
    printf("Write Result = %X\n", (uint32_t)data);
	bcm2835_delay(5);

	data = bcm2835_i2c_write("ABCDEFGHIJKLMNOP", 16);
    printf("Write Result = %X\n", (uint32_t)data);

    bcm2835_i2c_end();   
    bcm2835_close();
}

void testMasterIIC2() {
	int status = gpioInitialise();
	if (status < 0){
	    printf("Error initialize %d\n", status);
	    return;
	}

	int h = i2cOpen(1, 0x13, 0);

	if (h < 0){
		printf("error i2c open %d\n", h);
	    return;
	}

    status = i2cWriteByteData(h, 0x33, 0x41);
	//status = i2cWriteI2CBlockData(h, 0x11, "0123456789012345", 16);
    printf("status:%d\n", status);
	
    status = i2cWriteByteData(h, 0x33, 0x42);
	//status = i2cWriteI2CBlockData(h, 0x12, "abcdefghijklmnop", 16);
    printf("status:%d\n", status);

    status = i2cWriteByteData(h, 0x33, 0x43);
	//status = i2cWriteI2CBlockData(h, 0x13, "ABCDEFGHIJKLMNOP", 16);
    printf("status:%d\n", status);

	printf("%d %d %d\n", PI_BAD_HANDLE, PI_BAD_PARAM, PI_I2C_WRITE_FAILED);


	i2cClose(h);

   gpioTerminate();
}

// 18 (SDA)
// 19 (SCL)

void testSlaveIIC() {
	
	int status = gpioInitialise();
	if (status > 0) {
		int last_status = status;
		bsc_xfer_t xfer;
		xfer.control = (0x13<<16) | 0x305;

		memcpy(xfer.txBuf, "ABCD", 4);
		xfer.txCnt = 4;

		while(1) {
			status = bscXfer(&xfer);
			if (last_status != status) {
				printf("STATUS: %0X\n", status);
				last_status = status;
			}
            if (xfer.rxCnt > 0) {
				printf("RX:");
				for (int i = 0; i < xfer.rxCnt; ++i) {
					printf(" %02X", (uint32_t)xfer.rxBuf[i]);
				}
				printf("\n");
            }
		}
	} else {
		printf("Error intialize gpio %d\n", status);
	}
}

/*
STATUS: 400C2
STATUS: 401C2
STATUS: 402C2
STATUS: 403C2
STATUS: 10406
STATUS: 406
STATUS: C24
STATUS: 406
RX: 33 41 33 42 33 43
STATUS: 1404
STATUS: 406
RX: 33 41 33 42 33 43
STATUS: C24
STATUS: 1404
RX: 33 41
STATUS: 406
RX: 33 42 33 43 
*/

/*

SSSSS	number of bytes successfully copied to transmit FIFO
RRRRR	number of bytes in receieve FIFO
TTTTT	number of bytes in transmit FIFO
RB	receive busy
TE	transmit FIFO empty
RF	receive FIFO full
TF	transmit FIFO full
RE	receive FIFO empty
TB	transmit busy

S S S S |  S R R R | R R T T | T T T RB | TE TF RE TB


STATUS: 842C
STATUS: 406
RX: 30 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35
STATUS: 840C
RX: 30 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35
STATUS: 406
RX: 30 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35
*/

void runCamera(int gpio, int level, uint32_t tick, void* userdata) {
	printf("GPIO %d became %u at %d\n", gpio, level, tick);
	if (level == 1) {
		fbuffer = new FramesBuffer(camera_component->output[MMAL_CAMERA_VIDEO_PORT]->buffer_size, 150);
		start(camera_component);
		bcm2835_delay(500);
		stop(camera_component);
		printf("%d frames recorded\n", fbuffer->CurrentFrame);

		uint32_t width = 640;
		uint32_t height = 480;
		uint32_t depth = 3;

		char filename[100];
		for (int i = 0; i < fbuffer->CurrentFrame; ++i) {
			sprintf(filename, "../ccampic/%03d.jpg", i);
			write_jpeg_file(filename, fbuffer->GetFrame(i), width, height, depth);
		}
	}
}

/*

M    S
2 -> 18
3 -> 19
*/

void testCameraA() {

	printf("Setup\n");
	uint32_t width = 640;
	uint32_t height = 480;
	uint32_t depth = 3;
	uint32_t fps = 30;
	bcm2835_init();
	gpioInitialise();
	camera_component = setup(width, height, fps);
	if (camera_component != NULL) {
		bcm2835_delay(500);
		int status = gpioSetAlertFuncEx(2, runCamera, NULL);
		printf("status=%d\n", status);
		bcm2835_delay(1000000);
		mmal_component_destroy ( camera_component );
	} else {
		printf("Error setup camera\n");
	}

	
}

void testCameraB() {

	printf("Setup\n");
	uint32_t width = 640;
	uint32_t height = 480;
	uint32_t depth = 3;
	uint32_t fps = 30;
	bcm2835_init();
	gpioInitialise();
	gpioSetMode(18, PI_OUTPUT);
	gpioWrite(18, 0);
	camera_component = setup(width, height, fps);
	if (camera_component != NULL) {
		fbuffer = new FramesBuffer(camera_component->output[MMAL_CAMERA_VIDEO_PORT]->buffer_size, 150);
		bcm2835_delay(500);

		gpioWrite(18, 1);
		start(camera_component);
		bcm2835_delay(500);
		stop(camera_component);

		mmal_component_destroy ( camera_component );
		
		printf("%d frames recorded\n", fbuffer->CurrentFrame);

		char filename[100];
		for (int i = 0; i < fbuffer->CurrentFrame; ++i) {
			sprintf(filename, "../ccampic/%03d.jpg", i);
			write_jpeg_file(filename, fbuffer->GetFrame(i), width, height, depth);
		}
	} else {
		printf("Error setup camera\n");
	}


}

int main(int argn, char** argv) {
	if (argn > 1) {
		switch(argv[1][0]) {
		case 'c':
			testCamera();
			break;
		case 'm':
			//testMasterSPI();
			//testMasterSPI2();
			testMasterIIC();
			//testMasterIIC2();
			break;
		case 's':
			//testSlaveSPI();
			testSlaveIIC();
			break;
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
/*
static int fdMem        = -1;
static volatile uint32_t * bscsReg = MAP_FAILED;

void init(){
	if ((fdMem = open("/dev/mem", O_RDWR | O_SYNC) ) < 0)
   {
      DBG(DBG_ALWAYS,
         "\n" \
         "+---------------------------------------------------------+\n" \
         "|Sorry, you don't have permission to run this program.    |\n" \
         "|Try running as root, e.g. precede the command with sudo. |\n" \
         "+---------------------------------------------------------+\n\n");
      return -1;
   }
	
	bscsReg  = initMapMem(fdMem, BSCS_BASE,  BSCS_LEN);
	if (bscsReg == MAP_FAILED){
		printf("error");
	}

}

void bscInit2(int mode)
{
   bscsReg[BSC_CR]=0; // clear device
   bscsReg[BSC_RSR]=0; // clear underrun and overrun errors
   bscsReg[BSC_SLV]=0; // clear I2C slave address
   bscsReg[BSC_IMSC]=0xf; // mask off all interrupts
   bscsReg[BSC_ICR]=0x0f; // clear all interrupts

   gpioSetMode(BSC_SDA_MOSI, PI_ALT3);
   gpioSetMode(BSC_SCL_SCLK, PI_ALT3);
}

void bscTerm2(int mode)
{
   bscsReg[BSC_CR] = 0; // clear device
   bscsReg[BSC_RSR]=0; // clear underrun and overrun errors
   bscsReg[BSC_SLV]=0; // clear I2C slave address

   gpioSetMode(BSC_SDA_MOSI, PI_INPUT);
   gpioSetMode(BSC_SCL_SCLK, PI_INPUT);
}

int bscXfer2(bsc_xfer_t *xfer)
{
   static int bscMode = 0;

   int copied=0;
   int active, mode;

   if (xfer->control)
   {

      //bscMode (0=None, 1=I2C, 2=SPI) tracks which GPIO have been set to BSC mode
      if (xfer->control & 2) mode = 2; // SPI
      else                   mode = 1; // I2C

      if (mode > bscMode)
      {
         bscInit2(bscMode);
         bscMode = mode;
      }
   }
   else
   {
      if (bscMode) bscTerm2(bscMode);
      bscMode = 0;
      return 0; // leave ignore set
   }

   xfer->rxCnt = 0;

   bscsReg[BSC_SLV] = ((xfer->control)>>16) & 127;
   bscsReg[BSC_CR] = (xfer->control) & 0x3fff;
   bscsReg[BSC_RSR]=0; // clear underrun and overrun errors

   active = 1;

   while (active)
   {
      active = 0;

      while ((copied < xfer->txCnt) &&
             (!(bscsReg[BSC_FR] & BSC_FR_TXFF)))
      {
         bscsReg[BSC_DR] = xfer->txBuf[copied++];
         active = 1;
      }

      while ((xfer->rxCnt < BSC_FIFO_SIZE) &&
             (!(bscsReg[BSC_FR] & BSC_FR_RXFE)))
      {
         xfer->rxBuf[xfer->rxCnt++] = bscsReg[BSC_DR];
         active = 1;
      }

      if (!active)
      {
         active = bscsReg[BSC_FR] & (BSC_FR_RXBUSY | BSC_FR_TXBUSY);
      }

      if (active) myGpioSleep(0, 20);
   }

   bscFR = bscsReg[BSC_FR] & 0xffff;

   return (copied<<16) | bscFR;
}
*/
