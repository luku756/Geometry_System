#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <FreeImage/FreeImage.h>
#include <vector>

#include "Shaders/LoadShaders.h"
#include "My_Shading.h"
GLuint h_ShaderProgram_simple, h_ShaderProgram_TXPS, h_ShaderProgram_shadow, h_ShaderProgram_SHOW_SM;

// for simple shaders
GLint loc_ModelViewProjectionMatrix_simple, loc_primitive_color;

// for Phong Shading shaders
#define NUMBER_OF_LIGHT_SUPPORTED 4 
#define NUMBER_OF_LIGHT_COUNT 4
GLint loc_global_ambient_color;
loc_light_Parameters loc_light[NUMBER_OF_LIGHT_SUPPORTED];
loc_Material_Parameters loc_material;
GLint loc_ModelViewProjectionMatrix_TXPS, loc_ModelViewMatrix_TXPS, loc_ModelViewMatrixInvTrans_TXPS, loc_fakeMVPMats_TXPS;
GLint loc_base_texture, loc_flag_texture_mapping, loc_flag_fog, loc_flag_face, loc_objectId;
GLint loc_ShadowMatrix_TXPS[NUMBER_OF_LIGHT_COUNT], loc_shadow_texture[NUMBER_OF_LIGHT_COUNT];
GLint loc_modelViewMatrix_TXPS;
GLint loc_ViewInvMatrix_TXPS;
GLint loc_shadow_texture_SHOW_SM;

// for shadow shaders
GLint loc_ModelViewProjectionMatrix_shadow;

// for drawing shadow map
GLint loc_ModelViewProjectionMatrix_SHOW_SM;
GLint loc_near_clip_dist_SHOW_SM, loc_far_clip_dist_SHOW_SM;

// include glm/*.hpp only if necessary
//#include <glm/glm.hpp> 
#include <glm/gtc/matrix_transform.hpp> //translate, rotate, scale, lookAt, perspective, etc.
#include <glm/gtc/matrix_inverse.hpp> // inverseTranspose, etc.
glm::mat4 ModelViewProjectionMatrix, ModelViewMatrix;
glm::mat3 ModelViewMatrixInvTrans;
glm::mat4 ViewMatrix, ProjectionMatrix;
glm::mat4 ShadowMatrix, BiasMatrix, ViewMatrix_SHADOW[NUMBER_OF_LIGHT_COUNT], ProjectionMatrix_SHADOW;

#define TO_RADIAN 0.01745329252f  
#define TO_DEGREE 57.295779513f
#define BUFFER_OFFSET(offset) ((GLvoid *) (offset))

#define LOC_VERTEX 0
#define LOC_NORMAL 1
#define LOC_TEXCOORD 2

// lights in scene
Light_Parameters light[NUMBER_OF_LIGHT_SUPPORTED];
float light0_position_WC[2][4], light0_lerp_t; // for light animation

struct _flag {
	int texture_mapping;
	int fog;
	int face = 0;
	int camera;
	int show_shadow_map;
	int polygon_fill;
	int cull_face;
	int cam_control = 0;
	int cam_move;
	int cam_timestamp = 0;
	int tiger_move;
	int tiger_timestamp = 0;
	int tiger_speed;
} flag;

// for shadow mapping
struct _ShadowMapping {
	GLint texture_unit[NUMBER_OF_LIGHT_COUNT]; // Must be equal to N_NORMAL_TEXTURES_USED
	GLuint shadow_map_ID[NUMBER_OF_LIGHT_COUNT];
	GLsizei shadow_map_width, shadow_map_height;
	GLfloat near_dist, far_dist, shadow_map_border_color[4];
	GLuint FBO_ID[NUMBER_OF_LIGHT_COUNT];
} ShadowMapping;

struct _WINDOW_param {
	int width, height;
} WINDOW_param;

// texture stuffs
#define N_NORMAL_TEXTURES_USED 6

#define TEXTURE_INDEX_FLOOR 0
#define TEXTURE_INDEX_TIGER 1
#define TEXTURE_INDEX_MOON 2
#define TEXTURE_INDEX_EARTH 3
#define TEXTURE_INDEX_CAMERA 4
#define TEXTURE_INDEX_SHADOW 5

#define SHADOW_MAP_WIDTH 2048
#define SHADOW_MAP_HEIGHT 2048
#define SHADOW_MAP_NEAR_DIST 200.0f
#define SHADOW_MAP_FAR_DIST 1500.0f

GLuint texture_names[N_NORMAL_TEXTURES_USED];

//use geometry shader - 1, no - 0
#define UseGeometryShader 1

//시간 측정 - 1, no - 0
#define UseTimeCount 0

bool use_ec = false;

__int64 _start, _freq, _end;
#define CHECK_TIME_START QueryPerformanceFrequency((LARGE_INTEGER*)&_freq); QueryPerformanceCounter((LARGE_INTEGER*)&_start)
#define CHECK_TIME_END(a) QueryPerformanceCounter((LARGE_INTEGER*)&_end); a = (float)((float)(_end - _start) / (_freq * 1.0e-3f))
void set_up_scene_lights(void);

void My_glTexImage2D_from_file(const char *filename) {
	FREE_IMAGE_FORMAT tx_file_format;
	int tx_bits_per_pixel;
	FIBITMAP *tx_pixmap, *tx_pixmap_32;

	int width, height;
	GLvoid *data;

	tx_file_format = FreeImage_GetFileType(filename, 0);
	// assume everything is fine with reading texture from file: no error checking
	tx_pixmap = FreeImage_Load(tx_file_format, filename);
	tx_bits_per_pixel = FreeImage_GetBPP(tx_pixmap);

	fprintf(stdout, " * A %d-bit texture was read from %s.\n", tx_bits_per_pixel, filename);
	if (tx_bits_per_pixel == 32)
		tx_pixmap_32 = tx_pixmap;
	else {
		fprintf(stdout, " * Converting texture from %d bits to 32 bits...\n", tx_bits_per_pixel);
		tx_pixmap_32 = FreeImage_ConvertTo32Bits(tx_pixmap);
	}

	width = FreeImage_GetWidth(tx_pixmap_32);
	height = FreeImage_GetHeight(tx_pixmap_32);
	data = FreeImage_GetBits(tx_pixmap_32);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, data);
	fprintf(stdout, " * Loaded %dx%d RGBA texture into graphics memory.\n\n", width, height);

	FreeImage_Unload(tx_pixmap_32);
	if (tx_bits_per_pixel != 32)
		FreeImage_Unload(tx_pixmap);
}

// for tiger animation
int cur_frame_tiger = 0;
float rotation_angle_tiger = 0.0f;
int rotation_speed_tiger = 100;

float rotation_angle_camera = TO_RADIAN;
int rotation_speed_camera = 70;

#include "Objects.h"

void set_up_dragon_light() {

	int i;
	glm::vec4 position_EC;
	glm::mat4 ModelMatrix;

	ModelMatrix = glm::rotate(glm::mat4(1.0f), rotation_angle_tiger * 2, glm::vec3(0.0f, 1.0f, 0.0f));
	ModelMatrix = glm::translate(ModelMatrix, glm::vec3(-200.0f, 200.0f, 0.0f));

	glm::vec4 pos = glm::vec4(light[2].position[0], light[2].position[1], light[2].position[2], light[2].position[3]);
	glm::vec4 position_WC = ModelMatrix * pos;

	ViewMatrix_SHADOW[2] = glm::lookAt(glm::vec3(position_WC.x, position_WC.y, position_WC.z), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	glm::vec3 dir = glm::vec3(light[2].spot_direction[0], light[2].spot_direction[1], light[2].spot_direction[2]);
	glm::vec3 dir_wc = glm::mat3(ModelMatrix) * dir;

	glUseProgram(h_ShaderProgram_TXPS);

	position_EC = ViewMatrix * position_WC;
	glUniform4fv(loc_light[2].position, 1, &position_EC[0]);
	glm::vec3 direction_EC2 = glm::mat3(ViewMatrix) * dir_wc;
	glUniform3fv(loc_light[2].spot_direction, 1, &direction_EC2[0]);

	glUseProgram(0);
}

void build_shadow_map(int index) { // The first pass in shadow mapping
	glm::mat4 ViewProjectionMatrix_SHADOW;

	ViewProjectionMatrix_SHADOW = ProjectionMatrix_SHADOW * ViewMatrix_SHADOW[index];

	glBindFramebuffer(GL_FRAMEBUFFER, ShadowMapping.FBO_ID[index]);
	glViewport(0, 0, ShadowMapping.shadow_map_width, ShadowMapping.shadow_map_height);

	glClear(GL_DEPTH_BUFFER_BIT);

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(10.0f, 10.0f);

	glUseProgram(h_ShaderProgram_shadow);

	ModelViewProjectionMatrix = glm::translate(ViewProjectionMatrix_SHADOW, glm::vec3(-500.0f, 0.0f, 500.0f));
	ModelViewProjectionMatrix = glm::scale(ModelViewProjectionMatrix, glm::vec3(1000.0f, 1000.0f, 1000.0f));
	ModelViewProjectionMatrix = glm::rotate(ModelViewProjectionMatrix, -90.0f*TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_shadow, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	draw_floor();

	ModelViewProjectionMatrix = glm::rotate(ViewProjectionMatrix_SHADOW, -rotation_angle_tiger, glm::vec3(0.0f, 1.0f, 0.0f));
	ModelViewProjectionMatrix = glm::translate(ModelViewProjectionMatrix, glm::vec3(200.0f, 0.0f, 0.0f));
	ModelViewProjectionMatrix = glm::rotate(ModelViewProjectionMatrix, -90.0f*TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_shadow, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	draw_tiger();

	// for drawing Optimus
	ModelViewProjectionMatrix = glm::scale(ViewProjectionMatrix_SHADOW, glm::vec3(0.3f, 0.3f, 0.3f));
	ModelViewProjectionMatrix = glm::rotate(ModelViewProjectionMatrix, rotation_angle_tiger, glm::vec3(0.0f, 1.0f, 0.0f));
	ModelViewProjectionMatrix = glm::rotate(ModelViewProjectionMatrix, -90.0f*TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_shadow, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	draw_object(OBJECT_OPTIMUS, 1.0f, 1.0f, 1.0f, GL_CCW);

	// for drawing dragon	
	ModelViewProjectionMatrix = glm::rotate(ViewProjectionMatrix_SHADOW, rotation_angle_tiger * 2, glm::vec3(0.0f, 1.0f, 0.0f));
	ModelViewProjectionMatrix = glm::translate(ModelViewProjectionMatrix, glm::vec3(-200.0f, 200.0f, 0.0f));
	ModelViewProjectionMatrix = glm::scale(ModelViewProjectionMatrix, glm::vec3(5.0f, 5.0f, 5.0f));
	ModelViewProjectionMatrix = glm::rotate(ModelViewProjectionMatrix, -90.0f*TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_shadow, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	draw_object(OBJECT_DRAGON, 1.0f, 1.0f, 1.0f, GL_CCW);


	glUseProgram(0);

	glFinish();

	glDisable(GL_POLYGON_OFFSET_FILL);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


glm::vec3 fakeCam = glm::vec3(-200.0f, 100.0f, 200.0f);
glm::vec3 fakeCam_controlled = glm::vec3(-200.0f, 100.0f, 200.0f);
glm::vec3 fakeCam_origin = glm::vec3(-200.0f, 100.0f, 200.0f);
glm::vec3 realCam = glm::vec3(400.0f, 400.0f, 400.0f);
glm::vec3 realCam_origin = glm::vec3(400.0f, 400.0f, 400.0f);
glm::mat4 fakeViewMat, fakeMVPMat;

glm::vec3 controllCam = glm::vec3(0, 0, 0);//카메라 움직임
glm::vec3 controllCam_origin = glm::vec3(0.0f, 0.0f, -200.0f);

float camDist;

void setMatrixes(glm::mat4 ModelMatrix) {

	ModelViewMatrix = ViewMatrix * ModelMatrix;
	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	ModelViewMatrixInvTrans = glm::inverseTranspose(glm::mat3(ModelViewMatrix));

	if (UseGeometryShader == 1) {
		if (use_ec)
			fakeMVPMat = fakeViewMat * ModelMatrix;
		else
		fakeMVPMat = ProjectionMatrix * fakeViewMat * ModelMatrix;
		glUniformMatrix4fv(loc_fakeMVPMats_TXPS, 1, GL_FALSE, &fakeMVPMat[0][0]);
	}
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_TXPS, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniformMatrix4fv(loc_ModelViewMatrix_TXPS, 1, GL_FALSE, &ModelViewMatrix[0][0]);
	glUniformMatrix3fv(loc_ModelViewMatrixInvTrans_TXPS, 1, GL_FALSE, &ModelViewMatrixInvTrans[0][0]);

}

void draw_scene(glm::vec3 addpos) { // The second pass in shadow mapping

	//glFinish();

	glm::mat4 ModelMatrix;
	camDist = length(fakeCam_origin) / 2;
	glViewport(0, 0, WINDOW_param.width, WINDOW_param.height);

	//중앙 좌표계
	glUseProgram(h_ShaderProgram_simple);
	ModelViewMatrix = glm::scale(ViewMatrix, glm::vec3(50.0f, 50.0f, 50.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_simple, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glLineWidth(2.0f);
	draw_axes();
	glLineWidth(1.0f);

	//가짜 카메라(비디오 카메라) 위치.
	glUseProgram(h_ShaderProgram_simple);
	ModelMatrix = glm::rotate(glm::mat4(1.0f), rotation_angle_camera, glm::vec3(0.0f, 1.0f, 0.0f));

	fakeCam = glm::vec3(ModelMatrix * glm::vec4(fakeCam_origin, 1.0f));


	//가짜 카메라(비디오 카메라) 의 VIEW MATRIX

	if (UseGeometryShader == 1)
		fakeViewMat = glm::lookAt(fakeCam, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	ModelMatrix = glm::translate(ModelMatrix, fakeCam_origin);
	ModelMatrix = glm::scale(ModelMatrix, glm::vec3(50.0f, 50.0f, 50.0f));
	ModelMatrix = glm::rotate(ModelMatrix, -110.0f*TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
	ModelMatrix = glm::rotate(ModelMatrix, 135.0f*TO_RADIAN, glm::vec3(0.0f, 0.0f, 1.0f));

	ModelViewMatrix = ViewMatrix * ModelMatrix;
	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_simple, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glLineWidth(2.0f);
	draw_axes();
	glLineWidth(1.0f);

	//카메라
	if (flag.camera == 0) {

		glUseProgram(h_ShaderProgram_TXPS);
		ModelMatrix = glm::mat4(1.0f);

		ModelMatrix = glm::translate(ModelMatrix, addpos);
		ModelMatrix = glm::rotate(ModelMatrix, rotation_angle_camera, glm::vec3(0.0f, 1.0f, 0.0f));
		ModelMatrix = glm::translate(ModelMatrix, fakeCam_origin);
		ModelMatrix = glm::scale(ModelMatrix, glm::vec3(5.0f, 5.0f, 5.0f));
		ModelMatrix = glm::rotate(ModelMatrix, -110.0f*TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
		ModelMatrix = glm::rotate(ModelMatrix, 135.0f*TO_RADIAN, glm::vec3(0.0f, 0.0f, 1.0f));

		setMatrixes(ModelMatrix);
		draw_video();
		/*
				glUseProgram(h_ShaderProgram_simple);
				ModelViewProjectionMatrix = glm::scale(ModelViewProjectionMatrix, glm::vec3(50.0f, 50.0f, 50.0f));
				glUniformMatrix4fv(loc_ModelViewProjectionMatrix_simple, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
				glLineWidth(3.0f);
				draw_axes();
				glLineWidth(1.0f);*/
	}
	//달

	glUseProgram(h_ShaderProgram_TXPS);
	set_material_videocam();
	glUniform1i(loc_base_texture, TEXTURE_INDEX_MOON);
	glUniform1i(loc_objectId, OBJECT_MOON);

	ModelMatrix = glm::rotate(glm::mat4(1.0f), rotation_angle_tiger * 2, glm::vec3(0.0f, 1.0f, 0.0f));
	ModelMatrix = glm::translate(ModelMatrix, addpos);
	ModelMatrix = glm::translate(ModelMatrix, glm::vec3(-200.0f, 00.0f, 0.0f));
	ModelMatrix = glm::scale(ModelMatrix, glm::vec3(0.5f, 0.5f, 0.5f));
	ModelMatrix = glm::rotate(ModelMatrix, -90.0f*TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));

	setMatrixes(ModelMatrix);
	draw_sphere();

	glUseProgram(h_ShaderProgram_simple);
	ModelViewProjectionMatrix = glm::scale(ModelViewProjectionMatrix, glm::vec3(50.0f, 50.0f, 50.0f));
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_simple, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glLineWidth(3.0f);
	draw_axes();
	glLineWidth(1.0f);

	//지구

	glUseProgram(h_ShaderProgram_TXPS);
	set_material_videocam();
	glUniform1i(loc_base_texture, TEXTURE_INDEX_EARTH);
	glUniform1i(loc_objectId, OBJECT_EARTH);

	ModelMatrix = glm::rotate(glm::mat4(1.0f), -rotation_angle_tiger * 3, glm::vec3(0.0f, 1.0f, 0.0f));
	ModelMatrix = glm::translate(ModelMatrix, addpos);
	ModelMatrix = glm::translate(ModelMatrix, glm::vec3(0.0f, 00.0f, 0.0f));
	ModelMatrix = glm::scale(ModelMatrix, glm::vec3(3.0f, 3.0f, 3.0f));
	ModelMatrix = glm::rotate(ModelMatrix, -90.0f*TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));

	setMatrixes(ModelMatrix);
	draw_sphere();

	//호랑이

	ModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
	ModelMatrix = glm::translate(ModelMatrix, addpos);
	ModelMatrix = glm::rotate(ModelMatrix, rotation_angle_tiger * 4, glm::vec3(1.0f, 0.0f, 0.0f));
	ModelMatrix = glm::translate(ModelMatrix, glm::vec3(0.0f, 110.0f, 0.0f));
	ModelMatrix = glm::scale(ModelMatrix, glm::vec3(0.8f, 0.8f, 0.8f));
	ModelMatrix = glm::rotate(ModelMatrix, -90.0f*TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));

	setMatrixes(ModelMatrix);
	draw_tiger();

	glUseProgram(h_ShaderProgram_simple);
	ModelViewProjectionMatrix = glm::scale(ModelViewProjectionMatrix, glm::vec3(20.0f, 20.0f, 20.0f));
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_simple, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glLineWidth(3.0f);
	draw_axes();
	glLineWidth(1.0f);


	// for drawing Optimus
	glUseProgram(h_ShaderProgram_TXPS);

	ModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-100.0f, -100.0f, 200.0f));
	ModelMatrix = glm::translate(ModelMatrix, addpos);
	ModelMatrix = glm::scale(ModelMatrix, glm::vec3(0.2f, 0.2f, 0.2f));
	ModelMatrix = glm::rotate(ModelMatrix, rotation_angle_tiger, glm::vec3(0.0f, 1.0f, 0.0f));
	ModelMatrix = glm::rotate(ModelMatrix, -90.0f*TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));

	setMatrixes(ModelMatrix);
	draw_object(OBJECT_OPTIMUS, 1.0f, 1.0f, 1.0f, GL_CCW);

	glUseProgram(h_ShaderProgram_simple);
	ModelViewProjectionMatrix = glm::scale(ModelViewProjectionMatrix, glm::vec3(300.0f, 300.0f, 300.0f));
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_simple, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glLineWidth(3.0f);
	draw_axes();
	glLineWidth(1.0f);

	glUseProgram(0);
}

void draw_shadow_map(void) {
	glViewport(0, 0, WINDOW_param.width, WINDOW_param.height);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(h_ShaderProgram_SHOW_SM);
	draw_floor(); // Borrow this function to draw a unit rectangle.
	glUseProgram(0);

}
int count;
float timesum = 0;
// callbacks
void display(void) {
	if(UseTimeCount == 1)
	CHECK_TIME_START;

	//비디오 카메라 뷰.
	if (flag.camera == 1) {

		ViewMatrix = glm::lookAt(fakeCam, glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f));

		if (flag.cam_control == 1) {//카메라 조종 모드
			ViewMatrix = glm::rotate(ViewMatrix, controllCam.x*TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
			ViewMatrix = glm::rotate(ViewMatrix, controllCam.y*TO_RADIAN, glm::vec3(0.0f, 1.0f, 0.0f));
			ViewMatrix = glm::rotate(ViewMatrix, controllCam.z*TO_RADIAN, glm::vec3(0.0f, 0.0f, 1.0f));

		}
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if (UseTimeCount == 1) {
		for (int i = 0; i < 100; i++)
		{
			draw_scene(glm::vec3(i * 10, 0, 0));
		}
	}
	draw_scene(glm::vec3(0, 0, 0));
	glutSwapBuffers();

	if (UseTimeCount == 1) {
		glFinish();

		float time;
		CHECK_TIME_END(time);
		//printf("%f\n",time);
		timesum += time;
		count++;
		if (count == 10) {
			count = 0;
			printf("=================time : %f\n", timesum / 10);
			timesum = 0;
		}
	}
}

void timer_scene(int timer_flag) {
	if (timer_flag) {
		cur_frame_tiger = flag.tiger_timestamp % N_TIGER_FRAMES;
		rotation_angle_tiger = (flag.tiger_timestamp % 360)*TO_RADIAN;
		glutPostRedisplay();
		flag.tiger_timestamp = flag.tiger_timestamp++ % INT_MAX;
		glutTimerFunc(rotation_speed_tiger, timer_scene, flag.tiger_move);
	}
}


void timer_scene2(int timer_flag) {
	if (timer_flag) {
		rotation_angle_camera = (flag.cam_timestamp % 360)*TO_RADIAN;
		glutPostRedisplay();
		flag.cam_timestamp = flag.cam_timestamp++ % INT_MAX;
		glutTimerFunc(rotation_speed_camera, timer_scene2, flag.cam_move);
	}
}
glm::vec3 tmp;
void keyboard(unsigned char key, int x, int y) {
	int i;
	int light_flag = -1;
	glm::vec4 position_EC;


	if ((key >= '0') && (key <= '0' + NUMBER_OF_LIGHT_SUPPORTED - 1) && flag.cam_control == 0) {
		int light_ID = (int)(key - '0');

		glUseProgram(h_ShaderProgram_TXPS);
		light[light_ID].light_on = 1 - light[light_ID].light_on;
		glUniform1i(loc_light[light_ID].light_on, light[light_ID].light_on);
		glUseProgram(0);

		glutPostRedisplay();
		return;
	}

	switch (key) {
	case 'u':
		use_ec = !use_ec;

		break;
	case 'w':
		if (flag.cam_control == 1) {
			controllCam.x += 1;
			if (controllCam.x >= 360)
				controllCam.x = 0;
			//	printf("%f\n", controllCam.x);
			glm::mat4 ModelMatrix = glm::mat4(1.0f);
			ModelMatrix = glm::rotate(glm::mat4(1.0f), controllCam.x*TO_RADIAN, glm::vec3(1.0f, 0.0f, -1.0f));
			realCam = glm::vec3(ModelMatrix*glm::vec4(realCam_origin, 1.0f));
			if (controllCam.x < 126 || controllCam.x >= 306)
				ViewMatrix = glm::lookAt(realCam, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			else
				ViewMatrix = glm::lookAt(realCam, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
			set_up_scene_lights();
			glutPostRedisplay();
		}
		break;
	case 's':
		if (flag.cam_control == 1) {
			controllCam.x -= 1;
			if (controllCam.x < 0)
				controllCam.x = 359;
			glm::mat4 ModelMatrix = glm::mat4(1.0f);
			ModelMatrix = glm::rotate(glm::mat4(1.0f), controllCam.x*TO_RADIAN, glm::vec3(1.0f, 0.0f, -1.0f));
			realCam = glm::vec3(ModelMatrix*glm::vec4(realCam_origin, 1.0f));
			if (controllCam.x < 126 || controllCam.x >= 306)
				ViewMatrix = glm::lookAt(realCam, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			else
				ViewMatrix = glm::lookAt(realCam, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
			set_up_scene_lights();
			glutPostRedisplay();
		}
		break;

	case 'o'://카메라 컨트롤
		flag.cam_control = 1 - flag.cam_control;
		if (flag.cam_control == 0) {
			realCam = realCam_origin;
			controllCam = glm::vec3(0, 0, 0);
			ViewMatrix = glm::lookAt(realCam, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		}
		else if (flag.cam_control == 1) {
			tmp = glm::vec3(0, 400, 0);

		}
		glutPostRedisplay();
		break;
	case 'f':
		flag.fog = 1 - flag.fog;
		glUseProgram(h_ShaderProgram_TXPS);
		glUniform1i(loc_flag_fog, flag.fog);
		glUseProgram(0);
		glutPostRedisplay();
		break;
	case 'e':	//front/back face 칠하기
		flag.face = 1 - flag.face;
		glUseProgram(h_ShaderProgram_TXPS);
		glUniform1i(loc_flag_face, flag.face);
		glUseProgram(0);
		glutPostRedisplay();
		break;
	case 'a'://realcam <-> fakecam
		flag.camera = 1 - flag.camera;
		if (flag.camera == 1) {
			ViewMatrix = glm::lookAt(fakeCam, glm::vec3(0.0f, 0.0f, 0.0f),
				glm::vec3(0.0f, 1.0f, 0.0f));
		}
		else if (flag.camera == 0) {
			ViewMatrix = glm::lookAt(realCam, glm::vec3(0.0f, 0.0f, 0.0f),
				glm::vec3(0.0f, 1.0f, 0.0f));
		}

		glutPostRedisplay();
		break;
	case 'c':
		flag.cull_face = (flag.cull_face + 1) % 3;
		switch (flag.cull_face) {
		case 0:
			fprintf(stdout, "^^^ Face culling OFF.\n");
			glDisable(GL_CULL_FACE);
			glutPostRedisplay();
			break;
		case 1: // cull back faces;
			fprintf(stdout, "^^^ BACK face culled.\n");
			glCullFace(GL_BACK);
			glEnable(GL_CULL_FACE);
			glutPostRedisplay();
			break;
		case 2: // cull front faces;
			fprintf(stdout, "^^^ FRONT face culled.\n");
			glCullFace(GL_FRONT);
			glEnable(GL_CULL_FACE);
			glutPostRedisplay();
			break;
		}
		break;
	case 't':
		flag.texture_mapping = 1 - flag.texture_mapping;
		glUseProgram(h_ShaderProgram_TXPS);
		glUniform1i(loc_flag_texture_mapping, flag.texture_mapping);
		glUseProgram(0);
		glutPostRedisplay();
		break;

	case 'd':
		flag.show_shadow_map = 1 - flag.show_shadow_map;
		glutPostRedisplay();
		break;
	case 'm':
		flag.tiger_move = 1 - flag.tiger_move;
		if (flag.tiger_move)
			glutTimerFunc(rotation_speed_tiger, timer_scene, 1);
		break;
	case 'n':
		flag.cam_move = 1 - flag.cam_move;
		if (flag.cam_move)
			glutTimerFunc(rotation_speed_camera, timer_scene2, 1);
		break;
	case 'r':
		flag.tiger_speed = (++flag.tiger_speed) % 3;
		switch (flag.tiger_speed) {
		case 0:
			rotation_speed_tiger = 10;
			rotation_speed_camera = 7;
			break;
		case 1:
			rotation_speed_tiger = 100;
			rotation_speed_camera = 70;
			break;
		case 2:
			rotation_speed_tiger = 1000;
			rotation_speed_camera = 700;
			break;
		}
		break;
	case 'l':
		if (light[0].light_on) {
			light0_lerp_t += 0.025f;
			if (light0_lerp_t > 1.0001f)  // for numerical error
				light0_lerp_t = 0.0f;
			for (i = 0; i < 4; i++)
				light[0].position[i] = (1.0f - light0_lerp_t)*light0_position_WC[0][i]
				+ light0_lerp_t * light0_position_WC[1][i];
			ViewMatrix_SHADOW[0] = glm::lookAt(glm::vec3(light[0].position[0], light[0].position[1], light[0].position[2]),
				glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

			glUseProgram(h_ShaderProgram_TXPS);
			position_EC = ViewMatrix * glm::vec4(light[0].position[0], light[0].position[1], light[0].position[2], light[0].position[3]);
			glUniform4fv(loc_light[0].position, 1, &position_EC[0]);
			glUseProgram(0);
			glutPostRedisplay();
		}
		break;
	case 'p':
		flag.polygon_fill = 1 - flag.polygon_fill;
		if (flag.polygon_fill)
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		else
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glutPostRedisplay();
		break;
	case 27: // ESC key
		glutLeaveMainLoop(); // Incur destuction callback for cleanups
		break;
	}

	glutPostRedisplay();
}

void reshape(int width, int height) {
	float aspect_ratio;

	WINDOW_param.width = width;
	WINDOW_param.height = height;

	glViewport(0, 0, width, height);

	aspect_ratio = (float)width / height;
	ProjectionMatrix = glm::perspective(45.0f*TO_RADIAN, aspect_ratio, 1.0f, 1500.0f);

	glutPostRedisplay();
}

void cleanup(void) {
	glDeleteVertexArrays(1, &axes_VAO);
	glDeleteBuffers(1, &axes_VBO);

	glDeleteVertexArrays(1, &tiger_VAO);
	glDeleteBuffers(1, &tiger_VBO);

	glDeleteVertexArrays(1, &rectangle_VAO);
	glDeleteBuffers(1, &rectangle_VBO);

	glDeleteVertexArrays(1, &object_VAO[0]);
	glDeleteBuffers(1, &object_VBO[0]);

	glDeleteTextures(N_NORMAL_TEXTURES_USED, texture_names);
	glDeleteTextures(1, &ShadowMapping.shadow_map_ID[0]);

	glDeleteFramebuffers(1, &ShadowMapping.FBO_ID[0]);
}

void register_callbacks(void) {
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutReshapeFunc(reshape);
	glutTimerFunc(rotation_speed_tiger, timer_scene, 1);
	glutCloseFunc(cleanup);
}

void prepare_shader_program(void) {
	int i;
	char string[256];
	ShaderInfo shader_info_simple[3] = {
		{ GL_VERTEX_SHADER, "Shaders/simple.vert" },
	{ GL_FRAGMENT_SHADER, "Shaders/simple.frag" },
	{ GL_NONE, NULL }
	};
	ShaderInfo shader_info_TXPS[3] = {
		{ GL_VERTEX_SHADER, "Shaders/Phong_Tx.vert" },
	{ GL_FRAGMENT_SHADER, "Shaders/Phong_Tx.frag" },
	{ GL_NONE, NULL }
	};

	ShaderInfo shader_info_TXPS_geom[4] = {
		{ GL_VERTEX_SHADER, "Shaders/Phong_geo.vert.glsl" },
	{ GL_GEOMETRY_SHADER, "Shaders/Phong_geo.geom.glsl" },
	{ GL_FRAGMENT_SHADER, "Shaders/Phong_geo.frag.glsl" },
	{ GL_NONE, NULL }
	};

	ShaderInfo shader_info_shadow[3] = {
		{ GL_VERTEX_SHADER, "Shaders/Shadow.vert" },
	{ GL_FRAGMENT_SHADER, "Shaders/Shadow.frag" },
	{ GL_NONE, NULL }
	};

	ShaderInfo shader_info_SHOW_SM[3] = {
		{ GL_VERTEX_SHADER, "Shaders/SHOW_SM.vert" },
	{ GL_FRAGMENT_SHADER, "Shaders/SHOW_SM.frag" },
	{ GL_NONE, NULL }
	};

	//////////////////
	h_ShaderProgram_simple = LoadShaders(shader_info_simple);
	loc_primitive_color = glGetUniformLocation(h_ShaderProgram_simple, "u_primitive_color");
	loc_ModelViewProjectionMatrix_simple = glGetUniformLocation(h_ShaderProgram_simple, "u_ModelViewProjectionMatrix");

	//////////////////
	if (UseGeometryShader == 1)
		h_ShaderProgram_TXPS = LoadShaders(shader_info_TXPS_geom);
	else if (UseGeometryShader == 0)
		h_ShaderProgram_TXPS = LoadShaders(shader_info_TXPS);

	loc_ModelViewProjectionMatrix_TXPS = glGetUniformLocation(h_ShaderProgram_TXPS, "u_ModelViewProjectionMatrix");
	if (UseGeometryShader == 1)
		loc_fakeMVPMats_TXPS = glGetUniformLocation(h_ShaderProgram_TXPS, "u_fakeMVPMAt");
	loc_modelViewMatrix_TXPS = glGetUniformLocation(h_ShaderProgram_TXPS, "u_ModelMatrix");
	loc_ModelViewMatrix_TXPS = glGetUniformLocation(h_ShaderProgram_TXPS, "u_ModelViewMatrix");
	loc_ModelViewMatrixInvTrans_TXPS = glGetUniformLocation(h_ShaderProgram_TXPS, "u_ModelViewMatrixInvTrans");
	//loc_ShadowMatrix_TXPS = glGetUniformLocation(h_ShaderProgram_TXPS, "u_ShadowMatrix");
	loc_ViewInvMatrix_TXPS = glGetUniformLocation(h_ShaderProgram_TXPS, "u_ViewInvMatrix");

	loc_objectId = glGetUniformLocation(h_ShaderProgram_TXPS, "objectId");

	for (int i = 0; i < NUMBER_OF_LIGHT_COUNT; i++) {
		sprintf(string, "u_ShadowMatrix[%d]", i);
		loc_ShadowMatrix_TXPS[i] = glGetUniformLocation(h_ShaderProgram_TXPS, string);
	}

	loc_global_ambient_color = glGetUniformLocation(h_ShaderProgram_TXPS, "u_global_ambient_color");
	for (i = 0; i < NUMBER_OF_LIGHT_SUPPORTED; i++) {
		sprintf(string, "u_light[%d].light_on", i);
		loc_light[i].light_on = glGetUniformLocation(h_ShaderProgram_TXPS, string);
		sprintf(string, "u_light[%d].shadow_on", i);
		loc_light[i].shadow_on = glGetUniformLocation(h_ShaderProgram_TXPS, string);
		sprintf(string, "u_light[%d].position", i);
		loc_light[i].position = glGetUniformLocation(h_ShaderProgram_TXPS, string);
		sprintf(string, "u_light[%d].ambient_color", i);
		loc_light[i].ambient_color = glGetUniformLocation(h_ShaderProgram_TXPS, string);
		sprintf(string, "u_light[%d].diffuse_color", i);
		loc_light[i].diffuse_color = glGetUniformLocation(h_ShaderProgram_TXPS, string);
		sprintf(string, "u_light[%d].specular_color", i);
		loc_light[i].specular_color = glGetUniformLocation(h_ShaderProgram_TXPS, string);
		sprintf(string, "u_light[%d].spot_direction", i);
		loc_light[i].spot_direction = glGetUniformLocation(h_ShaderProgram_TXPS, string);
		sprintf(string, "u_light[%d].spot_exponent", i);
		loc_light[i].spot_exponent = glGetUniformLocation(h_ShaderProgram_TXPS, string);
		sprintf(string, "u_light[%d].spot_cutoff_angle", i);
		loc_light[i].spot_cutoff_angle = glGetUniformLocation(h_ShaderProgram_TXPS, string);
		sprintf(string, "u_light[%d].light_attenuation_factors", i);
		loc_light[i].light_attenuation_factors = glGetUniformLocation(h_ShaderProgram_TXPS, string);
	}

	loc_material.ambient_color = glGetUniformLocation(h_ShaderProgram_TXPS, "u_material.ambient_color");
	loc_material.diffuse_color = glGetUniformLocation(h_ShaderProgram_TXPS, "u_material.diffuse_color");
	loc_material.specular_color = glGetUniformLocation(h_ShaderProgram_TXPS, "u_material.specular_color");
	loc_material.emissive_color = glGetUniformLocation(h_ShaderProgram_TXPS, "u_material.emissive_color");
	loc_material.specular_exponent = glGetUniformLocation(h_ShaderProgram_TXPS, "u_material.specular_exponent");

	loc_base_texture = glGetUniformLocation(h_ShaderProgram_TXPS, "u_base_texture");
	for (i = 0; i < NUMBER_OF_LIGHT_COUNT; i++) {
		sprintf(string, "u_shadow_texture[%d]", i);
		loc_shadow_texture[i] = glGetUniformLocation(h_ShaderProgram_TXPS, string);
	}
	loc_flag_texture_mapping = glGetUniformLocation(h_ShaderProgram_TXPS, "u_flag_texture_mapping");
	loc_flag_fog = glGetUniformLocation(h_ShaderProgram_TXPS, "u_flag_fog");
	loc_flag_face = glGetUniformLocation(h_ShaderProgram_TXPS, "u_flag_face");

	//////////////////
	h_ShaderProgram_shadow = LoadShaders(shader_info_shadow);
	loc_ModelViewProjectionMatrix_simple = glGetUniformLocation(h_ShaderProgram_shadow, "u_ModelViewProjectionMatrix");

	/////////////////
	h_ShaderProgram_SHOW_SM = LoadShaders(shader_info_SHOW_SM);
	loc_ModelViewProjectionMatrix_SHOW_SM = glGetUniformLocation(h_ShaderProgram_SHOW_SM, "u_ModelViewProjectionMatrix");
	loc_shadow_texture_SHOW_SM = glGetUniformLocation(h_ShaderProgram_SHOW_SM, "u_shadow_texture");
	loc_near_clip_dist_SHOW_SM = glGetUniformLocation(h_ShaderProgram_SHOW_SM, "u_near_clip_dist");
	loc_far_clip_dist_SHOW_SM = glGetUniformLocation(h_ShaderProgram_SHOW_SM, "u_far_clip_dist");
}

void initialize_lights_and_material(void) { // follow OpenGL conventions for initialization
	int i;

	glUseProgram(h_ShaderProgram_TXPS);

	glUniform4f(loc_global_ambient_color, 0.115f, 0.115f, 0.115f, 1.0f);
	for (i = 0; i < NUMBER_OF_LIGHT_SUPPORTED; i++) {
		glUniform1i(loc_light[i].light_on, 0); // turn off all lights initially
		glUniform1i(loc_light[i].shadow_on, 0); // turn off all shadows initially
		glUniform4f(loc_light[i].position, 0.0f, 0.0f, 1.0f, 0.0f);
		glUniform4f(loc_light[i].ambient_color, 0.0f, 0.0f, 0.0f, 1.0f);
		if (i == 0) {
			glUniform4f(loc_light[i].diffuse_color, 1.0f, 1.0f, 1.0f, 1.0f);
			glUniform4f(loc_light[i].specular_color, 1.0f, 1.0f, 1.0f, 1.0f);
		}
		else {
			glUniform4f(loc_light[i].diffuse_color, 0.0f, 0.0f, 0.0f, 1.0f);
			glUniform4f(loc_light[i].specular_color, 0.0f, 0.0f, 0.0f, 1.0f);
		}
		glUniform3f(loc_light[i].spot_direction, 0.0f, 0.0f, -1.0f);
		glUniform1f(loc_light[i].spot_exponent, 0.0f); // [0.0, 128.0]
		glUniform1f(loc_light[i].spot_cutoff_angle, 180.0f); // [0.0, 90.0] or 180.0 (180.0 for no spot light effect)
		glUniform4f(loc_light[i].light_attenuation_factors, 1.0f, 0.0f, 0.0f, 0.0f); // .w != 0.0f for no ligth attenuation
	}

	glUniform4f(loc_material.ambient_color, 0.2f, 0.2f, 0.2f, 1.0f);
	glUniform4f(loc_material.diffuse_color, 0.8f, 0.8f, 0.8f, 1.0f);
	glUniform4f(loc_material.specular_color, 0.0f, 0.0f, 0.0f, 1.0f);
	glUniform4f(loc_material.emissive_color, 0.0f, 0.0f, 0.0f, 1.0f);
	glUniform1f(loc_material.specular_exponent, 0.0f); // [0.0, 128.0]

	for (i = 0; i < NUMBER_OF_LIGHT_COUNT; i++) {
		glUniform1i(loc_shadow_texture[i], ShadowMapping.texture_unit[i]);
	}
	glUseProgram(0);
}

void initialize_flags(void) {
	flag.texture_mapping = 0;
	flag.fog = 0;
	flag.polygon_fill = 1;
	flag.show_shadow_map = 0;
	flag.cull_face = 0;
	flag.camera = 0;
	flag.tiger_move = 1;
	flag.cam_move = 0;
	flag.tiger_speed = 1;

	glUseProgram(h_ShaderProgram_TXPS);
	glUniform1i(loc_flag_fog, flag.fog);
	glUniform1i(loc_flag_texture_mapping, flag.texture_mapping);
	glUseProgram(0);
}

void initialize_OpenGL(void) {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE);

	glFrontFace(GL_CCW);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

#define CAMERA_COORDINATE 400.0f
	realCam = glm::vec3(CAMERA_COORDINATE, CAMERA_COORDINATE, CAMERA_COORDINATE);
	ViewMatrix = glm::lookAt(realCam, glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f));

	initialize_lights_and_material();
	initialize_flags();

	glGenTextures(N_NORMAL_TEXTURES_USED, texture_names);
}

void set_up_scene_lights(void) {
	int i;
	glm::vec4 position_EC;

#define SF 1.0f
	light0_position_WC[0][0] = -SF * 500.0f;
	light0_position_WC[0][1] = SF * 600.0f;
	light0_position_WC[0][2] = SF * 400.0f;
	light0_position_WC[0][3] = 1.0f;
	light0_position_WC[1][0] = SF * 500.0f;
	light0_position_WC[1][1] = SF * 600.0f;
	light0_position_WC[1][2] = SF * 400.0f;
	light0_position_WC[1][3] = 1.0f;
	light0_lerp_t = 0.0f;

	// point_light_WC: use light 0
	light[0].light_on = 1;
	light[0].shadow_on = 0;
	for (i = 0; i < 4; i++)
		light[0].position[i] = (1.0f - light0_lerp_t)*light0_position_WC[0][i]
		+ light0_lerp_t * light0_position_WC[1][i]; // point light position in WC

	light[0].ambient_color[0] = 0.13f; light[0].ambient_color[1] = 0.13f;
	light[0].ambient_color[2] = 0.13f; light[0].ambient_color[3] = 1.0f;

	light[0].diffuse_color[0] = 0.5f; light[0].diffuse_color[1] = 0.5f;
	light[0].diffuse_color[2] = 0.5f; light[0].diffuse_color[3] = 1.5f;

	light[0].specular_color[0] = 0.8f; light[0].specular_color[1] = 0.8f;
	light[0].specular_color[2] = 0.8f; light[0].specular_color[3] = 1.0f;

	// spot_light_WC: do not use light 1 initially
	light[1].light_on = 0;
	light[1].shadow_on = 0;
	light[1].position[0] = 0.0f; light[1].position[1] = 550.0f; // spot light position in WC
	light[1].position[2] = -00.0f; light[1].position[3] = 1.0f;

	light[1].ambient_color[0] = 0.152f; light[1].ambient_color[1] = 0.152f;
	light[1].ambient_color[2] = 0.152f; light[1].ambient_color[3] = 1.0f;

	light[1].diffuse_color[0] = 0.572f; light[1].diffuse_color[1] = 0.572f;
	light[1].diffuse_color[2] = 0.572f; light[1].diffuse_color[3] = 1.0f;

	light[1].specular_color[0] = 0.772f; light[1].specular_color[1] = 0.772f;
	light[1].specular_color[2] = 0.772f; light[1].specular_color[3] = 1.0f;

	light[1].spot_direction[0] = 0.0f; light[1].spot_direction[1] = -500.0f; // spot light direction in WC
	light[1].spot_direction[2] = 00.0f;
	light[1].spot_cutoff_angle = 30.0f;
	light[1].spot_exponent = 8.0f;


	// spot_light_MC:

	light[2].light_on = 0;
	light[2].shadow_on = 0;
	light[2].position[0] = 30; light[2].position[1] = 250.0f; // spot light position in MC
	light[2].position[2] = 00; light[2].position[3] = 1.0f;

	light[2].ambient_color[0] = 0.152f; light[2].ambient_color[1] = 0.152f;
	light[2].ambient_color[2] = 0.152f; light[2].ambient_color[3] = 1.0f;

	light[2].diffuse_color[0] = 0.572f; light[2].diffuse_color[1] = 0.572f;
	light[2].diffuse_color[2] = 0.572f; light[2].diffuse_color[3] = 1.0f;

	light[2].specular_color[0] = 0.772f; light[2].specular_color[1] = 0.772f;
	light[2].specular_color[2] = 0.772f; light[2].specular_color[3] = 1.0f;

	light[2].spot_direction[0] = 300.0f; light[2].spot_direction[1] = -500.0f; // spot light direction in MC
	light[2].spot_direction[2] = 00.0f;
	light[2].spot_cutoff_angle = 20.0f;
	light[2].spot_exponent = 8.0f;


	glUseProgram(h_ShaderProgram_TXPS);
	glUniform1i(loc_light[0].light_on, light[0].light_on);
	glUniform1i(loc_light[0].shadow_on, light[0].shadow_on);
	// need to supply position in EC for shading
	position_EC = ViewMatrix * glm::vec4(light[0].position[0], light[0].position[1], light[0].position[2],
		light[0].position[3]);
	glUniform4fv(loc_light[0].position, 1, &position_EC[0]);
	glUniform4fv(loc_light[0].ambient_color, 1, light[0].ambient_color);
	glUniform4fv(loc_light[0].diffuse_color, 1, light[0].diffuse_color);
	glUniform4fv(loc_light[0].specular_color, 1, light[0].specular_color);

	glUniform1i(loc_light[1].light_on, light[1].light_on);
	glUniform1i(loc_light[1].shadow_on, light[1].shadow_on);
	// need to supply position in EC for shading
	position_EC = ViewMatrix * glm::vec4(light[1].position[0], light[1].position[1], light[1].position[2], light[1].position[3]);
	glUniform4fv(loc_light[1].position, 1, &position_EC[0]);
	glUniform4fv(loc_light[1].ambient_color, 1, light[1].ambient_color);
	glUniform4fv(loc_light[1].diffuse_color, 1, light[1].diffuse_color);
	glUniform4fv(loc_light[1].specular_color, 1, light[1].specular_color);
	// need to supply direction in EC for shading in this example shader
	// note that the viewing transform is a rigid body transform
	// thus transpose(inverse(mat3(ViewMatrix)) = mat3(ViewMatrix)
	glm::vec3 direction_EC = glm::mat3(ViewMatrix) * glm::vec3(light[1].spot_direction[0], light[1].spot_direction[1],
		light[1].spot_direction[2]);
	glUniform3fv(loc_light[1].spot_direction, 1, &direction_EC[0]);
	glUniform1f(loc_light[1].spot_cutoff_angle, light[1].spot_cutoff_angle);
	glUniform1f(loc_light[1].spot_exponent, light[1].spot_exponent);



	glUniform1i(loc_light[2].light_on, light[2].light_on);
	glUniform1i(loc_light[2].shadow_on, light[2].shadow_on);
	// need to supply position in EC for shading
	position_EC = ViewMatrix * glm::vec4(light[2].position[0], light[2].position[1], light[2].position[2], light[2].position[3]);
	glUniform4fv(loc_light[2].position, 1, &position_EC[0]);
	glUniform4fv(loc_light[2].ambient_color, 1, light[2].ambient_color);
	glUniform4fv(loc_light[2].diffuse_color, 1, light[2].diffuse_color);
	glUniform4fv(loc_light[2].specular_color, 1, light[2].specular_color);
	// need to supply direction in EC for shading in this example shader
	// note that the viewing transform is a rigid body transform
	// thus transpose(inverse(mat3(ViewMatrix)) = mat3(ViewMatrix)
	glm::vec3 direction_EC2 = glm::mat3(ViewMatrix) * glm::vec3(light[2].spot_direction[0], light[2].spot_direction[1],
		light[2].spot_direction[2]);
	glUniform3fv(loc_light[2].spot_direction, 1, &direction_EC2[0]);
	glUniform1f(loc_light[2].spot_cutoff_angle, light[2].spot_cutoff_angle);
	glUniform1f(loc_light[2].spot_exponent, light[2].spot_exponent);





	glUseProgram(0);
}

void prepare_scene(void) {
	prepare_axes();
	prepare_floor();
	prepare_tiger();
	prepare_OPTIMUS();
	prepare_dragon();
	prepare_sphere();
	prepare_video();
	set_up_scene_lights();


	//int i, n_bytes_per_vertex, n_bytes_per_triangle, tiger_n_total_triangles = 0;
	//char filename[512];

	//n_bytes_per_vertex = 8 * sizeof(float); // 3 for vertex, 3 for normal, and 2 for texcoord
	//n_bytes_per_triangle = 3 * n_bytes_per_vertex;

	//GLfloat* obj;


	//read_obj(&obj, n_bytes_per_triangle,"Data/sphere.obj");
}

void prepare_shadow_mapping(int index) {

	ShadowMapping.texture_unit[index] = TEXTURE_INDEX_SHADOW + index;
	ShadowMapping.shadow_map_width = SHADOW_MAP_WIDTH;
	ShadowMapping.shadow_map_height = SHADOW_MAP_HEIGHT;
	ShadowMapping.near_dist = SHADOW_MAP_NEAR_DIST;
	ShadowMapping.far_dist = SHADOW_MAP_FAR_DIST;
	ShadowMapping.shadow_map_border_color[0] = 1.0f;
	ShadowMapping.shadow_map_border_color[1] = 0.0f;
	ShadowMapping.shadow_map_border_color[2] = 0.0f;
	ShadowMapping.shadow_map_border_color[3] = 0.0f;

	glGenTextures(1, &ShadowMapping.shadow_map_ID[index]);

	glActiveTexture(GL_TEXTURE0 + ShadowMapping.texture_unit[index]);
	glBindTexture(GL_TEXTURE_2D, ShadowMapping.shadow_map_ID[index]);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT32, ShadowMapping.shadow_map_width, ShadowMapping.shadow_map_height);

	// Initialize the shadow map
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, ShadowMapping.shadow_map_border_color);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS); // or GL_LESS

	//변수 설정

	glUseProgram(h_ShaderProgram_TXPS);
	glUniform1i(loc_shadow_texture[index], ShadowMapping.texture_unit[index]);
	glUseProgram(0);

	// Initialize the Frame Buffer Object for rendering shadows
	glGenFramebuffers(1, &ShadowMapping.FBO_ID[index]);
	glBindFramebuffer(GL_FRAMEBUFFER, ShadowMapping.FBO_ID[index]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, ShadowMapping.shadow_map_ID[index], 0);
	glDrawBuffer(GL_NONE);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "Error: the framebuffer object for shadow mapping is not complete...\n");
		exit(-1);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	ViewMatrix_SHADOW[index] = glm::lookAt(glm::vec3(light[index].position[0], light[index].position[1], light[index].position[2]),
		glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	if (index == 0) {

		ProjectionMatrix_SHADOW = glm::perspective(TO_RADIAN*60.0f, 1.0f, ShadowMapping.near_dist, ShadowMapping.far_dist);
		BiasMatrix = glm::mat4(0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f);

		ModelViewProjectionMatrix = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

		glUseProgram(h_ShaderProgram_SHOW_SM);
		glUniform1i(loc_shadow_texture_SHOW_SM, ShadowMapping.texture_unit[0]);
		glUniformMatrix4fv(loc_ModelViewProjectionMatrix_SHOW_SM, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
		glUniform1f(loc_near_clip_dist_SHOW_SM, ShadowMapping.near_dist);
		glUniform1f(loc_far_clip_dist_SHOW_SM, ShadowMapping.far_dist);
		glUseProgram(0);
	}

}

void initialize_renderer(void) {
	register_callbacks();
	prepare_shader_program();
	initialize_OpenGL();
	prepare_scene();

	for (int i = 0; i < NUMBER_OF_LIGHT_COUNT; i++) {
		prepare_shadow_mapping(i);
	}


}

void initialize_glew(void) {
	GLenum error;

	glewExperimental = GL_TRUE;

	error = glewInit();
	if (error != GLEW_OK) {
		fprintf(stderr, "Error: %s\n", glewGetErrorString(error));
		exit(-1);
	}
	fprintf(stdout, "*********************************************************\n");
	fprintf(stdout, " - GLEW version supported: %s\n", glewGetString(GLEW_VERSION));
	fprintf(stdout, " - OpenGL renderer: %s\n", glGetString(GL_RENDERER));
	fprintf(stdout, " - OpenGL version supported: %s\n", glGetString(GL_VERSION));
	fprintf(stdout, "*********************************************************\n\n");
}

void greetings(char *program_name, char messages[][256], int n_message_lines) {
	fprintf(stdout, "**************************************************************\n\n");
	fprintf(stdout, "  PROGRAM NAME: %s\n\n", program_name);
	fprintf(stdout, "    This program was coded for CSE3170 students\n");
	fprintf(stdout, "      of Dept. of Comp. Sci. & Eng., Sogang University.\n\n");

	for (int i = 0; i < n_message_lines; i++)
		fprintf(stdout, "%s\n", messages[i]);
	fprintf(stdout, "\n**************************************************************\n\n");

	initialize_glew();
}

#define N_MESSAGE_LINES 1
void main(int argc, char *argv[]) {
	// Phong Shading
	char program_name[64] = "Sogang CSE4170 Tiger_Optimus_Shadows_GLSL";
	char messages[N_MESSAGE_LINES][256] = { "    - Keys used: '0', '1', 't', 'f', 's', 'd', 'r', 'l', 'ESC'" };

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_MULTISAMPLE);
	// glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(1200, 800);
	glutInitContextVersion(4, 0);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutCreateWindow(program_name);

	greetings(program_name, messages, N_MESSAGE_LINES);
	initialize_renderer();

	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
	glutMainLoop();
}