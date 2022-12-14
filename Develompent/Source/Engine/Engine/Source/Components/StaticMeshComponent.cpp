#include "Actors/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Render/Scene.h"
#include "Render/SceneUtils.h"

IMPLEMENT_CLASS( CStaticMeshComponent )

CStaticMeshComponent::CStaticMeshComponent()
{}

CStaticMeshComponent::~CStaticMeshComponent()
{}

void CStaticMeshComponent::Serialize( class CArchive& InArchive )
{
	Super::Serialize( InArchive );
	InArchive << staticMesh;
	InArchive << overrideMaterials;
}

void CStaticMeshComponent::LinkDrawList()
{
	check( scene );

	// If the primitive already added to scene - remove all draw policy links
	if ( elementDrawingPolicyLink )
	{
		UnlinkDrawList();
	}

	// If static mesh is valid - add to scene draw policy link
	TSharedPtr<CStaticMesh>		staticMeshRef = staticMesh.ToSharedPtr();
	if ( staticMeshRef )
	{
		elementDrawingPolicyLink = staticMeshRef->LinkDrawList( scene->GetSDG( SDG_World ), overrideMaterials );
	}
}

void CStaticMeshComponent::UnlinkDrawList()
{
	check( scene );

	// If the primitive already added to scene - remove all draw policy links
	if ( elementDrawingPolicyLink )
	{
		TSharedPtr<CStaticMesh>		staticMeshRef = staticMesh.ToSharedPtr();
		if ( staticMeshRef )
		{
			staticMeshRef->UnlinkDrawList( scene->GetSDG( SDG_World ), elementDrawingPolicyLink );
		}
		else
		{
			elementDrawingPolicyLink.Reset();
		}
	}
}

void CStaticMeshComponent::AddToDrawList( const class CSceneView& InSceneView )
{
	// If primitive is empty - exit from method
	if ( !bIsDirtyDrawingPolicyLink && !elementDrawingPolicyLink )
	{
		return;
	}

	// If drawing policy link is dirty - we update it
	if ( bIsDirtyDrawingPolicyLink || elementDrawingPolicyLink->bDirty )
	{
		bIsDirtyDrawingPolicyLink = false;
		
		LinkDrawList();
		if ( !staticMesh.IsAssetValid() )
		{
			return;
		}
	}

	AActor*		owner = GetOwner();

	// Add to mesh batch new instance
	const Matrix				transformationMatrix = GetComponentTransform().ToMatrix();
	for ( uint32 index = 0, count = elementDrawingPolicyLink->meshBatchLinks.size(); index < count; ++index )
	{
		const SMeshBatch*		meshBatch = elementDrawingPolicyLink->meshBatchLinks[ index ];
		++meshBatch->numInstances;
		meshBatch->instances.push_back( SMeshInstance{ transformationMatrix 
#if ENABLE_HITPROXY
										, owner ? owner->GetHitProxyId() : CHitProxyId()
#endif // ENABLE_HITPROXY

#if WITH_EDITOR
										, owner? owner->IsSelected() : false
#endif // WITH_EDITOR
										} );
	}
}
