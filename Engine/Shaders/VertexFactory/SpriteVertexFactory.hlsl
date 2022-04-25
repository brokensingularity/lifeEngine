#ifndef VERTEXFACTORY_H
#define VERTEXFACTORY_H 0

#include "Common.hlsl"
#include "VertexFactory/VertexFactoryCommon.hlsl"

struct FVertexFactoryInput
{
	float4 		position				: POSITION;
	float2 		texCoord0				: TEXCOORD0;
	float4		normal					: NORMAL0;
	
#if USE_INSTANCING
	float4x4 	instanceLocalToWorld 	: POSITION1;
#endif // USE_INSTANCING
};

bool		bFlipVertical;
bool		bFlipHorizontal;
float4		textureRect;
float2		spriteSize;

float4 VertexFactory_GetLocalPosition( FVertexFactoryInput InInput )
{
	return InInput.position * float4( spriteSize, 1.f, 1.f );
}

float4 VertexFactory_GetWorldPosition( FVertexFactoryInput InInput )
{
#if USE_INSTANCING
	return MulMatrix( InInput.instanceLocalToWorld, VertexFactory_GetLocalPosition( InInput ) );
#else
	return MulMatrix( localToWorldMatrix, VertexFactory_GetLocalPosition( InInput ) );
#endif // USE_INSTANCING
}

float2 VertexFactory_GetTexCoord( FVertexFactoryInput InInput, uint InTexCoordIndex )
{
	// TODO BS yehor.pohuliaka - Need fix fliping, because on tile texture is not correct work
	float2	outTexCoord = textureRect.xy + ( InInput.texCoord0 * textureRect.zw );	
	if ( bFlipVertical )
	{
		outTexCoord.y *= -1.f;
	}
	
	if ( bFlipHorizontal )
	{
		outTexCoord.x *= -1.f;
	}
	return outTexCoord;
}

float4 VertexFactory_GetColor( FVertexFactoryInput InInput, uint InColorIndex )
{
	return float4( 1.f, 1.f, 1.f, 1.f );
}

#endif // !VERTEXFACTORY_H