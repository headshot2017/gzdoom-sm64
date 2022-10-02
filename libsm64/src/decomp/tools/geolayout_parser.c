#include "geolayout_parser.h"
#include "../../debug_print.h"
#include "../include/geo_commands.h"
#include "../game/behavior_actions.h" // literally only for geo_move_mario_part_from_parent
#include "utils.h"
#include "convUtils.h"
#define NO_GEO_PLS
#include "displaylist_parser.h"
#undef NO_GEO_PLS
// References used
// https://hack64.net/wiki/doku.php?id=super_mario_64:geometry_layout_commands
// http://qubedstudios.rustedlogic.net/SM64GeoLayoutPtrsByLevels.txt
// https://hack64.net/wiki/doku.php?id=super_mario_64:function_list

#define GEO_DEBUG_PRINT //DEBUG_PRINT

uintptr_t convertPtr_follow(unsigned char* rom,unsigned int oldPtr){
    // first byte is the LevelBank, the rest are offset
    // [BB] [OO OO OO]
    // LevelBank 17 is at 0x1279B0 in the ROM data
    unsigned char levelBank = oldPtr >> 24;
    unsigned int offset = oldPtr & 0xFFFFFF;
    if (levelBank==0x17){
        unsigned int newPtr = 0x1279B0 + offset; // this should be the GeoLayout that is being referenced
        GEO_DEBUG_PRINT("Converted %x to %x - Now parsing that!",oldPtr,newPtr);
        GeoLayout* parsed=parse_full_geolayout(rom,rom+newPtr);
        return parsed;
    }else{
        GEO_DEBUG_PRINT("Unknown level bank: %d", levelBank);
        return 0;
    }
    return 0;
}

uintptr_t convertPtr(unsigned int oldPtr) {
    if (oldPtr == 0) {
        return 0;
    }
    uintptr_t out = (uint32_t)oldPtr;
    //GEO_DEBUG_PRINT("Converting pointer 0x%08x", oldPtr);
    switch (oldPtr){
        case 0x80277D6C:
            out = geo_mirror_mario_backface_culling;
            break;
        case 0x802770A4:
            out=geo_mirror_mario_set_alpha;
            break;
        case 0x80277150:
            out=geo_switch_mario_stand_run;
            break;
        case 0x802773A4:
            out=geo_mario_head_rotation;
            break;
        case 0x80277740:
            out=geo_switch_mario_cap_on_off;
            break;
        case 0x802771BC:
            out=geo_switch_mario_eyes;
            break;
        case 0x80277824:
            out=geo_mario_rotate_wing_cap_wings;
            break;
        case 0x802774F4:
            out=geo_switch_mario_hand;
            break;
        case 0x802775CC:
            out=geo_mario_hand_foot_scaler;
            break;
        case 0x8027795C:
            out=geo_switch_mario_hand_grab_pos;
            break;
        case 0x802B1BB0:
            out=geo_move_mario_part_from_parent;
            break;
        case 0x80277294:
            out=geo_mario_tilt_torso;
            break;
        case 0x802776D8:
            out=geo_switch_mario_cap_effect;
            break;
        default:
            GEO_DEBUG_PRINT("Unknown pointer %X", oldPtr);
            break;
    }
    return out;
}

GeoLayout* parse_full_geolayout(unsigned char* rom,unsigned char* data){
    int layoutAlloc=INITIAL_GEO_ALLOC*sizeof(GeoLayout);
    GeoLayout* layout=(GeoLayout*)malloc(layoutAlloc); // destination
    int memoryUsed=0; // how much memory is used in layout
    GeoLayout* buffer=(GeoLayout*)malloc(sizeof(GeoLayout)*5); // temporary buffer for storing current command
    bool done=false;
    int len=0;
    unsigned char* currentLayout=data;
    while(!done){
        done=parse_command(rom,currentLayout,buffer,&len);
        if ((memoryUsed+(len*2))>layoutAlloc){
            layoutAlloc+=INITIAL_GEO_ALLOC*sizeof(GeoLayout);
            layout=(GeoLayout*)realloc(layout,layoutAlloc);
        }
        memcpy(((unsigned char*)layout)+memoryUsed,buffer,(len*2));
        memoryUsed+=(len*2);
        currentLayout+=len;
    }
    free(buffer);
    return layout;
}

void paste_macro(GeoLayout* buf,GeoLayout a){
    *buf=a;
}

void paste_macro2(GeoLayout* buf,GeoLayout a,GeoLayout b){
    GeoLayout* ptr=buf;
    paste_macro(buf,a);
    ptr++;
    paste_macro(ptr,b);
}

void paste_macro3(GeoLayout* buf,GeoLayout a,GeoLayout b,GeoLayout c){
    GeoLayout* ptr=buf;
    paste_macro(buf,a);
    ptr++;
    paste_macro(ptr,b);
    ptr++;
    paste_macro(ptr,c);
}

void paste_macro4(GeoLayout* buf,GeoLayout a,GeoLayout b,GeoLayout c,GeoLayout d){
    GeoLayout* ptr=buf;
    paste_macro(buf,a);
    ptr++;
    paste_macro(ptr,b);
    ptr++;
    paste_macro(ptr,c);
    ptr++;
    paste_macro(ptr,d);
}

_Bool parse_command(unsigned char* rom,unsigned char* data,GeoLayout* buf,uint8_t *cmd_length){
    *cmd_length = 0;
    if (data[0]==0 && data[1]==0 && data[2]==0 && data[3]==0){
        // Geolayout command 00: Branch and Store. 00 00 00 00 [SS SS SS SS] - S = segmented address to branch to.
        unsigned int segmented_address = read_u32_be(data+4);
        GEO_DEBUG_PRINT("Branch and Store: %i",segmented_address);
        *cmd_length=8;
        paste_macro2(buf,GEO_BRANCH_AND_LINK(convertPtr_follow(rom,segmented_address)));
    }else if (data[0]==1 && data[1]==0 && data[2]==0 && data[3]==0){
        // GeoLayout command 01: Terminate Geometry Layout. 01 00 00 00
        *cmd_length=4;
        GEO_DEBUG_PRINT("Terminate Geometry Layout");
        paste_macro(buf,GEO_END());
        return true;
    }else if (data[0]==2 && data[2]==0 && data[3]==0){
        // GeoLayout command 02: Branch geometry layout. 02 [AA] 00 00 [SS SS SS SS] - AA is 0/1, 0 is jump, 1 is jump and store return addr. 
        // SS SS SS SS is the segmented address to branch to.
        unsigned int segmented_address = read_u32_be(data+4);
        unsigned char jump_and_store = data[1];
        GEO_DEBUG_PRINT("Branch geometry layout: %X (store ret addr %i)",segmented_address,jump_and_store);
        *cmd_length=8;
        paste_macro2(buf, GEO_BRANCH(jump_and_store, convertPtr_follow(rom,segmented_address)));
    }else if (data[0]==3 && data[1]==0 && data[2]==0 && data[3]==0){
        // GeoLayout command 03: Return from branch. 03 00 00 00
        *cmd_length=4;
        GEO_DEBUG_PRINT("Return from branch");
        paste_macro(buf, GEO_RETURN());
        return true;
    }else if (data[0]==4 && data[1]==0 && data[2]==0 && data[3]==0){
        // GeoLayout command 04: Open Node. 04 00 00 00
        *cmd_length=4;
        GEO_DEBUG_PRINT("Open Node");
        paste_macro(buf, GEO_OPEN_NODE());
    }else if (data[0]==5 && data[1]==0 && data[2]==0 && data[3]==0){
        // GeoLayout command 05: Close Node. 05 00 00 00
        *cmd_length=4;
        GEO_DEBUG_PRINT("Close Node");
        paste_macro(buf, GEO_CLOSE_NODE());
    }
    // 06: Store Current Node Pointer To Table (Unused)
    // 07: Set/OR/AND Node Flags (Unused)
    // 08: Set Screen Render Area (Only used in levels)
    // 09: Create Ortho Matrix (Sets ortho matrix for level backgrounds)
    // 0A: Set Camera Frustum (Only used in levels)
    else if (data[0]==0x0B && data[1]==0 && data[2]==0 && data[3]==0){
        // GeoLayout command 0B: Start Geo Layout. 0B 00 00 00
        *cmd_length=4;
        GEO_DEBUG_PRINT("Start Geo Layout");
        paste_macro(buf, GEO_NODE_START());   
    }else if (data[0]==0x0C && data[2]==0 && data[3]==0){
        // GeoLayout command 0C: Enable/Disable Z-Buffer. 0C [AA] 00 00
        // AA is 0/1, 0 is disable, 1 is enable.
        unsigned char enable_zbuffer = data[1];
        *cmd_length=4;
        GEO_DEBUG_PRINT("Enable/Disable Z-Buffer: %i",enable_zbuffer);
        paste_macro(buf, GEO_ZBUFFER(enable_zbuffer));
    }else if (data[0]==0x0D && data[1]==0 && data[2]==0 && data[3]==0){
        // GeoLayout command 0d: Set render range. ) 0D 00 00 00 [AA AA] [BB BB]
        // AA = (s16) Minimum distance value
        // BB = (s16) Maximum distance value
        unsigned short min_distance = read_u16_be(data+4);
        unsigned short max_distance = read_u16_be(data+6);
        *cmd_length=8;
        GEO_DEBUG_PRINT("Set render range: %i, %i",min_distance,max_distance);
        paste_macro2(buf, GEO_RENDER_RANGE(min_distance, max_distance));
    }else if (data[0]==0x0E && data[1]==0 && data[2]==0){
        // GeoLayout command 0E: Switch case. 0E 00 00 [NN] [AA AA AA AA]
        // N = Number of cases/display lists, starting at 01
        // AA = ASM function
        unsigned char num_cases = data[3];
        unsigned int asm_func = read_u32_be(data+4);
        *cmd_length=8;
        GEO_DEBUG_PRINT("Switch case: %i, %X",num_cases,asm_func);
        paste_macro2(buf, GEO_SWITCH_CASE(num_cases, convertPtr(asm_func)));
    }
    // 0F: Create Camera Graph Node (unused for libsm64)
    else if (data[0]==0x10 && data[1]==0 && data[2]==0 && data[3]==0){
        // GeoLayout command 10: Translate and Rotate. 10 [AA] [BB BB] [XX XX] [YY YY] [ZZ ZZ] [RX RX] [RY RY] [RZ RZ]
        //A	Branching flag
        //B	Used if A != 0
        //X	X translation offset (s16)
        //Y	Y translation offset (s16)
        //Z	Z translation offset (s16)
        //RX	X rotation (s16)
        //RY	Y rotation (s16)
        //RZ	Z rotation (s16)
        unsigned char branch_flag = data[1];
        unsigned short branch_flag_2 = read_u16_be(data+2);
        short x_translation = read_u16_be(data+4);
        short y_translation = read_u16_be(data+6);
        short z_translation = read_u16_be(data+8);
        short x_rotation = read_u16_be(data+10);
        short y_rotation = read_u16_be(data+12);
        short z_rotation = read_u16_be(data+14);
        // conbine branch flags
        branch_flag = (branch_flag<<4) | branch_flag_2;
        *cmd_length=0x10;
        GEO_DEBUG_PRINT("Translate and Rotate: %i, %i, %i, %i, %i, %i, %i",branch_flag,x_translation,y_translation,z_translation,x_rotation,y_rotation,z_rotation);
        paste_macro4(buf, GEO_TRANSLATE_ROTATE(branch_flag, x_translation, y_translation, z_translation, x_rotation, y_rotation, z_rotation));
    }else if (data[0]==0x11){
        // GeoLayout command 11: Translate Node & load DL/GL. 11 [B][L] [XX XX] [YY YY] [ZZ ZZ] {AA AA AA AA}
        //B	Include last 4 bytes if set to 8 (command will be C bytes long instead of 8)
        //L	Drawing layer if B is set, otherwise will always be zero
        //X	X translation offset (s16)
        //Y	Y translation offset (s16)
        //Z	Z translation offset (s16)
        //A	Segmented address with display list if B is set. Also creates a joint of some kind? (Does not work with animations?)
        unsigned char branch_flag = FIRST_NIBBLE(data[1]);
        unsigned char layer = SECOND_NIBBLE(data[1]);
        unsigned short x_translation = read_u16_be(data+2);
        unsigned short y_translation = read_u16_be(data+4);
        unsigned short z_translation = read_u16_be(data+6);
        unsigned int segmented_address = read_u32_be(data+8);
        *cmd_length=0x8;
        GEO_DEBUG_PRINT("Translate Node & load DL/GL: %i, %i, %i, %i, %i",branch_flag,layer,x_translation,y_translation,z_translation);
        if (branch_flag==0x8){
            cmd_length=0xC;
            paste_macro3(buf, GEO_TRANSLATE_WITH_DL(layer, x_translation, y_translation, z_translation, convertDLPtr(rom,segmented_address)));
            return false;
        }
        paste_macro2(buf, GEO_TRANSLATE(layer, x_translation, y_translation, z_translation));
    }else if (data[0]==0x12){
        // GeoLayout command 12: Rotate Node (and Load Display List or Start Geo Layout). 12 [B][L] [RX RX] [RY RY] [RZ RZ] {AA AA AA AA}
        //B	Include last 4 bytes if set to 8 (command will be C bytes long instead of 8)
        //L	Drawing layer if B is set, otherwise will always be zero
        //RX	X rotation (s16)
        //RY	Y rotation (s16)
        //RZ	Z rotation (s16)
        //A	Segmented address with display list if B is set. Also creates a joint of some kind? (Does not work with animations?)
        unsigned char branch_flag = FIRST_NIBBLE(data[1]);
        unsigned char layer = SECOND_NIBBLE(data[1]);
        unsigned short x_rotation = read_u16_be(data+2);
        unsigned short y_rotation = read_u16_be(data+4);
        unsigned short z_rotation = read_u16_be(data+6);
        unsigned int segmented_address = read_u32_be(data+8);
        *cmd_length=0x8;
        GEO_DEBUG_PRINT("Rotate Node: %i, %i, %i, %i, %i",branch_flag,layer,x_rotation,y_rotation,z_rotation);
        if (branch_flag==0x8){
            cmd_length=0xC;
            paste_macro3(buf, GEO_ROTATION_NODE_WITH_DL(layer, x_rotation, y_rotation, z_rotation, convertDLPtr(rom,segmented_address)));
            return false;
        }
        paste_macro2(buf, GEO_ROTATION_NODE(layer, x_rotation, y_rotation, z_rotation));
    }else if (data[0]==0x13){
        // GeoLayout command 13: Load Display List With Offset. 13 [LL] [XX XX] [YY YY] [ZZ ZZ] [AA AA AA AA]
        //L	Drawing layer
        //X	Offset on X axis (s16)
        //Y	Offset on Y axis (s16)
        //Z	Offset on Z axis (s16)
        //A	Segmented address with display list - If 0x00000000, an invisible rotation joint is created
        unsigned char layer = data[1];
        unsigned int x_offset = read_u16_be(data+2);
        unsigned int y_offset = read_u16_be(data+4);
        unsigned int z_offset = read_u16_be(data+6);
        unsigned int segmented_address = read_u32_be(data+8);
        *cmd_length=0xC;
        GEO_DEBUG_PRINT("Load Display List With Offset: %i, %i, %i, %i, %X",layer,x_offset,y_offset,z_offset,segmented_address);
        paste_macro3(buf, GEO_ANIMATED_PART(layer, x_offset, y_offset, z_offset, convertDLPtr(rom,segmented_address)));
    }else if (data[0]==0x14){
        // GeoLayout command 14: Billboard Model and Translate (and Load Display List or Start Geo Layout). 14 [B][L] [XX XX] [YY YY] [ZZ ZZ] {AA AA AA AA}
        //B	Include last 4 bytes if set to 8 (command will be C bytes long instead of 8)
        //L	Drawing layer if B is set, otherwise will always be zero
        //X	X translation offset (s16)
        //Y	Y translation offset (s16)
        //Z	Z translation offset (s16)
        //A	Segmented address with display list if B is set. Also creates a joint of some kind? (Does not work with animations?)
        unsigned char branch_flag = FIRST_NIBBLE(data[1]);
        unsigned char layer = SECOND_NIBBLE(data[1]);
        unsigned short x_translation = read_u16_be(data+2);
        unsigned short y_translation = read_u16_be(data+4);
        unsigned short z_translation = read_u16_be(data+6);
        unsigned int segmented_address = read_u32_be(data+8);
        *cmd_length=0x8;
        GEO_DEBUG_PRINT("Billboard Model and Translate: %i, %i, %i, %i, %i",branch_flag,layer,x_translation,y_translation,z_translation);
        if (branch_flag==0x8){
            cmd_length=0xC;
            paste_macro3(buf, GEO_BILLBOARD_WITH_PARAMS_AND_DL(layer, x_translation, y_translation, z_translation, convertDLPtr(rom,segmented_address)));
            return false;
        }
        paste_macro2(buf, GEO_BILLBOARD_WITH_PARAMS(layer, x_translation, y_translation, z_translation));
    }else if (data[0]==0x15){
        // GeoLayout command 15: Load Display List. 15 [LL] 00 00 [AA AA AA AA]
        //L	Drawing layer
        //A	Segmented address with display list
        unsigned char layer = data[1];
        unsigned int segmented_address = read_u32_be(data+4);
        *cmd_length=0x8;
        GEO_DEBUG_PRINT("Load Display List: %i, %X",layer,segmented_address);
        paste_macro2(buf, GEO_DISPLAY_LIST(layer, convertDLPtr(rom,segmented_address)));
    }else if (data[0]==0x16){
        // GeoLayout command 16: Start Geo Layout with Shadow. 16 00 00 [AA] 00 [BB] [CC CC]
        // A Shadow type
        // B Shadow solidity (00-FF)
        // C Shadow scale
        unsigned char shadow_type = data[3];
        unsigned char shadow_solidity = data[5];
        unsigned short shadow_scale = read_u16_be(data+6);
        *cmd_length=0x8;
        GEO_DEBUG_PRINT("Start Geo Layout with Shadow - %d, %d, %d", shadow_type, shadow_solidity, shadow_scale);
        paste_macro2(buf, GEO_SHADOW(shadow_type, shadow_solidity, shadow_scale));
    }else if (data[0]==0x17){
        // GeoLayout command 17: Create Object list. 17 00 00 00
        *cmd_length=0x4;
        GEO_DEBUG_PRINT("Create Object list");
        paste_macro(buf, GEO_RENDER_OBJ());
    }else if (data[0]==0x18){
        // GeoLayout command 18: Load Polygons ASM. 18 00 [XX XX] [AA AA AA AA]
        //X Parameters passed into asm function. a1->0x18
        //A ASM RAM Address
        unsigned short parameters = read_u16_be(data+2);
        unsigned int asm_address = read_u32_be(data+4);
        *cmd_length=0x8;
        GEO_DEBUG_PRINT("Load Polygons ASM: %i, %X",parameters,asm_address);
        paste_macro2(buf, GEO_ASM(parameters, convertPtr(asm_address)));
    }
    // 0x19 = Set background, not needed in libsm64
    else if (data[0]==0x1A || data[0]==0x1E || data[0]==0x1F){
        // GeoLayout command 1A: NO OP. 1A 00 00 00 00 00 00 00
        // Apparently unused, but implementing anyways I guess...
        *cmd_length=0x8;
        if (data[0]==0x1F)
            *cmd_length=0x10;
        GEO_DEBUG_PRINT("NO OP");
        paste_macro2(buf, GEO_NOP_1A());
    }else if (data[0]==0x1C){
        // GeoLayout command 1C: Create a held object scene graph node. 1C 00 [XX XX] [YY YY] [ZZ ZZ] [SS SS SS SS]
        //X	X position (s16)
        //Y	Y position (s16)
        //Z	Z position (s16)
        //S	ASM RAM Address
        // Strangely, this one isn't documented in hack64...
        unsigned char param=data[1];
        short offsetX = read_u16_be(data+2);
        short offsetY = read_u16_be(data+4);
        short offsetZ = read_u16_be(data+6);
        unsigned int asm_address = read_u32_be(data+8);
        *cmd_length=0xC;
        GEO_DEBUG_PRINT("Held object scene graph node: %i, %i, %i, %X",offsetX,offsetY,offsetZ,asm_address);
        paste_macro3(buf, GEO_HELD_OBJECT(param,offsetX, offsetY, offsetZ, convertPtr(asm_address)));
    }else if (data[0]==0x1D){
        // GeoLayout command 1D: Scale Model. 1D [A][B] 00 00 [SS SS SS SS] {DD DD DD DD}
        //A if MSbit is set, load B and ??
        //B	If MSbit of A is set, use for A2 to 8037B940
        //S	Scale percentage (0x10000 = 100%)
        //DD	optional word used if the MSbit of A is set
        unsigned char a = FIRST_NIBBLE(data[1]);
        unsigned char b = SECOND_NIBBLE(data[1]);
        unsigned int scale = read_u32_be(data+4);
        unsigned int d = read_u32_be(data+8);
        if (a==0){
            *cmd_length=0x8;
            GEO_DEBUG_PRINT("Scale Model: %i, %i, %i",a,b,scale);
            paste_macro2(buf, GEO_SCALE(b,scale));
            return false;
        }else{
            *cmd_length=0xC;
            GEO_DEBUG_PRINT("Scale Model with DL: %i, %i,%i, %X",a,b,scale,d);
            paste_macro3(buf, GEO_SCALE_WITH_DL(b,scale,convertDLPtr(rom,d)));
            return false;
        }
    }else{
        GEO_DEBUG_PRINT("Unknown GEO command.. first 4 bytes: %x %x %x %x", data[0], data[1], data[2], data[3]);
    }
    
    return false;
}