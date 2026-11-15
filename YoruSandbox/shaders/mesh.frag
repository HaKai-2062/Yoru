#version 460
#extension GL_EXT_buffer_reference : require

layout(buffer_reference, std430) readonly buffer VertexBuffer { float Unused; };

layout(push_constant) uniform PushConstants
{
	mat4 MVP;
	VertexBuffer Vertices;
	vec4 LightDirection;
} u_Push;

layout(location = 0) in vec3 v_Color;
layout(location = 1) in vec3 v_Normal;

layout(location = 0) out vec4 o_Color;

// The swapchain is UNORM (so ImGui's colors stay correct), lighting happens in
// linear space so it has to be encoded to sRGB here
vec3 LinearToSRGB(vec3 color)
{
	vec3 low = color * 12.92;
	vec3 high = 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055;
	return mix(high, low, lessThanEqual(color, vec3(0.0031308)));
}

void main()
{
	float diffuse = max(dot(normalize(v_Normal), -u_Push.LightDirection.xyz), 0.0);
	vec3 linearColor = v_Color * (0.15 + 0.85 * diffuse);
	o_Color = vec4(LinearToSRGB(linearColor), 1.0);
}
