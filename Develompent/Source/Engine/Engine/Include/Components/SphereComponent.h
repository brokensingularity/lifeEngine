/**
 * @file
 * @addtogroup Engine Engine
 *
 * Copyright Broken Singularity, All Rights Reserved.
 * Authors: Yehor Pohuliaka (zombiHello)
 */

#ifndef SPHERECOMPONENT_H
#define SPHERECOMPONENT_H

#include "Components/ShapeComponent.h"
#include "Render/Scene.h"
#include "Render/Material.h"

 /**
  * @ingroup Engine
  * Sphere component
  */
class CSphereComponent : public CShapeComponent
{
	DECLARE_CLASS( CSphereComponent, CShapeComponent )

public:
	/**
	 * Constructor
	 */
	CSphereComponent();

	/**
	 * @brief Serialize component
	 * @param[in] InArchive Archive for serialize
	 */
	virtual void Serialize( class CArchive& InArchive ) override;

	/**
	 * @brief Update the body setup parameters based on shape information
	 */
	virtual void UpdateBodySetup() override;

	/**
	 * @brief Adds mesh batches for draw in scene
	 *
	 * @param InSceneView Current view of scene
	 */
	virtual void AddToDrawList( const class CSceneView& InSceneView ) override;

	/**
	 * @brief Set SDG level
	 * @param InSDGLevel	SDG level for draw
	 */
	FORCEINLINE void SetSDGLevel( ESceneDepthGroup InSDGLevel )
	{
		pendingSDGLevel = InSDGLevel;
		bIsDirtyDrawingPolicyLink = true;
	}

	/**
	 * @brief Set radius
	 * @param InRadius	Radius
	 */
	FORCEINLINE void SetRadius( float InRadius )
	{
		radius = InRadius;
		bIsDirtyDrawingPolicyLink = true;
	}

	/**
	 * @brief Set material
	 * @param InMaterial	Material
	 */
	FORCEINLINE void SetMaterial( const TAssetHandle<CMaterial>& InMaterial )
	{
		material = InMaterial;
		bIsDirtyDrawingPolicyLink = true;
	}

	/**
	 * @brief Get SDG level
	 * @return Return SDG level for draw
	 */
	FORCEINLINE ESceneDepthGroup GetSDGLevel() const
	{
		return SDGLevel;
	}

	/**
	 * @brief Get radius
	 * @return Return radius
	 */
	FORCEINLINE float GetRadius() const
	{
		return radius;
	}

	/**
	 * @brief Get material
	 * @return Return material, if not exist return NULL
	 */
	FORCEINLINE TAssetHandle<CMaterial> GetMaterial() const
	{
		return material;
	}

private:
	/**
	 * @brief Typedef of drawing policy link
	 */
	typedef CMeshDrawList<CMeshDrawingPolicy>::SDrawingPolicyLink			DrawingPolicyLink_t;

	/**
	 * @brief Typedef of reference on drawing policy link in scene
	 */
	typedef CMeshDrawList<CMeshDrawingPolicy>::DrawingPolicyLinkRef_t		DrawingPolicyLinkRef_t;

	/**
	 * @brief Adds a draw policy link in SDGs
	 */
	virtual void LinkDrawList() override;

	/**
	 * @brief Removes a draw policy link from SDGs
	 */
	virtual void UnlinkDrawList() override;

	float						radius;					/**< Radius of sphere */
	ESceneDepthGroup			SDGLevel;				/**< Mesh on SDG level */
	ESceneDepthGroup			pendingSDGLevel;		/**< Pending SDG level */
	TAssetHandle<CMaterial>		material;				/**< Material */
	DrawingPolicyLinkRef_t		drawingPolicyLink;		/**< Drawing policy link in scene */
	const SMeshBatch*			meshBatchLink;			/**< Mesh batch in drawing policy link */
};

#endif // !SPHERECOMPONENT_H