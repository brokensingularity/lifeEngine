/**
 * @file
 * @addtogroup Engine Engine
 *
 * Copyright Broken Singularity, All Rights Reserved.
 * Authors: Yehor Pohuliaka (zombiHello)
 */

#ifndef BASEENGINE_H
#define BASEENGINE_H

#include <string>

#include "Misc/Object.h"
#include "Render/Texture.h"
#include "Render/Material.h"
#include "Core.h"

/**
 * @ingroup Engine
 * Class of base engine
 */
class CBaseEngine : public CObject
{
	DECLARE_CLASS( CBaseEngine, CObject )

public:
	/**
	 * Destructor
	 */
	virtual ~CBaseEngine() {}

	/**
	 * Initialize engine
	 */
	virtual void Init();

	/**
	 * Update logic of engine
	 * 
	 * @param[in] InDeltaTime Delta time
	 */
	virtual void Tick( float InDeltaSeconds );

	/**
	 * Shutdown engine
	 */
	virtual void Shutdown();

	/**
	 * Load map
	 * 
	 * @param[in] InMap Path to map
	 * @param[out] OutError Error of loading map
	 * @return Return true if map loaded successed, else return false
	 */
	virtual bool LoadMap( const std::wstring& InMap, std::wstring& OutError );

	/**
	 * @brief Process event
	 *
	 * @param[in] InWindowEvent Window event
	 */
	virtual void ProcessEvent( struct SWindowEvent& InWindowEvent );

	/**
	 * Get max tick rate in second
	 * @return Return max tick rate
	 */
	virtual float GetMaxTickRate() const;

	/**
	 * Get default texture
	 * @return Return default texture
	 */
	FORCEINLINE TAssetHandle<CTexture2D> GetDefaultTexture() const
	{
		return defaultTexture;
	}

	/**
	 * Get default material
	 * @return Return default material
	 */
	FORCEINLINE TAssetHandle<CMaterial> GetDefaultMaterial() const
	{
		return defaultMaterial;
	}

#if WITH_EDITOR
	/**
	 * Get default wireframe material
	 * @return Return default wireframe material
	 */
	FORCEINLINE TAssetHandle<CMaterial> GetDefaultWireframeMaterial() const
	{
		return defaultWireframeMaterial;
	}
#endif // WITH_EDITOR

protected:
	TAssetHandle<CTexture2D>		defaultTexture;					/**< Default texture */
	TAssetHandle<CMaterial>			defaultMaterial;				/**< Default material */

#if WITH_EDITOR
	TAssetHandle<CMaterial>			defaultWireframeMaterial;		/**< Default wireframe material */
#endif // WITH_EDITOR
};

#endif // !BASEENGINE_H