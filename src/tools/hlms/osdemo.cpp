
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include "shared/studiomodel/CStudioModel.h"
#include "engine/renderer/studiomodel/StudioSorting.h"
#include "engine/renderer/studiomodel/CStudioModelRenderer.h"
#include "engine/shared/renderer/studiomodel/IStudioModelRenderer.h"
#include "graphics/GraphicsUtils.h"
#include "graphics/GraphicsHelpers.h"

#ifdef EMSCRIPTEN
	#include <GL/glfw.h>
#else
	#define GLAPI extern
	#include <GL/osmesa.h>
	#include <GL/glu.h>     //replaced by default header in linux
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <glm/vec3.hpp> // glm::vec3
#include <glm/vec4.hpp> // glm::vec4, glm::ivec4
#include <glm/mat4x4.hpp> // glm::mat4
#include <glm/gtc/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale, glm::perspective
#include <glm/gtc/type_ptr.hpp> // glm::value_ptr

#include "lodepng.h"

namespace sprite
{
class ISpriteRenderer;
}

sprite::ISpriteRenderer* g_pSpriteRenderer = nullptr;
 
studiomdl::CStudioModelRenderer* g_pStudioMdlRenderer = new studiomdl::CStudioModelRenderer();

studiomdl::CStudioModel* loadModel(const char* path)
{
	studiomdl::CStudioModel* pModel;
	
	const auto res = studiomdl::LoadStudioModel( path, pModel );
	
	switch( res )
	{
	default:
	case studiomdl::StudioModelLoadResult::FAILURE:
		{
			printf("Error loading model\n");
			return NULL;
		}

	case studiomdl::StudioModelLoadResult::POSTLOADFAILURE:
		{
			printf("Error post-loading model\n");
			return NULL;
		}

	case studiomdl::StudioModelLoadResult::VERSIONDIFFERS:
		{
			printf("Error loading model: version differs\n");
			return NULL;
		}

	case studiomdl::StudioModelLoadResult::SUCCESS: break;
	}
	printf("Loaded model\n");
	
	return pModel;
}

struct RenderSettings {
	glm::vec3 origin;
	glm::vec3 angles;
	int seq;
	float frame;
	float scale;
	int width;
	int height;
	float fov;
	
	float idealFps;
	int seqFrames;
	
	RenderSettings() {
		origin = glm::vec3(0.0f, 72.0f, 0.0f);
		scale = 1.0f;
		seq = frame = 0;
		fov = 65.0f;
	}
};

glm::vec3 frustumCorners[8];

glm::vec3 getFrustumDimensionsAtPos(RenderSettings& settings, glm::vec3 pos) {
	// https://stackoverflow.com/questions/33436963/most-accurate-way-to-calculate-view-frustum-corners-in-world-space
	float mFOV = settings.fov*0.5f;
	float mNear = 1.0f;
	float mFar = 1024;
	float mRatio = (float)settings.width / (float)settings.height;
	glm::vec3 mPos = settings.origin * -1.0f;
	//mPos.z = 0;
	//mPos.y = -76;
	//mPos.x = -mPos.z + 0.2f;
	glm::vec3 mUp(1, 0, 0);
	glm::vec3 mFront(0, 1, 0);
	glm::vec3 mRight(0, 0, 1);
	
	float nearHeight = 2 * tan(mFOV / 2.0f) * mNear;
	float nearWidth = nearHeight * mRatio;

	float farHeight = 2 * tan(mFOV / 2.0f) * mFar;
	float farWidth = farHeight * mRatio;

	glm::vec3 fc = mPos + mFront * mFar;
	glm::vec3 nc = mPos + mFront * mNear;

	frustumCorners[0] = nc + (mUp * (nearHeight / 2.0f)) - (mRight * (nearWidth / 2.0f));
	frustumCorners[1] = nc + (mUp * (nearHeight / 2.0f)) + (mRight * (nearWidth / 2.0f));
	frustumCorners[2] = nc - (mUp * (nearHeight / 2.0f)) - (mRight * (nearWidth / 2.0f));
	frustumCorners[3] = nc - (mUp * (nearHeight / 2.0f)) + (mRight * (nearWidth / 2.0f));
	
	frustumCorners[4] = fc + (mUp * farHeight / 2.0f) - (mRight * farWidth / 2.0f);
	frustumCorners[5] = fc + (mUp * farHeight / 2.0f) + (mRight * farWidth / 2.0f);
	frustumCorners[6] = fc - (mUp * farHeight / 2.0f) - (mRight * farWidth / 2.0f);
	frustumCorners[7] = fc - (mUp * farHeight / 2.0f) + (mRight * farWidth / 2.0f);
	
	/*
	printf("\nFrustrum corners:\n");
	for (int i = 0; i < 8; i++) {
		printf("[%i] = (%.2f, %.2f, %.2f)\n", i, frustumCorners[i].x, frustumCorners[i].y, frustumCorners[i].z);
	}
	*/
	
	float near = frustumCorners[0].y;
	float far = frustumCorners[4].y;
	nearHeight = frustumCorners[0].x - frustumCorners[2].x;
	nearWidth = -(frustumCorners[0].z - frustumCorners[1].z);
	farHeight = frustumCorners[4].x - frustumCorners[6].x;
	farWidth = -(frustumCorners[4].z - frustumCorners[5].z);
	
	//printf("Near height: %.2f\n", nearHeight);
	//printf("Far height: %.2f\n", farHeight);
	
	
	float distance = (near-(-pos.y)) / (near-far); // 0-1 where 1 is touching the far plane
	//printf("Distance: %.2f\n", distance);
	
	float posWidth = nearWidth + (farWidth-nearWidth)*distance;
	float posHeight = nearHeight + (farHeight-nearHeight)*distance;
	
	return glm::vec3(posWidth, posHeight, 0);
}

void draw_cube(glm::vec3 min, glm::vec3 max) {
	glm::vec3 v1 = glm::vec3(min.x, min.y, min.z);
	glm::vec3 v2 = glm::vec3(max.x, min.y, min.z);
	glm::vec3 v3 = glm::vec3(max.x, max.y, min.z);
	glm::vec3 v4 = glm::vec3(min.x, max.y, min.z);
	
	
	glm::vec3 v5 = glm::vec3(min.x, min.y, max.z);
	glm::vec3 v6 = glm::vec3(max.x, min.y, max.z);
	glm::vec3 v7 = glm::vec3(max.x, max.y, max.z);
	glm::vec3 v8 = glm::vec3(min.x, max.y, max.z);
	
	
	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	glDisable( GL_TEXTURE_2D );
	glDisable( GL_CULL_FACE );
	glDisable( GL_DEPTH_BUFFER );
	glBegin(GL_LINE_STRIP);		
		glColor3f( 1, 0, 0 ); glVertex3f(v1.x, v1.y, v1.z);
		glColor3f( 1, 0, 0 ); glVertex3f(v2.x, v2.y, v2.z);
		glColor3f( 1, 0, 0 ); glVertex3f(v3.x, v3.y, v3.z);
		glColor3f( 1, 0, 0 ); glVertex3f(v4.x, v4.y, v4.z);
		glColor3f( 1, 0, 0 ); glVertex3f(v1.x, v1.y, v1.z);
	glEnd();

	glBegin(GL_LINE_STRIP);	
		glColor3f( 1, 0, 0 ); glVertex3f(v5.x, v5.y, v5.z);
		glColor3f( 1, 0, 0 ); glVertex3f(v6.x, v6.y, v6.z);
		glColor3f( 1, 0, 0 ); glVertex3f(v7.x, v7.y, v7.z);
		glColor3f( 1, 0, 0 ); glVertex3f(v8.x, v8.y, v8.z);
		glColor3f( 1, 0, 0 ); glVertex3f(v5.x, v5.y, v5.z);
	glEnd();
	
	glBegin(GL_LINES);	
		glColor3f( 1, 0, 0 ); glVertex3f(v1.x, v1.y, v1.z);
		glColor3f( 1, 0, 0 ); glVertex3f(v5.x, v5.y, v5.z);
		
		glColor3f( 1, 0, 0 ); glVertex3f(v2.x, v2.y, v2.z);
		glColor3f( 1, 0, 0 ); glVertex3f(v6.x, v6.y, v6.z);
		
		glColor3f( 1, 0, 0 ); glVertex3f(v3.x, v3.y, v3.z);
		glColor3f( 1, 0, 0 ); glVertex3f(v7.x, v7.y, v7.z);
		
		glColor3f( 1, 0, 0 ); glVertex3f(v4.x, v4.y, v4.z);
		glColor3f( 1, 0, 0 ); glVertex3f(v8.x, v8.y, v8.z);
	glEnd();
}


static void render_model(studiomdl::CStudioModel* mdl, RenderSettings settings, bool extentOnly=false)
{
	bool backfaceCulling = true;
	//RenderMode renderMode = RenderMode::TEXTURE_SHADED;
	RenderMode renderMode = RenderMode::TEXTURE_SHADED;
	
	auto mat = Mat4x4ModelView();
	
	if (!extentOnly) {
		//glClearColor( 63.0f / 255.0f, 127.0f / 255.0f, 127.0f / 255.0f, 1.0 );
		glClearColor( 0.2,0.4,0.4,0 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		glViewport( 0, 0, settings.width, settings.height );
		
		// set up projection
		glMatrixMode( GL_PROJECTION );
		glLoadIdentity();
		gluPerspective( settings.fov, ( GLfloat ) settings.width / ( GLfloat ) settings.height, 1.0f, 4096 );
		
		
		
		glMatrixMode( GL_MODELVIEW );
		glPushMatrix();
		glLoadIdentity();
		
		// setup camera
		glm::vec3 origin = glm::vec3(-settings.origin.z, settings.origin.y, settings.origin.x);
		glm::vec3 angles = glm::vec3(-90.0f, 0.0f, -90.0f);
		const glm::mat4x4 identity = Mat4x4ModelView();
		//auto mat = Mat4x4ModelView();
		mat = glm::translate(mat, -origin );
		mat = glm::rotate(mat, glm::radians( angles[ 2 ] ), glm::vec3{ 1, 0, 0 } );
		mat = glm::rotate(mat, glm::radians( angles[ 0 ] ), glm::vec3{ 0, 1, 0 } );
		mat = glm::rotate(mat, glm::radians( angles[ 1 ] ), glm::vec3{ 0, 0, 1 } );
		glLoadMatrixf( (const GLfloat*)&mat[0] );
		
		// more camera maybe?
		auto mat2 = Mat4x4ModelView();
		mat2 = glm::translate(mat2, -origin );
		mat2 = glm::rotate(mat2, glm::radians( angles[ 2 ] ), glm::vec3{ 1, 0, 0 } );
		mat2 = glm::rotate(mat2, glm::radians( angles[ 0 ] ), glm::vec3{ 0, 1, 0 } );
		mat2 = glm::rotate(mat2, glm::radians( angles[ 1 ] ), glm::vec3{ 0, 0, 1 } );
		const auto vecAbsOrigin = glm::inverse( mat2 )[ 3 ];
		g_pStudioMdlRenderer->SetViewerOrigin( glm::vec3( vecAbsOrigin ) );	

		glm::vec3 angViewerDir = glm::vec3(90.0f, 0.0f, 90.0f);
		angViewerDir = angViewerDir + 180.0f;
		glm::vec3 vecViewerRight;
		
		//We're using the up vector here since the in-game look can only be matched if chrome is rotated.
		AngleVectors( angViewerDir, nullptr, nullptr, &vecViewerRight );
		
		//Invert it so it points down instead of up. This allows chrome to match the in-game look.
		g_pStudioMdlRenderer->SetViewerRight( -vecViewerRight );
		
		graphics::helpers::SetupRenderMode( renderMode, backfaceCulling );
		
		//Determine if an odd number of scale values are negative. The cull face has to be changed if so.
		const glm::vec3& vecScale = glm::vec3(1,1,1);
		const float flScale = vecScale.x * vecScale.y * vecScale.z;
		glCullFace( flScale > 0 ? GL_FRONT : GL_BACK );
	
	}
	
	// setup model for rendering
	{
		renderer::DrawFlags_t flags = 0;
		
		studiomdl::CModelRenderInfo renderInfo;

		renderInfo.vecOrigin = glm::vec3(0,0,0);
		renderInfo.vecAngles = settings.angles;
		renderInfo.vecScale = glm::vec3(settings.scale,settings.scale,settings.scale);

		renderInfo.pModel = mdl;

		renderInfo.flTransparency = 1.0f;
		renderInfo.iSequence = settings.seq;
		renderInfo.flFrame = settings.frame;
		renderInfo.iBodygroup = 0;
		renderInfo.iSkin = 0;

		for( int iIndex = 0; iIndex < 2; ++iIndex )
		{
			renderInfo.iBlender[ iIndex ] = 0;
		}

		for( int iIndex = 0; iIndex < 4; ++iIndex )
		{
			renderInfo.iController[ iIndex ] = 127;
		}

		renderInfo.iMouth = 0;

		g_pStudioMdlRenderer->DrawModel( &renderInfo, flags );
	}
	
	int drawnPolys = g_pStudioMdlRenderer->GetDrawnPolygonsCount();
	
	glm::vec3 min = g_pStudioMdlRenderer->m_drawnCoordMin;
	glm::vec3 max = g_pStudioMdlRenderer->m_drawnCoordMax;
	min *= -0.2f;
	max *= -0.2f;
	//glm::vec4 min4 = glm::vec4(min.y, min.x, min.z, 1.0f);
	//glm::vec4 max4 = glm::vec4(max.y, max.x, max.z, 1.0f);
	//min = glm::vec3(min.y, min.x, min.z);
	//max = glm::vec3(max.y, max.x, max.z);
	
	glm::vec3 angles = glm::vec3(-90.0f, settings.angles.y, -90.0f);
	
	printf("Bounding box: (%f, %f, %f) (%f, %f, %f)\n", min.x, min.y, min.z, max.x, max.y, max.z);
	
	mat = Mat4x4ModelView();
	mat = glm::rotate(mat, glm::radians( angles[ 2 ] ), glm::vec3{ 1, 0, 0 } );
	mat = glm::rotate(mat, glm::radians( angles[ 0 ] ), glm::vec3{ 0, 1, 0 } );
	mat = glm::rotate(mat, glm::radians( angles[ 1 ] ), glm::vec3{ 0, 0, 1 } );
	mat = glm::translate(mat, min);
	min = glm::vec3( mat[3] );
	
	mat = Mat4x4ModelView();
	mat = glm::rotate(mat, glm::radians( angles[ 2 ] ), glm::vec3{ 1, 0, 0 } );
	mat = glm::rotate(mat, glm::radians( angles[ 0 ] ), glm::vec3{ 0, 1, 0 } );
	mat = glm::rotate(mat, glm::radians( angles[ 1 ] ), glm::vec3{ 0, 0, 1 } );
	mat = glm::translate(mat, max);
	max = glm::vec3( mat[3] );
	
	//min4 = (mat * min4);
	//max4 = (mat * max4);
	
	//min4 = (min4 * mat);
	//max4 = (max4 * mat);
	
	//min = glm::vec3(min4.x, min4.y, min4.z);
	//max = glm::vec3(max4.x, max4.y, max4.z);
	
	draw_cube(min, max);
	
	if (!extentOnly)
		glPopMatrix();
}

void center_and_scale_model(RenderSettings& settings)
{
	// update camera to center and zoom on the model
	glm::vec3 min = g_pStudioMdlRenderer->m_drawnCoordMin;
	glm::vec3 max = g_pStudioMdlRenderer->m_drawnCoordMax;
	glm::vec3 center = min + (max - min)*0.5f;
	glm::vec3 extentMax = center - min;
	glm::vec3 extentMin = max - center;
	
	// the idea is to calculate how big the model can be and still fit on the screen, but this isn't the way to do it.
	// TODO: calculate bounding cylinder from model (rotated bounding box) and scale that to match the view frustrum height.
	float maxX = glm::max(fabs(extentMin.x), fabs(extentMax.x));
	float maxY = glm::max(fabs(extentMin.y), fabs(extentMax.y));
	float maxZ = glm::max(fabs(extentMin.z), fabs(extentMax.z));
	float maxDimH = glm::max(maxX, maxY);
	float maxDimV = maxZ;
	
	glm::vec3 frustumDims = getFrustumDimensionsAtPos(settings, max);
	
	//printf("Frustum dims at %.2f: (%.2f, %.2f)\n", min.y, frustumDims.x, frustumDims.y);
	
	float idealMaxDimH = frustumDims.x/2.0f;
	float idealMaxDimV = frustumDims.y/2.0f;
	float scaleV = idealMaxDimV / maxDimV;
	float scaleH = idealMaxDimH / maxDimH;
	float scale = glm::min(scaleV, scaleH);
	
	//printf("Max extents: (%f, %f), Ideal extent: (%f, %f), scale: %f\n", maxDimV, maxDimH, idealMaxDimV, idealMaxDimH, scale);
	//printf("Bounding box: (%f, %f, %f) (%f, %f, %f)\n", min.x, min.y, min.z, max.x, max.y, max.z);
	
	min *= scale;
	max *= scale;
	center = min + (max - min)*0.5f;
	
	//settings.origin = glm::vec3(0, 72.0f, center.z);
	settings.scale = scale;
	settings.scale = 0.2f;
}

#ifdef EMSCRIPTEN

int init_webgl(int width, int height) {
	if (glfwInit() != GL_TRUE) {
		printf("glfwInit() failed\n");
		return 1;
	}

	if (glfwOpenWindow(width, height, 8, 8, 8, 8, 16, 0, GLFW_WINDOW) != GL_TRUE) {
		printf("glfwOpenWindow() failed\n");
		return 1;
	}
	
	return 0;
}

#include <emscripten/emscripten.h>
#include <sys/time.h>
#include <time.h>
#include <deque>

float angleStep = 1.0f;

RenderSettings settings;

studiomdl::CStudioModel* mdl = NULL;
studiomdl::CStudioModel* mdlTemp;
static volatile bool newModelReady = false;
static volatile bool shouldUnloadCurrentModel = false;

int width = 500;
int height = 800;

typedef unsigned long long uint64;

uint64 getSystemTime()
{
	timespec t, res;
	clock_gettime(CLOCK_MONOTONIC_RAW, &t);
	return t.tv_sec*1000000 + t.tv_nsec/1000;
}

uint64 lastFrameTime;
std::deque<float> framerates;
bool program_ready = false;


void em_loop() {
	
	if (shouldUnloadCurrentModel) {
		printf("Unloading model\n");
		if (mdl) {
			delete mdl;
			mdl = NULL;
		}
		glClearColor( 0,0,0, 0 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		shouldUnloadCurrentModel = false;
	}
	
	if (newModelReady) {
		if (mdl) delete mdl;
		mdl = mdlTemp;
		newModelReady = false;
		g_pStudioMdlRenderer->m_drawnCoordMin = g_pStudioMdlRenderer->m_drawnCoordMax = glm::vec3(0,0,0);
		
		studiohdr_t* header = mdl->GetStudioHeader();
		settings.idealFps = header->GetSequence(0)->fps;
		settings.seqFrames = header->GetSequence(0)->numframes;
		printf("Sequence frames: %i, FPS: %f\n", header->GetSequence(0)->numframes, header->GetSequence(0)->fps);
		
		// calculate model size then render again to correct the camera position
		em_loop(); settings.angles.y = 0;
		em_loop(); settings.angles.y = 0;
		
		// call js function to signal loading finished
		EM_ASM(
			hlms_model_load_complete(true);
		);
	}
	
	// for some reason this doesn't work in the main method
	if (!program_ready) {
		program_ready = true;
		EM_ASM(
			hlms_ready();
		);
	}
	
	if (!mdl) {
		return; // wait for a model to be loaded
	}
	
	uint64 now = emscripten_get_now();
	uint64 delta = now - lastFrameTime;
	lastFrameTime = now;
	if (delta == 0) delta = 1;
	float framerate = 1000.0f / (float)delta;
	framerates.push_back(framerate);
	
	if (framerates.size() > 16) {
		framerates.pop_front();
	}
	float avgFramerate = 0;
	for (int i = 0; i < framerates.size(); i++) {
		avgFramerate += framerates[i];
	}
	avgFramerate /= (float)framerates.size();

	if (settings.seqFrames > 1 && false) {
		settings.frame += settings.idealFps / framerate;
		while (settings.frame >= settings.seqFrames-1) {
			settings.frame -= (settings.seqFrames-1);
		}
	}

	settings.angles.y += 90.0f / framerate;
	while (settings.angles.y > 360) {
		settings.angles.y -= 360;
	}
	
	//printf("Dat time: %.1f\n", avgFramerate);

	render_model(mdl, settings);

	center_and_scale_model(settings);
}

bool file_exists(std::string fpath) {
	if (FILE *file = fopen(fpath.c_str(), "r"))
	{
		fclose(file);
		return true;
	}
	return false;
}


void download_file(std::string url, std::string local_file)
{
	if (file_exists(local_file))
	{
		printf("Loading %s from cache\n", local_file.c_str());
	} else {
		printf("Downloading file %s -> %s\n", url.c_str(), local_file.c_str());
		emscripten_wget(url.c_str(), local_file.c_str());
	}
}

// functions to be called from javascript
extern "C" {
	EMSCRIPTEN_KEEPALIVE void load_new_model(const char* model_folder_url, const char* model_name, const char* t_model, int seq_groups) {		
		std::string model_folder_url_str(model_folder_url);
		std::string model_name_str(model_name);
		std::string mdl_path = model_name_str + ".mdl";
		std::string t_mdl_path = std::string(t_model);
		
		std::string mdl_url = model_folder_url_str + mdl_path;
		
		download_file(mdl_url, mdl_path);
		if (t_mdl_path.size() > 0) {
			download_file(model_folder_url_str + t_mdl_path, t_mdl_path);
		}
		for (int i = 1; i < seq_groups; i++) {
			std::string seq_mdl_path = model_name_str;
			if (i < 10)
				seq_mdl_path += "0";
			seq_mdl_path += std::to_string(i) + ".mdl";
			download_file(model_folder_url_str + seq_mdl_path, seq_mdl_path);
		}

		mdlTemp = loadModel(mdl_path.c_str());
		newModelReady = mdlTemp != NULL;
		
		if (!newModelReady) {
			EM_ASM(
				hlms_model_load_complete(false);
			);
		}
	}
	
	EMSCRIPTEN_KEEPALIVE void unload_model() {
		shouldUnloadCurrentModel = true;
	}
}

int main( int argc, char *argv[] )
{
	setbuf(stdout, NULL);
	
	init_webgl(width, height);
	
	settings.width = width;
	settings.height = height;
	
	emscripten_set_main_loop(em_loop, 0, 1);
	
	return 0;
}
#else

unsigned char *buffer;

int init_mesa(int width, int height) {
	/* Create an RGBA-mode context */
	#if OSMESA_MAJOR_VERSION * 100 + OSMESA_MINOR_VERSION >= 305
	/* specify Z, stencil, accum sizes */
		OSMesaContext ctx = OSMesaCreateContextExt( GL_RGBA, 16, 0, 0, NULL );
	#else
		OSMesaContext ctx = OSMesaCreateContext( GL_RGBA, NULL );
	#endif
	if (!ctx) {
		printf("OSMesaCreateContext failed!\n");
		return 1;
	}

	/* Allocate the image buffer */
	buffer = (unsigned char *) malloc( width * height * 4 * sizeof(unsigned char));
	if (!buffer) {
		printf("Alloc image buffer failed!\n");
		return 1;
	}

	/* Bind the buffer to the context and make it current */
	if (!OSMesaMakeCurrent( ctx, buffer, GL_UNSIGNED_BYTE, width, height )) {
		printf("OSMesaMakeCurrent failed!\n");
		return 1;
	}
}

static void write_png(const char *filename, const unsigned char *buffer, int width, int height)
{
	bool downscale = false;
	unsigned char * imgBuffer = NULL;
	
	// flip opengl image
	if (downscale) {
		// flip opengl image and downsample
		// TODO: something better. This still has some jaggies, unlike imagemagick.
		imgBuffer = new unsigned char[width*height];
		unsigned int idx = 0;
		for (int y = height-2; y >= 0; y -= 2) {
			for (int x = 0; x < width-1; x += 2) {
				int i = (y*width + x)*4;
				int i2 = (y*width + x+1)*4;
				int i3 = ((y+1)*width + x)*4;
				int i4 = ((y+1)*width + x+1)*4;
				
				int r = (buffer[i+0] + buffer[i2+0] + buffer[i3+0] + buffer[i4+0]) / 4;
				int g = (buffer[i+1] + buffer[i2+1] + buffer[i3+1] + buffer[i4+1]) / 4;
				int b = (buffer[i+2] + buffer[i2+2] + buffer[i3+2] + buffer[i4+2]) / 4;
				int a = (buffer[i+3] + buffer[i2+3] + buffer[i3+3] + buffer[i4+3]) / 4;
				
				imgBuffer[idx++] = r;
				imgBuffer[idx++] = g;
				imgBuffer[idx++] = b;
				imgBuffer[idx++] = a;
			}
		}
		width /= 2;
		height /= 2;
	} else {
		imgBuffer = new unsigned char[width*height*4];
		unsigned int idx = 0;
		for (int y = height-1; y >= 0; y -= 1) {
			for (int x = 0; x < width; x += 1) {
				// copy RGBA all at once
				((unsigned int*)imgBuffer)[idx++] = ((unsigned int*)buffer)[y*width + x];
			}
		}
	}
	
	printf(".");
	
	unsigned error = lodepng::encode(filename, imgBuffer, width, height);
	if(error)
		printf("PNG encode error: %s\n", lodepng_error_text(error));
}

void print_model_info(studiomdl::CStudioModel* mdl) {
	studiohdr_t* header = mdl->GetStudioHeader();

	int tri_count = 0;
	// TODO: buffer overflow safety
	for (int b = 0; b < header->numbodyparts; b++) {
		mstudiobodyparts_t* bod = header->GetBodypart(b);
		for (int i = 0; i < bod->nummodels; i++)
		{
			mstudiomodel_t* mod = reinterpret_cast<mstudiomodel_t*>( header->GetData() + bod->modelindex + i*sizeof(mstudiomodel_t));
			
			for (int k = 0; k < mod->nummesh; k++) {
				mstudiomesh_t* mesh = reinterpret_cast<mstudiomesh_t*>( header->GetData() + mod->meshindex + k*sizeof(mstudiomesh_t));
				tri_count += mesh->numtris;
			}
		}
	}
	bool uses_t_model = header->numtextures == 0 && tri_count > 0;
	
	std::string sequence_names;
	
	std::vector<std::string> events;
	for (int i = 0; i < header->numseq; i++)
	{
		mstudioseqdesc_t* seq = header->GetSequence(i);
		
		std::string label = seq->label;
		if (sequence_names.length() == 0) {
			sequence_names = label;
		} else {
			sequence_names += "|" + label;
		}
	
		for (int k = 0; k < seq->numevents; k++)
		{
			mstudioevent_t* evt = reinterpret_cast<mstudioevent_t*>( header->GetData() + seq->eventindex + k*sizeof(mstudioevent_t));
			// TODO: buffer overflow safety

			std::string opt = evt->options;
			if (opt.length() == 0)
				continue;

			if (evt->event == 1004 || evt->event == 1008 || evt->event == 5004) // play sound
			{
				std::string snd = evt->options;
				if (snd[0] == '*')
					snd = snd.substr(1); // not sure why some models do this but it looks pointless.
				
				events.push_back(std::to_string(i) + "|" + std::to_string(evt->frame) + "|" + snd);
			}
			// TODO: muzzleflash configs? Are those even possible in player models?
		}
	}
	
	printf("!BEGIN_MODEL_INFO!\n");
	printf("tri_count=%i\n", tri_count);
	printf("t_model=%i\n", uses_t_model);
	printf("seq_groups=%i\n", header->numseqgroups);
	for (int i = 0; i < events.size(); i++) {
		printf("event=%s\n", events[i].c_str());
	}
	printf("!END_MODEL_INFO!\n");
}

int main( int argc, char *argv[] )
{
	setbuf(stdout, NULL);
	RenderSettings settings;
	bool infoMode = false;
	
	if (argc == 2) {
		infoMode = true;
	} else if (argc == 7) {
		infoMode = false;
	} else {
		printf("Wrong number of arguments (%i)\n\n", argc);
		
		printf("Usage:\n");
		printf("    hlms [model_name] [output_image_basename] [widthxheight] [sequence] [frames] [loops]\n");
		printf("    hlms [model_name]\n");
		return 1;
	}
	
	const char* mdlName = argv[1];
	if (infoMode) {
		// just print some info about the model
		studiomdl::CStudioModel* mdl = loadModel(mdlName);
		if (!mdl) {
			return 1;
		}
		print_model_info(mdl);
		return 0;
	}
	
	const char* imgName = argv[2];
	std::string dimstr = std::string(argv[3]);
	settings.seq = atoi(argv[4]);
	settings.frame = 0;
	int frames = atoi(argv[5]);
	int loops = atoi(argv[6]);
	int width = 500;
	int height = 800;
	
	std::size_t xidx = dimstr.find_first_of("x");
	if (xidx != std::string::npos) {
		width = atoi(dimstr.substr(0,xidx).c_str());
		height = atoi(dimstr.substr(xidx+1).c_str());
	} else {
		printf("Failed to read frame dimensions: %s\n", dimstr.c_str());
	}
	settings.width = width;
	settings.height = height;

	init_mesa(width, height);
	
	studiomdl::CStudioModel* mdl = loadModel(mdlName);
	if (!mdl) {
		return 1;
	}
	
	printf("Frame dimensions: %ix%i\n", width, height);
	
	printf("Animation frames: %i, sequence loops: %i\n", frames, loops);	
	
	printf("Calculating model extents...");
	float angleStep = 360.0f / (float)frames;
	for (int i = 0; i < frames; i++) {
		render_model(mdl, settings, true);
		settings.angles.y += angleStep;
	}
	printf("DONE\n");
	
	// now that we know how much space the model uses, draw again with a centered camera and create the images
	center_and_scale_model(settings);
		
	settings.angles = glm::vec3(0,0,0);
	for (int i = 0; i < frames; i++) {
		printf("Render frame %i/%i...", i+1, frames);
		render_model(mdl, settings);
		
		std::string idx = std::to_string(i);
		if (i < 100) {
			idx = "0" + idx;
		}
		if (i < 10) {
			idx = "0" + idx;
		}
		
		printf(" Write PNG..", i+1, frames);
		std::string fname = std::string(imgName) + idx + ".png";
		write_png(fname.c_str(), buffer, width, height);
		settings.angles.y += angleStep;
		printf("DONE\n", i+1, frames);
	}

	/* free the image buffer */
	free( buffer );

	//OSMesaDestroyContext( ctx );
 
   return 0;
}
#endif