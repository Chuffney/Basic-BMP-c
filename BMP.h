#ifndef CBMP_BMP_H
#define CBMP_BMP_H

#include <stdbool.h>
#include <stdint.h>

struct BMP
{
    bool initialised;       //private - not to be modified manually
    unsigned int width;
    unsigned int height;
    unsigned int bitDepth;
    uint8_t **R;
    uint8_t **G;
    uint8_t **B;
    uint8_t **A;
};

typedef enum
{
    Red,
    Green,
    Blue,
    Alpha
} RGBA;

char *addExtension(const char *fileName);
unsigned int swapEndianness4(unsigned int);
unsigned int swapEndianness2(unsigned int);
unsigned int mergeBytes(const uint8_t *array, int start, int num);
int *digestColourTable(uint8_t *colourTable);
void encode(char *str, unsigned int *position, unsigned int number, int bytes);

struct BMP *init();
int open(struct BMP*, const char *fileName);
void close(struct BMP*);
int export(const struct BMP*, const char *fileName);

void initMemory(struct BMP*, unsigned int width, unsigned int height);
void setSize(struct BMP*, unsigned int width, unsigned int height);
int setBitDepth(struct BMP*, int bitDepth);

#endif //CBMP_BMP_H
