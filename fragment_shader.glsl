#version 330

in vec2 uv;

out vec3 color;

uniform sampler2D background;
uniform sampler2D universe;

uniform float frame;

void main() {
	float cell_life = texture(universe, uv).r;
	float cell_bg 	= texture(background, uv).r;
	
	int cell_value = int(255 * cell_life);
	
	if(cell_value == 0) { color = vec3(cell_bg); }
	else if(cell_value < 20) { color = vec3(1.0, 0.0, 0.0); }
	else if(cell_value < 30) { color = vec3(0.0, 1.0, 0.0); }
	else if(cell_value < 40) { color = vec3(0.0, 0.0, 1.0); }
	else { color = vec3(0.0, 1.0, 1.0); }
	//color = vec3(cell_life);
}