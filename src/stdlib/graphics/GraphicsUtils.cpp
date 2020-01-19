#include <cassert>

#include <glm/gtc/type_ptr.hpp>

#include "shared/Const.h"

#include "shared/studiomodel/studio.h"

#include "GraphicsUtils.h"

namespace graphics
{
bool CalculateImageDimensions( const int iWidth, const int iHeight, int& iOutWidth, int& iOutHeight )
{
	if( iWidth <= 0 || iHeight <= 0 )
		return false;

	for( iOutWidth = 1; iOutWidth < iWidth; iOutWidth <<= 1 )
	{
	}

	if( iOutWidth > MAX_TEXTURE_DIMS )
		iOutWidth = MAX_TEXTURE_DIMS;

	for( iOutHeight = 1; iOutHeight < iHeight; iOutHeight <<= 1 )
	{
	}

	if( iOutHeight > MAX_TEXTURE_DIMS )
		iOutHeight = MAX_TEXTURE_DIMS;

	return true;
}

void Convert8to24Bit( const int iWidth, const int iHeight, const byte* const pData, const byte* const pPalette, byte* const pOutData )
{
	assert( pData );
	assert( pPalette );
	assert( pOutData );

	byte* pOut = pOutData;

	for( int y = 0; y < iHeight; ++y )
	{
		for( int x = 0; x < iWidth; ++x, pOut += 3 )
		{
			pOut[ 0 ] = pPalette[ pData[ x + y * iWidth ] * 3 ];
			pOut[ 1 ] = pPalette[ pData[ x + y * iWidth ] * 3 + 1 ];
			pOut[ 2 ] = pPalette[ pData[ x + y * iWidth ] * 3 + 2 ];
		}
	}
}

void FlipImageVertically( const int iWidth, const int iHeight, byte* const pData )
{
	assert( iWidth > 0 );
	assert( iHeight > 0 );
	assert( pData );

	const int iHalfHeight = iHeight / 2;

	for( int y = iHalfHeight; y < iHeight; ++y )
	{
		for( int x = 0; x < iWidth; ++x )
		{
			for( int i = 0; i < 3; ++i )
			{
				std::swap( pData[ ( x + y * iWidth ) * 3 + i ], pData[ ( x + ( iHeight - y - 1 ) * iWidth ) * 3 + i ] );
			}
		}
	}
}

void DrawBackground( GLuint backgroundTexture )
{
	if( backgroundTexture == GL_INVALID_TEXTURE_ID )
		return;

	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();

	glOrtho( 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, -1.0f );

	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glLoadIdentity();

	glDisable( GL_CULL_FACE );
	glEnable( GL_TEXTURE_2D );

	glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	glBindTexture( GL_TEXTURE_2D, backgroundTexture );

	glBegin( GL_TRIANGLE_STRIP );

	glTexCoord2f( 0, 0 );
	glVertex2f( 0, 0 );

	glTexCoord2f( 1, 0 );
	glVertex2f( 1, 0 );

	glTexCoord2f( 0, 1 );
	glVertex2f( 0, 1 );

	glTexCoord2f( 1, 1 );
	glVertex2f( 1, 1 );

	glEnd();

	glPopMatrix();

	glClear( GL_DEPTH_BUFFER_BIT );
	glBindTexture( GL_TEXTURE_2D, 0 );
}

void SetProjection( const float flFOV, const int iWidth, const int iHeight )
{
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	gluPerspective( flFOV, ( GLfloat ) iWidth / ( GLfloat ) iHeight, 1.0f, 1 << 24 );
}

void DrawBox( const glm::vec3* const v )
{
	glm::vec3 v1 = v[1];
	glm::vec3 v2 = v[3];
	glm::vec3 v3 = v[2];
	glm::vec3 v4 = v[0];
	glm::vec3 v5 = v[7];
	glm::vec3 v6 = v[5];
	glm::vec3 v7 = v[4];
	glm::vec3 v8 = v[6];

	glBegin(GL_LINE_STRIP);
	glColor3f(1, 0, 0); glVertex3f(v1.x, v1.y, v1.z);
	glColor3f(1, 0, 0); glVertex3f(v2.x, v2.y, v2.z);
	glColor3f(1, 0, 0); glVertex3f(v3.x, v3.y, v3.z);
	glColor3f(1, 0, 0); glVertex3f(v4.x, v4.y, v4.z);
	glColor3f(1, 0, 0); glVertex3f(v1.x, v1.y, v1.z);
	glEnd();

	glBegin(GL_LINE_STRIP);
	glColor3f(1, 0, 0); glVertex3f(v5.x, v5.y, v5.z);
	glColor3f(1, 0, 0); glVertex3f(v6.x, v6.y, v6.z);
	glColor3f(1, 0, 0); glVertex3f(v7.x, v7.y, v7.z);
	glColor3f(1, 0, 0); glVertex3f(v8.x, v8.y, v8.z);
	glColor3f(1, 0, 0); glVertex3f(v5.x, v5.y, v5.z);
	glEnd();

	glBegin(GL_LINES);
	glColor3f(1, 0, 0); glVertex3f(v1.x, v1.y, v1.z);
	glColor3f(1, 0, 0); glVertex3f(v5.x, v5.y, v5.z);

	glColor3f(1, 0, 0); glVertex3f(v2.x, v2.y, v2.z);
	glColor3f(1, 0, 0); glVertex3f(v6.x, v6.y, v6.z);

	glColor3f(1, 0, 0); glVertex3f(v3.x, v3.y, v3.z);
	glColor3f(1, 0, 0); glVertex3f(v7.x, v7.y, v7.z);

	glColor3f(1, 0, 0); glVertex3f(v4.x, v4.y, v4.z);
	glColor3f(1, 0, 0); glVertex3f(v8.x, v8.y, v8.z);
	glEnd();
}
}