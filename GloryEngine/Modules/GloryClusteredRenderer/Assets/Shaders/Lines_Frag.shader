#type frag
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 inColor;
layout(location = 1) out vec4 outColor;

void main()
{
	outColor = inColor;
}
