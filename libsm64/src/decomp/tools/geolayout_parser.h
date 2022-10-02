#pragma once

#include "../include/types.h"

GeoLayout* parse_full_geolayout(unsigned char* rom,unsigned char* data);
_Bool parse_command(unsigned char* rom,unsigned char* data,GeoLayout* buf,uint8_t *cmd_length);
uintptr_t convertPtr(unsigned int oldPtr);
uintptr_t convertPtr_follow(unsigned char* rom,unsigned int oldPtr);