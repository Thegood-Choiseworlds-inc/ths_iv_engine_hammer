//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SSE Math primitives.
//
//=====================================================================================//

#include <cfloat>	// Needed for FLT_EPSILON
#include "basetypes.h"
#include "tier0/dbg.h"
#include "mathlib/mathlib.h"
#include "mathlib/ssemath.h"
#include "mathlib/vector.h"
#include "mathlib/vector4d.h"
#include "sse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef COMPILER_MSVC64
// Implement for 64-bit Windows if needed.

static const uint32 _sincos_masks[]	  = { (uint32)0x0,  (uint32)~0x0 };
static const uint32 _sincos_inv_masks[] = { (uint32)~0x0, (uint32)0x0 };

//-----------------------------------------------------------------------------
// Macros and constants required by some of the SSE assembly:
//-----------------------------------------------------------------------------

#ifdef _WIN32
	#define _PS_EXTERN_CONST(Name, Val) \
		const __declspec(align(16)) float _ps_##Name[4] = { Val, Val, Val, Val }

	#define _PS_EXTERN_CONST_TYPE(Name, Type, Val) \
		const __declspec(align(16)) Type _ps_##Name[4] = { Val, Val, Val, Val }; \

	#define _EPI32_CONST(Name, Val) \
		static const __declspec(align(16)) __int32 _epi32_##Name[4] = { Val, Val, Val, Val }

	#define _PS_CONST(Name, Val) \
		static const __declspec(align(16)) float _ps_##Name[4] = { Val, Val, Val, Val }
#elif POSIX
	#define _PS_EXTERN_CONST(Name, Val) \
		const float _ps_##Name[4] __attribute__((aligned(16))) = { Val, Val, Val, Val }

	#define _PS_EXTERN_CONST_TYPE(Name, Type, Val) \
		const Type _ps_##Name[4]  __attribute__((aligned(16))) = { Val, Val, Val, Val }; \

	#define _EPI32_CONST(Name, Val) \
		static const int32 _epi32_##Name[4]  __attribute__((aligned(16))) = { Val, Val, Val, Val }

	#define _PS_CONST(Name, Val) \
		static const float _ps_##Name[4]  __attribute__((aligned(16))) = { Val, Val, Val, Val }
#endif

_PS_EXTERN_CONST( am_0, 0.0f );
_PS_EXTERN_CONST( am_1, 1.0f );
_PS_EXTERN_CONST( am_m1, -1.0f );
_PS_EXTERN_CONST( am_0p5, 0.5f );
_PS_EXTERN_CONST( am_1p5, 1.5f );
_PS_EXTERN_CONST( am_pi, (float)M_PI );
_PS_EXTERN_CONST( am_pi_o_2, (float)( M_PI / 2.0 ) );
_PS_EXTERN_CONST( am_2_o_pi, (float)( 2.0 / M_PI ) );
_PS_EXTERN_CONST( am_pi_o_4, (float)( M_PI / 4.0 ) );
_PS_EXTERN_CONST( am_4_o_pi, (float)( 4.0 / M_PI ) );
_PS_EXTERN_CONST_TYPE( am_sign_mask, uint32, 0x80000000 );
_PS_EXTERN_CONST_TYPE( am_inv_sign_mask, uint32, ~0x80000000 );
_PS_EXTERN_CONST_TYPE( am_min_norm_pos, int32, 0x00800000 );
_PS_EXTERN_CONST_TYPE( am_mant_mask, int32, 0x7f800000 );
_PS_EXTERN_CONST_TYPE( am_inv_mant_mask, int32, ~0x7f800000 );

_EPI32_CONST( 1, 1 );
_EPI32_CONST( 2, 2 );
_EPI32_CONST( 4, 4 );
_EPI32_CONST( inv1, ~1 );

_PS_CONST( sincos_p0, 0.15707963267948963959e1f );
_PS_CONST( sincos_p1, -0.64596409750621907082e0f );
_PS_CONST( sincos_p2, 0.7969262624561800806e-1f );
_PS_CONST( sincos_p3, -0.468175413106023168e-2f );

_PS_CONST( minus_cephes_DP1, -0.78515625 );
_PS_CONST( minus_cephes_DP2, -2.4187564849853515625e-4 );
_PS_CONST( minus_cephes_DP3, -3.77489497744594108e-8 );
_PS_CONST( sincof_p0, -1.9515295891E-4 );
_PS_CONST( sincof_p1, 8.3321608736E-3 );
_PS_CONST( sincof_p2, -1.6666654611E-1 );
_PS_CONST( coscof_p0, 2.443315711809948E-005 );
_PS_CONST( coscof_p1, -1.388731625493765E-003 );
_PS_CONST( coscof_p2, 4.166664568298827E-002 );

union xmm_mm_union
{
	fltx4 xmm;
	__m64 mm[2];
};

#define COPY_XMM_TO_MM( xmm_, mm0_, mm1_ ) {	\
	xmm_mm_union u; u.xmm = xmm_;				\
	mm0_ = u.mm[0];								\
	mm1_ = u.mm[1];								\
}

#define COPY_MM_TO_XMM( mm0_, mm1_, xmm_ ) {					\
	xmm_mm_union u; u.mm[0]=mm0_; u.mm[1]=mm1_; xmm_ = u.xmm;	\
}

template <bool sse2>
static FORCEINLINE void sincos_ps( fltx4 x, fltx4& s, fltx4& c )
{
	fltx4 xmm1, xmm2, xmm3 = _mm_setzero_ps();
	__m128i emm0, emm2, emm4;
	__m64 mm0, mm1, mm2, mm3, mm4, mm5;
	fltx4 sign_bit_sin = x;
	/* take the absolute value */
	x = _mm_and_ps( x, *reinterpret_cast<const fltx4*>( _ps_am_inv_sign_mask ) );
	/* extract the sign bit (upper one) */
	sign_bit_sin = _mm_and_ps( sign_bit_sin, *reinterpret_cast<const fltx4*>( _ps_am_sign_mask ) );

	/* scale by 4/Pi */
	fltx4 y = _mm_mul_ps( x, *reinterpret_cast<const fltx4*>( _ps_am_4_o_pi ) );

	fltx4 swap_sign_bit_sin;
	fltx4 poly_mask;
	if constexpr ( sse2 )
	{
		/* store the integer part of y in emm2 */
		emm2 = _mm_cvttps_epi32( y );

		/* j=(j+1) & (~1) (see the cephes sources) */
		emm2 = _mm_add_epi32( emm2, *reinterpret_cast<const __m128i*>( _epi32_1 ) );
		emm2 = _mm_and_si128( emm2, *reinterpret_cast<const __m128i*>( _epi32_inv1 ) );
		y = _mm_cvtepi32_ps( emm2 );

		emm4 = emm2;

		/* get the swap sign flag for the sine */
		emm0 = _mm_and_si128( emm2, *reinterpret_cast<const __m128i*>( _epi32_4 ) );
		emm0 = _mm_slli_epi32( emm0, 29 );
		swap_sign_bit_sin = _mm_castsi128_ps( emm0 );

		/* get the polynom selection mask for the sine*/
		emm2 = _mm_and_si128( emm2, *reinterpret_cast<const __m128i*>( _epi32_2 ) );
		emm2 = _mm_cmpeq_epi32( emm2, _mm_setzero_si128() );
		poly_mask = _mm_castsi128_ps( emm2 );
	}
	else
	{
		/* store the integer part of y in mm2:mm3 */
		xmm3 = _mm_movehl_ps( xmm3, y );
		mm2 = _mm_cvttps_pi32( y );
		mm3 = _mm_cvttps_pi32( xmm3 );

		/* j=(j+1) & (~1) (see the cephes sources) */
		mm2 = _mm_add_pi32( mm2, *reinterpret_cast<const __m64*>( _epi32_1 ) );
		mm3 = _mm_add_pi32( mm3, *reinterpret_cast<const __m64*>( _epi32_1 ) );
		mm2 = _mm_and_si64( mm2, *reinterpret_cast<const __m64*>( _epi32_inv1 ) );
		mm3 = _mm_and_si64( mm3, *reinterpret_cast<const __m64*>( _epi32_inv1 ) );

		//y = _mm_cvtpi32x2_ps( mm2, mm3 );
		y = _mm_movelh_ps( _mm_cvt_pi2ps( _mm_setzero_ps(), mm2 ), _mm_cvt_pi2ps( _mm_setzero_ps(), mm3 ) );

		mm4 = mm2;
		mm5 = mm3;

		/* get the swap sign flag for the sine */
		mm0 = _mm_and_si64( mm2, *reinterpret_cast<const __m64*>( _epi32_4 ) );
		mm1 = _mm_and_si64( mm3, *reinterpret_cast<const __m64*>( _epi32_4 ) );
		mm0 = _mm_slli_pi32( mm0, 29 );
		mm1 = _mm_slli_pi32( mm1, 29 );
		COPY_MM_TO_XMM( mm0, mm1, swap_sign_bit_sin );

		/* get the polynom selection mask for the sine */

		mm2 = _mm_and_si64( mm2, *reinterpret_cast<const __m64*>( _epi32_2 ) );
		mm3 = _mm_and_si64( mm3, *reinterpret_cast<const __m64*>( _epi32_2 ) );
		mm2 = _mm_cmpeq_pi32( mm2, _mm_setzero_si64() );
		mm3 = _mm_cmpeq_pi32( mm3, _mm_setzero_si64() );
		COPY_MM_TO_XMM( mm2, mm3, poly_mask );
		_mm_empty(); /* good-bye mmx */
	}

	/* The magic pass: "Extended precision modular arithmetic"
	   x = ((x - y * DP1) - y * DP2) - y * DP3; */
	xmm1 = *reinterpret_cast<const fltx4*>( _ps_minus_cephes_DP1 );
	xmm2 = *reinterpret_cast<const fltx4*>( _ps_minus_cephes_DP2 );
	xmm3 = *reinterpret_cast<const fltx4*>( _ps_minus_cephes_DP3 );
	xmm1 = _mm_mul_ps( y, xmm1 );
	xmm2 = _mm_mul_ps( y, xmm2 );
	xmm3 = _mm_mul_ps( y, xmm3 );
	x = _mm_add_ps( x, xmm1 );
	x = _mm_add_ps( x, xmm2 );
	x = _mm_add_ps( x, xmm3 );

	fltx4 sign_bit_cos;
	if constexpr ( sse2 )
	{
		emm4 = _mm_sub_epi32( emm4, *reinterpret_cast<const __m128i*>( _epi32_2 ) );
		emm4 = _mm_andnot_si128( emm4, *reinterpret_cast<const __m128i*>( _epi32_4 ) );
		emm4 = _mm_slli_epi32( emm4, 29 );
		sign_bit_cos = _mm_castsi128_ps( emm4 );
	}
	else
	{
		/* get the sign flag for the cosine */
		mm4 = _mm_sub_pi32( mm4, *reinterpret_cast<const __m64*>( _epi32_2 ) );
		mm5 = _mm_sub_pi32( mm5, *reinterpret_cast<const __m64*>( _epi32_2 ) );
		mm4 = _mm_andnot_si64( mm4, *reinterpret_cast<const __m64*>( _epi32_4 ) );
		mm5 = _mm_andnot_si64( mm5, *reinterpret_cast<const __m64*>( _epi32_4 ) );
		mm4 = _mm_slli_pi32( mm4, 29 );
		mm5 = _mm_slli_pi32( mm5, 29 );
		COPY_MM_TO_XMM( mm4, mm5, sign_bit_cos );
		_mm_empty(); /* good-bye mmx */
	}

	sign_bit_sin = _mm_xor_ps( sign_bit_sin, swap_sign_bit_sin );

	/* Evaluate the first polynom  (0 <= x <= Pi/4) */
	fltx4 z = _mm_mul_ps( x, x );
	y = *reinterpret_cast<const fltx4*>( _ps_coscof_p0 );

	y = _mm_mul_ps( y, z );
	y = _mm_add_ps( y, *reinterpret_cast<const fltx4*>( _ps_coscof_p1 ) );
	y = _mm_mul_ps( y, z );
	y = _mm_add_ps( y, *reinterpret_cast<const fltx4*>( _ps_coscof_p2 ) );
	y = _mm_mul_ps( y, z );
	y = _mm_mul_ps( y, z );
	fltx4 tmp = _mm_mul_ps( z, *reinterpret_cast<const fltx4*>( _ps_am_0p5 ) );
	y = _mm_sub_ps( y, tmp );
	y = _mm_add_ps( y, *reinterpret_cast<const fltx4*>( _ps_am_1 ) );

	/* Evaluate the second polynom  (Pi/4 <= x <= 0) */

	fltx4 y2 = *reinterpret_cast<const fltx4*>( _ps_sincof_p0 );
	y2 = _mm_mul_ps( y2, z );
	y2 = _mm_add_ps( y2, *reinterpret_cast<const fltx4*>( _ps_sincof_p1 ) );
	y2 = _mm_mul_ps( y2, z );
	y2 = _mm_add_ps( y2, *reinterpret_cast<const fltx4*>( _ps_sincof_p2 ) );
	y2 = _mm_mul_ps( y2, z );
	y2 = _mm_mul_ps( y2, x );
	y2 = _mm_add_ps( y2, x );

	/* select the correct result from the two polynoms */
	xmm3 = poly_mask;
	fltx4 ysin2 = _mm_and_ps( xmm3, y2 );
	fltx4 ysin1 = _mm_andnot_ps( xmm3, y );
	y2 = _mm_sub_ps( y2, ysin2 );
	y = _mm_sub_ps( y, ysin1 );

	xmm1 = _mm_add_ps( ysin1, ysin2 );
	xmm2 = _mm_add_ps( y, y2 );

	/* update the sign */
	s = _mm_xor_ps( xmm1, sign_bit_sin );
	c = _mm_xor_ps( xmm2, sign_bit_cos );
}

//-----------------------------------------------------------------------------
// SSE implementations of optimized routines:
//-----------------------------------------------------------------------------
float _SSE_Sqrt( float x )
{
	Assert( s_bMathlibInitialized );
	return _mm_cvtss_f32( _mm_sqrt_ss( _mm_load_ss( &x ) ) );
}

static const fltx4 f1   = _mm_set_ss( 1.0f );
static const fltx4 f3   = _mm_set_ss(3.0f);  // 3 as SSE value
static const fltx4 f05  = _mm_set_ss(0.5f);  // 0.5 as SSE value
static const fltx4 f0   = _mm_set_ss( 0.0f );
static const fltx4 fac2 = _mm_set_ss( 2.0f );
static const fltx4 fac3 = _mm_set_ss( 6.0f );
static const fltx4 fac4 = _mm_set_ss( 24.0f );
static const fltx4 fac5 = _mm_set_ss( 120.0f );
static const fltx4 fac6 = _mm_set_ss( 720.0f );
static const fltx4 fac7 = _mm_set_ss( 5040.0f );
static const fltx4 fac8 = _mm_set_ss( 40320.0f );
static const fltx4 fac9 = _mm_set_ss( 362880.0f );

// Intel / Kipps SSE RSqrt.  Significantly faster than above.
float _SSE_RSqrtAccurate(float a)
{
	FLTX4 xx = _mm_load_ss( &a );
	fltx4 xr = _mm_rsqrt_ss( xx );

	fltx4 xt = _mm_mul_ss( xr, xr );
	xt = _mm_mul_ss( xt, xx );
	xt = _mm_sub_ss( f3, xt );
	xt = _mm_mul_ss( xt, f05 );
	xr = _mm_mul_ss( xr, xt );

	_mm_store_ss( &a, xr );
	return a;
}

// Simple SSE rsqrt.  Usually accurate to around 6 (relative) decimal places
// or so, so ok for closed transforms.  (ie, computing lighting normals)
float _SSE_RSqrtFast( float x )
{
	Assert( s_bMathlibInitialized );
	return _mm_cvtss_f32( _mm_rsqrt_ss( _mm_load_ss( &x ) ) );
}

float FASTCALL _SSE_VectorNormalize( Vector& vec )
{
	Assert( s_bMathlibInitialized );

	// NOTE: This is necessary to prevent an memory overwrite...
	// sice vec only has 3 floats, we can't "movaps" directly into it.
	alignas( 16 ) float result[4];

	// Blah, get rid of these comparisons ... in reality, if you have all 3 as zero, it shouldn't
	// be much of a performance win, considering you will very likely miss 3 branch predicts in a row.
	if ( vec.x || vec.y || vec.z )
	{
#ifdef ALIGNED_VECTOR
		FLTX4 _v = _mm_load_ps( &vec.x );
#else
		const Vector4D val( vec ); // prevents reading outside of vec
		FLTX4 _v = _mm_loadu_ps( &val.x );
#endif
		fltx4 res = _mm_mul_ps( _v, _v );
		FLTX4 v2 = _mm_shuffle_ps( res, res, 1 );
		FLTX4 v3 = _mm_shuffle_ps( res, res, 2 );
		res = _mm_sqrt_ps( _mm_add_ps( res, _mm_add_ps( v3, v2 ) ) );
		res = _mm_shuffle_ps( res, res, 0 );
		_mm_store_ps( result, _mm_div_ps( _v, res ) );
		vec.x = result[0];
		vec.y = result[1];
		vec.z = result[2];

		float radius;
		_mm_store_ss( &radius, res );
		return radius;
	}

	return 0.f;
}

void FASTCALL _SSE_VectorNormalizeFast (Vector& vec)
{
	const float ool = _SSE_RSqrtAccurate( FLT_EPSILON + vec.x * vec.x + vec.y * vec.y + vec.z * vec.z );

	vec.x *= ool;
	vec.y *= ool;
	vec.z *= ool;
}

float _SSE_InvRSquared(const float* v)
{
	float	inv_r2 = 1.f;
#ifdef ALIGNED_VECTOR
	FLTX4 _v = _mm_load_ps( v );
#else
	FLTX4 _v = _mm_loadu_ps( v );
#endif
	fltx4 res = _mm_mul_ps( _v, _v );
	FLTX4 x = res;
	FLTX4 y = _mm_shuffle_ps( res, res, 1 );
	FLTX4 z = _mm_shuffle_ps( res, res, 2 );

	res = _mm_add_ps( x, _mm_add_ps( y, z ) );

	res = _mm_rcp_ps( _mm_max_ps( f0, res ) );

	_mm_store_ss( &inv_r2, res );

	return inv_r2;
}

void _SSE_SinCos( float x, float* s, float* c )
{
	fltx4 si, co;
	sincos_ps<false>( _mm_load_ss( &x ), si, co );
	_mm_store_ss( s, si );
	_mm_store_ss( c, co );
}

float _SSE_cos( float x )
{
	fltx4 si, co;
	sincos_ps<false>( _mm_load_ss( &x ), si, co );
	_mm_store_ss( &x, co );

	return x;
}

//-----------------------------------------------------------------------------
// SSE2 implementations of optimized routines:
//-----------------------------------------------------------------------------
void _SSE2_SinCos( float x, float* s, float* c )  // any x
{
	fltx4 si, co;
	sincos_ps<true>( _mm_load_ss( &x ), si, co );
	_mm_store_ss( s, si );
	_mm_store_ss( c, co );
}

float _SSE2_cos( float x )
{
	fltx4 si, co;
	sincos_ps<true>( _mm_load_ss( &x ), si, co );
	_mm_store_ss( &x, co );

	return x;
}

#if 0
// SSE Version of VectorTransform
void VectorTransformSSE(const float *in1, const matrix3x4_t& in2, float *out1)
{
	Assert( s_bMathlibInitialized );
	Assert( in1 != out1 );

#ifdef _WIN32
	__asm
	{
		mov eax, in1;
		mov ecx, in2;
		mov edx, out1;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		addss xmm0, [ecx+12]
		movss [edx], xmm0;
		add ecx, 16;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		addss xmm0, [ecx+12]
		movss [edx+4], xmm0;
		add ecx, 16;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		addss xmm0, [ecx+12]
		movss [edx+8], xmm0;
	}
#elif POSIX
	#warning "VectorTransformSSE C implementation only"
		out1[0] = DotProduct(in1, in2[0]) + in2[0][3];
		out1[1] = DotProduct(in1, in2[1]) + in2[1][3];
		out1[2] = DotProduct(in1, in2[2]) + in2[2][3];
#else
	#error "Not Implemented"
#endif
}
#endif

#if 0
void VectorRotateSSE( const float *in1, const matrix3x4_t& in2, float *out1 )
{
	Assert( s_bMathlibInitialized );
	Assert( in1 != out1 );

#ifdef _WIN32
	__asm
	{
		mov eax, in1;
		mov ecx, in2;
		mov edx, out1;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		movss [edx], xmm0;
		add ecx, 16;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		movss [edx+4], xmm0;
		add ecx, 16;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		movss [edx+8], xmm0;
	}
#elif POSIX
	#warning "VectorRotateSSE C implementation only"
		out1[0] = DotProduct( in1, in2[0] );
		out1[1] = DotProduct( in1, in2[1] );
		out1[2] = DotProduct( in1, in2[2] );
#else
	#error "Not Implemented"
#endif
}
#endif


// SSE DotProduct -- it's a smidgen faster than the asm DotProduct...
//   Should be validated too!  :)
//   NJS: (Nov 1 2002) -NOT- faster.  may time a couple cycles faster in a single function like
//   this, but when inlined, and instruction scheduled, the C version is faster.
//   Verified this via VTune
/*
vec_t DotProduct (const vec_t *a, const vec_t *c)
{
	vec_t temp;

	__asm
	{
		mov eax, a;
		mov ecx, c;
		mov edx, DWORD PTR [temp]
		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		movss [edx], xmm0;
		fld DWORD PTR [edx];
		ret
	}
}
*/

#endif // COMPILER_MSVC64
