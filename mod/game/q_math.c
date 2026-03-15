// Copyright (C) 1999-2000 Id Software, Inc.
//
// q_math.c -- stateless support routines that are included in each code module
#include "q_shared.h"

vec3_t	vec3_origin 	= { 0, 0, 0};
vec3_t	axisDefault[3]	= { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };

vec4_t	colorBlack		= { 0, 0, 0, 1};
vec4_t	colorRed		= { 1, 0, 0, 1};
vec4_t	colorGreen		= { 0, 1, 0, 1};
vec4_t	colorBlue		= { 0, 0, 1, 1};
vec4_t	colorYellow 	= { 1, 1, 0, 1};
vec4_t	colorMagenta	= { 1, 0, 1, 1};
vec4_t	colorCyan		= { 0, 1, 1, 1};
vec4_t	colorWhite		= { 1, 1, 1, 1};
vec4_t	colorLtGrey 	= { 0.75, 0.75, 0.75, 1};
vec4_t	colorMdGrey 	= { 0.5, 0.5, 0.5, 1};
vec4_t	colorDkGrey 	= { 0.25, 0.25, 0.25, 1};

vec4_t	g_color_table[10] = {
	{		 0.0f,		 0.0f,		 0.0f, 1.0f },
	{ 223.0f / 255.0f, 74.0f / 255.0f, 79.0f / 255.0f, 1.0f },
//	{		0.51f,			0.13f,		0.13f, 1	},
//	{		0.89f,		0.22f,		0.22f, 1	},
	{		 0.0f,		 1.0f,		 0.0f, 1.0f },
	{		 1.0f,		 1.0f,		 0.6f, 1.0f },
	{		0.61f,		0.81f,		 1.0f, 1.0f },
//	{		0.13f,		0.11f,		0.52f, 1	},
//	{		0.22f,		0.19f,		0.91f, 1	},
	{		 0.0f,		 1.0f,		 1.0f, 1.0f },
	{		 1.0f,		 0.0f,		 1.0f, 1.0f },
	{		 1.0f,		 1.0f,		 1.0f, 1.0f },
	{               0.88f,          0.31f,          0.13f, 1.f  },
	{               0.29f,          0.30f,          0.19f, 1.f  },
};

/*
vec4_t	g_color_table[8] =
	{
	{0.0, 0.0, 0.0, 1.0},
	{1.0, 0.0, 0.0, 1.0},
	{0.0, 1.0, 0.0, 1.0},
	{1.0, 1.0, 0.0, 1.0},
	{0.0, 0.0, 1.0, 1.0},
	{0.0, 1.0, 1.0, 1.0},
	{1.0, 0.0, 1.0, 1.0},
	{1.0, 1.0, 1.0, 1.0},
	};
*/

vec3_t	bytedirs[NUMVERTEXNORMALS] = {
	{ -0.525731f,  0.000000f,  0.850651f}, { -0.442863f,  0.238856f,  0.864188f},
	{ -0.295242f,  0.000000f,  0.955423f}, { -0.309017f,  0.500000f,  0.809017f},
	{ -0.162460f,  0.262866f,  0.951056f}, {  0.000000f,  0.000000f,  1.000000f},
	{  0.000000f,  0.850651f,  0.525731f}, { -0.147621f,  0.716567f,  0.681718f},
	{  0.147621f,  0.716567f,  0.681718f}, {  0.000000f,  0.525731f,  0.850651f},
	{  0.309017f,  0.500000f,  0.809017f}, {  0.525731f,  0.000000f,  0.850651f},
	{  0.295242f,  0.000000f,  0.955423f}, {  0.442863f,  0.238856f,  0.864188f},
	{  0.162460f,  0.262866f,  0.951056f}, { -0.681718f,  0.147621f,  0.716567f},
	{ -0.809017f,  0.309017f,  0.500000f}, { -0.587785f,  0.425325f,  0.688191f},
	{ -0.850651f,  0.525731f,  0.000000f}, { -0.864188f,  0.442863f,  0.238856f},
	{ -0.716567f,  0.681718f,  0.147621f}, { -0.688191f,  0.587785f,  0.425325f},
	{ -0.500000f,  0.809017f,  0.309017f}, { -0.238856f,  0.864188f,  0.442863f},
	{ -0.425325f,  0.688191f,  0.587785f}, { -0.716567f,  0.681718f, -0.147621f},
	{ -0.500000f,  0.809017f, -0.309017f}, { -0.525731f,  0.850651f,  0.000000f},
	{  0.000000f,  0.850651f, -0.525731f}, { -0.238856f,  0.864188f, -0.442863f},
	{  0.000000f,  0.955423f, -0.295242f}, { -0.262866f,  0.951056f, -0.162460f},
	{  0.000000f,  1.000000f,  0.000000f}, {  0.000000f,  0.955423f,  0.295242f},
	{ -0.262866f,  0.951056f,  0.162460f}, {  0.238856f,  0.864188f,  0.442863f},
	{  0.262866f,  0.951056f,  0.162460f}, {  0.500000f,  0.809017f,  0.309017f},
	{  0.238856f,  0.864188f, -0.442863f}, {  0.262866f,  0.951056f, -0.162460f},
	{  0.500000f,  0.809017f, -0.309017f}, {  0.850651f,  0.525731f,  0.000000f},
	{  0.716567f,  0.681718f,  0.147621f}, {  0.716567f,  0.681718f, -0.147621f},
	{  0.525731f,  0.850651f,  0.000000f}, {  0.425325f,  0.688191f,  0.587785f},
	{  0.864188f,  0.442863f,  0.238856f}, {  0.688191f,  0.587785f,  0.425325f},
	{  0.809017f,  0.309017f,  0.500000f}, {  0.681718f,  0.147621f,  0.716567f},
	{  0.587785f,  0.425325f,  0.688191f}, {  0.955423f,  0.295242f,  0.000000f},
	{  1.000000f,  0.000000f,  0.000000f}, {  0.951056f,  0.162460f,  0.262866f},
	{  0.850651f, -0.525731f,  0.000000f}, {  0.955423f, -0.295242f,  0.000000f},
	{  0.864188f, -0.442863f,  0.238856f}, {  0.951056f, -0.162460f,  0.262866f},
	{  0.809017f, -0.309017f,  0.500000f}, {  0.681718f, -0.147621f,  0.716567f},
	{  0.850651f,  0.000000f,  0.525731f}, {  0.864188f,  0.442863f, -0.238856f},
	{  0.809017f,  0.309017f, -0.500000f}, {  0.951056f,  0.162460f, -0.262866f},
	{  0.525731f,  0.000000f, -0.850651f}, {  0.681718f,  0.147621f, -0.716567f},
	{  0.681718f, -0.147621f, -0.716567f}, {  0.850651f,  0.000000f, -0.525731f},
	{  0.809017f, -0.309017f, -0.500000f}, {  0.864188f, -0.442863f, -0.238856f},
	{  0.951056f, -0.162460f, -0.262866f}, {  0.147621f,  0.716567f, -0.681718f},
	{  0.309017f,  0.500000f, -0.809017f}, {  0.425325f,  0.688191f, -0.587785f},
	{  0.442863f,  0.238856f, -0.864188f}, {  0.587785f,  0.425325f, -0.688191f},
	{  0.688191f,  0.587785f, -0.425325f}, { -0.147621f,  0.716567f, -0.681718f},
	{ -0.309017f,  0.500000f, -0.809017f}, {  0.000000f,  0.525731f, -0.850651f},
	{ -0.525731f,  0.000000f, -0.850651f}, { -0.442863f,  0.238856f, -0.864188f},
	{ -0.295242f,  0.000000f, -0.955423f}, { -0.162460f,  0.262866f, -0.951056f},
	{  0.000000f,  0.000000f, -1.000000f}, {  0.295242f,  0.000000f, -0.955423f},
	{  0.162460f,  0.262866f, -0.951056f}, { -0.442863f, -0.238856f, -0.864188f},
	{ -0.309017f, -0.500000f, -0.809017f}, { -0.162460f, -0.262866f, -0.951056f},
	{  0.000000f, -0.850651f, -0.525731f}, { -0.147621f, -0.716567f, -0.681718f},
	{  0.147621f, -0.716567f, -0.681718f}, {  0.000000f, -0.525731f, -0.850651f},
	{  0.309017f, -0.500000f, -0.809017f}, {  0.442863f, -0.238856f, -0.864188f},
	{  0.162460f, -0.262866f, -0.951056f}, {  0.238856f, -0.864188f, -0.442863f},
	{  0.500000f, -0.809017f, -0.309017f}, {  0.425325f, -0.688191f, -0.587785f},
	{  0.716567f, -0.681718f, -0.147621f}, {  0.688191f, -0.587785f, -0.425325f},
	{  0.587785f, -0.425325f, -0.688191f}, {  0.000000f, -0.955423f, -0.295242f},
	{  0.000000f, -1.000000f,  0.000000f}, {  0.262866f, -0.951056f, -0.162460f},
	{  0.000000f, -0.850651f,  0.525731f}, {  0.000000f, -0.955423f,  0.295242f},
	{  0.238856f, -0.864188f,  0.442863f}, {  0.262866f, -0.951056f,  0.162460f},
	{  0.500000f, -0.809017f,  0.309017f}, {  0.716567f, -0.681718f,  0.147621f},
	{  0.525731f, -0.850651f,  0.000000f}, { -0.238856f, -0.864188f, -0.442863f},
	{ -0.500000f, -0.809017f, -0.309017f}, { -0.262866f, -0.951056f, -0.162460f},
	{ -0.850651f, -0.525731f,  0.000000f}, { -0.716567f, -0.681718f, -0.147621f},
	{ -0.716567f, -0.681718f,  0.147621f}, { -0.525731f, -0.850651f,  0.000000f},
	{ -0.500000f, -0.809017f,  0.309017f}, { -0.238856f, -0.864188f,  0.442863f},
	{ -0.262866f, -0.951056f,  0.162460f}, { -0.864188f, -0.442863f,  0.238856f},
	{ -0.809017f, -0.309017f,  0.500000f}, { -0.688191f, -0.587785f,  0.425325f},
	{ -0.681718f, -0.147621f,  0.716567f}, { -0.442863f, -0.238856f,  0.864188f},
	{ -0.587785f, -0.425325f,  0.688191f}, { -0.309017f, -0.500000f,  0.809017f},
	{ -0.147621f, -0.716567f,  0.681718f}, { -0.425325f, -0.688191f,  0.587785f},
	{ -0.162460f, -0.262866f,  0.951056f}, {  0.442863f, -0.238856f,  0.864188f},
	{  0.162460f, -0.262866f,  0.951056f}, {  0.309017f, -0.500000f,  0.809017f},
	{  0.147621f, -0.716567f,  0.681718f}, {  0.000000f, -0.525731f,  0.850651f},
	{  0.425325f, -0.688191f,  0.587785f}, {  0.587785f, -0.425325f,  0.688191f},
	{  0.688191f, -0.587785f,  0.425325f}, { -0.955423f,  0.295242f,  0.000000f},
	{ -0.951056f,  0.162460f,  0.262866f}, { -1.000000f,  0.000000f,  0.000000f},
	{ -0.850651f,  0.000000f,  0.525731f}, { -0.955423f, -0.295242f,  0.000000f},
	{ -0.951056f, -0.162460f,  0.262866f}, { -0.864188f,  0.442863f, -0.238856f},
	{ -0.951056f,  0.162460f, -0.262866f}, { -0.809017f,  0.309017f, -0.500000f},
	{ -0.864188f, -0.442863f, -0.238856f}, { -0.951056f, -0.162460f, -0.262866f},
	{ -0.809017f, -0.309017f, -0.500000f}, { -0.681718f,  0.147621f, -0.716567f},
	{ -0.681718f, -0.147621f, -0.716567f}, { -0.850651f,  0.000000f, -0.525731f},
	{ -0.688191f,  0.587785f, -0.425325f}, { -0.587785f,  0.425325f, -0.688191f},
	{ -0.425325f,  0.688191f, -0.587785f}, { -0.425325f, -0.688191f, -0.587785f},
	{ -0.587785f, -0.425325f, -0.688191f}, { -0.688191f, -0.587785f, -0.425325f}
};

//==============================================================

int 	Q_rand( int *seed )
{
	*seed = (69069 * *seed + 1);
	return *seed;
}

/**
 * $(function)
 */
float	Q_random( int *seed )
{
	return (Q_rand( seed ) & 0xffff) / (float)0x10000;
}

/**
 * $(function)
 */
float	Q_crandom( int *seed )
{
	return 2.0 * (Q_random( seed ) - 0.5);
}

//=======================================================

signed char ClampChar( int i )
{
	if (i < -128)
	{
		return -128;
	}

	if (i > 127)
	{
		return 127;
	}
	return i;
}

/**
 * $(function)
 */
signed short ClampShort( int i )
{
	if (i < -32768)
	{
		return -32768;
	}

	if (i > 0x7fff)
	{
		return 0x7fff;
	}
	return i;
}

// this isn't a real cheap function to call!
int DirToByte( vec3_t dir )
{
	int    i, best;
	float  d, bestd;

	if (!dir)
	{
		return 0;
	}

	bestd = 0;
	best  = 0;

	for (i = 0 ; i < NUMVERTEXNORMALS ; i++)
	{
		d = DotProduct (dir, bytedirs[i]);

		if (d > bestd)
		{
			bestd = d;
			best  = i;
		}
	}

	return best;
}

/**
 * $(function)
 */
void ByteToDir( int b, vec3_t dir )
{
	if ((b < 0) || (b >= NUMVERTEXNORMALS))
	{
		VectorCopy( vec3_origin, dir );
		return;
	}
	VectorCopy (bytedirs[b], dir);
}

/**
 * $(function)
 */
unsigned ColorBytes3 (float r, float g, float b)
{
	unsigned  i;

	((byte *)&i)[0] = r * 255;
	((byte *)&i)[1] = g * 255;
	((byte *)&i)[2] = b * 255;

	return i;
}

/**
 * $(function)
 */
unsigned ColorBytes4 (float r, float g, float b, float a)
{
	unsigned  i;

	((byte *)&i)[0] = r * 255;
	((byte *)&i)[1] = g * 255;
	((byte *)&i)[2] = b * 255;
	((byte *)&i)[3] = a * 255;

	return i;
}

/**
 * $(function)
 */
float NormalizeColor( const vec3_t in, vec3_t out )
{
	float  max;

	max = in[0];

	if (in[1] > max)
	{
		max = in[1];
	}

	if (in[2] > max)
	{
		max = in[2];
	}

	if (!max)
	{
		VectorClear( out );
	}
	else
	{
		out[0] = in[0] / max;
		out[1] = in[1] / max;
		out[2] = in[2] / max;
	}
	return max;
}

/*
=====================
PlaneFromPoints

Returns false if the triangle is degenrate.
The normal will point out of the clock for clockwise ordered points
=====================
*/
qboolean PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c )
{
	vec3_t	d1, d2;
	float f;
	VectorSubtract( b, a, d1 );
	VectorSubtract( c, a, d2 );
	CrossProduct( d2, d1, plane );
	VectorNormalize( plane, f );
	if (f == 0)
	{
		return qfalse;
	}

	plane[3] = DotProduct( a, plane );
	return qtrue;
}

/*
===============
RotatePointAroundVector

This is not implemented very well...
===============
*/
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point,
				  float degrees )
{
	float	m[3][3];
	float	im[3][3];
	float	zrot[3][3];
	float	tmpmat[3][3];
	float	rot[3][3];
	//int i;
	vec3_t	vr, vup; //, vf;
	float	rad;

//	  vf[0] = dir[0];
//	  vf[1] = dir[1];
//	  vf[2] = dir[2];

	PerpendicularVector( vr, dir );
	CrossProduct( vr, dir, vup );

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = dir[0]; // was vf
	m[1][2] = dir[1];
	m[2][2] = dir[2];
/*
	memcpy( im, m, sizeof(im));
	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];
*/
	im[0][0] = m[0][0]; im[0][1] = m[1][0]; im[0][2] = m[2][0];
	im[1][0] = m[0][1]; im[1][1] = m[1][1]; im[1][2] = m[2][1];
	im[2][0] = m[0][2]; im[2][1] = m[1][2]; im[2][2] = m[2][2];


//	  memset( zrot, 0, sizeof(zrot));
	zrot[0][0]=1.0f;zrot[0][1]=0.0f;zrot[0][2]=0.0f;
	zrot[1][0]=0.0f;zrot[1][1]=1.0f;zrot[1][2]=0.0f;
	zrot[2][0]=0.0f;zrot[2][1]=0.0f;zrot[2][2]=1.0f;
//	  zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	rad    = DEG2RAD( degrees );
	zrot[0][0] = cos( rad );
	zrot[0][1] = sin( rad );
	zrot[1][0] = -sin( rad );
	zrot[1][1] = cos( rad );

	MatrixMultiply( m, zrot, tmpmat );
	MatrixMultiply( tmpmat, im, rot );

//	  for (i = 0; i < 3; i++) dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
	dst[0] = rot[0][0] * point[0] + rot[0][1] * point[1] + rot[0][2] * point[2];
	dst[1] = rot[1][0] * point[0] + rot[1][1] * point[1] + rot[1][2] * point[2];
	dst[2] = rot[2][0] * point[0] + rot[2][1] * point[1] + rot[2][2] * point[2];
}

/*
===============
RotateAroundDirection
===============
*/
void RotateAroundDirection( vec3_t axis[3], float yaw )
{
	// create an arbitrary axis[1]
	PerpendicularVector( axis[1], axis[0] );

	// rotate it around axis[0] by yaw
	if (yaw)
	{
		vec3_t	temp;

		VectorCopy( axis[1], temp );
		RotatePointAroundVector( axis[1], axis[0], temp, yaw );
	}

	// cross to get axis[2]
	CrossProduct( axis[0], axis[1], axis[2] );
}

/**
 * $(function)
 */
void RotateAroundDirection2( vec3_t axis[3], float yaw )
{
	vec3_t	temp;

	AngleVectorsU ( axis[0], NULL, NULL, axis[1] );

	VectorCopy( axis[1], temp );
	RotatePointAroundVector( axis[1], axis[0], temp, 90 );
	CrossProduct( axis[0], axis[1], axis[2] );

	VectorCopy( axis[0], temp );
	RotatePointAroundVector( axis[0], axis[1], temp, yaw );

	CrossProduct( axis[0], axis[1], axis[2] );
}

/**
 * $(function)
 */
void vectoangles( const vec3_t value1, vec3_t angles )
{
	float  forward;
	float  yaw, pitch;

	if ((value1[1] == 0) && (value1[0] == 0))
	{
		yaw = 0;

		if (value1[2] > 0)
		{
			pitch = 90;
		}
		else
		{
			pitch = 270;
		}
	}
	else
	{
		if (value1[0])
		{
			yaw = (atan2 ( value1[1], value1[0] ) * 180 / M_PI);
		}
		else if (value1[1] > 0)
		{
			yaw = 90;
		}
		else
		{
			yaw = 270;
		}

		if (yaw < 0)
		{
			yaw += 360;
		}

		forward = sqrt ( value1[0] * value1[0] + value1[1] * value1[1] );
		pitch	= (atan2(value1[2], forward) * 180 / M_PI);

		if (pitch < 0)
		{
			pitch += 360;
		}
	}

	angles[PITCH] = -pitch;
	angles[YAW]   = yaw;
	angles[ROLL]  = 0;
}

/*
=================
AnglesToAxis
=================
*/
/*
void AnglesToAxis( const vec3_t angles, vec3_t axis[3] )
{
	vec3_t	right;

	// angle vectors returns "right" instead of "y axis"
	AngleVectors( angles, axis[0], right, axis[2] );
	VectorSubtract( vec3_origin, right, axis[1] );
}
*/
/**
 * $(function)
 */
void AxisClear( vec3_t axis[3] )
{
	axis[0][0] = 1;
	axis[0][1] = 0;
	axis[0][2] = 0;
	axis[1][0] = 0;
	axis[1][1] = 1;
	axis[1][2] = 0;
	axis[2][0] = 0;
	axis[2][1] = 0;
	axis[2][2] = 1;
}

/**
 * $(function)
 */
void AxisCopy( vec3_t in[3], vec3_t out[3] )
{
	VectorCopy( in[0], out[0] );
	VectorCopy( in[1], out[1] );
	VectorCopy( in[2], out[2] );
}

/**
 * $(function)
 */
#if 0
//inlined
void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal )
{
	float	d;
	vec3_t	n;
	float	inv_denom;

	inv_denom = DotProduct( normal, normal );
	inv_denom = 1.0f / inv_denom;

	d	  = DotProduct( normal, p ) * inv_denom;

	n[0]	  = normal[0] * inv_denom;
	n[1]	  = normal[1] * inv_denom;
	n[2]	  = normal[2] * inv_denom;

	dst[0]	  = p[0] - d * n[0];
	dst[1]	  = p[1] - d * n[1];
	dst[2]	  = p[2] - d * n[2];
}
#endif
/*
================
MakeNormalVectors

Given a normalized forward vector, create two
other perpendicular vectors
================
*/
void MakeNormalVectors( const vec3_t forward, vec3_t right, vec3_t up)
{
	float  d;

	// this rotate and negate guarantees a vector
	// not colinear with the original
	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];

	d	 = DotProduct (right, forward);
	VectorMA (right, -d, forward, right);
	VectorNormalizeNR (right);
	CrossProduct (right, forward, up);
}

/**
 * $(function)
 */
void VectorRotate( vec3_t in, vec3_t matrix[3], vec3_t out )
{
	out[0] = DotProduct( in, matrix[0] );
	out[1] = DotProduct( in, matrix[1] );
	out[2] = DotProduct( in, matrix[2] );
}

//============================================================================

/*
** float q_rsqrt( float number )
*/
float Q_rsqrt( float number )
{
	long		 i;
	float		 x2, y;
	const float  threehalfs = 1.5F;

	x2 = number * 0.5F;
	y  = number;
	i  = *(long *)&y;						// evil floating point bit level hacking
	i  = 0x5f3759df - (i >> 1); 				// what the fuck?
	y  = *(float *)&i;
	y  = y * (threehalfs - (x2 * y * y));		// 1st iteration
//	y  = y * ( threehalfs - ( x2 * y * y ) );	// 2nd iteration, this can be removed

	return y;
}

/**
 * $(function)
 */
float Q_fabs( float f )
{
	int  tmp = *(int *)&f;

	tmp &= 0x7FFFFFFF;
	return *(float *)&tmp;
}

//============================================================

/*
===============
LerpAngle

===============
*/
float LerpAngle (float from, float to, float frac)
{
	float  a;

	if (to - from > 180)
	{
		to -= 360;
	}

	if (to - from < -180)
	{
		to += 360;
	}
	a = from + frac * (to - from);

	return a;
}

/*
===============
LerpVector
===============
*/
void LerpVector ( vec3_t from, vec3_t to, float lerp, vec3_t out )
{
	out[0] = from[0] + (to[0] - from[0]) * lerp;
	out[1] = from[1] + (to[1] - from[1]) * lerp;
	out[2] = from[2] + (to[2] - from[2]) * lerp;
}

/*
=================
AngleSubtract

Always returns a value from -180 to 180
=================
*/
/*
float	AngleSubtract( float a1, float a2 )
{
	float  a;

	a = a1 - a2;

	while (a > 180)
	{
		a -= 360;
	}

	while (a < -180)
	{
		a += 360;
	}
	return a;
}
*/
/**
 * $(function)
 */
/*
void AnglesSubtract( vec3_t v1, vec3_t v2, vec3_t v3 )
{
	v3[0] = AngleSubtract( v1[0], v2[0] );
	v3[1] = AngleSubtract( v1[1], v2[1] );
	v3[2] = AngleSubtract( v1[2], v2[2] );
}
*/
/* NOW A MACRO
float	AngleMod(float a) {
	a = (360.0/65536) * ((int)(a*(65536/360.0)) & 65535);
	return a;
}
*/

/*
=================
AngleNormalize360

returns angle normalized to the range [0 <= angle < 360]
=================
*/
float AngleNormalize360 ( float angle )
{
	return (360.0 / 65536) * ((int)(angle * (65536 / 360.0)) & 65535);
}

/*
=================
AngleNormalize180

returns angle normalized to the range [-180 < angle <= 180]
=================
*/
float AngleNormalize180 ( float angle )
{
	angle = AngleNormalize360( angle );

	if (angle > 180.0)
	{
		angle -= 360.0;
	}
	return angle;
}

/*
=================
AngleDelta

returns the normalized delta from angle1 to angle2
=================
*/
float AngleDelta ( float angle1, float angle2 )
{
	return AngleNormalize180( angle1 - angle2 );
}

//============================================================

/*
=================
SetPlaneSignbits
=================
*/
void SetPlaneSignbits (cplane_t *out)
{
	int  bits, j;

	// for fast box on planeside test
	bits = 0;

	for (j = 0 ; j < 3 ; j++)
	{
		if (out->normal[j] < 0)
		{
			bits |= 1 << j;
		}
	}
	out->signbits = bits;
}

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2

// this is the slow, general version
int BoxOnPlaneSide2 (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	int 	i;
	float	dist1, dist2;
	int 	sides;
	vec3_t	corners[2];

	for (i=0 ; i<3 ; i++)
	{
		if (p->normal[i] < 0)
		{
			corners[0][i] = emins[i];
			corners[1][i] = emaxs[i];
		}
		else
		{
			corners[1][i] = emins[i];
			corners[0][i] = emaxs[i];
		}
	}
	dist1 = DotProduct (p->normal, corners[0]) - p->dist;
	dist2 = DotProduct (p->normal, corners[1]) - p->dist;
	sides = 0;
	if (dist1 >= 0)
		sides = 1;
	if (dist2 < 0)
		sides |= 2;

	return sides;
}

==================
*/
#if !(defined __linux__ && defined __i386__ && !defined C_ONLY)
 #if defined __LCC__ || defined C_ONLY || !id386

/**
 * $(function)
 */
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	float  dist1, dist2;
	int    sides;

// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
		{
			return 1;
		}

		if (p->dist >= emaxs[p->type])
		{
			return 2;
		}
		return 3;
	}

// general case
	switch (p->signbits)
	{
		case 0:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			break;

		case 1:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			break;

		case 2:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			break;

		case 3:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			break;

		case 4:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			break;

		case 5:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			break;

		case 6:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			break;

		case 7:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			break;

		default:
			dist1 = dist2 = 0;	// shut up compiler
			break;
	}

	sides = 0;

	if (dist1 >= p->dist)
	{
		sides = 1;
	}

	if (dist2 < p->dist)
	{
		sides |= 2;
	}

	return sides;
}
 #else
  #pragma warning( disable: 4035 )

__declspec( naked/**
 * $(function)
 */
		) int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	static int	bops_initialized;
	static int	Ljmptab[8];

	__asm {
		push ebx

		cmp  bops_initialized, 1
		je initialized
		mov bops_initialized, 1

		mov Ljmptab[0 * 4], offset Lcase0
		mov Ljmptab[1 * 4], offset Lcase1
		mov Ljmptab[2 * 4], offset Lcase2
		mov Ljmptab[3 * 4], offset Lcase3
		mov Ljmptab[4 * 4], offset Lcase4
		mov Ljmptab[5 * 4], offset Lcase5
		mov Ljmptab[6 * 4], offset Lcase6
		mov Ljmptab[7 * 4], offset Lcase7

initialized:

		mov edx, dword ptr[4 + 12 + esp]
		mov ecx, dword ptr[4 + 4 + esp]
		xor eax, eax
		mov ebx, dword ptr[4 + 8 + esp]
		mov al, byte ptr[17 + edx]
		cmp al, 8
		jge Lerror
		fld dword ptr[0 + edx]
		fld st(0)
		jmp dword ptr[Ljmptab + eax * 4]
		Lcase0 :
			fmul dword ptr[ebx]
			fld dword ptr[0 + 4 + edx]
			fxch st(2)
			fmul dword ptr[ecx]
			fxch st(2)
			fld st(0)
			fmul dword ptr[4 + ebx]
			fld dword ptr[0 + 8 + edx]
			fxch st(2)
			fmul dword ptr[4 + ecx]
			fxch st(2)
			fld st(0)
			fmul dword ptr[8 + ebx]
			fxch st(5)
			faddp st(3), st(0)
			fmul dword ptr[8 + ecx]
			fxch st(1)
			faddp st(3), st(0)
			fxch st(3)
			faddp st(2), st(0)
			jmp LSetSides
			Lcase1 :
				fmul dword ptr[ecx]
				fld dword ptr[0 + 4 + edx]
				fxch st(2)
				fmul dword ptr[ebx]
				fxch st(2)
				fld st(0)
				fmul dword ptr[4 + ebx]
				fld dword ptr[0 + 8 + edx]
				fxch st(2)
				fmul dword ptr[4 + ecx]
				fxch st(2)
				fld st(0)
				fmul dword ptr[8 + ebx]
				fxch st(5)
				faddp st(3), st(0)
				fmul dword ptr[8 + ecx]
				fxch st(1)
				faddp st(3), st(0)
				fxch st(3)
				faddp st(2), st(0)
				jmp LSetSides
				Lcase2 :
					fmul dword ptr[ebx]
					fld dword ptr[0 + 4 + edx]
					fxch st(2)
					fmul dword ptr[ecx]
					fxch st(2)
					fld st(0)
					fmul dword ptr[4 + ecx]
					fld dword ptr[0 + 8 + edx]
					fxch st(2)
					fmul dword ptr[4 + ebx]
					fxch st(2)
					fld st(0)
					fmul dword ptr[8 + ebx]
					fxch st(5)
					faddp st(3), st(0)
					fmul dword ptr[8 + ecx]
					fxch st(1)
					faddp st(3), st(0)
					fxch st(3)
					faddp st(2), st(0)
					jmp LSetSides
					Lcase3 :
						fmul dword ptr[ecx]
						fld dword ptr[0 + 4 + edx]
						fxch st(2)
						fmul dword ptr[ebx]
						fxch st(2)
						fld st(0)
						fmul dword ptr[4 + ecx]
						fld dword ptr[0 + 8 + edx]
						fxch st(2)
						fmul dword ptr[4 + ebx]
						fxch st(2)
						fld st(0)
						fmul dword ptr[8 + ebx]
						fxch st(5)
						faddp st(3), st(0)
						fmul dword ptr[8 + ecx]
						fxch st(1)
						faddp st(3), st(0)
						fxch st(3)
						faddp st(2), st(0)
						jmp LSetSides
						Lcase4 :
							fmul dword ptr[ebx]
							fld dword ptr[0 + 4 + edx]
							fxch st(2)
							fmul dword ptr[ecx]
							fxch st(2)
							fld st(0)
							fmul dword ptr[4 + ebx]
							fld dword ptr[0 + 8 + edx]
							fxch st(2)
							fmul dword ptr[4 + ecx]
							fxch st(2)
							fld st(0)
							fmul dword ptr[8 + ecx]
							fxch st(5)
							faddp st(3), st(0)
							fmul dword ptr[8 + ebx]
							fxch st(1)
							faddp st(3), st(0)
							fxch st(3)
							faddp st(2), st(0)
							jmp LSetSides
							Lcase5 :
								fmul dword ptr[ecx]
								fld dword ptr[0 + 4 + edx]
								fxch st(2)
								fmul dword ptr[ebx]
								fxch st(2)
								fld st(0)
								fmul dword ptr[4 + ebx]
								fld dword ptr[0 + 8 + edx]
								fxch st(2)
								fmul dword ptr[4 + ecx]
								fxch st(2)
								fld st(0)
								fmul dword ptr[8 + ecx]
								fxch st(5)
								faddp st(3), st(0)
								fmul dword ptr[8 + ebx]
								fxch st(1)
								faddp st(3), st(0)
								fxch st(3)
								faddp st(2), st(0)
								jmp LSetSides
								Lcase6 :
									fmul dword ptr[ebx]
									fld dword ptr[0 + 4 + edx]
									fxch st(2)
									fmul dword ptr[ecx]
									fxch st(2)
									fld st(0)
									fmul dword ptr[4 + ecx]
									fld dword ptr[0 + 8 + edx]
									fxch st(2)
									fmul dword ptr[4 + ebx]
									fxch st(2)
									fld st(0)
									fmul dword ptr[8 + ecx]
									fxch st(5)
									faddp st(3), st(0)
									fmul dword ptr[8 + ebx]
									fxch st(1)
									faddp st(3), st(0)
									fxch st(3)
									faddp st(2), st(0)
									jmp LSetSides
									Lcase7 :
										fmul dword ptr[ecx]
										fld dword ptr[0 + 4 + edx]
										fxch st(2)
										fmul dword ptr[ebx]
										fxch st(2)
										fld st(0)
										fmul dword ptr[4 + ecx]
										fld dword ptr[0 + 8 + edx]
										fxch st(2)
										fmul dword ptr[4 + ebx]
										fxch st(2)
										fld st(0)
										fmul dword ptr[8 + ecx]
										fxch st(5)
										faddp st(3), st(0)
										fmul dword ptr[8 + ebx]
										fxch st(1)
										faddp st(3), st(0)
										fxch st(3)
										faddp st(2), st(0)
										LSetSides :
											faddp st(2), st(0)
											fcomp dword ptr[12 + edx]
											xor ecx, ecx
											fnstsw ax
											fcomp dword ptr[12 + edx]
											and ah, 1
											xor ah, 1
											add cl, ah
											fnstsw ax
											and ah, 1
											add ah, ah
											add cl, ah
											pop ebx
											mov eax, ecx
											ret
											Lerror :
												int 3
	}
}
  #pragma warning( default: 4035 )

 #endif
#endif

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds( const vec3_t mins, const vec3_t maxs )
{
	int i;
	vec3_t	corner;
	float	a, b;

	for (i = 0 ; i < 3 ; i++)
	{
		a	  = fabs( mins[i] );
		b	  = fabs( maxs[i] );
		corner[i] = a > b ? a : b;
	}

	return VectorLength (corner);
}

/**
 * $(function)
 */
void ClearBounds( vec3_t mins, vec3_t maxs )
{
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}

/**
 * $(function)
 */
void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs )
{
	if (v[0] < mins[0])
	{
		mins[0] = v[0];
	}

	if (v[0] > maxs[0])
	{
		maxs[0] = v[0];
	}

	if (v[1] < mins[1])
	{
		mins[1] = v[1];
	}

	if (v[1] > maxs[1])
	{
		maxs[1] = v[1];
	}

	if (v[2] < mins[2])
	{
		mins[2] = v[2];
	}

	if (v[2] > maxs[2])
	{
		maxs[2] = v[2];
	}
}

/**
 * $(function)
 */
int VectorCompare( const vec3_t v1, const vec3_t v2 )
{
	if ((v1[0] != v2[0]) || (v1[1] != v2[1]) || (v1[2] != v2[2]))
	{
		return 0;
	}

	return 1;
}

/**
 * $(function)
 */
/*
vec_t VectorNormalize( vec3_t v )
{
	float  length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrt (length);

	if (length)
	{
		ilength = 1 / length;
		v[0]   *= ilength;
		v[1]   *= ilength;
		v[2]   *= ilength;
	}

	return length;
}
*/
//
// fast vector normalize routine that does not check to make sure
// that length != 0, nor does it return length
//
void VectorNormalizeFast( vec3_t v )
{
	float  ilength;

	ilength = Q_rsqrt( DotProduct( v, v ));

	v[0]   *= ilength;
	v[1]   *= ilength;
	v[2]   *= ilength;
}

/**
 * $(function)
 */

vec_t VectorNormalize2( const vec3_t v, vec3_t out)
{
	float  length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrt (length);

	if (length)
	{
		ilength = 1 / length;
		out[0]	= v[0] * ilength;
		out[1]	= v[1] * ilength;
		out[2]	= v[2] * ilength;
	}
	else
	{
		VectorClear( out );
	}

	return length;
}

/**
 * $(function)
 */
void _VectorMA( const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc)
{
	vecc[0] = veca[0] + scale * vecb[0];
	vecc[1] = veca[1] + scale * vecb[1];
	vecc[2] = veca[2] + scale * vecb[2];
}

/**
 * $(function)
 */
vec_t _DotProduct( const vec3_t v1, const vec3_t v2 )
{
	return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

/**
 * $(function)
 */
void _VectorSubtract( const vec3_t veca, const vec3_t vecb, vec3_t out )
{
	out[0] = veca[0] - vecb[0];
	out[1] = veca[1] - vecb[1];
	out[2] = veca[2] - vecb[2];
}

/**
 * $(function)
 */
void _VectorAdd( const vec3_t veca, const vec3_t vecb, vec3_t out )
{
	out[0] = veca[0] + vecb[0];
	out[1] = veca[1] + vecb[1];
	out[2] = veca[2] + vecb[2];
}

/**
 * $(function)
 */
void _VectorCopy( const vec3_t in, vec3_t out )
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

/**
 * $(function)
 */
void _VectorScale( const vec3_t in, vec_t scale, vec3_t out )
{
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
}

/**
 * $(function)
 */
/*
void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross )
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}
*/
/**
 * $(function)
 */
/*
vec_t VectorLength( const vec3_t v )
{
	return sqrt (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}
*/
/**
 * $(function)
 */
vec_t VectorLengthSquared( const vec3_t v )
{
	return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

/**
 * $(function)
 */
vec_t Distance( const vec3_t p1, const vec3_t p2 )
{
	vec3_t	v;

	VectorSubtract (p2, p1, v);
	return VectorLength( v );
}

/**
 * $(function)
 */
vec_t DistanceSquared( const vec3_t p1, const vec3_t p2 )
{
	vec3_t	v;

	VectorSubtract (p2, p1, v);
	return VectorLengthSquared( v );
//	return v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
}

/**
 * $(function)
 */
void VectorInverse( vec3_t v )
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

/**
 * $(function)
 */
void Vector4Scale( const vec4_t in, vec_t scale, vec4_t out )
{
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
	out[3] = in[3] * scale;
}

/**
 * $(function)
 */
int Q_log2( int val )
{
	int  answer;

	answer = 0;

	while ((val >>= 1) != 0)
	{
		answer++;
	}
	return answer;
}

/*
=================
PlaneTypeForNormal
=================
*/
/*
int PlaneTypeForNormal (vec3_t normal) {
	if ( normal[0] == 1.0 )
		return PLANE_X;
	if ( normal[1] == 1.0 )
		return PLANE_Y;
	if ( normal[2] == 1.0 )
		return PLANE_Z;

	return PLANE_NON_AXIAL;
}
*/

/*
================
MatrixMultiply
================
*/
/*
void MatrixMultiply(float in1[3][3], float in2[3][3], float out[3][3])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
			in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
			in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
			in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
			in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
			in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
			in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
			in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
			in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
			in1[2][2] * in2[2][2];
}
*/

/**
 * $(function)
 */
/*
void MatrixMultiply4(float in1[4][4], float in2[4][4], float out[4][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
			in1[0][2] * in2[2][0] + in1[0][3] * in2[3][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
			in1[0][2] * in2[2][1] + in1[0][3] * in2[3][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
			in1[0][2] * in2[2][2] + in1[0][3] * in2[3][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
			in1[0][2] * in2[2][3] + in1[0][3] * in2[3][3];

	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
			in1[1][2] * in2[2][0] + in1[1][3] * in2[3][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
			in1[1][2] * in2[2][1] + in1[1][3] * in2[3][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
			in1[1][2] * in2[2][2] + in1[1][3] * in2[3][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
			in1[1][2] * in2[2][3] + in1[1][3] * in2[3][3];

	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
			in1[2][2] * in2[2][0] + in1[2][3] * in2[3][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
			in1[2][2] * in2[2][1] + in1[2][3] * in2[3][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
			in1[2][2] * in2[2][2] + in1[2][3] * in2[3][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
			in1[2][2] * in2[2][3] + in1[2][3] * in2[3][3];

	out[3][0] = in1[2][0] * in2[0][0] + in1[3][1] * in2[1][0] +
			in1[2][2] * in2[2][0] + in1[3][3] * in2[3][0];
	out[3][1] = in1[2][0] * in2[0][1] + in1[3][1] * in2[1][1] +
			in1[2][2] * in2[2][1] + in1[3][3] * in2[3][1];
	out[3][2] = in1[2][0] * in2[0][2] + in1[3][1] * in2[1][2] +
			in1[2][2] * in2[2][2] + in1[3][3] * in2[3][2];
	out[3][3] = in1[2][0] * in2[0][3] + in1[3][1] * in2[1][3] +
			in1[2][2] * in2[2][3] + in1[3][3] * in2[3][3];
}
*/
/**
 * MatrixRotate4
 */
void MatrixRotate4(float in1[4][4], float in2[3][3], float out[4][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
			in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
			in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
			in1[0][2] * in2[2][2];
	out[0][3] = 0.0f;

	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
			in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
			in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
			in1[1][2] * in2[2][2];
	out[1][3] = 0.0f;

	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
			in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
			in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
			in1[2][2] * in2[2][2];
	out[2][3] = 0.0f;

	out[3][0] = 0.0f;
	out[3][1] = 0.0f;
	out[3][2] = 0.0f;
	out[3][3] = 1.0f;

}

/**
 * MatrixConvert34
 */
/*
void MatrixConvert34(float in[3][3], float out[4][4])
{
	int x, y;
	memset(out, 0, sizeof(float) * 16);
	for (x = 0; x < 3; x++) {
		for (y = 0; y < 3; y++) {
			out[x][y] = in[x][y];
		}
	}
	out[3][3] = 1.0f;
}
*/
/**
 * $(function)
 */
/*
void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float		  angle;
	static float  sr, sp, sy, cr, cp, cy;

	// static to help MS compiler fp bugs

	angle = angles[YAW] * (M_PI * 2 / 360);
	sy	  = sin(angle);
	cy	  = cos(angle);
	angle = angles[PITCH] * (M_PI * 2 / 360);
	sp	  = sin(angle);
	cp	  = cos(angle);
	angle = angles[ROLL] * (M_PI * 2 / 360);
	sr	  = sin(angle);
	cr	  = cos(angle);

	if (forward)
	{
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}

	if (right)
	{
		right[0] = (-1 * sr * sp * cy + - 1 * cr * -sy);
		right[1] = (-1 * sr * sp * sy + - 1 * cr * cy);
		right[2] = -1 * sr * cp;
	}

	if (up)
	{
		up[0] = (cr * sp * cy + - sr * -sy);
		up[1] = (cr * sp * sy + - sr * cy);
		up[2] = cr * cp;
	}
}
*/
/*
** assumes "src" is normalized
*/
#if 0
//inlined
void PerpendicularVector( vec3_t dst, const vec3_t src )
{
	int pos;
	int i;
	float	minelem = 1.0F;
	vec3_t	tempvec;

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for (pos = 0, i = 0; i < 3; i++)
	{
		if (fabs( src[i] ) < minelem)
		{
			pos = i;
			minelem = fabs( src[i] );
		}
	}
	tempvec[0]	 = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	/*
	** project the point onto the plane defined by src
	*/
	ProjectPointOnPlane( dst, tempvec, src );

	/*
	** normalize the result
	*/
	VectorNormalizeNR( dst );
}
#endif

#define EPSILON 0.000001

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : utTestIntersection
// Description: Tests the intersection of a triangle with a ray
// Author	  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
int utTestIntersection ( vec3_t vert[3], vec3_t orig, vec3_t dir, float *u, float *v, float *t )
{
	vec3_t	edge[2];
	vec3_t	pvec;
	vec3_t	tvec;
	vec3_t	qvec;

	float	det;
	float	inv_det;

	// STEP 1:	Determine if the ray is on the same plane as the triangle.
	//		This is done by calculating a perpendicualr vector to the
	//		ray and one of the edges.  If that vector is at a 90 degree
	//		angle to the other edge as well then the ray lies on the same
	//		plane as the triangle.	An epsilon value is used to handle
	//		the infinity cases.

	// Calculate two of the triangles edges
	VectorSubtract ( vert[1], vert[0], edge[0]);
	VectorSubtract ( vert[2], vert[0], edge[1]);

	// begin calculating determinant - also used to calculate U parameter
	CrossProduct ( dir, edge[1], pvec );

	// if determinant is near zero, ray lies in plane of triangle
	det = DotProduct ( edge[0], pvec );

	if ((det > -EPSILON) && (det < EPSILON))
	{
		return 0;
	}

	inv_det = 1.0 / det;

	// calculate distance from vert0 to ray origin
	VectorSubtract ( orig, vert[0], tvec );

	// calculate U parameter and test bounds
	*u = DotProduct(tvec, pvec) * inv_det;

	if ((*u < 0.0) || (*u > 1.0))
	{
		return 0;
	}

	// prepare to test V parameter
	CrossProduct ( tvec, edge[0], qvec );

	// calculate V parameter and test bounds
	*v = DotProduct ( dir, qvec ) * inv_det;

	if ((*v < 0.0) || (*u + *v > 1.0))
	{
		return 0;
	}

	// calculate t, ray intersects triangle
	*t = DotProduct (edge[1], qvec ) * inv_det;

	return 1;
}

//--------------------------------------------------------------------------------
// Extra Math code by DensitY
//--------------------------------------------------------------------------------

/*
	This breaks then please tell me!

	I think the proper method is to use a dotp between the point vector and the end points

*/

#define SQR( n ) (n * n)

/*=---- Get distance from a 3d line segment ----=*/

float utSquaredDistance3dSegment( vec3_t SegPoint1, vec3_t SegPoint2, vec3_t Origin )
{
	float  Distance = 0;

	return Distance;
}

/**
 * $(function)
 */
void M4Mul(float mat[4][4], float m[4][4], float out[4][4])
{
	int  i;
	int  j;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			out[i][j] = (mat[i][0] * m[0][j]) + (mat[i][1] * m[1][j]) + (mat[i][2] * m[2][j]) + (mat[i][3] * m[3][j]);
		}
	}
}

/**
 * $(function)
 */
void M4Trans(float mat[4][4], vec3_t v, vec3_t out)
{
	out[0] = (mat[0][0] * v[0]) + (mat[1][0] * v[1]) + (mat[2][0] * v[2]) + mat[3][0];
	out[1] = (mat[0][1] * v[0]) + (mat[1][1] * v[1]) + (mat[2][1] * v[2]) + mat[3][1];
	out[2] = (mat[0][2] * v[0]) + (mat[1][2] * v[1]) + (mat[2][2] * v[2]) + mat[3][2];
}

/**
 * $(function)
 */
void m4_submat( float mr[16], float mb[9], int i, int j )
{
	int  ti, tj, idst = 0, jdst = 0;

	for (ti = 0; ti < 4; ti++)
	{
		if (ti < i)
		{
			idst = ti;
		}
		else
		if (ti > i)
		{
			idst = ti - 1;
		}

		for (tj = 0; tj < 4; tj++)
		{
			if (tj < j)
			{
				jdst = tj;
			}
			else
			if (tj > j)
			{
				jdst = tj - 1;
			}

			if ((ti != i) && (tj != j))
			{
				mb[idst * 3 + jdst] = mr[ti * 4 + tj];
			}
		}
	}
}

/**
 * $(function)
 */
float m4_det( float mr[16] )
{
	float  det, result = 0, i = 1;
	float  msub3[9];
	int    n;

	for (n = 0; n < 4; n++, i *= -1)
	{
		m4_submat( mr, msub3, 0, n );

		det = m3_det( msub3 );
		result += mr[n] * det * i;
	}

	return result ;
}

/**
 * $(function)
 */
float m3_det( float mat[9] )
{
	float  det;

	det = mat[0] * (mat[4] * mat[8] - mat[7] * mat[5])
		  - mat[1] * (mat[3] * mat[8] - mat[6] * mat[5])
		  + mat[2] * (mat[3] * mat[7] - mat[6] * mat[4]);

	return det ;
}

/**
 * $(function)
 */
int M4Inv(	float ma[16], float mr[16] )
{
	float  mdet = m4_det( ma );
	float  mtemp[16];
	int    i, j, sign;

	if (fabs( mdet ) < 0.0005)
	{
		return 0 ;
	}

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			sign = 1 - ((i + j) % 2) * 2;

			m4_submat( ma, mtemp, i, j );

			mr[i + j * 4] = (m3_det( mtemp ) * sign) / mdet;
		}
	}

	return 1 ;
}
