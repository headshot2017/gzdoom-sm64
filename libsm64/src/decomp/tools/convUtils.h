#pragma once
#include "../pc/libaudio_internal.h"

#define read_u16_le(p) ((uint8_t*)p)[1] * 0x100u + ((uint8_t*)p)[0]

struct EnvelopeMeta {
	uintptr_t orig;
	struct Envelope* addr;
	int size;
};

struct SampleList {
	int count;
	uintptr_t orig_addrs[256];
	struct Sample* addrs[256];
};

struct seqFile* parse_seqfile(unsigned char* seq);
struct CTL* parse_ctl_data(unsigned char* ctlData, uintptr_t* pos);
struct TBL* parse_tbl_data(unsigned char* tbl);
struct SEQ* parse_seq_data(unsigned char* seq);
void ptrs_to_offsets(struct seqFile* seqfile);
void ctl_free();
#define INITIAL_GFX_ALLOC 10
#define INITIAL_GEO_ALLOC 10