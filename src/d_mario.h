// SM64 additions

#ifndef __D_MARIO_H__
#define __D_MARIO_H__

#define MARIO_SCALE 4.f
#define IMARIO_SCALE 4

#include <inttypes.h>
#include <string.h>

#include "actor.h"
#include "gl/data/gl_matrix.h"

// opengl only atm
#include "gl/system/gl_system.h"

extern "C" {
	#include <libsm64.h>
	#include <decomp/include/sm64shared.h>
}

typedef float vec2[2];
typedef float vec3[3];
typedef float vec4[4];
typedef float mat4[4][4];


namespace MarioGlobal
{
	extern uint8_t* texture;
	extern GLuint shader;
	extern GLuint GLtexture;

	void initThings();
}


class MarioInstance
{
	GLuint vao;
	GLuint position_buf;
	GLuint normal_buf;
	GLuint color_buf;
	GLuint uv_buf;
	uint16_t *meshIndex;

	int marioId;
	float ticks;
	AActor *parent;

public:
	int first;
	float lastPos[3];
	float newPos[3];
	float lastGeom[SM64_GEO_MAX_TRIANGLES * 9];
	float newGeom[SM64_GEO_MAX_TRIANGLES * 9];
	SM64MarioInputs input;
	SM64MarioState state;
	SM64MarioGeometryBuffers geometry;


	MarioInstance(int id, AActor *Parent);
	~MarioInstance();

	void Tick(float addTicks);
	void UpdateModel(float ticks, float* position);
	void Render(VSMatrix &view, VSMatrix &projection);

	int ID() {return marioId;}
};

#endif
