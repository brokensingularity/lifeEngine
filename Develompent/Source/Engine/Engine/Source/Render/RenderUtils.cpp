#include "Render/RenderUtils.h"

/* Maps members of EPixelFormat to a FPixelFormatInfo describing the format */
FPixelFormatInfo		GPixelFormats[ PF_Max ] =
{
	// Name						BlockSizeX	BlockSizeY	BlockSizeZ	BlockBytes	NumComponents	PlatformFormat	Flags			Supported		EngineFormat

	{ TEXT( "Unknown" ),		0,			0,			0,			0,			0,				0,				0,				0,				PF_Unknown			},
	{ TEXT( "A8R8G8B8" ),		1,			1,			1,			4,			4,				0,				0,				1,				PF_A8R8G8B8			}
};