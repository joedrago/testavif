// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <jpeglib.h>

int main(int argc, char * argv[])
{
    printf("avif version: %s\n", avifVersion());

    if(argc < 4) {
        printf("Syntax: testavif in.avif out.jpg useParse\n");
        return 1;
    }

    avifBool useParse = AVIF_FALSE;
    if(!strcmp(argv[3], "true")) {
        useParse = AVIF_TRUE;
    }

    FILE * f = fopen(argv[1], "rb");
    if (!f)
        return 1;

    // Read raw .avif
    fseek(f, 0, SEEK_END);
    uint32_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    avifRWData raw = AVIF_DATA_EMPTY;
    avifRWDataRealloc(&raw, size);
    fread(raw.data, 1, size, f);
    fclose(f);

    avifImage * emptyImage = avifImageCreateEmpty();

    avifImage * image = NULL;
    avifDecoder * decoder = avifDecoderCreate();

    if(useParse) {
        printf("Decoding using avifDecoderParse()/avifDecoderNextImage() ...\n");

        image = emptyImage;
        avifResult decodeResult = avifDecoderRead(decoder, image, (avifROData *)&raw);
        if(decodeResult != AVIF_RESULT_OK) {
            printf("Failed to decode AVIF (using avifDecoderRead)\n");
            return 1; // leak!
        }
    } else {
        printf("Decoding using avifDecoderRead() ...\n");

        avifResult decodeResult = avifDecoderParse(decoder, (avifROData *)&raw);
        if(decodeResult != AVIF_RESULT_OK) {
            printf("Failed to decode AVIF (using avifDecoderParse)\n");
            return 1; // leak!
        }
        decodeResult = avifDecoderNextImage(decoder);
        if(decodeResult != AVIF_RESULT_OK) {
            printf("Failed to decode AVIF (using avifDecoderNextImage)\n");
            return 1; // leak!
        }
        image = decoder->image;
    }

    avifImageYUVToRGB(image);

    printf("Decoded image: %dx%d @ %dbpc\n", image->width, image->height, image->depth);
    if(image->depth != 8) {
        printf("Can't decode >8bpc in this test app\n");
        return 1; // leak!
    }

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    JSAMPROW row_pointer[1];
    int row_stride;
    unsigned char * outbuffer = NULL;
    unsigned long outsize = 0;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &outbuffer, &outsize);

    int pixelCount = image->width * image->height;
    uint8_t * jpegPixels = malloc(3 * pixelCount);
	for (int j = 0; j < image->height; ++j) {
		for (int i = 0; i < image->width; ++i) {
			uint8_t * jpegPixel = &jpegPixels[(i + (j * image->width)) * 3];
			jpegPixel[0] = image->rgbPlanes[0][i + (j * image->rgbRowBytes[0])];
			jpegPixel[1] = image->rgbPlanes[1][i + (j * image->rgbRowBytes[1])];
			jpegPixel[2] = image->rgbPlanes[2][i + (j * image->rgbRowBytes[2])];
		}
	}

    cinfo.image_width = image->width;
    cinfo.image_height = image->height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 90, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    row_stride = image->width * 3;
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &jpegPixels[cinfo.next_scanline * row_stride];
        (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);

    // Write raw JPEG
	FILE *outf = fopen(argv[2], "wb");
    fwrite(outbuffer, outsize, 1, outf);
    fclose(outf);
    printf("Wrote: %s\n", argv[2]);

    free(outbuffer);
    jpeg_destroy_compress(&cinfo);
    avifImageDestroy(emptyImage);
    avifDecoderDestroy(decoder);
    return 0;
}
