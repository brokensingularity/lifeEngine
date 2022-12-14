#if WITH_BOX2D
#include "Misc/PhysicsGlobals.h"
#include "Logger/LoggerMacros.h"
#include "System/PhysicsMaterial.h"
#include "System/PhysicsBoxGeometry.h"
#include "System/PhysicsEngine.h"
#include "PhysicsInterfaceBox2D.h"

static uint32 GCollisionChannelBits[] =
{
	0x0001,			// CC_WorldStatic
	0x0002,			// CC_Visibility
	0x0004,			// CC_Character
};
static_assert( ARRAY_COUNT( GCollisionChannelBits ) == CC_Max, "Need full init GCollisionChannelBits array" );

void SPhysicsActorHandleBox2D::OnMaterialUpdated( const TSharedPtr<CPhysicsMaterial>& InPhysMaterial )
{
	for ( auto itFixture = fixtureMap.begin(), itFixtureEnd = fixtureMap.end(); itFixture != itFixtureEnd; ++itFixture )
	{
		b2Fixture*						bx2Fixture = itFixture->second;
		SPhysicsShapeHandleBox2D*		shapeHandle = ( SPhysicsShapeHandleBox2D* )bx2Fixture->GetUserData().pointer;
		check( shapeHandle );

		if ( shapeHandle->physMaterial != InPhysMaterial )
		{
			continue;
		}

		bx2Fixture->SetDensity( InPhysMaterial->GetDensity() );
		bx2Fixture->SetFriction( bx2Body->GetType() == b2_staticBody ? InPhysMaterial->GetStaticFriction() : InPhysMaterial->GetDynamicFriction() );
		bx2Fixture->SetRestitution( InPhysMaterial->GetRestitution() );
	}
}

void SPhysicsActorHandleBox2D::OnMaterialDestroyed( const TSharedPtr<CPhysicsMaterial>& InPhysMaterial )
{
	float	defaultDensity			= 0.f;
	float	defaultStaticFriction	= 0.f;
	float	defaultDynamicFriction	= 0.f;
	float	defaultRestitution		= 0.f;
	
	TAssetHandle<CPhysicsMaterial>	defaultPhysMaterial		= GPhysicsEngine.GetDefaultPhysMaterial();
	TSharedPtr<CPhysicsMaterial>	defaultPhysMaterialRef	= defaultPhysMaterial.ToSharedPtr();
	if ( defaultPhysMaterialRef )
	{
		defaultDensity			= defaultPhysMaterialRef->GetDensity();
		defaultStaticFriction	= defaultPhysMaterialRef->GetStaticFriction();
		defaultDynamicFriction	= defaultPhysMaterialRef->GetDynamicFriction();
		defaultRestitution		= defaultPhysMaterialRef->GetRestitution();
	}

	for ( auto itFixture = fixtureMap.begin(), itFixtureEnd = fixtureMap.end(); itFixture != itFixtureEnd; ++itFixture )
	{
		b2Fixture*					bx2Fixture = itFixture->second;
		SPhysicsShapeHandleBox2D*	shapeHandle = ( SPhysicsShapeHandleBox2D* )bx2Fixture->GetUserData().pointer;
		check( shapeHandle );

		if ( shapeHandle->physMaterial != InPhysMaterial )
		{
			continue;
		}

		shapeHandle->physMaterial = defaultPhysMaterial;
		bx2Fixture->SetDensity( defaultDensity );
		bx2Fixture->SetFriction( bx2Body->GetType() == b2_staticBody ? defaultStaticFriction : defaultDynamicFriction );
		bx2Fixture->SetRestitution( defaultRestitution );
	}
}

void SPhysicsInterfaceBox2D::Init()
{
	LE_LOG( LT_Log, LC_Init, TEXT( "Box2D version: %i.%i.%i" ), b2_version.major, b2_version.minor, b2_version.revision );
}

void SPhysicsInterfaceBox2D::Shutdown()
{}

SPhysicsMaterialHandleBox2D SPhysicsInterfaceBox2D::CreateMaterial( const TSharedPtr<class CPhysicsMaterial>& InPhysMaterial )
{
	SPhysicsMaterialHandleBox2D		materialHandle;
	materialHandle.physMaterial		= InPhysMaterial->GetAssetHandle();
	return materialHandle;
}

SPhysicsShapeHandleBox2D SPhysicsInterfaceBox2D::CreateShapeGeometry( const struct SPhysicsBoxGeometry& InBoxGeometry )
{
	SPhysicsShapeHandleBox2D		shapeHandle;
	b2PolygonShape*					bx2BoxGeometry		= new b2PolygonShape();
	const b2Vec2					sizeInMeters		= b2Vec2( InBoxGeometry.extent.x / BOX2D_SCALE, InBoxGeometry.extent.y / BOX2D_SCALE );
	const b2Vec2					locationInMeters	= b2Vec2( InBoxGeometry.location.x / BOX2D_SCALE, InBoxGeometry.location.y / BOX2D_SCALE );
	const float						rotationInRadians	= SMath::QuaternionToAngles( InBoxGeometry.rotation ).y / BOX2D_ANGLES;
	
	b2Vec2							boxVerteces[] =
	{
		b2Vec2( 0.f,				0.f ),
		b2Vec2( sizeInMeters.x,		0.f ),
		b2Vec2( sizeInMeters.x,		sizeInMeters.y ),
		b2Vec2( 0.f,				sizeInMeters.y )
	};

	b2Transform			bx2Transform( locationInMeters, b2Rot( rotationInRadians ) );
	for ( uint32 index = 0, count = ARRAY_COUNT( boxVerteces ); index < count; ++index )
	{
		boxVerteces[ index ] = b2Mul( bx2Transform, boxVerteces[ index ] );
	}
	bx2BoxGeometry->Set( &boxVerteces[ 0 ], ARRAY_COUNT( boxVerteces ) );
	
	shapeHandle.bx2Shape			= bx2BoxGeometry;
	shapeHandle.physMaterial		= InBoxGeometry.material;
	shapeHandle.collisionProfile	= InBoxGeometry.collisionProfile;
	return shapeHandle;
}

SPhysicsActorHandleBox2D SPhysicsInterfaceBox2D::CreateActor( const SActorCreationParams& InParams )
{
	PhysicsActorHandle_t			actorHandle;
	b2BodyDef					bx2BodyDef;
	bx2BodyDef.type				= InParams.bStatic ? b2_staticBody : b2_dynamicBody;
	{
		b2Transform				bx2Transform = LE2BTransform( InParams.initialTM );
		bx2BodyDef.position		= bx2Transform.p;
		bx2BodyDef.angle		= bx2Transform.q.GetAngle();
	}
	bx2BodyDef.awake			= InParams.bStartAwake;
	bx2BodyDef.fixedRotation	= LE2BLockFlags( InParams.lockFlags );
	bx2BodyDef.enabled			= InParams.bSimulatePhysics;
	bx2BodyDef.gravityScale		= InParams.bEnableGravity ? 1.f : 0.f;
	actorHandle.bx2Body			= GPhysicsScene.GetBox2DWorld()->CreateBody( &bx2BodyDef );

	b2MassData			bx2MassData;
	bx2MassData.mass	= InParams.mass;
	bx2MassData.center	= b2Vec2( 0.f, 0.f );
	bx2MassData.I		= 0.f;
	actorHandle.bx2Body->SetMassData( &bx2MassData );

	return actorHandle;
}

void SPhysicsInterfaceBox2D::AttachShape( SPhysicsActorHandleBox2D& InActorHandle, const SPhysicsShapeHandleBox2D& InShapeHandle )
{
	check( IsValidActor( InActorHandle ) && IsValidShapeGeometry( InShapeHandle ) );

	// If we already attach this shape - exit from function
	{
		auto		it = InActorHandle.fixtureMap.find( InShapeHandle.bx2Shape );
		if ( it != InActorHandle.fixtureMap.end() )
		{
			return;
		}
	}

	// If physics material is not valid, we use default material
	TSharedPtr<CPhysicsMaterial>	physMaterialRef = InShapeHandle.physMaterial.ToSharedPtr();
	if ( !physMaterialRef )
	{
		physMaterialRef		= GPhysicsEngine.GetDefaultPhysMaterial().ToSharedPtr();
		check( physMaterialRef );
	}

	b2FixtureUserData			bx2FixtureUserData;
	bx2FixtureUserData.pointer	= ( uintptr_t )( &InShapeHandle );

	b2FixtureDef						bx2FixtureDef;
	bx2FixtureDef.shape					= InShapeHandle.bx2Shape;
	bx2FixtureDef.friction				= InActorHandle.bx2Body->GetType() == b2_staticBody ? physMaterialRef->GetStaticFriction() : physMaterialRef->GetDynamicFriction();
	bx2FixtureDef.density				= physMaterialRef->GetDensity();
	bx2FixtureDef.restitution			= physMaterialRef->GetRestitution();
	bx2FixtureDef.userData				= bx2FixtureUserData;
	bx2FixtureDef.filter.categoryBits	= GCollisionChannelBits[ InShapeHandle.collisionProfile->objectType ];
	bx2FixtureDef.filter.maskBits		= 0x0;
	for ( uint32 indexCollisionChannel = 0; indexCollisionChannel < CC_Max; ++indexCollisionChannel )
	{
		if ( InShapeHandle.collisionProfile->responses[ indexCollisionChannel ] != CR_Block )
		{
			continue;
		}

		bx2FixtureDef.filter.maskBits |= GCollisionChannelBits[ indexCollisionChannel ];
	}

	b2Fixture*		bx2Fixture = InActorHandle.bx2Body->CreateFixture( &bx2FixtureDef );
	InActorHandle.fixtureMap[ InShapeHandle.bx2Shape ]		= bx2Fixture;
	InActorHandle.physicsMaterialUpdateHandle		= physMaterialRef->OnPhysicsMaterialUpdate().Add(		std::bind( &SPhysicsActorHandleBox2D::OnMaterialUpdated, &InActorHandle, std::placeholders::_1 )	);
	InActorHandle.physicsMaterialDestroyedHandle	= physMaterialRef->OnPhysicsMaterialDestroyed().Add(	std::bind( &SPhysicsActorHandleBox2D::OnMaterialDestroyed, &InActorHandle, std::placeholders::_1 )	);
}

void SPhysicsInterfaceBox2D::DetachShape( SPhysicsActorHandleBox2D& InActorHandle, const SPhysicsShapeHandleBox2D& InShapeHandle )
{
	check( IsValidActor( InActorHandle ) && IsValidShapeGeometry( InShapeHandle ) );

	// Remove fixture
	auto		itFixture = InActorHandle.fixtureMap.find( InShapeHandle.bx2Shape );
	if ( itFixture == InActorHandle.fixtureMap.end() )
	{
		return;
	}

	InActorHandle.bx2Body->DestroyFixture( itFixture->second );
	InActorHandle.fixtureMap.erase( itFixture );

	TSharedPtr<CPhysicsMaterial>	physMaterialRef = InShapeHandle.physMaterial.ToSharedPtr();
	if ( physMaterialRef )
	{
		physMaterialRef->OnPhysicsMaterialUpdate().Remove( InActorHandle.physicsMaterialUpdateHandle );
		physMaterialRef->OnPhysicsMaterialDestroyed().Remove( InActorHandle.physicsMaterialDestroyedHandle );
	}
}
#endif // WITH_BOX2D