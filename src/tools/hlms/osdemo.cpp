
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
	
	float idealFps;
	int seqFrames;
	
	RenderSettings() {
		origin = glm::vec3(0.974525452f, 71.9231415f, 0.0f);
		scale = 1.0f;
		seq = frame = 0;
	}
};

static void render_model(studiomdl::CStudioModel* mdl, RenderSettings settings, bool extentOnly=false)
{
	float flFOV = 65.0f;
	bool backfaceCulling = true;
	//RenderMode renderMode = RenderMode::TEXTURE_SHADED;
	RenderMode renderMode = RenderMode::TEXTURE_SHADED;
	
	if (!extentOnly) {
		//glClearColor( 63.0f / 255.0f, 127.0f / 255.0f, 127.0f / 255.0f, 1.0 );
		glClearColor( 0,0,0, 0 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		glViewport( 0, 0, settings.width, settings.height );
		
		// set up projection
		glMatrixMode( GL_PROJECTION );
		glLoadIdentity();
		gluPerspective( flFOV, ( GLfloat ) settings.width / ( GLfloat ) settings.height, 1.0f, 1 << 24 );
		
		glMatrixMode( GL_MODELVIEW );
		glPushMatrix();
		glLoadIdentity();
		
		// setup camera
		glm::vec3 origin = glm::vec3(-settings.origin.z, settings.origin.y, settings.origin.x);
		glm::vec3 angles = glm::vec3(-90.0f, 0.0f, -90.0f);
		const glm::mat4x4 identity = Mat4x4ModelView();
		auto mat = Mat4x4ModelView();
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
	
		/*
		float fmodelview[16];
		glGetFloatv(GL_MODELVIEW_MATRIX, fmodelview);
		float fprojection[16];
		glGetFloatv(GL_PROJECTION_MATRIX, fprojection);
		glm::mat4 modelview = glm::mat4(fmodelview);
		glm::mat4 projection = glm::mat4(fprojection);
		//glm::vec3 frustrum_far_min = 
		*/
		
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
	float idealMaxDimV = 36.0f; // magic usually-works frustrum height.
	//float idealMaxDimH = idealMaxDimV * ((float)width/(float)height);
	float idealMaxDimH = 22.5f;
	float scaleV = idealMaxDimV / maxDimV;
	float scaleH = idealMaxDimH / maxDimH;
	float scale = glm::min(scaleV, scaleH);
	
	//printf("Max extents: (%f, %f), Ideal extent: (%f, %f), scale: %f\n", maxDimV, maxDimH, idealMaxDimV, idealMaxDimH, scale);
	//printf("Bounding box: (%f, %f, %f) (%f, %f, %f)\n", min.x, min.y, min.z, max.x, max.y, max.z);
	
	min *= scale;
	max *= scale;
	center = min + (max - min)*0.5f;
	
	settings.origin = glm::vec3(0, 75.0f, center.z);
	settings.scale = scale;
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

	if (settings.seqFrames > 1) {
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

// functions to be called from javascript
extern "C" {
	EMSCRIPTEN_KEEPALIVE void load_new_model(const char* url, const char* fpath) {		
		if (FILE *file = fopen(fpath, "r"))
		{
			fclose(file);
			printf("Loading %s from cache\n", fpath);
		} else {
			printf("Downloading file %s -> %s\n", url, fpath);
			emscripten_wget(url, fpath);
		}

		mdlTemp = loadModel(fpath);
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
	printf("HEWO WOYL\n");
	setbuf(stdout, NULL);
	
	printf("Prepare to init webgl\n");
	
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

int main( int argc, char *argv[] )
{
	setbuf(stdout, NULL);
	RenderSettings settings;
	
	if (argc != 7) {
		printf("Usage:\n");
		printf("hlms [model_name] [output_image_basename] [widthxheight] [sequence] [frames] [loops]\n");
		return 1;
	}
	
	const char* mdlName = argv[1];
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
	
	printf("Frame dimensions: %ix%i\n", width, height);
	
	printf("Animation frames: %i, sequence loops: %i\n", frames, loops);
	
	studiomdl::CStudioModel* mdl = loadModel(mdlName);
	if (!mdl) {
		return 1;
	}
	
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