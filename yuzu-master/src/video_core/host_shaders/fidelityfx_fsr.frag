// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

//!#version 460 core
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : require

// FidelityFX Super Resolution Sample
//
// Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

layout( push_constant ) uniform constants {
    uvec4 Const0;
    uvec4 Const1;
    uvec4 Const2;
    uvec4 Const3;
};

layout(set=0,binding=0) uniform sampler2D InputTexture;

#define A_GPU 1
#define A_GLSL 1
#define FSR_RCAS_PASSTHROUGH_ALPHA 1

#ifndef YUZU_USE_FP16
    #include "ffx_a.h"

    #if USE_EASU
        #define FSR_EASU_F 1
        AF4 FsrEasuRF(AF2 p) { AF4 res = textureGather(InputTexture, p, 0); return res; }
        AF4 FsrEasuGF(AF2 p) { AF4 res = textureGather(InputTexture, p, 1); return res; }
        AF4 FsrEasuBF(AF2 p) { AF4 res = textureGather(InputTexture, p, 2); return res; }
    #endif
    #if USE_RCAS
        #define FSR_RCAS_F 1
        AF4 FsrRcasLoadF(ASU2 p) { return texelFetch(InputTexture, ASU2(p), 0); }
        void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}
    #endif
#else
    #define A_HALF
    #include "ffx_a.h"

    #if USE_EASU
        #define FSR_EASU_H 1
        AH4 FsrEasuRH(AF2 p) { AH4 res = AH4(textureGather(InputTexture, p, 0)); return res; }
        AH4 FsrEasuGH(AF2 p) { AH4 res = AH4(textureGather(InputTexture, p, 1)); return res; }
        AH4 FsrEasuBH(AF2 p) { AH4 res = AH4(textureGather(InputTexture, p, 2)); return res; }
    #endif
    #if USE_RCAS
        #define FSR_RCAS_H 1
        AH4 FsrRcasLoadH(ASW2 p) { return AH4(texelFetch(InputTexture, ASU2(p), 0)); }
        void FsrRcasInputH(inout AH1 r,inout AH1 g,inout AH1 b){}
    #endif
#endif

#include "ffx_fsr1.h"

layout (location = 0) in vec2 frag_texcoord;
layout (location = 0) out vec4 frag_color;

void CurrFilter(AU2 pos) {
#if USE_EASU
    #ifndef YUZU_USE_FP16
        AF3 c;
        FsrEasuF(c, pos, Const0, Const1, Const2, Const3);
        frag_color = AF4(c, texture(InputTexture, frag_texcoord).a);
    #else
        AH3 c;
        FsrEasuH(c, pos, Const0, Const1, Const2, Const3);
        frag_color = AH4(c, texture(InputTexture, frag_texcoord).a);
    #endif
#endif
#if USE_RCAS
    #ifndef YUZU_USE_FP16
        AF4 c;
        FsrRcasF(c.r, c.g, c.b, c.a, pos, Const0);
        frag_color = c;
    #else
        AH4 c;
        FsrRcasH(c.r, c.g, c.b, c.a, pos, Const0);
        frag_color = c;
    #endif
#endif
}

void main() {
#if USE_RCAS
    CurrFilter(AU2(frag_texcoord * vec2(textureSize(InputTexture, 0))));
#else
    CurrFilter(AU2(gl_FragCoord.xy));
#endif
}
