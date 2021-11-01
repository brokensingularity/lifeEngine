/**
 * @file
 * @addtogroup Engine Engine
 *
 * Copyright Broken Singularity, All Rights Reserved.
 * Authors: Yehor Pohuliaka (zombiHello)
 */

#ifndef PLAYERSTART_H
#define PLAYERSTART_H

#include "Actors/Actor.h"

/**
 * @ingroup Engine
 * Actor for spawn player controller in world
 */
class APlayerStart : public AActor
{
	DECLARE_CLASS( APlayerStart, AActor )

public:
	/**
	 * Destructor
	 */
	virtual ~APlayerStart();

	/**
	 * Overridable native event for when play begins for this actor
	 */
	virtual void BeginPlay() override;
};

#endif // !PLAYERSTART_H