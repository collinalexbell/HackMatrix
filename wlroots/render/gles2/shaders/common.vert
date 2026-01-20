uniform mat3 proj;
uniform mat3 tex_proj;
attribute vec2 pos;
varying vec2 v_texcoord;

void main() {
	vec3 pos3 = vec3(pos, 1.0);
	gl_Position = vec4(pos3 * proj, 1.0);
	v_texcoord = (pos3 * tex_proj).xy;
}
