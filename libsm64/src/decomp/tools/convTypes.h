#pragma once

#define TYPE_CTL 1
#define TYPE_TBL 2
#define TYPE_SEQ 3

struct Loop
{
    unsigned int start;
    unsigned int end;
    int count;
    unsigned int pad;
    short state[1];
};


struct Book
{
    int order; // must be 2
    int npredictors; // must be 2
    short table[32]; // 8 * order * npredictors
};

struct Sample{
    unsigned int zero;
    uintptr_t addr;
    struct Loop* loop; // must not be null
    struct Book* book; // must not be null
    unsigned int sample_size;
};

struct Sound{
    struct Sample* sample_addr;
    float tuning;
};

struct delay_arg{
    unsigned short delay;
    unsigned short arg;
};

struct Envelope{
    struct delay_arg delay_args[1]; // array of [(delay,arg)]
};

struct Drum{
    unsigned char release_rate;
    unsigned char pan;
    unsigned char loaded;
    unsigned char pad;
    struct Sound snd;
    struct Envelope* env_addr;
};

struct Instrument{
    unsigned char loaded;
    unsigned char normal_range_lo;
    unsigned char normal_range_hi;
    unsigned char release_rate;
    struct Envelope* env_addr;
    struct Sound sound_lo;
    struct Sound sound_med;
    struct Sound sound_hi;
};

struct TBL{
    unsigned char* data;
};

struct SEQ{
    unsigned char* data;
};

struct CTL 
{
    unsigned int numInstruments;
    unsigned int numDrums;
    unsigned int shared;
    unsigned int iso_date;
    struct Drum** drum_pointers;
    struct Instrument* instrument_pointers[1];
};

struct seqObject{
    uintptr_t offset __attribute__((aligned (8)));
    unsigned int len __attribute__((aligned (8)));
};

struct seqFile{
    unsigned short revision;
    unsigned short seqCount;
	unsigned int pad;
    struct seqObject seqArray[1];
} __attribute__((aligned (16)));