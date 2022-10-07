// Some commonly used SM64 stuff

#ifndef __D_MARIO_H__
#define __D_MARIO_H__

#define MARIO_SCALE 4.f
#define IMARIO_SCALE 4

#include <inttypes.h>
#include <string.h>
#include "gl/data/gl_matrix.h"

// opengl only atm
#include "gl/system/gl_system.h"

extern "C" {
	#include <libsm64.h>
}

namespace MarioGlobal
{
	extern uint8_t* texture;
	extern GLuint shader;
	extern GLuint GLtexture;

	void initThings();
}

class MarioRenderer
{
	GLuint vao;
	GLuint position_buf;
	GLuint normal_buf;
	GLuint color_buf;
	GLuint uv_buf;

	uint16_t *meshIndex;
	SM64MarioGeometryBuffers *geo;

public:
	int first;

	MarioRenderer(SM64MarioGeometryBuffers *geometry);
	~MarioRenderer();

	void Update();
	void Render(VSMatrix &view, VSMatrix &projection);
};

#endif
