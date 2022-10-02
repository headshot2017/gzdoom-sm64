#include "displaylist_parser.h"
#include "libmio0.h"
#include "../../debug_print.h"
#include "utils.h"
#include <stdarg.h>
#include "convUtils.h"
#include <stdio.h>

// References:
// https://hack64.net/wiki/doku.php?id=super_mario_64:fast3d_display_list_commands
// https://hack64.net/wiki/doku.php?id=super_mario_64:rom_memory_map
//
// In a real rom, Mario's data gets decompressed to memory, at 0x8007EC20
// It's loaded from 0x114750-0x1279B0 in MIO0 format. All the data is there, (Lights, vertices, textures)

static unsigned char* mario_data=0x0;
#define DL_DEBUG_PRINT

void paste_gfx_macro(Gfx* buf,int argnum,...){
    va_list args;
    va_start(args,argnum);
    Gfx* gfx=buf;
    for (int i = 0;i<argnum;i++){
        memcpy(gfx,args+i*(sizeof(Gfx*)),sizeof(Gfx));
        gfx++;
    }
    va_end(args);
}

void load_mario_data_from_rom( uint8_t *rom){
    DL_DEBUG_PRINT("Loading Mario data from ROM...");
    mio0_header_t head;
    uint8_t *in_buf = rom + 0x114750;

    mio0_decode_header( in_buf, &head );
    uint8_t *out_buf = malloc( head.dest_size );
    mio0_decode( in_buf, out_buf, NULL );

    mario_data = (char*)out_buf;
    DL_DEBUG_PRINT("Mario data loaded. Size: %d", head.dest_size);
}

Gfx* convertDLPtrMD(unsigned char* mario_data,unsigned int ptr){
    
    ptr=ptr-0x4000000; // offset in our decompressed data, assumes bank is 0x04
    unsigned char* newPtr=mario_data+(unsigned int)ptr;
    Gfx* parsed=parseGFX(mario_data,newPtr);
    return parsed;
}

Gfx* convertDLPtr(unsigned char* rom,unsigned char* ptr) {
    if (ptr==0)
        return 0;
    if (mario_data==0) {
        load_mario_data_from_rom(rom);
    }
    return convertDLPtrMD(mario_data,ptr);
}

Vtx* convertVTXPtrMD(unsigned char* mario_data,unsigned char* ptr,int amount){
    if (ptr==0)
        return 0;
    ptr=ptr-0x4000000; // offset in our decompressed data
    unsigned char* newPtr=mario_data+(unsigned int)ptr;
    return parseVTX(mario_data,newPtr,amount);
}

Lights1* parseLight(unsigned char* mario_data,unsigned char* ptr){
    if (ptr==0)
        return 0;
    ptr=ptr-0x4000000; // offset in our decompressed data
    unsigned char* newPtr=mario_data+(unsigned int)ptr;
    Lights1* lights = malloc(sizeof(Lights1));
    Lights1 tempLight = gdSPDefLights1(newPtr[0],newPtr[1],newPtr[2],newPtr[8],newPtr[9],newPtr[10],newPtr[16],newPtr[17],newPtr[18]);
    memcpy(lights,&tempLight,sizeof(Lights1));
    return lights;
}

bool parseGFXCommand(unsigned char* mario_data,unsigned char* rawData,unsigned char* buf,uint8_t *cmd_length){
    // commands in rawData are ALWAYS 8 bytes long
    *cmd_length=0;
    unsigned int A;
    unsigned int S;
    unsigned int T;
    unsigned int I;
    unsigned int W;
    unsigned int H;
    unsigned int B;
    unsigned int N;
    unsigned int L;
    uintptr_t Taa;
    uintptr_t Tbb;
    uintptr_t Tcc;
    unsigned char on;
    switch (rawData[0]){
        case 0x00: // G_SPNOOP - No operation. This should only be used for debugging purposes.
        case 0x01: // G_MTX - UI stuff?
        case 0xB6: // G_CLEARGEOMETRYMODE (gsSPClearGeometryMode)
        case 0xB7: // G_SETGEOMETRYMODE
        case 0xB9: // G_SetOtherMode_L (gsDPSetAlphaCompare)
        case 0xE6: // G_RDPLOADSYNC (gsDPLoadSync)
        case 0xE7: // G_RDPPIPESYNC
        case 0xE8: // G_RDPTILESYNC
        case 0xF3: // G_LOADBLOCK (gsDPLoadBlock)
        case 0xF5: // G_SETTILE
        case 0xFB: // G_SETENVCOLOR
        case 0xFC: // G_SETCOMBINE
        case 0xFE: // G_SETZIMG
            paste_gfx_macro(buf,1,GFXCMD_None);
            DL_DEBUG_PRINT("NO_OP (%X)",rawData[0]);
            *cmd_length=1;
            return false;
        case 0x03: // G_MOVEMEM, aka. GFXCMD_Light (gsSPLight)
            //03 [ii] [nn nn] [aa aa aa aa]
            // ii =	Index into table of DMEM addresses7
            // nn = size in bytes of memory to be moved
            // aa =	DMEM address
            // for ROM it's like (gsSPLight,0x86,0x0010,0x04000008) (gsSPLight,0x88,0x0010,0x04000000)
            // in decomp it's gsSPLight(&mario_blue_lights_group.l, 1) gsSPLight(&mario_blue_lights_group.a, 2)
            I = rawData[1];
            N = read_u16_be(rawData+2);
            A = read_u32_be(rawData+4);
            *cmd_length=3;
            DL_DEBUG_PRINT("gsSPLight(%X,%d) size",A,I);
            if (I==0x86){ // light
                Lights1* lights = parseLight(mario_data,A-8);
                paste_gfx_macro(buf,*cmd_length,gsSPLight(&lights->l,1));
            }else if (I==0x88){ // ambient
                Lights1* lights = parseLight(mario_data,A);
                paste_gfx_macro(buf,*cmd_length,gsSPLight(&lights->a,2));
            }
            
            return false;
        case 0x04: // G_VTX
            /*
            Fills the vertex buffer with vertex information (ex. coordinates, color values). Max amount of bytes to load in F3D is 0x100 (16 vertices).
            04 [N][I] [LL LL] [SS SS SS SS]
            N =	Number of vertices
            I = Where to start writing vertices inside the vertex buffer
            L = Length of vertex data to write in bytes((N+1)*0x10)
            S = Segmented address to load vertices from
            */
            N = FIRST_NIBBLE(rawData[1]);
            I = SECOND_NIBBLE(rawData[1]);
            L = read_u16_be(rawData+2);
            S = read_u32_be(rawData+4);
            DL_DEBUG_PRINT("gsSPVertex(%X,%X,%X)",N,I,S);
            *cmd_length=4;
            paste_gfx_macro(buf,*cmd_length,gsSPVertex(convertVTXPtrMD(mario_data,S,N+1),N+1,I)); // this might be wrong
            return false;
        case 0x06: // G_DL (gsSPDisplayList)
            /*
            Signifies the start of a Display List. May be used to link data and branch the current DL.
            06 [AA] 00 00 [BB BB BB BB]
            AA = 00 = store return address, 01 = don't store (end DL after branch)
            B = Segmenteded address to branch to
            */
            //unsigned char A = rawData[1];
            B = read_u32_be(rawData+4);
            DL_DEBUG_PRINT("gsSPDisplayList(%X)",B);
            *cmd_length=2;
            Gfx* SDL = convertDLPtrMD(mario_data,B);
            paste_gfx_macro(buf,*cmd_length,gsSPDisplayList(SDL));
            return false;
        case 0xB8: // G_ENDDL
            DL_DEBUG_PRINT("gsSPEndDisplayList()");
            *cmd_length=1;
            paste_gfx_macro(buf,*cmd_length,gsSPEndDisplayList());
            return true;
        case 0xBB: // G_TEXTURE (gsSPTexture)
            // Sets the texture scaling factor.
            // BB 00 [xx] [nn] [ss ss] [tt tt]
            // for libsm64, only s, t, n needed
            // ss = s scale
            // tt = t scale
            // n = enable/disable
            on = rawData[3];
            S = read_u16_be(rawData+4);
            T = read_u16_be(rawData+6);
            DL_DEBUG_PRINT("gsSPTexture(%d,%d,0,0,%d)",S,T,on);
            *cmd_length=4;
            paste_gfx_macro(buf,*cmd_length,gsSPTexture(S,T,0,0,on));
            return false;
        case 0xBF: // G_TRI1 (gsSP1Triangle)
            // Renders one triangle according to the vertices inside the vertex buffer
            // BF 00 00 00 00 [AA] [BB] [CC]
            // AA =	Vertex index 1
            // BB =	Vertex index 2
            // CC =	Vertex index 3
            Taa = rawData[5]/0x0A;
            Tbb = rawData[6]/0x0A;
            Tcc = rawData[7]/0x0A;
            DL_DEBUG_PRINT("gsSP1Triangle(%X,%X,%X,0x0)",Taa,Tbb,Tcc);
            *cmd_length=5;
            paste_gfx_macro(buf,*cmd_length,gsSP1Triangle(Taa,Tbb,Tcc,0x0));
            return false;
        case 0xF2: // G_SETTILESIZE (gsDPSetTileSize)
            // F2 [SS S][T TT] 0[I] [WW W][H HH]
            // SSS = Upper-left corner of texture to load, S-axis
            // TTT = Upper-left corner of texture to load, T-axis
            // I = Tile descriptor to load into
            // WWW = Width (Hack64 mentions (width - 1) « 2)
            // HHH = Height (Hack64 mentions (height - 1) « 2)
            // Note: Those things Hack64 mentions don't apply here (don't need to handle them) because they *should* be encoded already.
            // Also note: These are all in BIG ENDIAN format.
            S = (rawData[1] << 4) | (rawData[2] >> 4);
            T = ((rawData[2] & 0x0F) << 8) | rawData[3];
            I = SECOND_NIBBLE(rawData[4]);
            W = (rawData[5] << 4) | (rawData[6] >> 4);
            H = ((rawData[6] & 0x0F)<< 8) | rawData[7];
            DL_DEBUG_PRINT("gsDPSetTileSize(0,%X,%X,%X,%X)",S,T,W,H);
            *cmd_length=5;
            paste_gfx_macro(buf,*cmd_length,gsDPSetTileSize(0,S,T,W,H));
            return false;
        case 0xFD: // G_SETTIMG (gsDPSetTextureImage??)
            // Sets the texture image offset
            // FD [xx] 00 00 [bb bb bb bb]
            // xx = texture format/bit size - fffi i000 (fff = format, i = bit size)
            // bb bb bb bb = texture offset (segmented)
            //unsigned char xx = rawData[1];
            B = read_u32_be(rawData+4);
            //unsigned char fff = (xx & 0xF0) >> 5;
            //unsigned char i = (xx & 0x0F) >> 3;
            DL_DEBUG_PRINT("gsDPSetTextureImage(0,0,1,%X)",B);
            switch(B){
                case 0x04000090: // metal texture
                    B=0;
                    break;
                case 0x04001090: // yellow button
                    B=1;
                    break;
                case 0x04001890: // 'M' logo
                    B=2;
                    break;
                case 0x04002090: // Sideburns
                    B=3;
                    break;
                case 0x04002890: // Mustache
                    B=4;
                    break;
                case 0x04003090: // Eyes front
                    B=5;
                    break;
                case 0x04003890: // Eyes half closed
                    B=6;
                    break;
                case 0x04004090: // Eyes closed
                    B=7;
                    break;
                case 0x04007890: // Eyes dead
                    B=8;
                    break;
                case 0x04008090: // Wings half 1
                    B=9;
                    break;
                case 0x04009090: // Wings half 2
                    B=10;
                    break;
                default:
                    DL_DEBUG_PRINT("Unhandled texture image offset: %X",B);
                    B=0;
                    break;
            }
            *cmd_length=2;
            paste_gfx_macro(buf,*cmd_length,gsDPSetTextureImage(0,0,1,B));
            return false;
        default:
            DL_DEBUG_PRINT("Unknown GFX command: %X",rawData[0]);
            return true;
    }
}

Gfx* parseGFX(unsigned char* mario_data,unsigned char* rawData){
    int layoutAlloc=INITIAL_GFX_ALLOC*sizeof(GeoLayout);
    Gfx* gfxList = malloc(layoutAlloc);
    int memoryUsed=0; // how much memory is used in current gfx
    int currentDataPos=0;
    unsigned char* buffer = malloc(sizeof(Gfx)*10);
    uint8_t len = 0;
    bool done=false;
    while (!done){
        done=parseGFXCommand(mario_data,rawData+currentDataPos,buffer,&len);
        if ((memoryUsed+len*8)>=layoutAlloc){
            layoutAlloc+=INITIAL_GFX_ALLOC*sizeof(GeoLayout);
            gfxList = realloc(gfxList,layoutAlloc);
        }
        memcpy(((unsigned char*)gfxList)+memoryUsed,buffer,len*8);
        memoryUsed+=len*8;
        currentDataPos+=8;
    }
    return gfxList;
}

Vtx* parseVTX(unsigned char* mario_data,unsigned char* rawData,int amount){
    Vtx* vtxList = malloc(sizeof(Vtx)*amount);
    for (int i=0;i<amount;i++){
        unsigned char* rawVtxPtr = rawData+i*16;
        Vtx tempVtx = {{{read_s16_be(rawVtxPtr),read_s16_be(rawVtxPtr+2),read_s16_be(rawVtxPtr+4)},read_s16_be(rawVtxPtr+6),{read_s16_be(rawVtxPtr+8),read_s16_be(rawVtxPtr+10)},{rawVtxPtr[12],rawVtxPtr[13],rawVtxPtr[14],rawVtxPtr[15]}}};
        memcpy(vtxList+i,&tempVtx,sizeof(Vtx));
    }

    return vtxList;
}