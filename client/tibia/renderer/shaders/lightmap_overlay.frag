#version 440
// Outputs the classic light colour; pipeline blend is DstColor * Src (multiply).

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D light;

void main()
{
    fragColor = texture(light, v_texCoord);
}
