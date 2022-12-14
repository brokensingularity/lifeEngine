#include "Actors/ELPlayerController.h"
#include "Actors/Character.h"

IMPLEMENT_CLASS( AELPlayerController )

AELPlayerController::AELPlayerController()
{
	bShowMouseCursor	= false;
	bConstrainYaw		= true;
}

void AELPlayerController::SetupInputComponent()
{
	// Bind actions
	inputComponent->BindAction( TEXT( "Exit" ), IE_Released,	std::bind( &AELPlayerController::ExitFromGame, this ) );
	inputComponent->BindAction( TEXT( "Jump" ), IE_Pressed,		std::bind( &AELPlayerController::Jump, this ) );
	inputComponent->BindAction( TEXT( "Jump" ), IE_Released,	std::bind( &AELPlayerController::StopJump, this ) );

	// Bind axis
	inputComponent->BindAxis( TEXT( "MoveForward" ),	std::bind( &AELPlayerController::MoveForward, this, std::placeholders::_1	) );
	inputComponent->BindAxis( TEXT( "MoveRight" ),		std::bind( &AELPlayerController::MoveRight, this, std::placeholders::_1		) );
	inputComponent->BindAxis( TEXT( "Turn" ),			std::bind( &AELPlayerController::Turn, this, std::placeholders::_1			) );
	inputComponent->BindAxis( TEXT( "LookUp" ),			std::bind( &AELPlayerController::LookUp, this, std::placeholders::_1		) );
}

void AELPlayerController::ExitFromGame()
{
	GIsRequestingExit = true;
}

void AELPlayerController::MoveForward( float InValue )
{
	character->Walk( character->GetActorForwardVector(), InValue );
}

void AELPlayerController::MoveRight( float InValue )
{
	character->Walk( character->GetActorRightVector(), InValue );
}

void AELPlayerController::Turn( float InValue )
{
	AddYawInput( InValue );
}

void AELPlayerController::LookUp( float InValue )
{
	AddPitchInput( InValue );
}

void AELPlayerController::Jump()
{
	character->Jump();
}

void AELPlayerController::StopJump()
{
	character->StopJump();
}