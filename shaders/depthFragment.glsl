#version 330 core
in vec4 FragPos;
in float Color;
uniform bool appTransparent;
uniform bool isApp;
uniform int fromLightIndex;
uniform int numLights;
const int MAX_LIGHTS = 100;
uniform vec3 lightPos[MAX_LIGHTS];
uniform float far_plane[MAX_LIGHTS];

void main() {
  if(isApp && appTransparent) {
    discard;
  }

  // get distance between fragment and light source
  float lightDistance = length(FragPos.xyz - lightPos[fromLightIndex]);

  // map to [0;1] range by dividing by far_plane
  lightDistance = lightDistance / far_plane[fromLightIndex];

  // write this as modified depth
  gl_FragDepth = lightDistance;

}

