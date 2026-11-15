#version 460
#extension GL_EXT_buffer_reference : require

struct Vertex
{
	vec3 Position;
	float UVX;
	vec3 Normal;
	float UVY;
	vec4 Color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
	Vertex Vertices[];
};

// Must match MeshPushConstants in MeshLayer.h
layout(push_constant) uniform PushConstants
{
	mat4 MVP;
	VertexBuffer Vertices;
	vec4 LightDirection;	// object space
} u_Push;

layout(location = 0) out vec3 v_Color;
layout(location = 1) out vec3 v_Normal;

void main()
{
	Vertex v = u_Push.Vertices.Vertices[gl_VertexIndex];
	gl_Position = u_Push.MVP * vec4(v.Position, 1.0);
	v_Color = v.Color.rgb;
	v_Normal = v.Normal;
}
