// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2002-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_dynlight1.cpp
** dynamic light application
**
**/

#include "gl/system/gl_system.h"
#include "c_dispatch.h"
#include "p_local.h"
#include "vectors.h"
#include "gl/gl_functions.h"
#include "g_level.h"
#include "actorinlines.h"
#include "a_dynlight.h"

#include "gl/system/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/data/gl_data.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_material.h"


//==========================================================================
//
// Light related CVARs
//
//==========================================================================

CVAR (Bool, gl_lights_checkside, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_light_sprites, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_light_particles, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_light_shadowmap, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

CVAR(Int, gl_attenuate, -1, 0);	// This is mainly a debug option.

//==========================================================================
//
// Sets up the parameters to render one dynamic light onto one plane
//
//==========================================================================
bool gl_GetLight(int group, Plane & p, ADynamicLight * light, bool checkside, FDynLightData &ldata)
{
	DVector3 pos = light->PosRelative(group);
	float radius = (light->GetRadius());

	float dist = fabsf(p.DistToPoint(pos.X, pos.Z, pos.Y));

	if (radius <= 0.f) return false;
	if (dist > radius) return false;
	if (checkside && gl_lights_checkside && p.PointOnSide(pos.X, pos.Z, pos.Y))
	{
		return false;
	}

	gl_AddLightToList(group, light, ldata, false);
	return true;
}

//==========================================================================
//
// Add one dynamic light to the light data list
//
//==========================================================================
void gl_AddLightToList(int group, ADynamicLight * light, FDynLightData &ldata, bool hudmodel)
{
	int i = 0;

	DVector3 pos = light->PosRelative(group);
	float radius = light->GetRadius();

	if (hudmodel)
	{
		// HUD model is already translated and rotated. We must rotate the lights into that view space.

		DVector3 rotation;
		DVector3 localpos = pos - r_viewpoint.Pos;

		rotation.X = localpos.X * r_viewpoint.Angles.Yaw.Sin() - localpos.Y * r_viewpoint.Angles.Yaw.Cos();
		rotation.Y = localpos.X * r_viewpoint.Angles.Yaw.Cos() + localpos.Y * r_viewpoint.Angles.Yaw.Sin();
		rotation.Z = localpos.Z;
		localpos = rotation;

		rotation.X = localpos.X;
		rotation.Y = localpos.Y * r_viewpoint.Angles.Pitch.Sin() - localpos.Z * r_viewpoint.Angles.Pitch.Cos();
		rotation.Z = localpos.Y * r_viewpoint.Angles.Pitch.Cos() + localpos.Z * r_viewpoint.Angles.Pitch.Sin();
		localpos = rotation;

		rotation.Y = localpos.Y;
		rotation.Z = localpos.Z * r_viewpoint.Angles.Roll.Sin() - localpos.X * r_viewpoint.Angles.Roll.Cos();
		rotation.X = localpos.Z * r_viewpoint.Angles.Roll.Cos() + localpos.X * r_viewpoint.Angles.Roll.Sin();
		localpos = rotation;

		pos = localpos;
	}

	float cs;
	if (light->IsAdditive()) 
	{
		cs = 0.2f;
		i = 2;
	}
	else 
	{
		cs = 1.0f;
	}

	float r = light->GetRed() / 255.0f * cs;
	float g = light->GetGreen() / 255.0f * cs;
	float b = light->GetBlue() / 255.0f * cs;

	if (light->IsSubtractive())
	{
		DVector3 v(r, g, b);
		float length = (float)v.Length();
		
		r = length - r;
		g = length - g;
		b = length - b;
		i = 1;
	}

	// Store attenuate flag in the sign bit of the float.
	float shadowIndex = GLRenderer->mShadowMap.ShadowMapIndex(light) + 1.0f;
	bool attenuate;

	if (gl_attenuate == -1) attenuate = !!(light->lightflags & LF_ATTENUATE);
	else attenuate = !!gl_attenuate;

	if (attenuate) shadowIndex = -shadowIndex;

	float *data = &ldata.arrays[i][ldata.arrays[i].Reserve(8)];
	data[0] = pos.X;
	data[1] = pos.Z;
	data[2] = pos.Y;
	data[3] = radius;
	data[4] = r;
	data[5] = g;
	data[6] = b;
	data[7] = shadowIndex;
}

