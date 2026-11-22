#version 460
#extension GL_EXT_buffer_reference : require

layout(buffer_reference, std430) readonly buffer Vertices { vec4 Unused; };

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	Vertices VertexBuffer;
	uvec2 Padding;
	vec4 LightDirection;
} u_Push;

layout(location = 0) in vec3 v_Normal;

layout(location = 0) out vec4 o_Color;

// Swapchain is UNORM, lighting is linear
vec3 LinearToSRGB(vec3 color)
{
	vec3 low = color * 12.92;
	vec3 high = 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055;
	return mix(high, low, lessThanEqual(color, vec3(0.0031308)));
}

void main()
{
	// Flat color with diffuse
	vec3 baseColor = vec3(0.1, 0.35, 0.8);
	float diffuse = max(dot(normalize(v_Normal), -u_Push.LightDirection.xyz), 0.0);
	o_Color = vec4(LinearToSRGB(baseColor * (0.15 + 0.85 * diffuse)), 1.0);
}
