#version 320 es
/* Copyright (c) 2019-2023, Arm Limited and Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

precision mediump float;

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec4 in_color;
layout (location = 0) out vec4 out_color;

layout (binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 projection;
} ubo;

void main()
{
    gl_Position = ubo.projection * ubo.model * vec4(in_position, 0.0, 1.0);

    out_color = in_color;
}
