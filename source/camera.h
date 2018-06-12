#pragma once

#include "common.h"

#define CAMERA_BUFFER_WIDTH 400
#define CAMERA_BUFFER_HEIGHT 240
#define CAMERA_BUFFER_SIZE CAMERA_BUFFER_WIDTH*CAMERA_BUFFER_HEIGHT
#define CAMERA_BUFFER_SIZE_BYTES CAMERA_BUFFER_SIZE*sizeof(u16)

typedef struct {
    volatile bool stop, done;
    C2D_Image image;
    Handle mutex;
    u16 camera_buffer[CAMERA_BUFFER_SIZE];
} camera_arg;

extern camera_arg * arg;

void startCameraThread();
void closeCameraThread();
void convertCameraBuffer();
