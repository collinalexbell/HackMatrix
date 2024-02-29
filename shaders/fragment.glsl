#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
in vec3 lineColor;
flat in int BlockType;
flat in int IsLookedAt;
flat in int Selection;

uniform sampler2DArray allBlocks;
uniform sampler2D texture_diffuse1;
uniform sampler2D app0;
uniform sampler2D app1;
uniform sampler2D app2;
uniform sampler2D app3;
uniform sampler2D app4;
uniform sampler2D app5; uniform sampler2D app6;
uniform bool isApp;
uniform bool isModel;
uniform bool isLine;
uniform bool appSelected;
uniform bool isMesh;
uniform bool isDynamicObject;
uniform int totalBlockTypes;
uniform float time;

vec3 palette( float t ) {
	vec3 a = vec3(0.5, 0.5, 0.5);
	vec3 b = vec3(0.5, 0.5, 0.5);
	vec3 c = vec3(1.0, 1.0, 1.0);
	vec3 d = vec3(0.263,0.416,0.557);

	return a + b*cos( 6.28318*(c*t+d) );
}

//https://www.shadertoy.com/view/mtyGWy
vec4 floor( vec2 fragCoord ) {
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
	return texture(tex, TexCoord * vec2(1,-1));
}

void main()
{
	// need to pass this in as vertex data, but hold for now
	if(isApp) {
		if(BlockType == 0) {
			FragColor = colorFromTexture(app0, TexCoord);
		} else if (BlockType == 1) {
			FragColor = colorFromTexture(app1, TexCoord);
		} else if (BlockType == 2) {
			FragColor = colorFromTexture(app2, TexCoord);
		} else if (BlockType == 3) {
			FragColor = colorFromTexture(app3, TexCoord);
		} else if (BlockType == 4) {
			FragColor = colorFromTexture(app4, TexCoord);
		} else if (BlockType == 5) {
			FragColor = colorFromTexture(app5, TexCoord);
		} else if (BlockType == 6) {
			FragColor = colorFromTexture(app6, TexCoord);
		}
		if(!appSelected) {
			//FragColor = mix(FragColor, floor(TexCoord), 0.1);
		}
	} else if(isLine) {
		FragColor = vec4(lineColor, lineColor.r);
  } else if(isDynamicObject) {
    FragColor = vec4(0.5, 0.5, 0.5, 1.0);
  } else if (isModel) {
    //FragColor = texture(texture_diffuse1, TexCoord);
    FragColor = vec4(0.9, 0.3, 0.3, 1.0);
	} else {
    FragColor = texture(allBlocks, vec3(TexCoord.x, TexCoord.y, BlockType));
	}
	if(Selection == 1) {
		FragColor = FragColor * vec4(2.0,1.0,1.0,1.0);
	}
	if(IsLookedAt == 1) {
		FragColor = FragColor * vec4(2.0,2.0,2.0,1.0);
	}
}
