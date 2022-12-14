#include <AL/al.h>
#include <AL/alc.h>

#include "Misc/CoreGlobals.h"
#include "System/AudioDevice.h"
#include "Logger/LoggerMacros.h"
#include "System/Config.h"
#include "Containers/StringConv.h"
#include "Core.h"

uint32 appSampleFormatToEngine( ESampleFormat InSampleFormat )
{
	switch ( InSampleFormat )
	{
	case SF_Mono8:		return AL_FORMAT_MONO8;
	case SF_Mono16:		return AL_FORMAT_MONO16;
	case SF_Stereo8:	return AL_FORMAT_STEREO8;
	case SF_Stereo16:	return AL_FORMAT_STEREO16;
	case SF_Unknown:
	default:
		appErrorf( TEXT( "Unknown sample format 0x%X" ), InSampleFormat );
		return 0;
	}
}

std::wstring appSampleFormatToText( ESampleFormat InSampleFormat )
{
	switch ( InSampleFormat )
	{
	case SF_Mono8:		return TEXT( "8 bit (Mono)" );
	case SF_Mono16:		return TEXT( "16 bit (Mono)" );
	case SF_Stereo8:	return TEXT( "8 bit (Stereo)" );
	case SF_Stereo16:	return TEXT( "16 bit (Stereo)" );
	case SF_Unknown:
	default:
		appErrorf( TEXT( "Unknown sample format 0x%X" ), InSampleFormat );
		return TEXT( "Unknown");
	}
}

uint32 appGetNumSampleBytes( ESampleFormat InSampleFormat )
{
	switch ( InSampleFormat )
	{
	case SF_Mono8:		return 8;
	case SF_Mono16:		return 16;
	case SF_Stereo8:	return 16;
	case SF_Stereo16:	return 32;
	case SF_Unknown:
	default:
		appErrorf( TEXT( "Unknown sample format 0x%X" ), InSampleFormat );
		return 0;
	}
}

CAudioDevice::CAudioDevice()
	: bIsMuted( false )
	, alDevice( nullptr )
	, alContext( nullptr )
	, globalVolume( 100.f )
	, platformAudioHeadroom( 1.f )
{}

CAudioDevice::~CAudioDevice()
{
	Shutdown();
}

void CAudioDevice::Init()
{
	// Open audio device
	alDevice = alcOpenDevice( nullptr );
	if ( !alDevice )
	{
		LE_LOG( LT_Warning, LC_Init, TEXT( "Failed to create the audio device" ) );
		return;
	}

	// Create and make current context
	alContext = alcCreateContext( alDevice, nullptr );
	if ( !alContext )
	{
		LE_LOG( LT_Warning, LC_Init, TEXT( "Failed to create the audio context" ) );
		return;
	}

	alcMakeContextCurrent( alContext );

	// Print available playback devices
	LE_LOG( LT_Log, LC_Init, TEXT( "Available playback devices: %s" ),	ANSI_TO_TCHAR( IsExtensionSupported( "ALC_ENUMERATE_ALL_EXT" ) ? alcGetString( nullptr, ALC_ALL_DEVICES_SPECIFIER ) : alcGetString( nullptr, ALC_DEVICE_SPECIFIER ) ) );
	LE_LOG( LT_Log, LC_Init, TEXT( "Available capture devices: %s" ),	ANSI_TO_TCHAR( alcGetString( nullptr, ALC_CAPTURE_DEVICE_SPECIFIER ) ) );
	LE_LOG( LT_Log, LC_Init, TEXT( "Default playback device: %s" ),		ANSI_TO_TCHAR( IsExtensionSupported( "ALC_ENUMERATE_ALL_EXT" ) ? alcGetString( nullptr, ALC_DEFAULT_ALL_DEVICES_SPECIFIER ) : alcGetString( nullptr, ALC_DEFAULT_DEVICE_SPECIFIER ) ) );
	LE_LOG( LT_Log, LC_Init, TEXT( "Default capture device: %s" ),		ANSI_TO_TCHAR( alcGetString( nullptr, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER ) ) );
	LE_LOG( LT_Log, LC_Init, TEXT( "Selected audio device: %s" ),		ANSI_TO_TCHAR( IsExtensionSupported( "ALC_ENUMERATE_ALL_EXT" ) ? alcGetString( alDevice, ALC_ALL_DEVICES_SPECIFIER ) : alcGetString( alDevice, ALC_DEVICE_SPECIFIER ) ) );

	ALCint			major = 0;
	ALCint			minor = 0;
	alcGetIntegerv( alDevice, ALC_MAJOR_VERSION, 1, &major );
	alcGetIntegerv( alDevice, ALC_MINOR_VERSION, 1, &minor );
	LE_LOG( LT_Log, LC_Init, TEXT( "ALC version %i.%i" ), major, minor );
	LE_LOG( LT_Log, LC_Init, TEXT( "ALC extensions: %s" ),		ANSI_TO_TCHAR( alcGetString( alDevice, ALC_EXTENSIONS ) ) );

	// Print info by OpenAL and extensions		
	LE_LOG( LT_Log, LC_Init, TEXT( "OpenAL vendor: %s" ),		ANSI_TO_TCHAR( alGetString( AL_VENDOR ) ) );
	LE_LOG( LT_Log, LC_Init, TEXT( "OpenAL renderer: %s" ),		ANSI_TO_TCHAR( alGetString( AL_RENDERER ) ) );
	LE_LOG( LT_Log, LC_Init, TEXT( "OpenAL version: %s" ),		ANSI_TO_TCHAR( alGetString( AL_VERSION ) ) );
	LE_LOG( LT_Log, LC_Init, TEXT( "OpenAL extensions: %s" ),	ANSI_TO_TCHAR( alGetString( AL_EXTENSIONS ) ) );

	// Init platform audio headroom
	float		headroom = GConfig.GetValue( CT_Engine, TEXT( "Audio.Audio" ), TEXT( "PlatformHeadroomDB" ) ).GetNumber();
	if ( headroom != 0.f )
	{
		// Convert dB to linear volume
		platformAudioHeadroom = SMath::Pow( 10.f, headroom / 20.f );
	}
	else
	{
		platformAudioHeadroom = 1.f;
	}

	// Getting global volume from config
	float		globalVolume = 1.f;
	{
		CConfigValue		configGlobalVolume = GConfig.GetValue( CT_Engine, TEXT( "Audio.Audio" ), TEXT( "GlobalVolume" ) );
		if ( configGlobalVolume.IsValid() && ( configGlobalVolume.GetType() == CConfigValue::T_Int || configGlobalVolume.GetType() == CConfigValue::T_Float ) )
		{
			globalVolume = configGlobalVolume.GetNumber();
		}
	}

	// Initialize listener spatial
	SetListenerSpatial( SMath::vectorZero, SMath::vectorForward, SMath::vectorUp );
	SetGlobalVolume( globalVolume );
}

void CAudioDevice::Shutdown()
{
	alcMakeContextCurrent( nullptr );
	if ( alContext )
	{
		alcDestroyContext( alContext );
	}

	if ( alDevice )
	{
		alcCloseDevice( alDevice );
	}

	alContext = nullptr;
	alDevice = nullptr;
}

bool CAudioDevice::IsExtensionSupported( const std::string& InExtension ) const
{
	if ( InExtension.size() > 2 && InExtension.substr( 0, 3 ) == "ALC" )
	{
		check( alDevice );
		return alcIsExtensionPresent( alDevice, InExtension.c_str() ) != ALC_FALSE;
	}
	else
	{
		return alIsExtensionPresent( InExtension.c_str() ) != ALC_FALSE;
	}
}