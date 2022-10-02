#include <stdlib.h>

#include "d_mario.h"
#include "doomtype.h"
#include "i_system.h"

#include "gl/system/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/models/gl_models.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_geometric.h"
#include "gl/utility/gl_convert.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/shaders/gl_shader.h"

typedef float vec2[2];
typedef float vec3[3];
typedef float vec4[4];
typedef float mat4[4][4];

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


MarioRenderer::MarioRenderer(SM64MarioGeometryBuffers *geometry) : geo(geometry)
{
	meshIndex = (uint16_t*)malloc( 3 * SM64_GEO_MAX_TRIANGLES * sizeof(uint16_t) );
	for( int i = 0; i < 3 * SM64_GEO_MAX_TRIANGLES; ++i )
		meshIndex[i] = i;

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	#define X( loc, buff, arr, type ) do { \
		glGenBuffers(1, &buff); \
		glBindBuffer(GL_ARRAY_BUFFER, buff); \
		glBufferData(GL_ARRAY_BUFFER, sizeof( type ) * 3 * SM64_GEO_MAX_TRIANGLES, arr, GL_DYNAMIC_DRAW); \
		glEnableVertexAttribArray(loc); \
		glVertexAttribPointer(loc, sizeof(type) / sizeof(float), GL_FLOAT, GL_FALSE, sizeof(type), NULL); \
	} while( 0 )

	X(6, position_buf, geometry->position, vec3);
	X(7, normal_buf,   geometry->normal,   vec3);
	X(8, color_buf,    geometry->color,    vec3);
	X(9, uv_buf,       geometry->uv,       vec2);

	#undef X
}

MarioRenderer::~MarioRenderer()
{
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &position_buf);
	glDeleteBuffers(1, &normal_buf);
	glDeleteBuffers(1, &color_buf);
	glDeleteBuffers(1, &uv_buf);
	free(meshIndex);
}

void MarioRenderer::Update()
{
    glBindBuffer(GL_ARRAY_BUFFER, position_buf);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof( vec3 ) * 3 * SM64_GEO_MAX_TRIANGLES, geo->position);
    glBindBuffer(GL_ARRAY_BUFFER, normal_buf);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof( vec3 ) * 3 * SM64_GEO_MAX_TRIANGLES, geo->normal);
    glBindBuffer(GL_ARRAY_BUFFER, color_buf);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof( vec3 ) * 3 * SM64_GEO_MAX_TRIANGLES, geo->color);
    glBindBuffer(GL_ARRAY_BUFFER, uv_buf);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof( vec2 ) * 3 * SM64_GEO_MAX_TRIANGLES, geo->uv);
}

void MarioRenderer::Render(VSMatrix &view, VSMatrix &projection)
{
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
	glDrawElements(GL_TRIANGLES, geo->numTrianglesUsed * 3, GL_UNSIGNED_SHORT, meshIndex);

	//glEnable(GL_CLIP_DISTANCE0);
	//glEnable(GL_CLIP_DISTANCE1);
	//glEnable(GL_CLIP_DISTANCE2);
	//glEnable(GL_CLIP_DISTANCE3);
	glDepthMask(GL_FALSE);
	//glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glActiveTexture(GL_TEXTURE0);
}
