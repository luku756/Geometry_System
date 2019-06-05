#version 400

layout(triangles) in;
layout(triangle_strip, max_vertices = 15) out;

in vec3 VNormal[];
in vec3 VPosition[];
in vec2 VTEXCOORD[];
in vec4 fake_position[];

out vec3 v_position_EC;
out vec3 v_normal_EC;
out vec2 v_tex_coord;
flat out int front;

bool isFrontFacing(vec3 a, vec3 b, vec3 c)//front이면 true 리턴.
{
	//정점 순서는 a-b-c
	//a->b 벡터, a->c 벡터 둘을 x-y평면에 정사영(z=0)시킨 뒤 두 벡터를 외적하였다.
	//이러면 x,y 성분이 0이고 x 성분이 아래와 같이 나오는데, z값이 양수라는 것은 전방(카메라)를 향하는 것이므로 front이다.
	return ((a.x * b.y - b.x * a.y) + (b.x * c.y - c.x * b.y) + (c.x * a.y - a.x * c.y)) > 0;
}

void main()
{
	//ndc 계산
	//vec3 p0 = fake_position[0].xyz / fake_position[0].w;
	//vec3 p1 = fake_position[1].xyz / fake_position[1].w;
	//vec3 p2 = fake_position[2].xyz / fake_position[2].w;

	////cc 계산
	float w01, w02;
	w01 = fake_position[0].w/fake_position[1].w;
	w02 = fake_position[0].w/fake_position[2].w;
	vec3 p0 = fake_position[0].xyz ;
	vec3 p1 = fake_position[1].xyz * w01 ;
	vec3 p2 = fake_position[2].xyz * w02 ;

	int frontflag = 0;

	if (isFrontFacing(p0, p1, p2)) {
		frontflag = 1;
	}
	
	v_normal_EC = VNormal[0];
	v_position_EC = VPosition[0] ;
	v_tex_coord = VTEXCOORD[0];
	front = frontflag;
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();

	v_normal_EC = VNormal[1];
	v_position_EC = VPosition[1] ;
	v_tex_coord = VTEXCOORD[1];
	front = frontflag;
	gl_Position = gl_in[1].gl_Position;
	EmitVertex();

	v_normal_EC = VNormal[2];
	v_position_EC = VPosition[2] ;
	v_tex_coord = VTEXCOORD[2];
	front = frontflag;
	gl_Position = gl_in[2].gl_Position;
	EmitVertex();


	EndPrimitive();

}
