#version 330

#define NUMBER_OF_LIGHT_COUNT 4 

uniform mat4 u_ModelViewMatrix;
uniform mat3 u_ModelViewMatrixInvTrans;
uniform mat4 u_ModelViewProjectionMatrix;
uniform mat4 u_fakeMVPMAt;

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_tex_coord;

out vec3 VPosition;
out vec3 VNormal;
out vec2 VTEXCOORD;
out vec4 fake_position;

void main(void) {	
	VPosition = vec3(u_ModelViewMatrix*vec4(a_position, 1.0f));
	VNormal = normalize(u_ModelViewMatrixInvTrans*a_normal);
	VTEXCOORD = a_tex_coord;

	fake_position = u_fakeMVPMAt * vec4(a_position, 1.0f);
	gl_Position = u_ModelViewProjectionMatrix * vec4(a_position, 1.0f);
}