#ifndef EDITORCOMMON_H
#define EDITORCOMMON_H
#pragma once

#include "tier1/utlvector.h"
#include "Color.h"

static inline constexpr float NODE_DRAW_SHADOW_DELTA = 5;
static inline constexpr Color NODE_DRAW_COLOR_SHADOW( 0, 0, 0, 50 );

static inline constexpr Color DRAWCOLOR_ERRORLEVEL_UNDEFINED( 140, 140, 160, 255 );
static inline constexpr Color DRAWCOLOR_ERRORLEVEL_FAIL( 128, 0, 0, 255 );

static inline constexpr Color DRAWCOLOR_BRIDGE_TMP_COL_ORANGE( 240, 160, 0, 255 );
static inline constexpr Color DRAWCOLOR_BRIDGE_TMP_COL_TRANS( 0, 0, 0, 196 );

static inline constexpr Color JACK_COLOR_NORM( 15, 15, 15, 255 );
static inline constexpr Color JACK_COLOR_R( 200, 0, 0, 255 );
static inline constexpr Color JACK_COLOR_G( 0, 200, 0, 255 );
static inline constexpr Color JACK_COLOR_B( 0, 0, 200, 255 );
static inline constexpr Color JACK_COLOR_A( 200, 200, 200, 255 );
static inline constexpr Color JACK_COLOR_PPMASTER( 50, 180, 60, 255 );

static inline constexpr Color JACK_COLOR_NAME( 210, 210, 220, 255 );
static inline constexpr Color JACK_COLOR_NAME_FOCUS( 160, 255, 160, 255 );

static inline constexpr Color JACK_COLOR_FOCUS( 240, 200, 50, 255 );

static inline constexpr Color JACK_COLOR_DATATYPE_UNDEFINED( 100, 100, 100, 255 );
static inline constexpr Color JACK_COLOR_DATATYPE( 160, 160, 160, 255 );
static inline constexpr Color JACK_COLOR_DATATYPE_DARK( 32, 32, 32, 255 );

static inline constexpr int JACK_TEXT_INSET = 3;

static inline constexpr float VIEWZOOM_OUT_MAX = 8.0f;
static inline constexpr float VIEWZOOM_IN_MAX = 0.5f;

Color Lerp( float perc, const Color& a, const Color& b );

void SetupVguiTex( int& var, const char* tex );

template <typename T>
inline void SafeDeleteVector( CUtlVector<T*>& vec )
{
	CUtlVector<T*> b = std::move( vec );
	b.PurgeAndDeleteElements();
}

#endif