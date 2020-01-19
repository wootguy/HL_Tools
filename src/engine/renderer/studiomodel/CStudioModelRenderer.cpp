#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

#include "shared/Logging.h"

#include "lib/LibInterface.h"

#include "cvar/CCVar.h"
#include "cvar/CVarUtils.h"

#include "graphics/GraphicsUtils.h"

#include "shared/studiomodel/CStudioModel.h"
#include "shared/renderer/studiomodel/IStudioModelRendererListener.h"
#include "StudioSorting.h"

#include "CStudioModelRenderer.h"

//Double to float conversion
#pragma warning( disable: 4244 )

cvar::CCVar g_ShowBones( "r_showbones", cvar::CCVarArgsBuilder().FloatValue( 0 ).HelpInfo( "If non-zero, shows model bones" ) );
cvar::CCVar g_ShowAttachments( "r_showattachments", cvar::CCVarArgsBuilder().FloatValue( 0 ).HelpInfo( "If non-zero, shows model attachments" ) );
cvar::CCVar g_ShowEyePosition( "r_showeyeposition", cvar::CCVarArgsBuilder().FloatValue( 0 ).HelpInfo( "If non-zero, shows model eye position" ) );
cvar::CCVar g_ShowHitboxes( "r_showhitboxes", cvar::CCVarArgsBuilder().FloatValue( 0 ).HelpInfo( "If non-zero, shows model hitboxes" ) );
cvar::CCVar g_ShowStudioNormals( "r_showstudionormals", cvar::CCVarArgsBuilder().FloatValue( 0 ).HelpInfo( "If non-zero, shows studio normals" ) );

//TODO: this is temporary until lighting can be moved somewhere else

DEFINE_COLOR_CVAR( , r_lighting, 255, 255, 255, "Lighting", cvar::CCVarArgsBuilder().Flags( cvar::Flag::ARCHIVE ).Callback( cvar::ColorCVarChanged ) );
DEFINE_COLOR_CVAR( , r_wireframecolor, 255, 0, 0, "Wireframe overlay color", cvar::CCVarArgsBuilder().Flags( cvar::Flag::ARCHIVE ).Callback( cvar::ColorCVarChanged ) );

namespace studiomdl
{
REGISTER_SINGLE_INTERFACE( ISTUDIOMODELRENDERER_NAME, CStudioModelRenderer );

CStudioModelRenderer::CStudioModelRenderer()
{
}

CStudioModelRenderer::~CStudioModelRenderer()
{
}

bool CStudioModelRenderer::Initialize()
{
	m_uiModelsDrawnCount = 0;

	m_uiDrawnPolygonsCount = 0;

	return true;
}

void CStudioModelRenderer::Shutdown()
{
}

void CStudioModelRenderer::RunFrame()
{
}

unsigned int CStudioModelRenderer::DrawModel( studiomdl::CModelRenderInfo* const pRenderInfo, const renderer::DrawFlags_t flags )
{
	if( !pRenderInfo )
	{
		Error( "CStudioModelRenderer::DrawModel: Called with null render info!\n" );
		return 0;
	}

	m_pRenderInfo = pRenderInfo;

	if( pRenderInfo->pModel )
	{
		m_pStudioHdr = pRenderInfo->pModel->GetStudioHeader();
		m_pTextureHdr = pRenderInfo->pModel->GetTextureHeader();
	}
	else
	{
		Error( "CStudioModelRenderer::DrawModel: Called with null model!\n" );
		return 0;
	}

	++m_uiModelsDrawnCount; // render data cache cookie

	m_pxformverts = &m_xformverts[ 0 ];
	m_pvlightvalues = &m_lightvalues[ 0 ];

	if( m_pStudioHdr->numbodyparts == 0 )
		return 0;

	glPushMatrix();

	auto origin = m_pRenderInfo->vecOrigin;

	//The game applies a 1 unit offset to make view models look nicer
	//See https://github.com/ValveSoftware/halflife/blob/c76dd531a79a176eef7cdbca5a80811123afbbe2/cl_dll/view.cpp#L665-L668
	if( flags & renderer::DrawFlag::IS_VIEW_MODEL )
	{
		origin.z -= 1;
	}

	glTranslatef( origin[ 0 ], origin[ 1 ], origin[ 2 ] );

	glRotatef( m_pRenderInfo->vecAngles[ 1 ], 0, 0, 1 );
	glRotatef( m_pRenderInfo->vecAngles[ 0 ], 0, 1, 0 );
	glRotatef( m_pRenderInfo->vecAngles[ 2 ], 1, 0, 0 );

	glScalef( m_pRenderInfo->vecScale.x, m_pRenderInfo->vecScale.y, m_pRenderInfo->vecScale.z );

	SetUpBones();

	SetupLighting();

	unsigned int uiDrawnPolys = 0;

	if( m_pListener )
		m_pListener->OnPreDraw( *this, *m_pRenderInfo );

	if( !( flags & renderer::DrawFlag::NODRAW ) )
	{
		for( int i = 0; i < m_pStudioHdr->numbodyparts; i++ )
		{
			SetupModel( i );
			if( m_pRenderInfo->flTransparency > 0.0f )
				uiDrawnPolys += DrawPoints( false );
		}
	}

	if( flags & renderer::DrawFlag::WIREFRAME_OVERLAY )
	{
		//TODO: restore render mode after this? - Solokiller
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		glDisable( GL_TEXTURE_2D );
		glDisable( GL_CULL_FACE );
		glEnable( GL_DEPTH_TEST );

		for( int i = 0; i < m_pStudioHdr->numbodyparts; i++ )
		{
			SetupModel( i );
			if( m_pRenderInfo->flTransparency > 0.0f )
				uiDrawnPolys += DrawPoints( true );
		}
	}

	// draw bones
	if( g_ShowBones.GetBool() )
	{
		DrawBones();
	}

	if( g_ShowAttachments.GetBool() )
	{
		DrawAttachments();
	}

	if( g_ShowEyePosition.GetBool() )
	{
		DrawEyePosition();
	}

	if( g_ShowHitboxes.GetBool() || true )
	{
		DrawHitBoxes();
	}

	if( g_ShowStudioNormals.GetBool() )
	{
		DrawNormals();
	}

	//Call this after the above debug operations so overlaying works properly.
	if( m_pListener )
		m_pListener->OnPostDraw( *this, *m_pRenderInfo );

	glPopMatrix();

	m_uiDrawnPolygonsCount += uiDrawnPolys;

	return uiDrawnPolys;
}

void CStudioModelRenderer::DrawSingleBone( const int iBone )
{
	if( !m_pStudioHdr || iBone < 0 || iBone >= m_pStudioHdr->numbones )
		return;

	const mstudiobone_t* const pbones = m_pStudioHdr->GetBones();
	glDisable( GL_TEXTURE_2D );
	glDisable( GL_DEPTH_TEST );

	if( pbones[ iBone ].parent >= 0 )
	{
		#ifndef EMSCRIPTEN
		glPointSize( 10.0f );
		#endif
		glColor3f( 0, 0.7f, 1 );
		glBegin( GL_LINES );
		glVertex3f( m_bonetransform[ pbones[ iBone ].parent ][ 0 ][ 3 ], m_bonetransform[ pbones[ iBone ].parent ][ 1 ][ 3 ], m_bonetransform[ pbones[ iBone ].parent ][ 2 ][ 3 ] );
		glVertex3f( m_bonetransform[ iBone ][ 0 ][ 3 ], m_bonetransform[ iBone ][ 1 ][ 3 ], m_bonetransform[ iBone ][ 2 ][ 3 ] );
		glEnd();

		glColor3f( 0, 0, 0.8f );
		glBegin( GL_POINTS );
		if( pbones[ pbones[ iBone ].parent ].parent != -1 )
			glVertex3f( m_bonetransform[ pbones[ iBone ].parent ][ 0 ][ 3 ], m_bonetransform[ pbones[ iBone ].parent ][ 1 ][ 3 ], m_bonetransform[ pbones[ iBone ].parent ][ 2 ][ 3 ] );
		glVertex3f( m_bonetransform[ iBone ][ 0 ][ 3 ], m_bonetransform[ iBone ][ 1 ][ 3 ], m_bonetransform[ iBone ][ 2 ][ 3 ] );
		glEnd();
	}
	else
	{
		// draw parent bone node
		#ifndef EMSCRIPTEN
		glPointSize( 10.0f );
		#endif
		glColor3f( 0.8f, 0, 0 );
		glBegin( GL_POINTS );
		glVertex3f( m_bonetransform[ iBone ][ 0 ][ 3 ], m_bonetransform[ iBone ][ 1 ][ 3 ], m_bonetransform[ iBone ][ 2 ][ 3 ] );
		glEnd();
	}

	#ifndef EMSCRIPTEN
	glPointSize( 1.0f );
	#endif
}

void CStudioModelRenderer::DrawSingleAttachment( const int iAttachment )
{
	if( !m_pStudioHdr || iAttachment < 0 || iAttachment >= m_pStudioHdr->numattachments )
		return;

	glDisable( GL_TEXTURE_2D );
	glDisable( GL_CULL_FACE );
	glDisable( GL_DEPTH_TEST );

	mstudioattachment_t *pattachments = m_pStudioHdr->GetAttachments();
	glm::vec3 v[ 4 ];
	VectorTransform( pattachments[ iAttachment ].org, m_bonetransform[ pattachments[ iAttachment ].bone ], v[ 0 ] );
	VectorTransform( pattachments[ iAttachment ].vectors[ 0 ], m_bonetransform[ pattachments[ iAttachment ].bone ], v[ 1 ] );
	VectorTransform( pattachments[ iAttachment ].vectors[ 1 ], m_bonetransform[ pattachments[ iAttachment ].bone ], v[ 2 ] );
	VectorTransform( pattachments[ iAttachment ].vectors[ 2 ], m_bonetransform[ pattachments[ iAttachment ].bone ], v[ 3 ] );
	glBegin( GL_LINES );
	glColor3f( 0, 1, 1 );
	glVertex3fv( glm::value_ptr( v[ 0 ] ) );
	glColor3f( 1, 1, 1 );
	glVertex3fv( glm::value_ptr( v[ 1 ] ) );
	glColor3f( 0, 1, 1 );
	glVertex3fv( glm::value_ptr( v[ 0 ] ) );
	glColor3f( 1, 1, 1 );
	glVertex3fv( glm::value_ptr( v[ 2 ] ) );
	glColor3f( 0, 1, 1 );
	glVertex3fv( glm::value_ptr( v[ 0 ] ) );
	glColor3f( 1, 1, 1 );
	glVertex3fv( glm::value_ptr( v[ 3 ] ) );
	glEnd();

	#ifndef EMSCRIPTEN
	glPointSize( 10 );
	#endif
	glColor3f( 0, 1, 0 );
	glBegin( GL_POINTS );
	glVertex3fv( glm::value_ptr( v[ 0 ] ) );
	glEnd();
	#ifndef EMSCRIPTEN
	glPointSize( 1 );
	#endif
}

void CStudioModelRenderer::DrawBones()
{
	const mstudiobone_t* const pbones = m_pStudioHdr->GetBones();
	glDisable( GL_TEXTURE_2D );
	glDisable( GL_DEPTH_TEST );

	for( int i = 0; i < m_pStudioHdr->numbones; i++ )
	{
		if( pbones[ i ].parent >= 0 )
		{
			#ifndef EMSCRIPTEN
			glPointSize( 3.0f );
			#endif
			glColor3f( 1, 0.7f, 0 );
			glBegin( GL_LINES );
			glVertex3f( m_bonetransform[ pbones[ i ].parent ][ 0 ][ 3 ], m_bonetransform[ pbones[ i ].parent ][ 1 ][ 3 ], m_bonetransform[ pbones[ i ].parent ][ 2 ][ 3 ] );
			glVertex3f( m_bonetransform[ i ][ 0 ][ 3 ], m_bonetransform[ i ][ 1 ][ 3 ], m_bonetransform[ i ][ 2 ][ 3 ] );
			glEnd();

			glColor3f( 0, 0, 0.8f );
			glBegin( GL_POINTS );
			if( pbones[ pbones[ i ].parent ].parent != -1 )
				glVertex3f( m_bonetransform[ pbones[ i ].parent ][ 0 ][ 3 ], m_bonetransform[ pbones[ i ].parent ][ 1 ][ 3 ], m_bonetransform[ pbones[ i ].parent ][ 2 ][ 3 ] );
			glVertex3f( m_bonetransform[ i ][ 0 ][ 3 ], m_bonetransform[ i ][ 1 ][ 3 ], m_bonetransform[ i ][ 2 ][ 3 ] );
			glEnd();
		}
		else
		{
			// draw parent bone node
			#ifndef EMSCRIPTEN
			glPointSize( 5.0f );
			#endif
			glColor3f( 0.8f, 0, 0 );
			glBegin( GL_POINTS );
			glVertex3f( m_bonetransform[ i ][ 0 ][ 3 ], m_bonetransform[ i ][ 1 ][ 3 ], m_bonetransform[ i ][ 2 ][ 3 ] );
			glEnd();
		}
	}

	#ifndef EMSCRIPTEN
	glPointSize( 1.0f );
	#endif
}

void CStudioModelRenderer::DrawAttachments()
{
	glDisable( GL_TEXTURE_2D );
	glDisable( GL_CULL_FACE );
	glDisable( GL_DEPTH_TEST );

	for( int i = 0; i < m_pStudioHdr->numattachments; i++ )
	{
		mstudioattachment_t *pattachments = m_pStudioHdr->GetAttachments();
		glm::vec3 v[ 4 ];
		VectorTransform( pattachments[ i ].org, m_bonetransform[ pattachments[ i ].bone ], v[ 0 ] );
		VectorTransform( pattachments[ i ].vectors[ 0 ], m_bonetransform[ pattachments[ i ].bone ], v[ 1 ] );
		VectorTransform( pattachments[ i ].vectors[ 1 ], m_bonetransform[ pattachments[ i ].bone ], v[ 2 ] );
		VectorTransform( pattachments[ i ].vectors[ 2 ], m_bonetransform[ pattachments[ i ].bone ], v[ 3 ] );
		glBegin( GL_LINES );
		glColor3f( 1, 0, 0 );
		glVertex3fv( glm::value_ptr( v[ 0 ] ) );
		glColor3f( 1, 1, 1 );
		glVertex3fv( glm::value_ptr( v[ 1 ] ) );
		glColor3f( 1, 0, 0 );
		glVertex3fv( glm::value_ptr( v[ 0 ] ) );
		glColor3f( 1, 1, 1 );
		glVertex3fv( glm::value_ptr( v[ 2 ] ) );
		glColor3f( 1, 0, 0 );
		glVertex3fv( glm::value_ptr( v[ 0 ] ) );
		glColor3f( 1, 1, 1 );
		glVertex3fv( glm::value_ptr( v[ 3 ] ) );
		glEnd();

		#ifndef EMSCRIPTEN
		glPointSize( 5 );
		#endif
		glColor3f( 0, 1, 0 );
		glBegin( GL_POINTS );
		glVertex3fv( glm::value_ptr( v[ 0 ] ) );
		glEnd();
		#ifndef EMSCRIPTEN
		glPointSize( 1 );
		#endif
	}
}

void CStudioModelRenderer::DrawEyePosition()
{
	glDisable( GL_TEXTURE_2D );
	glDisable( GL_CULL_FACE );
	glDisable( GL_DEPTH_TEST );

	#ifndef EMSCRIPTEN
	glPointSize( 7 );
	#endif
	glColor3f( 1, 0, 1 );
	glBegin( GL_POINTS );
	glVertex3fv( glm::value_ptr( m_pStudioHdr->eyeposition ) );
	glEnd();
	#ifndef EMSCRIPTEN
	glPointSize( 1 );
	#endif
}

void CStudioModelRenderer::DrawHitBoxes()
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);

	for( int i = 0; i < m_pStudioHdr->numhitboxes; i++ )
	{
		mstudiobbox_t *pbboxes = m_pStudioHdr->GetHitBoxes();
		glm::vec3 v[ 8 ], v2[ 8 ];

		glm::vec3 bbmin = pbboxes[ i ].bbmin;
		glm::vec3 bbmax = pbboxes[ i ].bbmax;

		v[ 0 ][ 0 ] = bbmin[ 0 ];
		v[ 0 ][ 1 ] = bbmax[ 1 ];
		v[ 0 ][ 2 ] = bbmin[ 2 ];

		v[ 1 ][ 0 ] = bbmin[ 0 ];
		v[ 1 ][ 1 ] = bbmin[ 1 ];
		v[ 1 ][ 2 ] = bbmin[ 2 ];

		v[ 2 ][ 0 ] = bbmax[ 0 ];
		v[ 2 ][ 1 ] = bbmax[ 1 ];
		v[ 2 ][ 2 ] = bbmin[ 2 ];

		v[ 3 ][ 0 ] = bbmax[ 0 ];
		v[ 3 ][ 1 ] = bbmin[ 1 ];
		v[ 3 ][ 2 ] = bbmin[ 2 ];

		v[ 4 ][ 0 ] = bbmax[ 0 ];
		v[ 4 ][ 1 ] = bbmax[ 1 ];
		v[ 4 ][ 2 ] = bbmax[ 2 ];

		v[ 5 ][ 0 ] = bbmax[ 0 ];
		v[ 5 ][ 1 ] = bbmin[ 1 ];
		v[ 5 ][ 2 ] = bbmax[ 2 ];

		v[ 6 ][ 0 ] = bbmin[ 0 ];
		v[ 6 ][ 1 ] = bbmax[ 1 ];
		v[ 6 ][ 2 ] = bbmax[ 2 ];

		v[ 7 ][ 0 ] = bbmin[ 0 ];
		v[ 7 ][ 1 ] = bbmin[ 1 ];
		v[ 7 ][ 2 ] = bbmax[ 2 ];

		VectorTransform( v[ 0 ], m_bonetransform[ pbboxes[ i ].bone ], v2[ 0 ] );
		VectorTransform( v[ 1 ], m_bonetransform[ pbboxes[ i ].bone ], v2[ 1 ] );
		VectorTransform( v[ 2 ], m_bonetransform[ pbboxes[ i ].bone ], v2[ 2 ] );
		VectorTransform( v[ 3 ], m_bonetransform[ pbboxes[ i ].bone ], v2[ 3 ] );
		VectorTransform( v[ 4 ], m_bonetransform[ pbboxes[ i ].bone ], v2[ 4 ] );
		VectorTransform( v[ 5 ], m_bonetransform[ pbboxes[ i ].bone ], v2[ 5 ] );
		VectorTransform( v[ 6 ], m_bonetransform[ pbboxes[ i ].bone ], v2[ 6 ] );
		VectorTransform( v[ 7 ], m_bonetransform[ pbboxes[ i ].bone ], v2[ 7 ] );

		graphics::DrawBox( v2 );
	}
}

void CStudioModelRenderer::DrawNormals()
{
	glDisable( GL_TEXTURE_2D );

	glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	glBegin( GL_LINES );

	for( int iBodyPart = 0; iBodyPart < m_pStudioHdr->numbodyparts; ++iBodyPart )
	{
		SetupModel( iBodyPart );

		auto pvertbone = ( const byte* ) ( m_pStudioHdr->GetData() + m_pModel->vertinfoindex );
		auto pnormbone = ( const byte* ) ( m_pStudioHdr->GetData() + m_pModel->norminfoindex );
		auto ptexture = m_pTextureHdr->GetTextures();

		auto pMeshes = ( const mstudiomesh_t* ) ( m_pStudioHdr->GetData() + m_pModel->meshindex );

		auto pstudioverts = ( const glm::vec3* ) ( m_pStudioHdr->GetData() + m_pModel->vertindex );
		auto pstudionorms = ( const glm::vec3* ) ( m_pStudioHdr->GetData() + m_pModel->normindex );

		auto pskinref = m_pTextureHdr->GetSkins();

		const int iSkinNum = m_pRenderInfo->iSkin;

		if( iSkinNum != 0 && iSkinNum < m_pTextureHdr->numskinfamilies )
			pskinref += ( iSkinNum * m_pTextureHdr->numskinref );

		for( int i = 0; i < m_pModel->numverts; i++ )
		{
			VectorTransform( pstudioverts[ i ], m_bonetransform[ pvertbone[ i ] ], m_pxformverts[ i ] );
		}

		//
		// clip and draw all triangles
		//

		glm::vec3* lv = m_pvlightvalues;

		for( int j = 0; j < m_pModel->nummesh; j++ )
		{
			int flags = ptexture[ pskinref[ pMeshes[ j ].skinref ] ].flags;

			for( int i = 0; i < pMeshes[ j ].numnorms; i++, ++lv, ++pstudionorms, pnormbone++ )
			{
				Lighting( *lv, *pnormbone, flags, *pstudionorms );

				// FIX: move this check out of the inner loop
				if( flags & STUDIO_NF_CHROME )
					Chrome( m_chrome[ reinterpret_cast<glm::vec3*>( lv ) - m_pvlightvalues ], *pnormbone, *pstudionorms );
			}
		}

		//Reset
		pstudionorms = ( const glm::vec3* ) ( m_pStudioHdr->GetData() + m_pModel->normindex );

		for( int j = 0; j < m_pModel->nummesh; j++ )
		{
			auto& mesh = pMeshes[ j ];
			auto ptricmds = ( const short* ) ( m_pStudioHdr->GetData() + mesh.triindex );

			const mstudiotexture_t& texture = ptexture[ pskinref[ mesh.skinref ] ];

			const glm::vec3* vecTriangles[ 3 ];

			int i;

			while( i = *( ptricmds++ ) )
			{
				//Keep track of the total number of vertices. Used to invert strip normals.
				const int total = i;

				if( i < 0 )
				{
					i = -i;

					vecTriangles[ 0 ] = &m_pxformverts[ ptricmds[ 0 ] ];
					vecTriangles[ 1 ] = &m_pxformverts[ ptricmds[ 4 ] ];

					i -= 2;
					ptricmds += 8;

					for( ; i > 0; --i, ptricmds += 4 )
					{
						vecTriangles[ 2 ] = &m_pxformverts[ ptricmds[ 0 ] ];
						
						const glm::vec3 vecCenter( ( *( vecTriangles[ 0 ] ) + *( vecTriangles[ 1 ] ) + *( vecTriangles[ 2 ] ) ) / 3.0f );

						glm::vec3 vecNormal( glm::cross( *( vecTriangles[ 2 ] ) - *( vecTriangles[ 0 ] ), *( vecTriangles[ 1 ] ) - *( vecTriangles[ 0 ] ) ) );

						vecNormal = glm::normalize( vecNormal );

						glVertex3fv( glm::value_ptr( vecCenter ) );
						glVertex3fv( glm::value_ptr( vecCenter + vecNormal ) );

						vecTriangles[ 1 ] = vecTriangles[ 2 ];
					}
				}
				else
				{
					vecTriangles[ 0 ] = &m_pxformverts[ ptricmds[ 0 ] ];
					vecTriangles[ 1 ] = &m_pxformverts[ ptricmds[ 4 ] ];

					i -= 2;
					ptricmds += 8;

					for( ; i > 0; --i, ptricmds += 4 )
					{
						vecTriangles[ 2 ] = &m_pxformverts[ ptricmds[ 0 ] ];

						const glm::vec3 vecCenter( ( *( vecTriangles[ 0 ] ) + *( vecTriangles[ 1 ] ) + *( vecTriangles[ 2 ] ) ) / 3.0f );

						glm::vec3 vecNormal( glm::cross( *( vecTriangles[ 2 ] ) - *( vecTriangles[ 0 ] ), *( vecTriangles[ 1 ] ) - *( vecTriangles[ 0 ] ) ) );

						vecNormal = glm::normalize( vecNormal );

						if( ( ( i % 2 ) == 0 ) ^ ( ( total % 2 ) == 0 ) )
							vecNormal *= -1;

						glVertex3fv( glm::value_ptr( vecCenter ) );
						glVertex3fv( glm::value_ptr( vecCenter + vecNormal ) );

						vecTriangles[ 0 ] = vecTriangles[ 1 ];
						vecTriangles[ 1 ] = vecTriangles[ 2 ];
					}
				}
			}
		}
	}

	glEnd();
}

void CStudioModelRenderer::SetUpBones()
{
	static glm::vec3 pos[ MAXSTUDIOBONES ];
	static glm::vec4 q[ MAXSTUDIOBONES ];
					 
	static glm::vec3 pos2[ MAXSTUDIOBONES ];
	static glm::vec4 q2[ MAXSTUDIOBONES ];
	static glm::vec3 pos3[ MAXSTUDIOBONES ];
	static glm::vec4 q3[ MAXSTUDIOBONES ];
	static glm::vec3 pos4[ MAXSTUDIOBONES ];
	static glm::vec4 q4[ MAXSTUDIOBONES ];

	if( m_pRenderInfo->iSequence >= m_pStudioHdr->numseq )
	{
		m_pRenderInfo->iSequence = 0;
	}

	mstudioseqdesc_t* const pseqdesc = m_pStudioHdr->GetSequence( m_pRenderInfo->iSequence );

	const mstudioanim_t* panim = m_pRenderInfo->pModel->GetAnim( pseqdesc );

	CalcRotations( pos, q, pseqdesc, panim, m_pRenderInfo->flFrame );

	if( pseqdesc->numblends > 1 )
	{
		panim += m_pStudioHdr->numbones;
		CalcRotations( pos2, q2, pseqdesc, panim, m_pRenderInfo->flFrame );
		float s = m_pRenderInfo->iBlender[ 0 ] / 255.0;

		SlerpBones( q, pos, q2, pos2, s );

		if( pseqdesc->numblends == 4 )
		{
			panim += m_pStudioHdr->numbones;
			CalcRotations( pos3, q3, pseqdesc, panim, m_pRenderInfo->flFrame );

			panim += m_pStudioHdr->numbones;
			CalcRotations( pos4, q4, pseqdesc, panim, m_pRenderInfo->flFrame );

			s = m_pRenderInfo->iBlender[ 0 ] / 255.0;
			SlerpBones( q3, pos3, q4, pos4, s );

			s = m_pRenderInfo->iBlender[ 1 ] / 255.0;
			SlerpBones( q, pos, q3, pos3, s );
		}
	}

	const mstudiobone_t* const pbones = m_pStudioHdr->GetBones();

	glm::mat3x4 bonematrix;

	for( int i = 0; i < m_pStudioHdr->numbones; i++ )
	{
		QuaternionMatrix( q[ i ], bonematrix );

		bonematrix[ 0 ][ 3 ] = pos[ i ][ 0 ];
		bonematrix[ 1 ][ 3 ] = pos[ i ][ 1 ];
		bonematrix[ 2 ][ 3 ] = pos[ i ][ 2 ];

		if( pbones[ i ].parent == -1 )
		{
			m_bonetransform[ i ] = bonematrix;
		}
		else
		{
			R_ConcatTransforms( m_bonetransform[ pbones[ i ].parent ], bonematrix, m_bonetransform[ i ] );
		}
	}
}

void CStudioModelRenderer::CalcRotations( glm::vec3* pos, glm::vec4* q, const mstudioseqdesc_t* const pseqdesc, const mstudioanim_t* panim, const float f )
{
	const int frame = ( int ) f;
	const float s = ( f - frame );

	// add in programatic controllers
	CalcBoneAdj();

	auto pbone = m_pStudioHdr->GetBones();

	for( int i = 0; i < m_pStudioHdr->numbones; i++, pbone++, panim++ )
	{
		CalcBoneQuaternion( frame, s, pbone, panim, q[ i ] );
		CalcBonePosition( frame, s, pbone, panim, pos[ i ] );
	}

	if( pseqdesc->motiontype & STUDIO_X )
		pos[ pseqdesc->motionbone ][ 0 ] = 0.0;
	if( pseqdesc->motiontype & STUDIO_Y )
		pos[ pseqdesc->motionbone ][ 1 ] = 0.0;
	if( pseqdesc->motiontype & STUDIO_Z )
		pos[ pseqdesc->motionbone ][ 2 ] = 0.0;
}

void CStudioModelRenderer::CalcBoneAdj()
{
	const auto* const pbonecontroller = m_pStudioHdr->GetBoneControllers();

	for( int j = 0; j < m_pStudioHdr->numbonecontrollers; j++ )
	{
		const auto i = pbonecontroller[ j ].index;

		float value;

		if( i <= 3 )
		{
			// check for 360% wrapping
			if( pbonecontroller[ j ].type & STUDIO_RLOOP )
			{
				value = m_pRenderInfo->iController[ i ] * ( 360.0 / 256.0 ) + pbonecontroller[ j ].start;
			}
			else
			{
				value = m_pRenderInfo->iController[ i ] / 255.0;
				if( value < 0 ) value = 0;
				if( value > 1.0 ) value = 1.0;
				value = ( 1.0 - value ) * pbonecontroller[ j ].start + value * pbonecontroller[ j ].end;
			}
			// Con_DPrintf( "%d %d %f : %f\n", m_controller[j], m_prevcontroller[j], value, dadt );
		}
		else
		{
			value = m_pRenderInfo->iMouth / 64.0;
			if( value > 1.0 ) value = 1.0;
			value = ( 1.0 - value ) * pbonecontroller[ j ].start + value * pbonecontroller[ j ].end;
			// Con_DPrintf("%d %f\n", mouthopen, value );
		}
		switch( pbonecontroller[ j ].type & STUDIO_TYPES )
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			m_Adj[ j ] = value * ( Q_PI / 180.0 );
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			m_Adj[ j ] = value;
			break;
		}
	}
}

void CStudioModelRenderer::CalcBoneQuaternion( const int frame, const float s, const mstudiobone_t* const pbone, const mstudioanim_t* const panim, glm::vec4& q )
{
	glm::vec3			angle1, angle2;

	for( int j = 0; j < 3; j++ )
	{
		if( panim->offset[ j + 3 ] == 0 )
		{
			angle2[ j ] = angle1[ j ] = pbone->value[ j + 3 ]; // default;
		}
		else
		{
			auto panimvalue = ( const mstudioanimvalue_t* ) ( ( const byte* ) panim + panim->offset[ j + 3 ] );
			auto k = frame;
			while( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
			}
			// Bah, missing blend!
			if( panimvalue->num.valid > k )
			{
				angle1[ j ] = panimvalue[ k + 1 ].value;

				if( panimvalue->num.valid > k + 1 )
				{
					angle2[ j ] = panimvalue[ k + 2 ].value;
				}
				else
				{
					if( panimvalue->num.total > k + 1 )
						angle2[ j ] = angle1[ j ];
					else
						angle2[ j ] = panimvalue[ panimvalue->num.valid + 2 ].value;
				}
			}
			else
			{
				angle1[ j ] = panimvalue[ panimvalue->num.valid ].value;
				if( panimvalue->num.total > k + 1 )
				{
					angle2[ j ] = angle1[ j ];
				}
				else
				{
					angle2[ j ] = panimvalue[ panimvalue->num.valid + 2 ].value;
				}
			}
			angle1[ j ] = pbone->value[ j + 3 ] + angle1[ j ] * pbone->scale[ j + 3 ];
			angle2[ j ] = pbone->value[ j + 3 ] + angle2[ j ] * pbone->scale[ j + 3 ];
		}

		if( pbone->bonecontroller[ j + 3 ] != -1 )
		{
			angle1[ j ] += m_Adj[ pbone->bonecontroller[ j + 3 ] ];
			angle2[ j ] += m_Adj[ pbone->bonecontroller[ j + 3 ] ];
		}
	}

	if( !VectorCompare( angle1, angle2 ) )
	{
		glm::vec4 q1, q2;

		AngleQuaternion( angle1, q1 );
		AngleQuaternion( angle2, q2 );
		QuaternionSlerp( q1, q2, s, q );
	}
	else
	{
		AngleQuaternion( angle1, q );
	}
}

void CStudioModelRenderer::CalcBonePosition( const int frame, const float s, const mstudiobone_t* const pbone, const mstudioanim_t* const panim, glm::vec3& pos )
{
	for( int j = 0; j < 3; j++ )
	{
		pos[ j ] = pbone->value[ j ]; // default;
		if( panim->offset[ j ] != 0 )
		{
			auto panimvalue = ( mstudioanimvalue_t * ) ( ( byte * ) panim + panim->offset[ j ] );

			auto k = frame;
			// find span of values that includes the frame we want
			while( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
			}
			// if we're inside the span
			if( panimvalue->num.valid > k )
			{
				// and there's more data in the span
				if( panimvalue->num.valid > k + 1 )
				{
					pos[ j ] += ( panimvalue[ k + 1 ].value * ( 1.0 - s ) + s * panimvalue[ k + 2 ].value ) * pbone->scale[ j ];
				}
				else
				{
					pos[ j ] += panimvalue[ k + 1 ].value * pbone->scale[ j ];
				}
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if( panimvalue->num.total <= k + 1 )
				{
					pos[ j ] += ( panimvalue[ panimvalue->num.valid ].value * ( 1.0 - s ) + s * panimvalue[ panimvalue->num.valid + 2 ].value ) * pbone->scale[ j ];
				}
				else
				{
					pos[ j ] += panimvalue[ panimvalue->num.valid ].value * pbone->scale[ j ];
				}
			}
		}
		if( pbone->bonecontroller[ j ] != -1 )
		{
			pos[ j ] += m_Adj[ pbone->bonecontroller[ j ] ];
		}
	}
}

void CStudioModelRenderer::SlerpBones( glm::vec4* q1, glm::vec3* pos1, glm::vec4* q2, glm::vec3* pos2, float s )
{
	glm::vec4 q3;

	if( s < 0 ) s = 0;
	else if( s > 1.0 ) s = 1.0;

	const float s1 = 1.0 - s;

	for( int i = 0; i < m_pStudioHdr->numbones; i++ )
	{
		QuaternionSlerp( q1[ i ], q2[ i ], s, q3 );
		q1[ i ] = q3;

		pos1[ i ] = pos1[ i ] * s1 + pos2[ i ] * s;
	}
}

void CStudioModelRenderer::SetupLighting()
{
	m_ambientlight = 32;
	m_shadelight = 192;

	m_lightcolor[ 0 ] = r_lighting_r.GetInt();
	m_lightcolor[ 1 ] = r_lighting_g.GetInt();
	m_lightcolor[ 2 ] = r_lighting_b.GetInt();

	for( int i = 0; i < m_pStudioHdr->numbones; i++ )
	{
		VectorIRotate( m_lightvec, m_bonetransform[ i ], m_blightvec[ i ] );
	}
}

void CStudioModelRenderer::SetupModel( int bodypart )
{
	if( bodypart > m_pStudioHdr->numbodyparts )
	{
		// Con_DPrintf ("CStudioModelRenderer::SetupModel: no such bodypart %d\n", bodypart);
		bodypart = 0;
	}

	m_pModel = m_pRenderInfo->pModel->GetModelByBodyPart( m_pRenderInfo->iBodygroup, bodypart );
}

unsigned int CStudioModelRenderer::DrawPoints( const bool bWireframe )
{
	unsigned int uiDrawnPolys = 0;

	auto pvertbone = ( ( byte * ) m_pStudioHdr + m_pModel->vertinfoindex );
	auto pnormbone = ( ( byte * ) m_pStudioHdr + m_pModel->norminfoindex );
	auto ptexture = m_pTextureHdr->GetTextures();

	auto pmesh = ( mstudiomesh_t * ) ( ( byte * ) m_pStudioHdr + m_pModel->meshindex );

	auto pstudioverts = ( const glm::vec3* ) ( ( const byte* ) m_pStudioHdr + m_pModel->vertindex );
	auto pstudionorms = ( const glm::vec3* ) ( ( const byte* ) m_pStudioHdr + m_pModel->normindex );

	auto pskinref = m_pTextureHdr->GetSkins();

	if( m_pRenderInfo->iSkin != 0 && m_pRenderInfo->iSkin < m_pTextureHdr->numskinfamilies )
		pskinref += ( m_pRenderInfo->iSkin * m_pTextureHdr->numskinref );

	for( int i = 0; i < m_pModel->numverts; i++ )
	{
		VectorTransform( pstudioverts[ i ], m_bonetransform[ pvertbone[ i ] ], m_pxformverts[ i ] );
	}

	SortedMesh_t meshes[ MAXSTUDIOMESHES ];

	//
	// clip and draw all triangles
	//

	glm::vec3* lv = m_pvlightvalues;
	for( int j = 0; j < m_pModel->nummesh; j++ )
	{
		int flags = ptexture[ pskinref[ pmesh[ j ].skinref ] ].flags;

		meshes[ j ].pMesh = &pmesh[ j ];
		meshes[ j ].flags = flags;

		for( int i = 0; i < pmesh[ j ].numnorms; i++, ++lv, ++pstudionorms, pnormbone++ )
		{
			Lighting( *lv, *pnormbone, flags, *pstudionorms );

			// FIX: move this check out of the inner loop
			if( flags & STUDIO_NF_CHROME )
				Chrome( m_chrome[ reinterpret_cast<glm::vec3*>( lv ) - m_pvlightvalues ], *pnormbone, *pstudionorms );
		}
	}

	//Sort meshes by render modes so additive meshes are drawn after solid meshes.
	//Masked meshes are drawn before solid meshes.
	std::stable_sort( meshes, meshes + m_pModel->nummesh, CompareSortedMeshes );

	uiDrawnPolys += DrawMeshes( bWireframe, meshes, ptexture, pskinref );

	glDepthMask( GL_TRUE );

	return uiDrawnPolys;
}

unsigned int CStudioModelRenderer::DrawMeshes( const bool bWireframe, const SortedMesh_t* pMeshes, const mstudiotexture_t* pTextures, const short* pSkinRef )
{
	//Set here since it never changes. Much more efficient.
	if( bWireframe )
		glColor4f( r_wireframecolor_r.GetFloat() / 255.0f,
				   r_wireframecolor_g.GetFloat() / 255.0f,
				   r_wireframecolor_b.GetFloat() / 255.0f,
				   m_pRenderInfo->flTransparency );

	unsigned int uiDrawnPolys = 0;

	//Polygons may overlap, so make sure they can blend together. - Solokiller
	glDepthFunc( GL_LEQUAL );

	for( int j = 0; j < m_pModel->nummesh; j++ )
	{
		auto pmesh = pMeshes[ j ].pMesh;
		auto ptricmds = ( short * ) ( ( byte * ) m_pStudioHdr + pmesh->triindex );

		const mstudiotexture_t& texture = pTextures[ pSkinRef[ pmesh->skinref ] ];

		const auto s = 1.0 / ( float ) texture.width;
		const auto t = 1.0 / ( float ) texture.height;

		if( texture.flags & STUDIO_NF_ADDITIVE )
			glDepthMask( GL_FALSE );
		else
			glDepthMask( GL_TRUE );

		if( texture.flags & STUDIO_NF_ADDITIVE )
		{
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE );
		}
		else if( m_pRenderInfo->flTransparency < 1.0f )
		{
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		}
		else
			glDisable( GL_BLEND );

		if( texture.flags & STUDIO_NF_MASKED )
		{
			glEnable( GL_ALPHA_TEST );
			glAlphaFunc( GL_GREATER, 0.5f );
		}

		if( !bWireframe )
		{
			glBindTexture( GL_TEXTURE_2D, m_pRenderInfo->pModel->GetTextureId( pSkinRef[ pmesh->skinref ] ) );
		}

		int i;

		const int MAX_TRIS_PER_BODYGROUP = 4080;
		const int MAX_VERTS_PER_CALL = MAX_TRIS_PER_BODYGROUP*3;
		static float vertexData[MAX_VERTS_PER_CALL*3];
		static float texCoordData[MAX_VERTS_PER_CALL*2];
		static float colorData[MAX_VERTS_PER_CALL * 4];
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		int totalElements = 0;
		int texCoordIdx = 0;
		int colorIdx = 0;
		int vertexIdx = 0;
		int stripIdx = 0;

		while ( i = *( ptricmds++ ) )
		{
			int drawMode = GL_TRIANGLE_STRIP;
			if( i < 0 )
			{
				i = -i;
				drawMode = GL_TRIANGLE_FAN;
			}

			int polies = i - 2;
			uiDrawnPolys += polies;

			int elementsThisStrip = 0;
			int fanStartVertIdx = vertexIdx;
			int fanStartTexIdx = texCoordIdx;
			int fanStartColorIdx = colorIdx;

			for( ; i > 0; i--, ptricmds += 4 )
			{
				if (elementsThisStrip++ >= 3) { // first 3 verts are always the first triangle
					// convert to GL_TRIANGLES
					if (drawMode == GL_TRIANGLE_STRIP) {
						int v1PosIdx = vertexIdx - 3 * 2;
						int v2PosIdx = vertexIdx - 3 * 1;
						int v1TexIdx = texCoordIdx - 2 * 2;
						int v2TexIdx = texCoordIdx - 2 * 1;
						int v1ColorIdx = colorIdx - 4 * 2;
						int v2ColorIdx = colorIdx - 4 * 1;

						texCoordData[texCoordIdx++] = texCoordData[v1TexIdx];
						texCoordData[texCoordIdx++] = texCoordData[v1TexIdx + 1];
						colorData[colorIdx++] = colorData[v1ColorIdx];
						colorData[colorIdx++] = colorData[v1ColorIdx + 1];
						colorData[colorIdx++] = colorData[v1ColorIdx + 2];
						colorData[colorIdx++] = colorData[v1ColorIdx + 3];
						vertexData[vertexIdx++] = vertexData[v1PosIdx];
						vertexData[vertexIdx++] = vertexData[v1PosIdx + 1];
						vertexData[vertexIdx++] = vertexData[v1PosIdx + 2];

						texCoordData[texCoordIdx++] = texCoordData[v2TexIdx];
						texCoordData[texCoordIdx++] = texCoordData[v2TexIdx + 1];
						colorData[colorIdx++] = colorData[v2ColorIdx];
						colorData[colorIdx++] = colorData[v2ColorIdx + 1];
						colorData[colorIdx++] = colorData[v2ColorIdx + 2];
						colorData[colorIdx++] = colorData[v2ColorIdx + 3];
						vertexData[vertexIdx++] = vertexData[v2PosIdx];
						vertexData[vertexIdx++] = vertexData[v2PosIdx + 1];
						vertexData[vertexIdx++] = vertexData[v2PosIdx + 2];

						totalElements += 2;
						elementsThisStrip += 2;

					}
					else if (drawMode == GL_TRIANGLE_FAN) {
						int v1PosIdx = fanStartVertIdx;
						int v2PosIdx = vertexIdx - 3 * 1;
						int v1TexIdx = fanStartTexIdx;
						int v2TexIdx = texCoordIdx - 2 * 1;
						int v1ColorIdx = fanStartColorIdx;
						int v2ColorIdx = colorIdx - 4 * 1;

						texCoordData[texCoordIdx++] = texCoordData[v1TexIdx];
						texCoordData[texCoordIdx++] = texCoordData[v1TexIdx + 1];
						colorData[colorIdx++] = colorData[v1ColorIdx];
						colorData[colorIdx++] = colorData[v1ColorIdx + 1];
						colorData[colorIdx++] = colorData[v1ColorIdx + 2];
						colorData[colorIdx++] = colorData[v1ColorIdx + 3];
						vertexData[vertexIdx++] = vertexData[v1PosIdx];
						vertexData[vertexIdx++] = vertexData[v1PosIdx + 1];
						vertexData[vertexIdx++] = vertexData[v1PosIdx + 2];

						texCoordData[texCoordIdx++] = texCoordData[v2TexIdx];
						texCoordData[texCoordIdx++] = texCoordData[v2TexIdx + 1];
						colorData[colorIdx++] = colorData[v2ColorIdx];
						colorData[colorIdx++] = colorData[v2ColorIdx + 1];
						colorData[colorIdx++] = colorData[v2ColorIdx + 2];
						colorData[colorIdx++] = colorData[v2ColorIdx + 3];
						vertexData[vertexIdx++] = vertexData[v2PosIdx];
						vertexData[vertexIdx++] = vertexData[v2PosIdx + 1];
						vertexData[vertexIdx++] = vertexData[v2PosIdx + 2];

						totalElements += 2;
						elementsThisStrip += 2;
					}
				}

				if ( !bWireframe )
				{
					if( texture.flags & STUDIO_NF_CHROME )
					{
						texCoordData[texCoordIdx++] = m_chrome[ptricmds[1]][0] * s;
						texCoordData[texCoordIdx++] = m_chrome[ptricmds[1]][1] * t;
					}
					else
					{
						texCoordData[texCoordIdx++] = ptricmds[2] * s;
						texCoordData[texCoordIdx++] = ptricmds[3] * t;
					}

					if( texture.flags & STUDIO_NF_ADDITIVE )
					{
						colorData[colorIdx++] = 1.0f;
						colorData[colorIdx++] = 1.0f;
						colorData[colorIdx++] = 1.0f;
						colorData[colorIdx++] = m_pRenderInfo->flTransparency;
					}
					else
					{
						const glm::vec3& lightVec = m_pvlightvalues[ ptricmds[ 1 ] ];
						colorData[colorIdx++] = lightVec[0];
						colorData[colorIdx++] = lightVec[1];
						colorData[colorIdx++] = lightVec[2];
						colorData[colorIdx++] = m_pRenderInfo->flTransparency;
					}
				}

				glm::vec3 vert = m_pxformverts[ ptricmds[ 0 ] ];
				if (vert.x < m_drawnCoordMin.x) m_drawnCoordMin.x = vert.x;
				if (vert.y < m_drawnCoordMin.y) m_drawnCoordMin.y = vert.y;
				if (vert.z < m_drawnCoordMin.z) m_drawnCoordMin.z = vert.z;
				if (vert.x > m_drawnCoordMax.x) m_drawnCoordMax.x = vert.x;
				if (vert.y > m_drawnCoordMax.y) m_drawnCoordMax.y = vert.y;
				if (vert.z > m_drawnCoordMax.z) m_drawnCoordMax.z = vert.z;
				
				vertexData[vertexIdx++] = vert.x;
				vertexData[vertexIdx++] = vert.y;
				vertexData[vertexIdx++] = vert.z;

				totalElements++;
			}



			// flip odd tris in strips (simpler than adding special logic when creating the arrays, and index buffers
			// (for glDrawElements) don't seem to work with emscripten.
			if (drawMode == GL_TRIANGLE_STRIP) {
				for (int p = 1; p < polies; p += 2) {
					int polyOffset = p * 3;

					for (int k = 0; k < 3; k++)
					{
						int vstart = polyOffset*3 + fanStartVertIdx + k;
						float t = vertexData[vstart];
						vertexData[vstart] = vertexData[vstart + 3];
						vertexData[vstart + 3] = t;
					}
					for (int k = 0; k < 2; k++)
					{
						int vstart = polyOffset*2 + fanStartTexIdx + k;
						float t = texCoordData[vstart];
						texCoordData[vstart] = texCoordData[vstart + 2];
						texCoordData[vstart + 2] = t;
					}
					for (int k = 0; k < 4; k++)
					{
						int vstart = polyOffset*4 + fanStartColorIdx + k;
						float t = colorData[vstart];
						colorData[vstart] = colorData[vstart + 4];
						colorData[vstart + 4] = t;
					}
				}
			}
		}

		glVertexPointer(3, GL_FLOAT, sizeof(float) * 3, vertexData);
		glTexCoordPointer(2, GL_FLOAT, sizeof(float) * 2, texCoordData);
		glColorPointer(4, GL_FLOAT, sizeof(float) * 4, colorData);

		glDrawArrays(GL_TRIANGLES, 0, totalElements);


		if ( texture.flags & STUDIO_NF_MASKED )
			glDisable( GL_ALPHA_TEST );
	}

	return uiDrawnPolys;
}

void CStudioModelRenderer::Lighting( glm::vec3& lv, int bone, int flags, const glm::vec3& normal )
{
	const float ambient = std::max( 0.1f, ( float ) m_ambientlight / 255.0f ); // to avoid divison by zero
	const float shade = m_shadelight / 255.0f;
	glm::vec3 illum{ ambient };

	if( flags & STUDIO_NF_FULLBRIGHT )
	{
		lv = glm::vec3{ 1, 1, 1 };
		return;
	}
	else if( flags & STUDIO_NF_FLATSHADE )
	{
		VectorMA( illum, 0.8f, glm::vec3{ shade }, illum );
	}
	else
	{
		auto lightcos = glm::dot( normal, m_blightvec[ bone ] ); // -1 colinear, 1 opposite

		if( lightcos > 1.0f ) lightcos = 1;

		illum += m_shadelight / 255.0f;

		auto r = m_flLambert;
		if( r < 1.0f ) r = 1.0f;
		lightcos = ( lightcos + ( r - 1.0f ) ) / r; // do modified hemispherical lighting
		if( lightcos > 0.0f ) VectorMA( illum, -lightcos, glm::vec3{ shade }, illum );

		if( illum[ 0 ] <= 0 ) illum[ 0 ] = 0;
		if( illum[ 1 ] <= 0 ) illum[ 1 ] = 0;
		if( illum[ 2 ] <= 0 ) illum[ 2 ] = 0;
	}

	float max = VectorMax( illum );

	if( max > 1.0f )
		lv = illum * ( 1.0f / max );
	else lv = illum;

	const glm::vec3 lightcolor{ m_lightcolor.GetRed() / 255.0f, m_lightcolor.GetGreen() / 255.0f, m_lightcolor.GetBlue() / 255.0f };

	lv *= lightcolor;
}


void CStudioModelRenderer::Chrome( glm::vec2& chrome, int bone, const glm::vec3& normal )
{
	if( m_chromeage[ bone ] != m_uiModelsDrawnCount )
	{
		// calculate vectors from the viewer to the bone. This roughly adjusts for position
		// vector pointing at bone in world reference frame
		auto tmp = m_vecViewerOrigin * -1.0f;

		tmp[ 0 ] += m_bonetransform[ bone ][ 0 ][ 3 ];
		tmp[ 1 ] += m_bonetransform[ bone ][ 1 ][ 3 ];
		tmp[ 2 ] += m_bonetransform[ bone ][ 2 ][ 3 ];

		VectorNormalize( tmp );
		// g_chrome t vector in world reference frame
		auto chromeupvec = glm::cross( tmp, -m_vecViewerRight );
		VectorNormalize( chromeupvec );
		// g_chrome s vector in world reference frame
		auto chromerightvec = glm::cross( tmp, chromeupvec );
		VectorNormalize( chromerightvec );

		VectorIRotate( -chromeupvec, m_bonetransform[ bone ], m_chromeup[ bone ] );
		VectorIRotate( chromerightvec, m_bonetransform[ bone ], m_chromeright[ bone ] );

		m_chromeage[ bone ] = m_uiModelsDrawnCount;
	}

	// calc s coord
	auto n = glm::dot( normal, m_chromeright[ bone ] );
	chrome[ 0 ] = ( n + 1.0 ) * 32;

	// calc t coord
	n = glm::dot( normal, m_chromeup[ bone ] );
	chrome[ 1 ] = ( n + 1.0 ) * 32;
}
}