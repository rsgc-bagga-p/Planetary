/*
 *  NodeAlbum.cpp
 *  Bloom
 *
 *  Created by Robert Hodgin on 1/21/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include "cinder/app/AppBasic.h"
#include "NodeArtist.h"
#include "NodeAlbum.h"
#include "NodeTrack.h"
#include "cinder/Rand.h"
#include "cinder/gl/gl.h"
#include "cinder/PolyLine.h"
#include "Globals.h"

using namespace ci;
using namespace ci::ipod;
using namespace std;

NodeAlbum::NodeAlbum( Node *parent, int index, const Font &font, const Font &smallFont, const Surface &surfaces )
	: Node( parent, index, font, smallFont, surfaces )
{
	mGen				= G_ALBUM_LEVEL;
	mPos				= mParentNode->mPos;
	
	mIsHighlighted		= true;
	mIsBlockedBySun		= false;
	mBlockedBySunPer	= 1.0f;
	mHasAlbumArt		= false;
// NOW SET IN setChildOrbitRadii()
//	mIdealCameraDist	= mRadius * 13.5f;
	mEclipseStrength	= 0.0f;
}


void NodeAlbum::setData( PlaylistRef album )
{	
	
// ALBUM INFORMATION
	mAlbum				= album;
	mNumTracks			= mAlbum->size();
	mHighestPlayCount	= 0;
	mLowestPlayCount	= 10000;
	for( int i = 0; i < mNumTracks; i++ ){
		float numPlays = (*mAlbum)[i]->getPlayCount();
		if( numPlays < mLowestPlayCount )
			mLowestPlayCount = numPlays;
		if( numPlays > mHighestPlayCount )
			mHighestPlayCount = numPlays;
	}
	
	
	
// ORBIT RADIUS	
	// FIXME: bad c++?
	float numAlbums		= ((NodeArtist*)mParentNode)->getNumAlbums() + 2.0f;
	
	float invAlbumPer	= 1.0f/(float)numAlbums;
	float albumNumPer	= (float)mIndex * invAlbumPer;
	
	float minAmt		= mParentNode->mOrbitRadiusMin;
	float maxAmt		= mParentNode->mOrbitRadiusMax;
	float deltaAmt		= maxAmt - minAmt;
	mOrbitRadiusDest	= minAmt + deltaAmt * albumNumPer;// + Rand::randFloat( maxAmt * invAlbumPer * 0.35f );
	
	
// COLORS
	string name		= getName();
	char c1			= ' ';
	char c2			= ' ';
	if( name.length() >= 3 ){
		c1 = name[1];
		c2 = name[2];
	}
	
	int c1Int = constrain( int(c1), 32, 127 );
	int c2Int = constrain( int(c2), 32, 127 );
	
	mAsciiPer = ( c1Int - 32 )/( 127.0f - 32 );
	
	mHue				= mAsciiPer;
	mSat				= ( 1.0f - sin( mHue * M_PI ) ) * 0.1f + 0.15f;
	mColor				= Color( CM_HSV, mHue, mSat * 0.5f, 1.0f );
	mGlowColor			= mParentNode->mGlowColor;
	mEclipseColor       = mColor;


// PHYSICAL PROPERTIES
	mHasRings			= false;
	if( mNumTracks > 2 ) mHasRings = true;
	mTotalLength		= mAlbum->getTotalLength();
	mReleaseYear		= (*mAlbum)[0]->getReleaseYear();
	
	mRadiusDest			= mParentNode->mRadiusDest * constrain( mTotalLength * 0.00002f, 0.01f, 0.04f );//Rand::randFloat( 0.01f, 0.035f );
	mRadius				= mRadiusDest;
	mCloudLayerRadius	= mRadiusDest * 0.025f;
	
	mSphere				= Sphere( mPos, mRadius );
	mAxialTilt			= Rand::randFloat( 5.0f );
    mAxialVel			= Rand::randFloat( 10.0f, 45.0f );
	
// CHILD ORBIT RADIUS CONSTRAINTS
	mOrbitRadiusMin		= mRadius * 3.0f;
	mOrbitRadiusMax		= mRadius * 8.5f;
	

// TEXTURE IDs
    mPlanetTexIndex		= 3 * G_NUM_PLANET_TYPE_OPTIONS + c1Int%6;
	mCloudTexIndex		= c2Int%G_NUM_CLOUD_TYPES;
	
	
// CREATE PLANET TEXTURE
	int totalWidth		= 256;
	mAlbumArtSurface	= (*mAlbum)[0]->getArtwork( Vec2i( totalWidth, totalWidth ) );
	if( mAlbumArtSurface ){
		int w			= 128;
		int halfWidth	= w/2;
		int h			= 128;
		int halfHeight	= h/2;
		int x			= mAsciiPer * ( totalWidth - w );
		int y			= mAsciiPer * halfHeight;
		Area a			= Area( x, y, x+w, y+h );
		Surface crop	= mAlbumArtSurface.clone( a );
		
		Surface::Iter iter = crop.getIter();
		while( iter.line() ) {
			while( iter.pixel() ) {
				if( iter.x() >= halfWidth ){
					int xi = x + iter.x() - halfWidth;
					int yi = y + iter.y();
					ColorA c = mAlbumArtSurface.getPixel( Vec2i( xi, yi ) );
					iter.r() = c.r * 255.0f;
					iter.g() = c.g * 255.0f;
					iter.b() = c.b * 255.0f;
				} else {
					int xi = x + (halfWidth-1) - iter.x();
					int yi = y + iter.y();
					ColorA c = mAlbumArtSurface.getPixel( Vec2i( xi, yi ) );
					iter.r() = c.r * 255.0f;
					iter.g() = c.g * 255.0f;
					iter.b() = c.b * 255.0f;
				}
			}
		}
		mAlbumArtTex		= gl::Texture( crop );
		mHasAlbumArt		= true;
	}
}


void NodeAlbum::update( const Matrix44f &mat )
{
	double playbackTime		= app::getElapsedSeconds();
	double percentPlayed	= playbackTime/mOrbitPeriod;
	mOrbitAngle				= percentPlayed * TWO_PI + mOrbitStartAngle;

    Vec3f prevTransPos  = mTransPos;
    // if mTransPos hasn't been set yet, use a guess:
    // FIXME: set mTransPos correctly in the constructor
    if (prevTransPos.length() < 0.0001) prevTransPos = mat * mPos;    
	
	mRelPos		= Vec3f( cos( mOrbitAngle ), 0.0f, sin( mOrbitAngle ) ) * mOrbitRadius;
	mPos		= mParentNode->mPos + mRelPos;
	
/////////////////////////
// CALCULATE ECLIPSE VARS
    if( mParentNode->mDistFromCamZAxis < 0.0f && mDistFromCamZAxis < 0.0f ) //&& ( mIsSelected || mIsPlaying )
	{		
		Vec2f p		= mScreenPos;
		float r		= mSphereScreenRadius;
		float rsqrd = r * r;
		
		Vec2f P		= mParentNode->mScreenPos;
		float R		= mParentNode->mSphereScreenRadius;
		float Rsqrd	= R * R;
		float A		= M_PI * Rsqrd;
		
		float c		= p.distance( P );
		mEclipseDirBasedAlpha = 1.0f - constrain( c, 0.0f, 750.0f )/750.0f;
		if( mEclipseDirBasedAlpha > 0.9f )
			mEclipseDirBasedAlpha = 0.9f - ( mEclipseDirBasedAlpha - 0.9f ) * 9.0f;
		
		
		if( c < r + R ){
			float csqrd = c * c;
			float cos1	= ( Rsqrd + csqrd - rsqrd )/( 2.0f * R * c );
			float CBA	= acos( constrain( cos1, -1.0f, 1.0f ) );
			float CBD	= CBA * 2.0f;
			
			float cos2	= ( rsqrd + csqrd - Rsqrd )/( 2.0f * r * c );
			float CAB	= acos( constrain( cos2, -1.0f, 1.0f ) );
			float CAD	= CAB * 2.0f;
			float intersectingArea = CBA * Rsqrd - 0.5f * Rsqrd * sin( CBD ) + 0.5f * CAD * rsqrd - 0.5f * rsqrd * sin( CAD );
			mEclipseStrength = pow( 1.0f - ( A - intersectingArea ) / A, 2.0f );
			
			if( mDistFromCamZAxisPer > 0.0f ){
				if( mEclipseStrength > mParentNode->mEclipseStrength )
					mParentNode->mEclipseStrength = mEclipseStrength;
			}
		}
		
		mEclipseAngle = atan2( P.y - p.y, P.x - p.x );
		
		// if the album is further away from the camera than the sun,
		// check to see if it is behind the sun.
		if( mDistFromCamZAxis < mParentNode->mDistFromCamZAxis ){
			if( c < R * 1.6f && c > R * 0.8f ){
				mBlockedBySunPer = ( c - R )/(R*0.8f);
			} else if( c < R * 0.8f ){
				mBlockedBySunPer = 0.0f;
			} else {
				mBlockedBySunPer = 1.0f;
			}
		} else {
			mBlockedBySunPer = 1.0f;
		}
    } else {
		mBlockedBySunPer = 1.0f;
	}

	mEclipseColor = ( mColor + Color::white() ) * 0.5f * ( 1.0f - mEclipseStrength * 0.5f );
// END CALCULATE ECLIPSE VARS
/////////////////////////////
	
	
	Node::update( mat );
	
	mTransVel = mTransPos - prevTransPos;	
}

void NodeAlbum::drawEclipseGlow()
{
	Node::drawEclipseGlow();
}

void NodeAlbum::drawPlanet( const vector<gl::Texture> &planets )
{	
	if( mDistFromCamZAxis < -0.02f ){
		mAxialRot = Vec3f( 0.0f, app::getElapsedSeconds() * mAxialVel * 0.75f, mAxialTilt );
		glEnable( GL_RESCALE_NORMAL );
		glEnableClientState( GL_VERTEX_ARRAY );
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		glEnableClientState( GL_NORMAL_ARRAY );
		int numVerts;
		
		// when the planet goes offscreen, the screenradius becomes huge. 
		// so if the screen radius is greater than 500, assume it is offscreen and just render a lo-res version
		// consider frustum culling?
		if( mSphereScreenRadius < 500.0f ){
			if( mSphereScreenRadius > 75.0f ){
				glVertexPointer( 3, GL_FLOAT, 0, mSphereHiVertsRes );
				glTexCoordPointer( 2, GL_FLOAT, 0, mSphereHiTexCoordsRes );
				glNormalPointer( GL_FLOAT, 0, mSphereHiNormalsRes );
				numVerts = mTotalHiVertsRes;
			} else if( mSphereScreenRadius > 35.0f ){
				glVertexPointer( 3, GL_FLOAT, 0, mSphereMdVertsRes );
				glTexCoordPointer( 2, GL_FLOAT, 0, mSphereMdTexCoordsRes );
				glNormalPointer( GL_FLOAT, 0, mSphereMdNormalsRes );
				numVerts = mTotalMdVertsRes;
			} else if( mSphereScreenRadius > 10.0f ){
				glVertexPointer( 3, GL_FLOAT, 0, mSphereLoVertsRes );
				glTexCoordPointer( 2, GL_FLOAT, 0, mSphereLoTexCoordsRes );
				glNormalPointer( GL_FLOAT, 0, mSphereLoNormalsRes );
				numVerts = mTotalLoVertsRes;
			} else {
				glVertexPointer( 3, GL_FLOAT, 0, mSphereTyVertsRes );
				glTexCoordPointer( 2, GL_FLOAT, 0, mSphereTyTexCoordsRes );
				glNormalPointer( GL_FLOAT, 0, mSphereTyNormalsRes );
				numVerts = mTotalTyVertsRes;
			}
		} else {
			glVertexPointer( 3, GL_FLOAT, 0, mSphereLoVertsRes );
			glTexCoordPointer( 2, GL_FLOAT, 0, mSphereLoTexCoordsRes );
			glNormalPointer( GL_FLOAT, 0, mSphereLoNormalsRes );
			numVerts = mTotalLoVertsRes;
		}
		
		gl::pushModelView();
		gl::translate( mTransPos );
		gl::scale( Vec3f( mRadius, mRadius, mRadius ) * mDeathPer );
		gl::rotate( mMatrix );
		gl::rotate( mAxialRot );
		gl::color( ColorA( mEclipseColor + ( 1.0f - mBlockedBySunPer ), mBlockedBySunPer ) );
		
		if( mHasAlbumArt ){
			mAlbumArtTex.enableAndBind();
		} else {
			planets[mPlanetTexIndex].enableAndBind();
		}
		
		glDrawArrays( GL_TRIANGLES, 0, numVerts );
		gl::popModelView();
		
		glDisableClientState( GL_VERTEX_ARRAY );
		glDisableClientState( GL_TEXTURE_COORD_ARRAY );
		glDisableClientState( GL_NORMAL_ARRAY );
	}
}


void NodeAlbum::drawClouds( const vector<gl::Texture> &clouds )
{
	/*
	if( mSphereScreenRadius > 5.0f && mDistFromCamZAxisPer > 0.0f ){
		mAxialRot = Vec3f( 0.0f, app::getElapsedSeconds() * mAxialVel, mAxialTilt );
		
		glEnableClientState( GL_VERTEX_ARRAY );
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		glEnableClientState( GL_NORMAL_ARRAY );
		int numVerts;
		// when the planet goes offscreen, the screenradius becomes huge. 
		// so if the screen radius is greater than 500, assume it is offscreen and just render a lo-res version
		// consider frustum culling?
		if( mSphereScreenRadius < 500.0f ){
			if( mSphereScreenRadius > 75.0f ){
				glVertexPointer( 3, GL_FLOAT, 0, mSphereHiVertsRes );
				glTexCoordPointer( 2, GL_FLOAT, 0, mSphereHiTexCoordsRes );
				glNormalPointer( GL_FLOAT, 0, mSphereHiNormalsRes );
				numVerts = mTotalHiVertsRes;
			} else if( mSphereScreenRadius > 35.0f ){
				glVertexPointer( 3, GL_FLOAT, 0, mSphereMdVertsRes );
				glTexCoordPointer( 2, GL_FLOAT, 0, mSphereMdTexCoordsRes );
				glNormalPointer( GL_FLOAT, 0, mSphereMdNormalsRes );
				numVerts = mTotalMdVertsRes;
			} else if( mSphereScreenRadius > 10.0f ){
				glVertexPointer( 3, GL_FLOAT, 0, mSphereLoVertsRes );
				glTexCoordPointer( 2, GL_FLOAT, 0, mSphereLoTexCoordsRes );
				glNormalPointer( GL_FLOAT, 0, mSphereLoNormalsRes );
				numVerts = mTotalLoVertsRes;
			} else {
				glVertexPointer( 3, GL_FLOAT, 0, mSphereTyVertsRes );
				glTexCoordPointer( 2, GL_FLOAT, 0, mSphereTyTexCoordsRes );
				glNormalPointer( GL_FLOAT, 0, mSphereTyNormalsRes );
				numVerts = mTotalTyVertsRes;
			}
		} else {
			glVertexPointer( 3, GL_FLOAT, 0, mSphereLoVertsRes );
			glTexCoordPointer( 2, GL_FLOAT, 0, mSphereLoTexCoordsRes );
			glNormalPointer( GL_FLOAT, 0, mSphereLoNormalsRes );
			numVerts = mTotalLoVertsRes;
		}
		
		
		gl::enableAlphaBlending();
		gl::pushModelView();
		gl::translate( mTransPos );
		gl::pushModelView();
		float radius = mRadius * mDeathPer + mCloudLayerRadius;
		float alpha = max( ( mDistFromCamZAxis + 5.0f ) * 0.25f, 0.0f );
		gl::scale( Vec3f( radius, radius, radius ) );
		//glEnable( GL_RESCALE_NORMAL );
		gl::rotate( mMatrix );
		gl::rotate( mAxialRot );
// SHADOW CLOUDS
//				glDisable( GL_LIGHTING );
		gl::color( ColorA( 0.0f, 0.0f, 0.0f, alpha * 0.5f ) );
		clouds[mCloudTexIndex].enableAndBind();
		glDrawArrays( GL_TRIANGLES, 0, numVerts );
//				glEnable( GL_LIGHTING );
		gl::popModelView();
			
// LIT CLOUDS
		gl::enableAdditiveBlending();
		gl::pushModelView();
		radius = mRadius * mDeathPer + mCloudLayerRadius*2.0f;
		gl::scale( Vec3f( radius, radius, radius ) );
		//glEnable( GL_RESCALE_NORMAL );
		gl::rotate( mMatrix );
		gl::rotate( mAxialRot );
		gl::enableAdditiveBlending();
		gl::color( ColorA( mEclipseColor, alpha ) );
		glDrawArrays( GL_TRIANGLES, 0, numVerts );
		gl::popModelView();
		gl::popModelView();
		
		glDisableClientState( GL_VERTEX_ARRAY );
		glDisableClientState( GL_TEXTURE_COORD_ARRAY );
		glDisableClientState( GL_NORMAL_ARRAY );
	}
	 */
}


void NodeAlbum::drawAtmosphere( const gl::Texture &tex, const gl::Texture &directionalTex, float pinchAlphaPer )
{
	//if( mIsHighlighted || mIsSelected || mIsPlaying ){
		if( mDistFromCamZAxisPer > 0.02f ){
			Vec2f dir		= mScreenPos - app::getWindowCenter();
			float dirLength = dir.length()/500.0f;
			float angle		= atan2( dir.y, dir.x );
			float stretch	= dirLength * 0.1f;
			gl::enableAdditiveBlending();
			float alpha = ( 1.0f - dirLength * 0.75f ) + ( mEclipseStrength );
			
			gl::color( ColorA( mColor, alpha * mDeathPer * mBlockedBySunPer ) );
			
			Vec2f radius = Vec2f( mRadius * ( 1.0f + stretch ), mRadius ) * 2.46f;
			//Vec2f radius = Vec2f( mRadius, mRadius ) * 2.46f;
			
			tex.enableAndBind();
			Vec3f posOffset = Vec3f( cos(angle), sin(angle), 0.0f ) * stretch * 0.1f;
			gl::drawBillboard( mTransPos - posOffset, radius, -toDegrees( angle ), mBbRight, mBbUp );
			tex.disable();

			gl::color( ColorA( mColor, alpha * mEclipseDirBasedAlpha * mDeathPer * mBlockedBySunPer ) );
			directionalTex.enableAndBind();
			gl::drawBillboard( mTransPos, radius, -toDegrees( mEclipseAngle ), mBbRight, mBbUp );
			directionalTex.disable();
		}
	//}
}


void NodeAlbum::drawOrbitRing( float pinchAlphaPer, float camAlpha, const gl::Texture &orbitRingGradient, GLfloat *ringVertsLowRes, GLfloat *ringTexLowRes, GLfloat *ringVertsHighRes, GLfloat *ringTexHighRes )
{		
	float newPinchAlphaPer = pinchAlphaPer;
	if( G_ZOOM < G_ALBUM_LEVEL - 0.5f ){
		newPinchAlphaPer = pinchAlphaPer;
	} else {
		newPinchAlphaPer = 1.0f;
	}
	
	
	
	if( mIsPlaying ){
		gl::color( ColorA( BRIGHT_BLUE, 0.5f * camAlpha * mDeathPer ) );
	} else {
		gl::color( ColorA( BLUE, 0.5f * camAlpha * mDeathPer ) );
	}
	
	gl::pushModelView();
	gl::translate( mParentNode->mTransPos );
	gl::scale( Vec3f( mOrbitRadius, mOrbitRadius, mOrbitRadius ) );
	gl::rotate( mMatrix );
	gl::rotate( Vec3f( 90.0f, 0.0f, toDegrees( mOrbitAngle ) ) );
	
	orbitRingGradient.enableAndBind();
	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glVertexPointer( 2, GL_FLOAT, 0, ringVertsHighRes );
	glTexCoordPointer( 2, GL_FLOAT, 0, ringTexHighRes );
	glDrawArrays( GL_LINE_STRIP, 0, G_RING_HIGH_RES );
	glDisableClientState( GL_VERTEX_ARRAY );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	orbitRingGradient.disable();
	gl::popModelView();
	
	
	
	Node::drawOrbitRing( pinchAlphaPer, camAlpha, orbitRingGradient, ringVertsLowRes, ringTexLowRes, ringVertsHighRes, ringTexHighRes );
}



void NodeAlbum::drawRings( const gl::Texture &tex, GLfloat *planetRingVerts, GLfloat *planetRingTexCoords, float camAlpha )
{
	if( mHasRings && G_ZOOM > G_ARTIST_LEVEL ){
		if( mIsSelected || mIsPlaying ){
			gl::enableAdditiveBlending();
			
			gl::pushModelView();
			gl::translate( mTransPos );
			float c = mRadius * 7.0f;
			gl::scale( Vec3f( c, c, c ) );
			gl::rotate( mMatrix );
			gl::rotate( Vec3f( 0.0f, app::getElapsedSeconds() * mAxialVel * 0.2f, 0.0f ) );
			
			
			float zoomPer = constrain( 1.0f - ( mGen - G_ZOOM ), 0.0f, 1.0f );
			gl::color( ColorA( mColor, camAlpha * zoomPer ) );
			tex.enableAndBind();
			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glVertexPointer( 3, GL_FLOAT, 0, planetRingVerts );
			glTexCoordPointer( 2, GL_FLOAT, 0, planetRingTexCoords );
			
			glDrawArrays( GL_TRIANGLES, 0, 6 );
			
			glDisableClientState( GL_VERTEX_ARRAY );
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			tex.disable();
			gl::popModelView();
		}
	}
}


void NodeAlbum::select()
{
	if( !mIsSelected ){
		if( mChildNodes.size() == 0 ){
			for (int i = 0; i < mNumTracks; i++) {
				TrackRef track		= (*mAlbum)[i];
				string name			= track->getTitle();
				NodeTrack *newNode	= new NodeTrack( this, i, mFont, mSmallFont, mSurfaces );
				mChildNodes.push_back( newNode );
				newNode->setData( track, mAlbum, mAlbumArtSurface );
			}
			
			for( vector<Node*>::iterator it = mChildNodes.begin(); it != mChildNodes.end(); ++it ){
				(*it)->setSphereData( mTotalHiVertsRes, mSphereHiVertsRes, mSphereHiTexCoordsRes, mSphereHiNormalsRes,
									  mTotalMdVertsRes, mSphereMdVertsRes, mSphereMdTexCoordsRes, mSphereMdNormalsRes,
									  mTotalLoVertsRes, mSphereLoVertsRes, mSphereLoTexCoordsRes, mSphereLoNormalsRes,
									  mTotalTyVertsRes, mSphereTyVertsRes, mSphereTyTexCoordsRes, mSphereTyNormalsRes );
			}
			
			setChildOrbitRadii();
			
			
		} else {
			for( vector<Node*>::iterator it = mChildNodes.begin(); it != mChildNodes.end(); ++it ){
				(*it)->setIsDying( false );
			}
		}
	}	
	Node::select();
}

void NodeAlbum::setChildOrbitRadii()
{
	float orbitRadius = mOrbitRadiusMin;
	float orbitOffset;
	for( vector<Node*>::iterator it = mChildNodes.begin(); it != mChildNodes.end(); ++it ){
		orbitOffset = (*it)->mRadiusDest * 2.0f;
		orbitRadius += orbitOffset;
		(*it)->mOrbitRadiusDest = orbitRadius;
		orbitRadius += orbitOffset;
	}
	
	mIdealCameraDist	= orbitRadius * 2.0f;
}

float NodeAlbum::getReleaseYear()
{
	return mReleaseYear;
}

string NodeAlbum::getName()
{
	string name = mAlbum->getAlbumTitle();
	if( name.size() < 1 ) name = "Untitled";
	return name;
}

uint64_t NodeAlbum::getId()
{
    return mAlbum->getAlbumId();
}
