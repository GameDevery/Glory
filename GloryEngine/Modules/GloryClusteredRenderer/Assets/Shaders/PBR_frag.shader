#type frag

#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "Internal/ObjectData.glsl"
#include "Internal/Textured.glsl"

#define FEATURE_TEXTURED
#define FEATURE_TRANSPARENT

layout(std430, binding = 1) buffer PropertiesSSBO
{
	vec4 Color;
	float AmbientOcclusion;
	float RoughnessFactor;
	float MetallicFactor;
} Properties;

#ifdef WITH_TEXTURED
layout(binding = 0) uniform sampler2D texSampler;
layout(binding = 1) uniform sampler2D normalSampler;
layout(binding = 2) uniform sampler2D ambientSampler;
layout(binding = 3) uniform sampler2D roughnessSampler;
layout(binding = 4) uniform sampler2D metalnessSampler;
#endif

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 inColor;
#ifdef WITH_TEXTURED
layout(location = 2) in mat3 TBN;
#else
layout(location = 2) in vec3 inNormal;
#endif

layout(location = 0) out uvec4 outID;
layout(location = 2) out vec4 outColor;
layout(location = 3) out vec4 outNormal;
layout(location = 5) out vec4 outData;

void main()
{
#ifdef WITH_TEXTURED

	vec4 texColor = texture(texSampler, fragTexCoord);
#ifdef WITH_TRANSPARENT
	if (texColor.a < 1.0) discard;
#endif
	outColor = (TextureEnabled(0) ? vec4(pow(texColor.rgb, vec3(2.2)), texColor.a) : Properties.Color) * inColor;
	vec3 normal = TextureEnabled(1) ? normalize(TBN * (texture(normalSampler, fragTexCoord).xyz * 2.0 - 1.0)) : TBN[2];
	float ambient = TextureEnabled(2) ? texture(ambientSampler, fragTexCoord).r : Properties.AmbientOcclusion;
	float roughness = TextureEnabled(3) ? texture(roughnessSampler, fragTexCoord).g : Properties.RoughnessFactor;
	float metalic = TextureEnabled(4) ? texture(metalnessSampler, fragTexCoord).b : Properties.MetallicFactor;
#else
	outColor = inColor * Properties.Color;
	vec3 normal = inNormal;
	float ambient = Properties.AmbientOcclusion;
	float roughness = Properties.RoughnessFactor;
	float metalic = Properties.MetallicFactor;
#endif
	outNormal = vec4((normalize(normal) + 1.0) * 0.5, 1.0);
	outID = Object.ObjectID;

	vec4 data;
	data.r = ambient;
	data.g = roughness;
	data.b = metalic;
	data.a = 1.0;
	outData = data;
}
