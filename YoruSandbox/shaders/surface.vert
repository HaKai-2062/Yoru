#version 460
#extension GL_EXT_buffer_reference : require

// Must match SurfaceVertex in FluidLayer.h
struct SurfaceVertex
{
	vec4 Position;
	vec4 Normal;
};

layout(buffer_reference, std430) readonly buffer Vertices
{
	SurfaceVertex Values[];
};

// Must match SurfacePushConstants in FluidLayer.h
layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	Vertices VertexBuffer;
	uvec2 Padding;
	vec4 LightDirection;
} u_Push;

layout(location = 0) out vec3 v_Normal;

void main()
{
	// Positions are already in world space, no model matrix
	SurfaceVertex v = u_Push.VertexBuffer.Values[gl_VertexIndex];
	gl_Position = u_Push.ViewProjection * vec4(v.Position.xyz, 1.0);
	v_Normal = v.Normal.xyz;
}
