//-------------------------------------------------------------------------
//
// The MIT License (MIT)
//
// Copyright (c) 2013 Andrew Duncan
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//-------------------------------------------------------------------------

#define _GNU_SOURCE

#include <assert.h>
#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "backgroundLayer.h"
#include "imageLayer.h"
#include "key.h"
#include "loadpng.h"

#include "bcm_host.h"

//-------------------------------------------------------------------------

#define NDEBUG

//-------------------------------------------------------------------------

const char *program = NULL;

//-------------------------------------------------------------------------

volatile bool run = true;

//-------------------------------------------------------------------------

#define XOFFSCREEN 10000  

static void
signalHandler(
    int signalNumber)
{
    switch (signalNumber)
    {
    case SIGINT:
    case SIGTERM:

        run = false;
        break;
    };
}

//-------------------------------------------------------------------------

void usage(void)
{
    fprintf(stderr, "Usage: %s ", program);
    fprintf(stderr, "[-b <RGBA>] [-d <number>] [-l <layer>] ");
    fprintf(stderr, "[-x <offset>] [-y <offset>] <file.png>\n");
    fprintf(stderr, "    -b - set background colour 16 bit RGBA\n");
    fprintf(stderr, "         e.g. 0x000F is opaque black\n");
    fprintf(stderr, "    -d - Raspberry Pi display number\n");
    fprintf(stderr, "    -l - DispmanX layer number\n");
    fprintf(stderr, "    -x - offset (pixels from the left)\n");
    fprintf(stderr, "    -y - offset (pixels from the top)\n");
    fprintf(stderr, "    -t - timeout in ms\n");
    fprintf(stderr, "    -n - non-interactive mode\n");
    fprintf(stderr, "    -i - start with image invisible (interactive mode)\n");
    fprintf(stderr, "    -h - hide lower layers\n");

    exit(EXIT_FAILURE);
}

//-------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    uint16_t background = 0x000F;
    int32_t layer = 1;
    uint32_t displayNumber = 0;
    int32_t xOffset = 0;
    int32_t yOffset = 0;
    uint32_t timeout = 0;
    uint32_t images = 0;
    uint16_t activeImage = 0;
    bool xOffsetSet = false;
    bool yOffsetSet = false;
    bool interactive = true;
    bool startInvisible = false;
    bool hideLowerLayers = false;
    
    int32_t xPos[16];
    int32_t yPos[16];

    program = basename(argv[0]);

    //---------------------------------------------------------------------

    int opt = 0;

    while ((opt = getopt(argc, argv, "b:d:l:x:y:t:nih")) != -1)
    {
        switch(opt)
        {
        case 'b':

            background = strtol(optarg, NULL, 16);
            break;

        case 'd':

            displayNumber = strtol(optarg, NULL, 10);
            break;

        case 'l':

            layer = strtol(optarg, NULL, 10);
            break;

        case 'x':

            xOffset = strtol(optarg, NULL, 10);
            xOffsetSet = true;
            break;

        case 'y':

            yOffset = strtol(optarg, NULL, 10);
            yOffsetSet = true;
            break;
        
        case 't':

            timeout = atoi(optarg);
            break;

        case 'n':

            interactive = false;
            break;
            
        case 'i':

            startInvisible = true;
            break;

        case 'h':

            hideLowerLayers = true;
            break;
            
        default:

            usage();
            break;
        }
    }

    //---------------------------------------------------------------------

    if (optind >= argc && background <= 0)
    {
        usage();
    }

    //---------------------------------------------------------------------

    IMAGE_LAYER_T imageLayer[16];

    const char *imagePath[16];
    
    for (int i = 0; i < 16; i++) {
        
        if (optind >= argc)
            break;
        
        imagePath[i] = argv[optind];
        
        if(strcmp(imagePath[i], "-") == 0)
        {
            // Use stdin
            if (loadPngFile(&(imageLayer[i].image), stdin) == false)
            {
                fprintf(stderr, "unable to load %s\n", imagePath[i]);
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            // Load image from path
            if (loadPng(&(imageLayer[i].image), imagePath[i]) == false)
            {
                fprintf(stderr, "unable to load %s\n", imagePath[i]);
                exit(EXIT_FAILURE);
            }
        }
    
        optind++;
        images++;
    }

    //---------------------------------------------------------------------

    if (signal(SIGINT, signalHandler) == SIG_ERR)
    {
        perror("installing SIGINT signal handler");
        exit(EXIT_FAILURE);
    }

    //---------------------------------------------------------------------

    if (signal(SIGTERM, signalHandler) == SIG_ERR)
    {
        perror("installing SIGTERM signal handler");
        exit(EXIT_FAILURE);
    }

    //---------------------------------------------------------------------

    bcm_host_init();

    //---------------------------------------------------------------------

    DISPMANX_DISPLAY_HANDLE_T display
        = vc_dispmanx_display_open(displayNumber);
    assert(display != 0);

    //---------------------------------------------------------------------

    DISPMANX_MODEINFO_T info;
    int result = vc_dispmanx_display_get_info(display, &info);
    assert(result == 0);

    //---------------------------------------------------------------------

    BACKGROUND_LAYER_T backgroundLayer;

    if (background > 0)
    {
        initBackgroundLayer(&backgroundLayer, background, layer - 1);
    }

    //---------------------------------------------------------------------
    
    DISPMANX_UPDATE_HANDLE_T update;
    
    update = vc_dispmanx_update_start(0);
    assert(update != 0);
    
    if (background > 0)
    {
        addElementBackgroundLayer(&backgroundLayer, display, update);
    }
    
    for (int i = 0; i < images; i++) 
    {
        createResourceImageLayer(&imageLayer[i], layer);
        
        if (xOffsetSet == false)
        {
            xPos[i] = (info.width - imageLayer[i].image.width) / 2;
        }
        else
        {
            xPos[i] = xOffset;
        }

        if (yOffsetSet == false)
        {
            yPos[i] = (info.height - imageLayer[i].image.height) / 2;
        }
        else
        {
            yPos[i] = yOffset;
        }

        addElementImageLayerOffset(&imageLayer[i],
                                startInvisible ? XOFFSCREEN : xPos[i],
                                yPos[i],
                                display,
                                update,
                                hideLowerLayers);
        
        layer++;
    }
    
    result = vc_dispmanx_update_submit_sync(update);
    assert(result == 0);

    //---------------------------------------------------------------------

    uint32_t currentTime = 0;

    // Sleep for 10 milliseconds every run-loop
    const int sleepMilliseconds = 10;

    while (run)
    {
        int c = 0;
        int num = 0;
        
        if (interactive && keyPressed(&c))
        {
            c = tolower(c);
            
            switch (c)
            {
            case 48 ... 57:
                
                num = c - 48;
                
                if (num < images) {

                    update = vc_dispmanx_update_start(0);
                    assert(update != 0);
                    
                    // Move active image off screen

                    moveImageLayer(&imageLayer[activeImage], XOFFSCREEN, 0, update);
                    
                    // Move requested image on screen

                    moveImageLayer(&imageLayer[num], xPos[num], yPos[num], update);

                    result = vc_dispmanx_update_submit_sync(update);
                    assert(result == 0);
                    
                    activeImage = num;
                }
                
                break;
            
            case 'v':
                
                update = vc_dispmanx_update_start(0);
                assert(update != 0);
                
                for (int i = 0; i < images; i++) 
                {
                    moveImageLayer(&imageLayer[i], xPos[i], yPos[i], update);
                }
                
                result = vc_dispmanx_update_submit_sync(update);
                assert(result == 0);
                
                break;
            
            case 'i':

                update = vc_dispmanx_update_start(0);
                assert(update != 0);
                
                for (int i = 0; i < images; i++) 
                {
                    moveImageLayer(&imageLayer[i], XOFFSCREEN, 0, update);
                }
                
                result = vc_dispmanx_update_submit_sync(update);
                assert(result == 0);
                
                break;
            
            case 'c':
            case 27:

                run = false;
                break;
            }
        }

        //---------------------------------------------------------------------

        usleep(sleepMilliseconds * 1000);

        currentTime += sleepMilliseconds;
        if (timeout != 0 && currentTime >= timeout) {
            run = false;
        }
    }

    //---------------------------------------------------------------------

    keyboardReset();

    //---------------------------------------------------------------------

    if (background > 0)
    {
        destroyBackgroundLayer(&backgroundLayer);
    }

    for (int i = 0; i < images; i++) 
    {
        destroyImageLayer(&imageLayer[i]);
    }

    //---------------------------------------------------------------------

    result = vc_dispmanx_display_close(display);
    assert(result == 0);

    //---------------------------------------------------------------------

    return 0;
}

