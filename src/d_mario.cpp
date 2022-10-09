#include <stdlib.h>

#include <decomp/include/audio_defines.h>

#include "d_mario.h"
#include "doomtype.h"
#include "i_system.h"
#include "p_local.h"

#include "gl/system/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/models/gl_models.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_geometric.h"
#include "gl/utility/gl_convert.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/shaders/gl_shader.h"

const char *MARIO_SHADER =
"\n uniform mat4 view;"
"\n uniform mat4 projection;"
"\n uniform sampler2D marioTex;"
"\n "
"\n v2f vec3 v_color;"
"\n v2f vec3 v_normal;"
"\n v2f vec3 v_light;"
"\n v2f vec2 v_uv;"
"\n "
"\n #ifdef VERTEX"
"\n "
"\n     in vec3 position;"
"\n     in vec3 normal;"
"\n     in vec3 color;"
"\n     in vec2 uv;"
"\n "
"\n     void main()"
"\n     {"
"\n         v_color = color;"
"\n         v_normal = normal;"
"\n         v_light = transpose( mat3( view )) * normalize( vec3( 1 ));"
"\n         v_uv = uv;"
"\n "
"\n         gl_Position = projection * view * vec4( position, 1. );"
"\n     }"
"\n "
"\n #endif"
"\n #ifdef FRAGMENT"
"\n "
"\n     out vec4 color;"
"\n "
"\n     void main() "
"\n     {"
"\n         float light = .5 + .5 * clamp( dot( v_normal, v_light ), 0., 1. );"
"\n         vec4 texColor = texture2D( marioTex, v_uv );"
"\n         vec3 mainColor = mix( v_color, texColor.rgb, texColor.a ); // v_uv.x >= 0. ? texColor.a : 0. );"
"\n         color = vec4( mainColor * light, 1 );"
"\n     }"
"\n "
"\n #endif";

GLuint shader_compile( const char *shaderContents, size_t shaderContentsLength, GLenum shaderType )
{
    const GLchar *shaderDefine = shaderType == GL_VERTEX_SHADER 
        ? "\n#version 130\n#define VERTEX  \n#define v2f out\n" 
        : "\n#version 130\n#define FRAGMENT\n#define v2f in \n";

    const GLchar *shaderStrings[2] = { shaderDefine, shaderContents };
    GLint shaderStringLengths[2] = { (GLint)strlen( shaderDefine ), (GLint)shaderContentsLength };

    GLuint shader = glCreateShader( shaderType );
    glShaderSource( shader, 2, shaderStrings, shaderStringLengths );
    glCompileShader( shader );

    GLint isCompiled;
    glGetShaderiv( shader, GL_COMPILE_STATUS, &isCompiled );
    if( isCompiled == GL_FALSE ) 
    {
        GLint maxLength;
        glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &maxLength );
        char *log = (char*)malloc( maxLength );
        glGetShaderInfoLog( shader, maxLength, &maxLength, log );

        I_FatalError( "Error in Mario shader: %s\n%s\n%s\n", log, shaderStrings[0], shaderStrings[1] );
    }

    return shader;
}

namespace MarioGlobal
{
	uint8_t* texture;
	GLuint shader;
	GLuint GLtexture;

	void initThings()
	{
		// initialize shader
		GLuint vert = shader_compile(MARIO_SHADER, strlen(MARIO_SHADER), GL_VERTEX_SHADER);
		GLuint frag = shader_compile(MARIO_SHADER, strlen(MARIO_SHADER), GL_FRAGMENT_SHADER);

		shader = glCreateProgram();
		glAttachShader(shader, vert);
		glAttachShader(shader, frag);

		const GLchar *attribs[] = {"position", "normal", "color", "uv"};
		for (int i=6; i<10; i++) glBindAttribLocation(shader, i, attribs[i-6]);

		glLinkProgram(shader);
		glDetachShader(shader, vert);
		glDetachShader(shader, frag);

		// initialize texture
		glGenTextures( 1, &GLtexture );
		glBindTexture( GL_TEXTURE_2D, GLtexture );
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SM64_TEXTURE_WIDTH, SM64_TEXTURE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture);
	}
}


MarioInstance::MarioInstance(int id, AActor *Parent) : marioId(id), parent(Parent)
{
	meshIndex = (uint16_t*)malloc( 3 * SM64_GEO_MAX_TRIANGLES * sizeof(uint16_t) );
	for( int i = 0; i < 3 * SM64_GEO_MAX_TRIANGLES; ++i )
		meshIndex[i] = i;

	geometry.position = (float*)malloc( sizeof(float) * 9 * SM64_GEO_MAX_TRIANGLES );
	geometry.color    = (float*)malloc( sizeof(float) * 9 * SM64_GEO_MAX_TRIANGLES );
	geometry.normal   = (float*)malloc( sizeof(float) * 9 * SM64_GEO_MAX_TRIANGLES );
	geometry.uv       = (float*)malloc( sizeof(float) * 6 * SM64_GEO_MAX_TRIANGLES );
	geometry.numTrianglesUsed = 0;
	memset(&input, 0, sizeof(SM64MarioInputs));
	memset(&state, 0, sizeof(SM64MarioState));

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	#define X( loc, buff, arr, type ) do { \
		glGenBuffers(1, &buff); \
		glBindBuffer(GL_ARRAY_BUFFER, buff); \
		glBufferData(GL_ARRAY_BUFFER, sizeof( type ) * 3 * SM64_GEO_MAX_TRIANGLES, arr, GL_DYNAMIC_DRAW); \
		glEnableVertexAttribArray(loc); \
		glVertexAttribPointer(loc, sizeof(type) / sizeof(float), GL_FLOAT, GL_FALSE, sizeof(type), NULL); \
	} while( 0 )

	X(6, position_buf, geometry.position, vec3);
	X(7, normal_buf,   geometry.normal,   vec3);
	X(8, color_buf,    geometry.color,    vec3);
	X(9, uv_buf,       geometry.uv,       vec2);

	#undef X

	first = 0;
	memset(lastGeom, 0, sizeof(float) * 9 * SM64_GEO_MAX_TRIANGLES);
	memset(newGeom, 0, sizeof(float) * 9 * SM64_GEO_MAX_TRIANGLES);
	memset(lastPos, 0, sizeof(float) * 3);
	memset(newPos, 0, sizeof(float) * 3);
	ticks = 0;
}

MarioInstance::~MarioInstance()
{
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &position_buf);
	glDeleteBuffers(1, &normal_buf);
	glDeleteBuffers(1, &color_buf);
	glDeleteBuffers(1, &uv_buf);
	free(meshIndex);

	sm64_mario_delete(marioId);

	free(geometry.position);
	free(geometry.normal);
	free(geometry.color);
	free(geometry.uv);
}

void MarioInstance::Tick(float addTicks)
{
	if (first < 2) return;

	ticks += addTicks;

	if (parent->health > 0)
		sm64_mario_set_health(marioId, (parent->health <= 20) ? 0x200 : 0x880); // mario panting animation if low on health
	else
		sm64_mario_kill(marioId);

	while (ticks > 1.f/30)
	{
		for (int i=0; i<9 * geometry.numTrianglesUsed; i++)
		{
			if (i<3) lastPos[i] = newPos[i];
			lastGeom[i] = newGeom[i];
		}

		ticks -= 1./30;
		sm64_mario_tick(marioId, &input, &state, &geometry);

		for (int i=0; i<3; i++)
			newPos[i] = state.position[i];

		for (int i=0; i<3 * geometry.numTrianglesUsed; i++)
		{
			// set scales, make Z negative and aspect ratio correction (unsquish Mario)
			newGeom[i*3+0] = ((geometry.position[i*3+0] - state.position[0]) * 1.15f + state.position[0]) / MARIO_SCALE;
			newGeom[i*3+1] = geometry.position[i*3+1] / MARIO_SCALE;
			newGeom[i*3+2] = ((geometry.position[i*3+2] - state.position[2]) * 1.15f + state.position[2]) / -MARIO_SCALE;

			geometry.normal[i*3+2] *= -1;
		}

		// hurt other objects/enemies in range
		AActor *link, *next;
		for (link = parent->Sector->thinglist; link != NULL; link = next)
		{
			next = link->snext;

			if (!(link->flags & MF_SHOOTABLE))
				continue;			// not shootable (observer or dead)

			if (link == parent)
				continue;

			if (link->health <= 0)
				continue;			// dead

			if (link->flags2 & MF2_DORMANT)
				continue;			// don't target dormant things

			if (link->flags7 & MF7_NEVERTARGET)
				continue;

			float dist = sqrtf(pow(link->Pos().X*MARIO_SCALE - state.position[0], 2) + pow(link->Pos().Y*MARIO_SCALE + state.position[2], 2));
			float ydist = fabs(link->Pos().Z*MARIO_SCALE - state.position[1])/4.5f;
			if (dist > 150 || ydist > link->Height)
				continue;

			if (sm64_mario_attack(marioId, link->Pos().X*MARIO_SCALE, link->Pos().Z*MARIO_SCALE, -link->Pos().Y*MARIO_SCALE, 0))
			{
				int damage = 10;
				if (state.action == ACT_JUMP_KICK)
					damage += 5;
				else if (state.action == ACT_GROUND_POUND)
				{
					damage += 15;
					sm64_set_mario_action(marioId, ACT_TRIPLE_JUMP);
					sm64_play_sound_global(SOUND_ACTION_HIT);
				}

				AInventory *item = parent->FindInventory("PowerStrength"); // detect Berserk pack
				if (state.flags & MARIO_METAL_CAP || item)
					damage += 30;

				P_DamageMobj(link, parent, parent, damage, (FName)RADF_HURTSOURCE);
			}
		}
	}

	UpdateModel(ticks, state.position);
}

void MarioInstance::UpdateModel(float ticks, float* position)
{
	if (first < 2) return;

	for (int i=0; i<9 * geometry.numTrianglesUsed; i++)
	{
		if (i<3) position[i] = lastPos[i] + ((newPos[i] - lastPos[i]) * (ticks / (1.f/30)));
		geometry.position[i] = lastGeom[i] + ((newGeom[i] - lastGeom[i]) * (ticks / (1.f/30)));
	}

    glBindBuffer(GL_ARRAY_BUFFER, position_buf);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof( vec3 ) * 3 * SM64_GEO_MAX_TRIANGLES, geometry.position);
    glBindBuffer(GL_ARRAY_BUFFER, normal_buf);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof( vec3 ) * 3 * SM64_GEO_MAX_TRIANGLES, geometry.normal);
    glBindBuffer(GL_ARRAY_BUFFER, color_buf);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof( vec3 ) * 3 * SM64_GEO_MAX_TRIANGLES, geometry.color);
    glBindBuffer(GL_ARRAY_BUFFER, uv_buf);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof( vec2 ) * 3 * SM64_GEO_MAX_TRIANGLES, geometry.uv);
}

void MarioInstance::Render(VSMatrix &view, VSMatrix &projection)
{
	if (first < 2) return;

	glDisable(GL_CLIP_DISTANCE0);
	glDisable(GL_CLIP_DISTANCE1);
	glDisable(GL_CLIP_DISTANCE2);
	glDisable(GL_CLIP_DISTANCE3);
	//glDisable(GL_CLIP_DISTANCE4);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	//glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	glUseProgram(MarioGlobal::shader);
	glActiveTexture(GL_TEXTURE2); // binding to texture 0 or 1 results in black. bind to texture 2 instead
	glBindTexture(GL_TEXTURE_2D, MarioGlobal::GLtexture);
	glBindVertexArray(vao);
	glUniformMatrix4fv(glGetUniformLocation(MarioGlobal::shader, "view"), 1, GL_FALSE, (GLfloat*)&view);
	glUniformMatrix4fv(glGetUniformLocation(MarioGlobal::shader, "projection"), 1, GL_FALSE, (GLfloat*)&projection);
	glUniform1i(glGetUniformLocation(MarioGlobal::shader, "marioTex"), 2);
	glDrawElements(GL_TRIANGLES, geometry.numTrianglesUsed * 3, GL_UNSIGNED_SHORT, meshIndex);

	//glEnable(GL_CLIP_DISTANCE0);
	//glEnable(GL_CLIP_DISTANCE1);
	//glEnable(GL_CLIP_DISTANCE2);
	//glEnable(GL_CLIP_DISTANCE3);
	glDepthMask(GL_FALSE);
	//glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glActiveTexture(GL_TEXTURE0);
}
