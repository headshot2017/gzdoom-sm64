#ifndef LIB_SM64_H
#define LIB_SM64_H
#define _XOPEN_SOURCE 500

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
    #ifdef SM64_LIB_EXPORT
        #define SM64_LIB_FN __declspec(dllexport)
    #else
        #define SM64_LIB_FN __declspec(dllimport)
    #endif
#else
    #define SM64_LIB_FN
#endif

struct SM64Surface
{
    int16_t type;
    int16_t force;
    uint16_t terrain;
    int32_t vertices[3][3];
};

struct SM64MarioInputs
{
    float camLookX, camLookZ;
    float stickX, stickY;
    uint8_t buttonA, buttonB, buttonZ;
};

struct SM64ObjectTransform
{
    float position[3];
    float eulerRotation[3];
};

struct SM64SurfaceObject
{
    struct SM64ObjectTransform transform;
    uint32_t surfaceCount;
    struct SM64Surface *surfaces;
};

struct SM64MarioState
{
    float position[3];
    float velocity[3];
    float faceAngle;
    int16_t health;
	uint32_t action;
	uint32_t flags;
	uint32_t particleFlags;
	int16_t invincTimer;
};

struct SM64MarioGeometryBuffers
{
    float *position;
    float *normal;
    float *color;
    float *uv;
    uint16_t numTrianglesUsed;
};

struct SM64MarioColorGroup
{
    uint8_t shade[3];
    uint8_t color[3];
};

struct SM64MarioModelColors
{
	struct SM64MarioColorGroup blue;
	struct SM64MarioColorGroup red;
	struct SM64MarioColorGroup white;
	struct SM64MarioColorGroup brown1;
	struct SM64MarioColorGroup beige;
	struct SM64MarioColorGroup brown2;
};

struct SM64Animation
{
	int16_t flags;
	int16_t animYTransDivisor;
	int16_t startFrame;
	int16_t loopStart;
	int16_t loopEnd;
	int16_t unusedBoneCount;
	int16_t* values;
	uint16_t* index;
	uint32_t length;
};

struct SM64AnimInfo
{
	int16_t animID;
	int16_t animYTrans;
	struct SM64Animation *curAnim;
	int16_t animFrame;
	uint16_t animTimer;
	int32_t animFrameAccelAssist;
	int32_t animAccel;
};


typedef void (*SM64DebugPrintFunctionPtr)( const char * );

enum
{
    SM64_TEXTURE_WIDTH = 64 * 11,
    SM64_TEXTURE_HEIGHT = 64,
    SM64_GEO_MAX_TRIANGLES = 1024,
};

extern SM64_LIB_FN void sm64_global_init( uint8_t *rom, uint8_t *outTexture, SM64DebugPrintFunctionPtr debugPrintFunction );
extern SM64_LIB_FN void sm64_global_terminate( void );

extern SM64_LIB_FN void sm64_static_surfaces_load( const struct SM64Surface *surfaceArray, uint32_t numSurfaces );

extern SM64_LIB_FN int32_t sm64_mario_create( float x, float y, float z, int16_t rx, int16_t ry, int16_t rz, uint8_t fake );
extern SM64_LIB_FN void sm64_mario_tick( int32_t marioId, const struct SM64MarioInputs *inputs, struct SM64MarioState *outState, struct SM64MarioGeometryBuffers *outBuffers );
extern SM64_LIB_FN struct SM64AnimInfo* sm64_mario_get_anim_info( int32_t marioId, int16_t rot[3] );
extern SM64_LIB_FN void sm64_mario_anim_tick( int32_t marioId, uint32_t stateFlags, struct SM64AnimInfo* animInfo, struct SM64MarioGeometryBuffers *outBuffers, int16_t rot[3] );
extern SM64_LIB_FN void sm64_mario_delete( int32_t marioId );

extern SM64_LIB_FN void sm64_set_mario_action(int32_t marioId, uint32_t action);
extern SM64_LIB_FN void sm64_set_mario_action_arg(int32_t marioId, uint32_t action, uint32_t actionArg);
extern SM64_LIB_FN void sm64_set_mario_animation(int32_t marioId, int32_t animID);
extern SM64_LIB_FN void sm64_set_mario_anim_frame(int32_t marioId, int16_t animFrame);
extern SM64_LIB_FN void sm64_set_mario_state(int32_t marioId, uint32_t flags);
extern SM64_LIB_FN void sm64_set_mario_position(int32_t marioId, float x, float y, float z);
extern SM64_LIB_FN void sm64_set_mario_angle(int32_t marioId, float x, float y, float z);
extern SM64_LIB_FN void sm64_set_mario_faceangle(int32_t marioId, float y);
extern SM64_LIB_FN void sm64_set_mario_velocity(int32_t marioId, float x, float y, float z);
extern SM64_LIB_FN void sm64_set_mario_forward_velocity(int32_t marioId, float vel);
extern SM64_LIB_FN void sm64_set_mario_water_level(int32_t marioId, signed int level);
extern SM64_LIB_FN signed int sm64_get_mario_water_level(int32_t marioId);
extern SM64_LIB_FN void sm64_set_mario_floor_override(int32_t marioId, uint16_t terrain, int16_t floorType);
extern SM64_LIB_FN void sm64_mario_take_damage(int32_t marioId, uint32_t damage, uint32_t subtype, float x, float y, float z);
extern SM64_LIB_FN void sm64_mario_heal(int32_t marioId, uint8_t healCounter);
extern SM64_LIB_FN void sm64_mario_set_health(int32_t marioId, uint16_t health);
extern SM64_LIB_FN void sm64_mario_kill(int32_t marioId);
extern SM64_LIB_FN void sm64_mario_interact_cap(int32_t marioId, uint32_t capFlag, uint16_t capTime, uint8_t playMusic);
extern SM64_LIB_FN bool sm64_mario_attack(int32_t marioId, float x, float y, float z, float hitboxHeight);

extern SM64_LIB_FN uint32_t sm64_surface_object_create( const struct SM64SurfaceObject *surfaceObject );
extern SM64_LIB_FN void sm64_surface_object_move( uint32_t objectId, const struct SM64ObjectTransform *transform );
extern SM64_LIB_FN void sm64_surface_object_delete( uint32_t objectId );

extern SM64_LIB_FN void sm64_seq_player_play_sequence(uint8_t player, uint8_t seqId, uint16_t arg2);
extern SM64_LIB_FN void sm64_play_music(uint8_t player, uint16_t seqArgs, uint16_t fadeTimer);
extern SM64_LIB_FN void sm64_stop_background_music(uint16_t seqId);
extern SM64_LIB_FN void sm64_fadeout_background_music(uint16_t arg0, uint16_t fadeOut);
extern SM64_LIB_FN uint16_t sm64_get_current_background_music();
extern SM64_LIB_FN void sm64_play_sound(int32_t soundBits, float *pos);
extern SM64_LIB_FN void sm64_play_sound_global(int32_t soundBits);
extern SM64_LIB_FN int sm64_get_version();

void audio_tick();
void* audio_thread(void* param);

#endif//LIB_SM64_H
