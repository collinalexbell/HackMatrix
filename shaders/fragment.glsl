#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
in vec3 FragPos;
in vec3 lineColor;
in vec4 ModelColor;
in vec3 Normal;
in vec3 Barycentric;
in vec3 VoxelColor;

uniform sampler2DArray allBlocks;
uniform sampler2D texture_diffuse1;
uniform sampler2D appTex;
uniform bool isApp;
uniform vec3 dynamicObjectColor;
uniform bool SHADOWS_ENABLED;
uniform bool isModel;
uniform bool isLine;
uniform bool appSelected;
uniform bool appFocused;
uniform bool isMesh;
uniform float uAmbientStrength;
uniform bool isDynamicObject;
uniform bool isLight;
uniform bool appTransparent;
uniform bool directRender;
uniform bool isVoxel;
uniform bool voxelsEnabled;
uniform float time;
const int MAX_LIGHTS = 10;
uniform int numLights;
uniform samplerCube depthCubeMap0;
uniform samplerCube depthCubeMap1;
uniform samplerCube depthCubeMap2;
uniform samplerCube depthCubeMap3;
uniform samplerCube depthCubeMap4;
uniform vec3 lightPos[MAX_LIGHTS];
uniform vec3 lightColor[MAX_LIGHTS];
uniform vec3 viewPos;
uniform float far_plane[MAX_LIGHTS];

struct Material {
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	float shininess;
};

uniform Material material;

vec3 palette( float t ) {
	vec3 a = vec3(0.5, 0.5, 0.5);
	vec3 b = vec3(0.5, 0.5, 0.5);
	vec3 c = vec3(1.0, 1.0, 1.0);
	vec3 d = vec3(0.263,0.416,0.557);

	return a + b*cos( 6.28318*(c*t+d) );
}

//https://www.shadertoy.com/view/mtyGWy
vec4 floorEffect( vec2 fragCoord ) {
	vec2 uv = (fragCoord * 2.0 - vec2(1,1)) / vec2(1,1);
	vec2 uv0 = uv;
	vec3 finalColor = vec3(0.0);

	for (float i = 0.0; i < 4.0; i++) {
		uv = fract(uv * 1.5) - 0.5;

		float d = length(uv) * exp(-length(uv0));

		vec3 col = palette(length(uv0) + i*.4 + time*.4);

		d = sin(d*8. + time)/8.;
		d = abs(d);

		d = pow(0.01 / d, 1.2);

		finalColor += col * d;
	}
	return vec4(finalColor, 1.0);
}

vec4 colorFromTexture(sampler2D tex, vec2 coord) {
	vec2 sampleCoord = coord;
	if(!directRender) {
		// Flip Y for in-world quads; directRender already uses screen space.
		sampleCoord = vec2(coord.x, 1.0 - coord.y);
	}
	if(appTransparent) {
		return texture(tex, sampleCoord);
	} else {
		return vec4(vec3(texture(tex, sampleCoord)), 1);
	}
}

// Debug: visualize texture coordinates to verify UVs.
vec4 debugTexcoordColor(vec2 coord) {
	return vec4(coord.x, coord.y, 0.0, 1.0);
}


float ShadowCalculation(samplerCube depthMap, vec3 fragPos, vec3 norm, vec3 lightDir, int lightIndex)
{
	// get vector between fragment position and light position
	vec3 fragToLight = fragPos - lightPos[lightIndex];
	// use the light to fragment vector to sample from the depth map
	float closestDepth = texture(depthMap, fragToLight).r;
	// it is currently in linear range between [0,1]. Re-transform back to original value
	closestDepth *= far_plane[lightIndex];
	// now get current linear depth as the length between the fragment and light position
	float currentDepth = length(fragToLight);
	// now test for shadows
	float bias = max(0.1 * (1.0 - dot(norm, lightDir)), 0.6);
	float shadow = currentDepth -  bias > closestDepth ? 1.0 : 0.0;

	return shadow;
}

vec4 Light(int i, samplerCube depthMap) {
	// ambient
	//if(uAmbientStrength > 0.0011) {
	//float ambientStrength = uAmbientStrength; // much lighter
	//} else {
	// strong visual attenuation in range [0.1,0.15] 
	float ambientStrength = 0.20; // lighter 
				      //}
	vec3 ambient = ambientStrength * lightColor[i];

	// diffuse
	vec3 norm = normalize(Normal);
	vec3 lightDir = normalize(lightPos[i] - FragPos);

	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * lightColor[i];

	float specularStrength = 0.04;
	float shininess = 32.0;
	vec3 viewDir = normalize(viewPos - FragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
	vec3 specular = specularStrength * spec * lightColor[i];

	// calculate shadow
	float shadow = 0.0; 

	if(SHADOWS_ENABLED) {
		shadow = ShadowCalculation(depthMap, FragPos, norm, lightDir, i);                      
	}
	//float shadow = 0.0;
	//vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;
	return vec4(ambient + (1.0-shadow) * diffuse + specular, 1.0);
}

void main()
{
	// need to pass this in as vertex data, but hold for now
	if(isApp) {
		// Visualize TexCoord mapping instead of sampling the app texture.
		FragColor = colorFromTexture(appTex, TexCoord);
		//FragColor = mix(FragColor, floorEffect(TexCoord), 0.1);
	} else if (isVoxel && voxelsEnabled) {
		float edgeWidth = 0.03;
		float edge = 1.0 - smoothstep(edgeWidth, edgeWidth * 2.0,
				min(min(Barycentric.x, Barycentric.y), Barycentric.z));
		vec3 edgeGlow = vec3(1.0, 0.9, 0.6) * (1.2 + 0.3 * sin(time * 2.0));
		vec3 color = VoxelColor;
		color = mix(color, edgeGlow, edge);
		FragColor = vec4(color, 1.0);
	} else if (isModel) {

		if(isLight) {
			FragColor = vec4(lightColor[0], 1.0);
		} else {
			vec3 lightOutput = vec3(0.0,0.0,0.0);

			if(numLights == 1) {
				lightOutput += vec3(Light(0, depthCubeMap0));
			} else if (numLights == 2) {
				lightOutput += vec3(Light(0, depthCubeMap0));
				lightOutput += vec3(Light(1, depthCubeMap1));

			} else if (numLights == 3) {
				lightOutput += vec3(Light(0, depthCubeMap0));
				lightOutput += vec3(Light(1, depthCubeMap1));
				lightOutput += vec3(Light(2, depthCubeMap2));
			} else if (numLights == 4) {
				lightOutput += vec3(Light(0, depthCubeMap0));
				lightOutput += vec3(Light(1, depthCubeMap1));
				lightOutput += vec3(Light(2, depthCubeMap2));
				lightOutput += vec3(Light(3, depthCubeMap3));
			} else if (numLights == 5) {
				lightOutput += vec3(Light(0, depthCubeMap0));
				lightOutput += vec3(Light(1, depthCubeMap1));
				lightOutput += vec3(Light(2, depthCubeMap2));
				lightOutput += vec3(Light(3, depthCubeMap3));
				lightOutput += vec3(Light(4, depthCubeMap4));
			}


			FragColor = vec4(lightOutput,1.0) * texture(texture_diffuse1, TexCoord);
		}
	} else if (isLine) {
		FragColor = vec4(lineColor, 1.0);
	} else if (isDynamicObject) {
    FragColor = vec4(VoxelColor,1);
  }
}
