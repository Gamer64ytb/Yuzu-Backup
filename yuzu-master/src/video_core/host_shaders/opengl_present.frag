// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 430 core

layout (location = 0) in vec2 frag_tex_coord;
layout (location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D color_texture;

void main() {
    color = vec4(texture(color_texture, frag_tex_coord));
}
