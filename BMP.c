#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "BMP.h"

char *addExtension(const char *fileName)
{
    size_t length = strlen(fileName);
    char *str;
    if (length < 5 || (memcmp(fileName + length - 4, ".bmp", 4) && memcmp(fileName + length - 4, ".BMP", 4)))
    {
        str = malloc(length + 5);
        str = strcpy(str, fileName);
        str = strcat(str, ".bmp");
        length += 5;
    } else
    {
        str = malloc(length + 1);
        str = strcpy(str, fileName);
    }
    str[length] = '\0';
    return str;
}

unsigned int swapEndianness4(unsigned int num)
{
    return (num >> 24) | ((num << 8) & 0x00FF0000) | ((num >> 8) & 0x0000FF00) | (num << 24);
}

unsigned int swapEndianness2(unsigned int num)
{
    return (num >> 8) | ((num << 8) & 0xFF00);
}

unsigned int mergeBytes(const uint8_t *array, int start, int num)
{
    int result = 0;
    for (int i = 0; i < num; i++)
    {
        result <<= 8;
        result += array[start + i];
    }
    return result;
}

int *digestColourTable(uint8_t *colourTable)
{
    int *table = malloc(4 * sizeof(int));
    unsigned int merged;
    for (int i = 0; i < 4; i++)
    {
        merged = mergeBytes(colourTable, i * 4, 4);
        switch (merged)
        {
            case 0xFF00:
                table[Red] = i;
                break;
            case 0xFF0000:
                table[Green] = i;
                break;
            case 0xFF000000:
                table[Blue] = i;
                break;
            case 0xFF:
                table[Alpha] = i;
        }
    }
    return table;
}

void encode(char *str, unsigned int *position, unsigned int number, int bytes)
{
    for (int i = 1; i <= bytes; i++)
    {
        str[*position + bytes - i] = (char) (number & 0xFF);
        number >>= 8;
    }
    *position += bytes;
}

struct BMP *init()
{
    struct BMP *bmp = malloc(sizeof(struct BMP));
    bmp->initialised = false;
    bmp->bitDepth = 24;
    bmp->width = 0;
    bmp->height = 0;
    return bmp;
}

static inline void encodeSwap(char *str, unsigned int *position, unsigned int number)
{
    encode(str, position, swapEndianness4(number), 4);
}

int open(struct BMP *bmp, const char *fileName)
{
    bmp->initialised = false;
    FILE *file = fopen(addExtension(fileName), "rb");
    if (file == NULL)
    {
        return 1; //the specified file does not exist
    }
    uint8_t header[40];
    fread(header, 1, 0x1E, file);
    unsigned int offset = swapEndianness4(mergeBytes(header, 0x0A, 4));
    bmp->width = swapEndianness4(mergeBytes(header, 0x12, 4));
    bmp->height = swapEndianness4(mergeBytes(header, 0x16, 4));
    bmp->bitDepth = swapEndianness2(mergeBytes(header, 0x1C, 2));
    initMemory(bmp, bmp->width, bmp->height);
    unsigned int padding = (4 - ((bmp->width * (bmp->bitDepth / 8)) % 4)) % 4;
    uint8_t *rawData = malloc((bmp->width * bmp->height * (bmp->bitDepth / 8)) + (padding * bmp->height));
    unsigned int position = 0;
    switch (bmp->bitDepth)
    {
        case 32:
            fread(header, 1, 40, file);
            fseek(file, (long) offset, SEEK_SET);
            int *colourTable = digestColourTable(header + 24);
            fread(rawData, 1, bmp->width * bmp->height * 4, file);
            for (uint32_t y = bmp->height - 1; y < UINT32_MAX; y--)
            {
                for (unsigned int x = 0; x < bmp->width; x++)
                {
                    for (int i = 0; i < 4; i++)
                    {
                        if (colourTable[Red] == i)
                        {
                            bmp->R[x][y] = rawData[position + i];
                        } else if (colourTable[Green] == i)
                        {
                            bmp->G[x][y] = rawData[position + i];
                        } else if (colourTable[Blue] == i)
                        {
                            bmp->B[x][y] = rawData[position + i];
                        } else if (colourTable[Alpha] == i)
                        {
                            bmp->A[x][y] = rawData[position + i];
                        }
                    }
                    position += 4;
                }
            }
            free(colourTable);
            break;
        case 24:
            fseek(file, (long) offset, SEEK_SET);
            for (uint32_t y = bmp->height - 1; y < UINT32_MAX; y--)
            {
                for (unsigned int x = 0; x < bmp->width; x++)
                {
                    bmp->B[x][y] = rawData[position];
                    bmp->G[x][y] = rawData[position + 1];
                    bmp->R[x][y] = rawData[position + 2];
                    position += 3;
                }
                position += padding;
            }
    }
    free(rawData);
    fclose(file);
    bmp->initialised = true;
    return 0;
}

void close(struct BMP *bmp)
{
    if (bmp->initialised)
    {
        for (int i = 0; i < bmp->width; i++)
        {
            free(bmp->R[i]);
            free(bmp->G[i]);
            free(bmp->B[i]);
        }
        free(bmp->R);
        free(bmp->G);
        free(bmp->B);
        if (bmp->bitDepth == 32)
        {
            for (int i = 0; i < bmp->width; i++)
            {
                free(bmp->A[i]);
            }
            free(bmp->A);
        }
        bmp->initialised = false;
    }
}

int export(const struct BMP *bmp, const char *fileName)
{
    if (bmp->initialised)
    {
        FILE *file = fopen(addExtension(fileName), "wb");
        if (file == NULL)
        {
            return 2; //failed to write to file
        }
        unsigned int fileSize;
        unsigned int padding;
        unsigned int position;
        char *outputString;
        switch (bmp->bitDepth)
        {
            case 24:
                padding = (4 - ((bmp->width * (bmp->bitDepth / 8)) % 4)) % 4;
                fileSize = (((bmp->width * 3) + padding) * bmp->height) + 54;
                break;
            case 32:
                padding = 0;
                fileSize = (bmp->width * bmp->height * 4) + 138;
                break;
            default:
                fclose(file);
                return 3; //corrupted BMP struct
        }
        outputString = malloc(fileSize);
        outputString[0] = 'B';
        outputString[1] = 'M';
        position = 2;
        encodeSwap(outputString, &position, fileSize);
        encode(outputString, &position, 0, 4);
        switch (bmp->bitDepth)
        {
            case 24:
                encodeSwap(outputString, &position, 54);    //data offset
                encodeSwap(outputString, &position, 40);    //length of header
                encodeSwap(outputString, &position, bmp->width);
                encodeSwap(outputString, &position, bmp->height);
                encode(outputString, &position, swapEndianness2(1), 2);
                encode(outputString, &position, swapEndianness2(bmp->bitDepth), 2);
                encode(outputString, &position, 0, 24);
                break;
            case 32:
                encodeSwap(outputString, &position, 138);   //data offset
                encodeSwap(outputString, &position, 124);   //length of header
                encodeSwap(outputString, &position, bmp->width);
                encodeSwap(outputString, &position, bmp->height);
                encode(outputString, &position, swapEndianness2(1), 2);
                encode(outputString, &position, swapEndianness2(bmp->bitDepth), 2);
                encodeSwap(outputString, &position, 3);
                encodeSwap(outputString, &position, (bmp->width * bmp->height * 4));
                encode(outputString, &position, 0x130B0000, 4);
                encode(outputString, &position, 0x130B0000, 4);
                encode(outputString, &position, 0, 8);
                encode(outputString, &position, 0x000000FF, 4);     //colour table
                encode(outputString, &position, 0x0000FF00, 4);
                encode(outputString, &position, 0x00FF0000, 4);
                encode(outputString, &position, 0xFF000000, 4);
                outputString[position] = 'B';
                outputString[position + 1] = 'G';
                outputString[position + 2] = 'R';
                outputString[position + 3] = 's';
                position += 4;
                encode(outputString, &position, 0, 64);
                break;
        }
        for (uint32_t y = bmp->height - 1; y < INT32_MAX; y--)
        {
            for (unsigned int x = 0; x < bmp->width; x++)
            {
                if (bmp->bitDepth == 32)
                {
                    encode(outputString, &position, bmp->A[x][y], 1);
                }
                encode(outputString, &position, bmp->B[x][y], 1);
                encode(outputString, &position, bmp->G[x][y], 1);
                encode(outputString, &position, bmp->R[x][y], 1);
            }
            encode(outputString, &position, 0, (signed) padding);
        }

        fwrite(outputString, fileSize, 1, file);
        fclose(file);
        free(outputString);
        return 0;   //success
    } else
    {
        return 1;   //image not initialised
    }
}

void initMemory(struct BMP *bmp, unsigned int width, unsigned int height)
{
    if (!bmp->initialised)
    {
        bmp->R = malloc(width * sizeof(uint8_t *));
        bmp->G = malloc(width * sizeof(uint8_t *));
        bmp->B = malloc(width * sizeof(uint8_t *));
        if (bmp->bitDepth == 32)
        {
            bmp->A = malloc(width * sizeof(uint8_t *));
        }
        for (int i = 0; i < width; i++)
        {
            bmp->R[i] = calloc(height, sizeof(uint8_t));
            bmp->G[i] = calloc(height, sizeof(uint8_t));
            bmp->B[i] = calloc(height, sizeof(uint8_t));
            if (bmp->bitDepth == 32)
            {
                bmp->A[i] = malloc(height);
                for (int j = 0; j < height; j++)
                {
                    bmp->A[i][j] = 255;
                }
            }
        }
    }
}

void setSize(struct BMP *bmp, unsigned int width, unsigned int height)
{
    if (bmp->initialised)
    {
        if (width < bmp->width)
        {
            for (unsigned int i = width; i < bmp->width; i++)
            {
                free(bmp->R[i]);
                free(bmp->G[i]);
                free(bmp->B[i]);
                if (bmp->bitDepth == 32)
                {
                    free(bmp->A[i]);
                }
            }
            bmp->R = realloc(bmp->R, width);
            bmp->G = realloc(bmp->G, width);
            bmp->B = realloc(bmp->B, width);
            if (bmp->bitDepth == 32)
            {
                bmp->A = realloc(bmp->A, width);
            }
        } else if (width > bmp->width)
        {
            bmp->R = realloc(bmp->R, width);
            bmp->G = realloc(bmp->G, width);
            bmp->B = realloc(bmp->B, width);
            if (bmp->bitDepth == 32)
            {
                bmp->A = realloc(bmp->A, width);
            }
            for (unsigned int i = bmp->width; i < width; i++)
            {
                bmp->R[i] = calloc(height, sizeof(uint8_t));
                bmp->G[i] = calloc(height, sizeof(uint8_t));
                bmp->B[i] = calloc(height, sizeof(uint8_t));
            }
        }
        if (height != bmp->height)
        {
            for (unsigned int i = 0; i < bmp->width; i++)
            {
                bmp->R[i] = realloc(bmp->R[i], height);
                bmp->G[i] = realloc(bmp->G[i], height);
                bmp->B[i] = realloc(bmp->B[i], height);
            }
            if (height > bmp->height)
            {
                for (unsigned int x = 0; x < width; x++)
                {
                    memset(&bmp->R[x][bmp->height], 0, height - bmp->height * sizeof(uint8_t));
                }
            }
        }
    } else
    {
        initMemory(bmp, width, height);
        bmp->initialised = true;
    }
    bmp->width = width;
    bmp->height = height;
}

int setBitDepth(struct BMP *bmp, int bitDepth)
{
    if (!(bitDepth == 24 || bitDepth == 32))
    {
        return 1;   //unsupported bit depth
    } else
    {
        if(bmp->bitDepth == 24 && bitDepth == 32)
        {
            bmp->A = malloc(bmp->width);
            for (int i = 0; i < bmp->height; i++)
            {
                bmp->A[i] = malloc(bmp->height);
                for (int j = 0; j < bmp->height; j++)
                {
                    bmp->A[i][j] = 255;
                }
            }
        }
        bmp->bitDepth = bitDepth;
        return 0;
    }
}