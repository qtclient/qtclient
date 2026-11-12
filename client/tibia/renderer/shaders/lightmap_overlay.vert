#version 440
// Classic lightmap overlay: textured quad in the LightMap item's local space.

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec2 v_texCoord;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
};

void main()
{
    v_texCoord = texCoord;
    gl_Position = qt_Matrix * vec4(position, 0.0, 1.0);
}
