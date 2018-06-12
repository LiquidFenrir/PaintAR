#include "camera.h"

camera_arg * arg = NULL;

void cameraThreadFunction(void* void_arg)
{
    (void)void_arg;
    Handle events[2] = {0};
    u32 transferUnit;

    u16* buffer = new u16[CAMERA_BUFFER_SIZE];
    camInit();
    CAMU_SetSize(SELECT_OUT1, SIZE_CTR_TOP_LCD, CONTEXT_A);
    CAMU_SetOutputFormat(SELECT_OUT1, OUTPUT_RGB_565, CONTEXT_A);
    CAMU_SetFrameRate(SELECT_OUT1, FRAME_RATE_30);
    CAMU_SetNoiseFilter(SELECT_OUT1, true);
    CAMU_SetAutoExposure(SELECT_OUT1, true);
    CAMU_SetAutoWhiteBalance(SELECT_OUT1, true);
    CAMU_Activate(SELECT_OUT1);
    CAMU_GetBufferErrorInterruptEvent(&events[1], PORT_CAM1);
    CAMU_SetTrimming(PORT_CAM1, false);
    CAMU_GetMaxBytes(&transferUnit, CAMERA_BUFFER_WIDTH, CAMERA_BUFFER_HEIGHT);
    CAMU_SetTransferBytes(PORT_CAM1, transferUnit, CAMERA_BUFFER_WIDTH, CAMERA_BUFFER_HEIGHT);
    CAMU_ClearBuffer(PORT_CAM1);
    CAMU_SetReceiving(&events[0], buffer, PORT_CAM1, CAMERA_BUFFER_SIZE_BYTES, (s16) transferUnit);
    CAMU_StartCapture(PORT_CAM1);

    while(!arg->stop)
    {
        s32 index = 0;
        svcWaitSynchronizationN(&index, events, 2, false, U64_MAX);
        switch(index+1)
        {
            case 1:
                svcCloseHandle(events[0]);
                events[0] = 0;
                svcWaitSynchronization(arg->mutex, U64_MAX);
                memcpy(arg->camera_buffer, buffer, CAMERA_BUFFER_SIZE_BYTES);
                GSPGPU_FlushDataCache(arg->camera_buffer, CAMERA_BUFFER_SIZE_BYTES);
                svcReleaseMutex(arg->mutex);
                CAMU_SetReceiving(&events[0], buffer, PORT_CAM1, CAMERA_BUFFER_SIZE_BYTES, transferUnit);
                break;
            case 2:
                svcCloseHandle(events[0]);
                events[0] = 0;
                CAMU_ClearBuffer(PORT_CAM1);
                CAMU_SetReceiving(&events[0], buffer, PORT_CAM1, CAMERA_BUFFER_SIZE_BYTES, transferUnit);
                CAMU_StartCapture(PORT_CAM1);
                break;
            default:
                break;
        }
    }

    CAMU_StopCapture(PORT_CAM1);

    bool busy = false;
    while(R_SUCCEEDED(CAMU_IsBusy(&busy, PORT_CAM1)) && busy)
    {
        svcSleepThread(1e6);
    }

    CAMU_ClearBuffer(PORT_CAM1);
    CAMU_Activate(SELECT_NONE);
    camExit();
    
    delete[] buffer;

    for(int i = 0; i < 2; i++)
    {
        if(events[i] != 0)
        {
            svcCloseHandle(events[i]);
            events[i] = 0;
        }
    }

    arg->done = true;
}

void startCameraThread()
{
    arg = new camera_arg;
    arg->stop = false;
    svcCreateMutex(&arg->mutex, false);

    C3D_Tex * tex = new C3D_Tex;
    static const Tex3DS_SubTexture subt3x = { 512, 256, 0.0f, 1.0f, 1.0f, 0.0f };
    arg->image = (C2D_Image){ tex, &subt3x };
    C3D_TexInit(arg->image.tex, 512, 256, GPU_RGB565);
    C3D_TexSetFilter(arg->image.tex, GPU_LINEAR, GPU_LINEAR);

    if(threadCreate(cameraThreadFunction, NULL, 0x10000, 0x1A, 1, true) == NULL)
    {
        arg->done = true;
    }
}

void closeCameraThread()
{
    arg->stop = true;
    while(!arg->done)
        svcSleepThread(1e6);

    svcCloseHandle(arg->mutex);
    delete arg->image.tex;
    delete arg;
}

void convertCameraBuffer()
{
    for(u32 x = 0; x < CAMERA_BUFFER_WIDTH; x++)
    {
        for(u32 y = 0; y < CAMERA_BUFFER_HEIGHT; y++)
        {
            u32 dstPos = ((((y >> 3) * (512 >> 3) + (x >> 3)) << 6) + ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3))) * 2;
            u32 srcPos = (y * CAMERA_BUFFER_WIDTH + x) * sizeof(u16);
            memcpy(&((u8*)arg->image.tex->data)[dstPos], &((u8*)arg->camera_buffer)[srcPos], sizeof(u16));
        }
    }
}
