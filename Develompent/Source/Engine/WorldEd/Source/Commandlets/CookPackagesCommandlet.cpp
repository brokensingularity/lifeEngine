#include <stb_image.h>
#include <tmxlite/Map.hpp>
#include <tmxlite/TileLayer.hpp>
#include <tmxlite/Object.hpp>
#include <vector>

#include "Commandlets/CookPackagesCommandlet.h"
#include "Containers/StringConv.h"
#include "Containers/String.h"
#include "Misc/CoreGlobals.h"
#include "Misc/EngineGlobals.h"
#include "Misc/TableOfContents.h"
#include "Misc/EngineMisc.h"
#include "System/BaseFileSystem.h"
#include "System/Archive.h"
#include "System/Config.h"
#include "System/World.h"
#include "System/Config.h"
#include "System/AudioBuffer.h"
#include "Logger/LoggerMacros.h"
#include "Render/Shaders/ShaderCompiler.h"

// Actors
#include "Actors/PlayerStart.h"
#include "Actors/Sprite.h"

// Vertex factories
#include "Render/VertexFactory/StaticMeshVertexFactory.h"
#include "Render/VertexFactory/SpriteVertexFactory.h"

IMPLEMENT_CLASS( CCookPackagesCommandlet )

/** Default package extension */
#define DEFAULT_PACKAGE_EXTENSION		TEXT( "pak" )

/** Default map extension */
#define DEFAULT_MAP_EXTENSION			TEXT( "map" )

/**
 * Struct of TMX object for spawn actor in world
 */
struct STMXObject
{
	std::wstring					name;		/**< Actor name */
	std::wstring					className;	/**< Class actor */
	CTransform						transform;	/**< Transform */
	std::vector< CActorVar >		actorVars;	/**< Actor properties */
};

/**
 * Convert TMX property type to text
*/
FORCEINLINE std::wstring TMXPropertyTypeToText( tmx::Property::Type InTmxPropertyType )
{
	switch ( InTmxPropertyType )
	{
	case tmx::Property::Type::Boolean:		return TEXT( "Boolean" );
	case tmx::Property::Type::Colour:		return TEXT( "Colour" );
	case tmx::Property::Type::File:			return TEXT( "File" );
	case tmx::Property::Type::Float:		return TEXT( "Float" );
	case tmx::Property::Type::Int:			return TEXT( "Int" );
	case tmx::Property::Type::Object:		return TEXT( "Object" );
	case tmx::Property::Type::String:		return TEXT( "String" );
	case tmx::Property::Type::Undef:		return TEXT( "Undef" );
	default:								return TEXT( "Unknown" );
	}
}

CCookPackagesCommandlet::CCookPackagesCommandlet()
	: cookedShaderPlatform( SP_Unknown )
	, cookedPlatform( PLATFORM_Unknown )
{}

/**
* ---------------------
 * Cooking map
 * --------------------
 */

bool CCookPackagesCommandlet::CookMap( const SResourceInfo& InMapInfo )
{
	LE_LOG( LT_Log, LC_Commandlet, TEXT( "Cooking map '%s'" ), InMapInfo.filename.c_str() );

	// Load TMX map
	tmx::Map		tmxMap;
	if ( !tmxMap.load( TCHAR_TO_ANSI( InMapInfo.path.c_str() ) ) )
	{
		appErrorf( TEXT( "Map '%s' not found" ), InMapInfo.path.c_str() );
		return false;
	}

	// Loading tilesets from TMX
	std::vector< STMXTileset >			tilesets;
	if ( !LoadTMXTilests( tmxMap, tilesets ) )
	{
		appErrorf( TEXT( "Failed loading TMX tilesets" ) );
		return false;
	}

	// Clear world for spawn new actors
	GWorld->CleanupWorld();

	// Spawn tiles
	SpawnTilesInWorld( tmxMap, tilesets );

	// Spawn actors
	SpawnActorsInWorld( tmxMap, tilesets );

	// Serialize world to HDD
	CArchive*		archive = GFileSystem->CreateFileWriter( CString::Format( TEXT( "%s") PATH_SEPARATOR TEXT( "%s.%s" ), GCookedDir.c_str(), InMapInfo.filename.c_str(), extensionInfo.map.c_str() ), AW_NoFail );
	archive->SetType( AT_World );
	archive->SerializeHeader();
	GWorld->Serialize( *archive );
	delete archive;
	return true;
}

bool CCookPackagesCommandlet::LoadTMXTilests( const tmx::Map& InTMXMap, std::vector<STMXTileset>& OutTilesets )
{
	const std::vector< tmx::Tileset >&		tmxTilesets		= InTMXMap.getTilesets();
	const tmx::Vector2u&					mapTileSize		= InTMXMap.getTileSize();
	for ( uint32 index = 0, count = tmxTilesets.size(); index < count; ++index )
	{
		const tmx::Tileset&		tmxTileset		= tmxTilesets[ index ];
		std::wstring			tmxTilesetName	= ANSI_TO_TCHAR( tmxTileset.getName().c_str() );
		tmx::Vector2u			tmxTilesetSize	= tmxTileset.getImageSize();
		tmx::Vector2u			tmxTileSize		= tmxTileset.getTileSize();
		int32					countCulumns	= tmxTilesetSize.x / tmxTileSize.x;
		int32					countRows		= tmxTilesetSize.y / tmxTileSize.y;
		TAssetHandle<CMaterial>	tilesetMaterial = GPackageManager->FindAsset( tmxTilesetName, AT_Unknown );
		if ( !tilesetMaterial.IsAssetValid() )
		{
			std::wstring		packageName;
			std::wstring		assetName;
			EAssetType			assetType;
			SResourceInfo		resourceInfo;

			ParseReferenceToAsset( tmxTilesetName, packageName, assetName, assetType );
			if ( assetType != AT_Material )
			{
				appErrorf( TEXT( "Asset '%s' is not material" ), tmxTilesetName.c_str() );
				return false;
			}

			if ( !FindResource( materialsMap, packageName, assetName, resourceInfo ) )
			{
				appErrorf( TEXT( "Material '%s' not founded" ), tmxTilesetName.c_str() );
				return false;
			}

			bool	result = CookMaterial( resourceInfo, tilesetMaterial );
			if ( !result )
			{
				appErrorf( TEXT( "Failed cooking material '%s'" ), tmxTilesetName.c_str() );
				return false;
			}
		}

		// Save info about tileset
		STMXTileset			tileset;
		tileset.firstGID	= tmxTileset.getFirstGID();
		tileset.lastGID		= tmxTileset.getLastGID();
		tileset.material	= tilesetMaterial;
		tileset.tileSize	= Vector2D( tmxTileSize.x, tmxTileSize.y );
		tileset.tileOffset	= Vector2D( ( float )tmxTileSize.x / mapTileSize.x, ( float )tmxTileSize.y / mapTileSize.y );

		// Precalculates rectangles in tileset
		for ( uint32 y = 0; y < countRows; ++y )
		{
			for ( uint32 x = 0; x < countCulumns; ++x )
			{
				RectFloat_t		rect;
				rect.width		= tmxTileSize.x;
				rect.height		= tmxTileSize.y;
				rect.top		= y * tmxTileSize.y;
				rect.left		= x * tmxTileSize.x;

				// Convert texture rect in range from 0 to 1
				rect.width		/= tmxTilesetSize.x;
				rect.height		/= tmxTilesetSize.y;
				rect.left		/= tmxTilesetSize.x;
				rect.top		/= tmxTilesetSize.y;

				tileset.textureRects.push_back( rect );
			}
		}

		OutTilesets.push_back( tileset );
	}

	return true;
}

bool CCookPackagesCommandlet::FindTileset( const std::vector<STMXTileset>& InTilesets, uint32 InIDTile, STMXTileset& OutTileset, RectFloat_t& OutTextureRect ) const
{
	for ( uint32 indexTileset = 0, countTilesets = InTilesets.size(); indexTileset < countTilesets; ++indexTileset )
	{
		const STMXTileset&		tileset = InTilesets[ indexTileset ];
		if ( InIDTile < tileset.firstGID || InIDTile > tileset.lastGID )
		{
			continue;
		}

		OutTileset		= tileset;
		OutTextureRect	= tileset.textureRects[ InIDTile - tileset.firstGID ];
		return true;
	}

	return false;
}

void CCookPackagesCommandlet::SpawnTilesInWorld( const tmx::Map& InTMXMap, const std::vector<STMXTileset>& InTilesets )
{
	const std::vector< tmx::Layer::Ptr >&		tmxLayers	= InTMXMap.getLayers();
	const tmx::Vector2u&						mapSize		= InTMXMap.getTileCount();
	const tmx::Vector2u&						mapTileSize = InTMXMap.getTileSize();
	for ( uint32 indexLayer = 0, countLayers = tmxLayers.size(); indexLayer < countLayers; ++indexLayer )
	{
		if ( tmxLayers[ indexLayer ]->getType() == tmx::Layer::Type::Tile )
		{
			tmx::TileLayer*									tmxLayer = ( tmx::TileLayer* )tmxLayers[ indexLayer ].get();
			const std::vector< tmx::TileLayer::Tile >&		tmxTiles = tmxLayer->getTiles();

			int32		x = 0;
			int32		y = mapSize.y-1;
			for ( uint32 indexTile = 0, countTiles = tmxTiles.size(); indexTile < countTiles; ++indexTile )
			{
				const tmx::TileLayer::Tile&			tile = tmxTiles[ indexTile ];
				if ( tile.ID != 0 )
				{
					// Find material for tile
					STMXTileset		tileset;
					RectFloat_t		textureRect;
					bool			result = FindTileset( InTilesets, tile.ID, tileset, textureRect );
					checkMsg( result, TEXT( "Not founded tileset for tile with ID %i" ), tile.ID );

					ASprite*			sprite				= GWorld->SpawnActor< ASprite >( Vector( x * mapTileSize.x, y * mapTileSize.y, indexLayer ) );					
					CSpriteComponent*	spriteComponent		= sprite->GetSpriteComponent();
					spriteComponent->SetType( ST_Static );
					spriteComponent->SetMaterial( tileset.material );
					spriteComponent->SetSpriteSize( Vector2D( tileset.tileSize.x, tileset.tileSize.y ) );
					spriteComponent->SetTextureRect( textureRect );
					sprite->SetName( TEXT( "ASprite_Tile" ) );
					sprite->SetStatic( true );
				}

				++x;
				if ( x >= mapSize.x )
				{
					x = 0;
					--y;
				}
			}
		}
	}
}

void CCookPackagesCommandlet::SpawnActorsInWorld( const tmx::Map& InTMXMap, const std::vector<STMXTileset>& InTileset )
{
	const std::vector< tmx::Layer::Ptr >&		tmxLayers = InTMXMap.getLayers();

	// Getting max coord XY in coords system of Tiled Map Editor
	Vector2D		tmxMaxXY;
	{
		const tmx::Vector2u&	mapSize			= InTMXMap.getTileCount();
		const tmx::Vector2u&	mapTileSize		= InTMXMap.getTileSize();
		tmxMaxXY.x = mapSize.x * mapTileSize.x;
		tmxMaxXY.y = mapSize.y * mapTileSize.y;
	}

	for ( uint32 indexLayer = 0, countLayers = tmxLayers.size(); indexLayer < countLayers; ++indexLayer )
	{
		if ( tmxLayers[ indexLayer ]->getType() == tmx::Layer::Type::Object )
		{
			tmx::ObjectGroup*					tmxObjectGroup	= ( tmx::ObjectGroup* )tmxLayers[ indexLayer ].get();
			const std::vector< tmx::Object >&	tmxObjects		= tmxObjectGroup->getObjects();

			for ( uint32 indexObject = 0, countObjects = tmxObjects.size(); indexObject < countObjects; ++indexObject )
			{			
				// Getting all parameters from object
				const tmx::Object&		object = tmxObjects[ indexObject ];							
				STMXObject				tmxObject;
				tmxObject.name = ANSI_TO_TCHAR( object.getName().c_str() );

				// Getting transformation and settings from object
				{
					const tmx::FloatRect&		objectAABB = object.getAABB();

					// Getting position
					{
						const tmx::Vector2f&		location = object.getPosition();
						tmxObject.transform.SetLocation( Vector( objectAABB.left, tmxMaxXY.y - objectAABB.top - objectAABB.height, indexLayer ) );
					}

					// Getting rotation
					{
						tmxObject.transform.SetRotation( SMath::AnglesToQuaternionXYZ( Vector( 0.f, 0.f, object.getRotation() ) ) );
					}

					// Getting size
					{
						CActorVar		actorVar;
						actorVar.SetName( TEXT( "Size" ) );
						actorVar.SetValueVector3D( Vector( objectAABB.width, objectAABB.height, 1.f ) );
						tmxObject.actorVars.push_back( actorVar );
					}

					// If tile id in object more 0 - getting tile for setting actor view
					uint32		tileID = object.getTileID();
					if ( tileID > 0 )
					{
						// Find material for tile
						STMXTileset			tileset;
						RectFloat_t			textureRect;
						if ( FindTileset( InTileset, tileID, tileset, textureRect ) )
						{
							CActorVar		varMaterial;
							varMaterial.SetName( TEXT( "Material" ) );
							varMaterial.SetValueMaterial( tileset.material );

							CActorVar		varTextureRect;
							varTextureRect.SetName( TEXT( "TextureRect" ) );
							varTextureRect.SetValueRectFloat( textureRect );

							tmxObject.actorVars.push_back( varMaterial );
							tmxObject.actorVars.push_back( varTextureRect );
						}
						else
						{
							LE_LOG( LT_Warning, LC_Commandlet, TEXT( "For actor '%s' not founded tile with ID %i" ), tmxObject.name.c_str(), tileID );
						}
					}
				}

				// Getting all parameters from object
				const std::vector< tmx::Property >&			objectProperties = object.getProperties();
				for ( uint32 indexPropery = 0, countProperties = objectProperties.size(); indexPropery < countProperties; ++indexPropery )
				{
					const tmx::Property&		objectProperty = objectProperties[ indexPropery ];
					bool						bIsClassName = false;
					CActorVar					actorVar;

					actorVar.SetName( ANSI_TO_TCHAR( objectProperty.getName().c_str() ) );
					if ( actorVar.GetName() == TEXT( "Class" ) )
					{
						bIsClassName = true;
						checkMsg( objectProperty.getType() == tmx::Property::Type::String, TEXT( "Class name must be is string" ) );
						tmxObject.className = ANSI_TO_TCHAR( objectProperty.getStringValue().c_str() );
						continue;
					}

					switch ( objectProperty.getType() )
					{
					case tmx::Property::Type::Boolean:
						actorVar.SetValueBool( objectProperty.getBoolValue() );
						break;

					case tmx::Property::Type::Colour:
					{
						const tmx::Colour&		tmxColor = objectProperty.getColourValue();
						actorVar.SetValueColor( CColor( tmxColor.r, tmxColor.g, tmxColor.b, tmxColor.a ) );
						break;
					}

					case tmx::Property::Type::Float:
						actorVar.SetValueFloat( objectProperty.getFloatValue() );
						break;

					case tmx::Property::Type::Int:
						actorVar.SetValueInt( objectProperty.getIntValue() );
						break;

					case tmx::Property::Type::String:
						actorVar.SetValueString( ANSI_TO_TCHAR( objectProperty.getStringValue().c_str() ) );
						break;

					default:
						LE_LOG( LT_Warning, LC_Commandlet, TEXT( "Property '%s' in actor '%s' have not supported type '%s'" ), actorVar.GetName().c_str(), tmxObject.name.c_str(), TMXPropertyTypeToText( objectProperty.getType() ).c_str() );
						break;
					}

					tmxObject.actorVars.push_back( actorVar );
				}

				// Spawn actor and init properties if class name is valid
				if ( !tmxObject.className.empty() )
				{
					CClass*		classActor = CClass::StaticFindClass( tmxObject.className.c_str() );
					if ( !classActor )
					{
						LE_LOG( LT_Warning, LC_Commandlet, TEXT( "Actor '%s' not spanwed because class '%s' not founded" ), tmxObject.className.c_str() );
						continue;
					}

					ActorRef_t		actor = GWorld->SpawnActor( classActor, tmxObject.transform.GetLocation(), tmxObject.transform.GetRotation() );
					actor->SetName( tmxObject.name.c_str() );
					if ( !actor->InitProperties( tmxObject.actorVars, this ) )
					{
						appErrorf( TEXT( "Failed init properties in actor '%s'" ), tmxObject.name.c_str() );
						return;
					}
				}
				else
				{
					LE_LOG( LT_Warning, LC_Commandlet, TEXT( "Actor '%s' not spawned because class name in properties not setted" ), tmxObject.name.c_str() );
				}
			}
		}
	}
}

/**
 * --------------------
 * Cooking material
 * --------------------
 */

bool CCookPackagesCommandlet::CookMaterial( const SResourceInfo& InMaterialInfo, TAssetHandle<CMaterial>& OutMaterial )
{
	LE_LOG( LT_Log, LC_Commandlet, TEXT( "Cooking material '%s:%s'" ), InMaterialInfo.packageName.c_str(), InMaterialInfo.filename.c_str() );

	// Parse material in JSON format
	CConfig		lmtMaterial;
	{
		CArchive*		arMaterial = GFileSystem->CreateFileReader( InMaterialInfo.path, AR_NoFail );
		lmtMaterial.Serialize( *arMaterial );
		delete arMaterial;
	}

	// Getting general data
	bool				bIsEditorContent		= lmtMaterial.GetValue( TEXT( "Material" ), TEXT( "IsEditorContent" ) ).GetBool();
	if ( bIsEditorContent && !GIsCookEditorContent )
	{
		LE_LOG( LT_Log, LC_Commandlet, TEXT( "... Skiped editor content" ) );
		return true;
	}
	else if ( bIsEditorContent )
	{
		LE_LOG( LT_Log, LC_Commandlet, TEXT( "... Editor content" ) );
	}

	bool				bIsTwoSided				= lmtMaterial.GetValue( TEXT( "Material" ), TEXT( "IsTwoSided" ) ).GetBool();
	bool				bIsWireframe			= lmtMaterial.GetValue( TEXT( "Material" ), TEXT( "IsWireframe" ) ).GetBool();

	// Getting usage flags
	uint32						usageFlags = MU_AllMeshes;
	std::vector< uint64 >		usedVertexFectories;
	{
		CConfigValue	configVarUsageFlags = lmtMaterial.GetValue( TEXT( "Material" ), TEXT( "Usage" ) );
		if ( configVarUsageFlags.GetType() == CConfigValue::T_Object )
		{
			// If exist Usage object, we reset usage flags to MU_None
			usageFlags = MU_None;

			// Get usage flags
			CConfigObject		configObjUsageFlags		= configVarUsageFlags.GetObject();
			bool				bStaticMesh				= configObjUsageFlags.GetValue( TEXT( "StaticMesh" ) ).GetBool();
			bool				bSprite					= configObjUsageFlags.GetValue( TEXT( "Sprite" ) ).GetBool();

			// Combine all flags
			if ( bStaticMesh )
			{
				usageFlags |= MU_StaticMesh;
				usedVertexFectories.push_back( CStaticMeshVertexFactory::staticType.GetHash() );
			}

			if ( bSprite )
			{
				usageFlags |= MU_Sprite;
				usedVertexFectories.push_back( CSpriteVertexFactory::staticType.GetHash() );
			}
		}
		else
		{
			LE_LOG( LT_Warning, LC_Commandlet, TEXT( "Not correct type in block 'Material::Usage', must be is object. Usage flags setted to MU_AllMeshes" ) );
		}
	}

	// Getting shader types
	std::vector< CShaderMetaType* >		shaderMetaTypes;
	{
		CConfigValue	configVarShadersType = lmtMaterial.GetValue( TEXT( "Material" ), TEXT( "ShadersType" ) );
		check( configVarShadersType.GetType() == CConfigValue::T_Array );

		std::vector< CConfigValue >		configValues = configVarShadersType.GetArray();
		for ( uint32 index = 0, count = configValues.size(); index < count; ++index )
		{
			std::wstring		shaderMetaTypeName	= configValues[ index ].GetString();
			CShaderMetaType*	shaderMetaType		= GShaderManager->FindShaderType( shaderMetaTypeName );
			if ( !shaderMetaType )
			{
				appErrorf( TEXT( "Shader '%s' not found in engine" ), shaderMetaTypeName.c_str() );
				return false;
			}

			// Compile shader if need
			for ( uint32 index = 0, count = usedVertexFectories.size(); index < count; ++index )
			{
				if ( shaderCache.IsExist( shaderMetaTypeName, usedVertexFectories[ index ] ) )
				{
					continue;
				}

				std::wstring				errorMsg;
				CShaderCompiler				shaderCompiler;
				CVertexFactoryMetaType*		vfType = CVertexFactoryMetaType::SContainerVertexFactoryMetaType::Get()->FindRegisteredType( usedVertexFectories[ index ] );
				bool						result = shaderCompiler.CompileShader( shaderMetaType, cookedShaderPlatform, shaderCache, errorMsg, vfType );
				if ( !result )
				{
					appErrorf( TEXT( "Failed cached shader '%s' for vertex factory '%s'\n\n%s" ), shaderMetaTypeName.c_str(), vfType->GetName().c_str(), errorMsg.c_str() );
					return false;
				}
			}

			shaderMetaTypes.push_back( shaderMetaType );
		}
	}

	// Getting scalar parameters
	std::unordered_map< std::wstring, float >		scalarParameters;
	{
		CConfigValue	configVarScalarParameters = lmtMaterial.GetValue( TEXT( "Material" ), TEXT( "ScalarParameters" ) );
		if ( configVarScalarParameters.IsValid() )
		{
			check( configVarScalarParameters.GetType() == CConfigValue::T_Array );

			std::vector< CConfigValue >		configObjects = configVarScalarParameters.GetArray();
			for ( uint32 index = 0, count = configObjects.size(); index < count; ++index )
			{
				CConfigObject	configObject	= configObjects[ index ].GetObject();
				std::wstring	name			= configObject.GetValue( TEXT( "Name" ) ).GetString();
				float			value			= configObject.GetValue( TEXT( "Value" ) ).GetNumber();
				scalarParameters.insert( std::make_pair( name, value ) );
			}
		}
	}

	// Getting texture parameters
	std::unordered_map< std::wstring, TAssetHandle<CTexture2D> >		textureParameters;
	{
		CConfigValue	configVarTextureParameters = lmtMaterial.GetValue( TEXT( "Material" ), TEXT( "TextureParameters" ) );
		if ( configVarTextureParameters.IsValid() )
		{
			check( configVarTextureParameters.GetType() == CConfigValue::T_Array );

			std::vector< CConfigValue >		configObjects = configVarTextureParameters.GetArray();
			for ( uint32 index = 0, count = configObjects.size(); index < count; ++index )
			{
				CConfigObject				configObject	= configObjects[ index ].GetObject();
				std::wstring				name			= configObject.GetValue( TEXT( "Name" ) ).GetString();
				std::wstring				assetReference	= configObject.GetValue( TEXT( "AssetReference" ) ).GetString();
				TAssetHandle<CTexture2D>	texture			= GPackageManager->FindAsset( assetReference, AT_Unknown );
				if ( !texture.IsAssetValid() )
				{
					std::wstring		packageName;
					std::wstring		assetName;
					EAssetType			assetType;
					SResourceInfo		resourceInfo;

					ParseReferenceToAsset( assetReference, packageName, assetName, assetType );
					if ( assetType != AT_Texture2D )
					{
						appErrorf( TEXT( "Asset '%s' is not texture" ), assetReference.c_str() );
						return false;
					}

					if ( !FindResource( texturesMap, packageName, assetName, resourceInfo ) )
					{
						appErrorf( TEXT( "Texture '%s' not founded" ), assetReference.c_str() );
						return false;
					}

					bool	result = CookTexture2D( resourceInfo, texture );
					if ( !result )
					{
						appErrorf( TEXT( "Failed cooking texture '%s'" ), assetReference.c_str() );
						return false;
					}
				}

				textureParameters.insert( std::make_pair( name, texture ) );
			}
		}
	}

	// Create material and saving to package
	TSharedPtr<CMaterial>		materialRef = MakeSharedPtr<CMaterial>();
	materialRef->SetAssetName( InMaterialInfo.filename );
	materialRef->SetTwoSided( bIsTwoSided );
	materialRef->SetWireframe( bIsWireframe );
	materialRef->SetUsageFlags( usageFlags );

	// Set scalar parameters
	for ( auto it = scalarParameters.begin(), itEnd = scalarParameters.end(); it != itEnd; ++it )
	{
		materialRef->SetScalarParameterValue( it->first, it->second );
	}

	// Set texture parameters
	for ( auto it = textureParameters.begin(), itEnd = textureParameters.end(); it != itEnd; ++it )
	{
		materialRef->SetTextureParameterValue( it->first, it->second );
	}

	OutMaterial = TAssetHandle<CMaterial>( materialRef, MakeSharedPtr<SAssetReference>( AT_Material, materialRef->GetGUID() ) );

	// Save to package
	return SaveToPackage( InMaterialInfo, OutMaterial );
}

/**
 * ----------------------
 * Cooking texture 2D
 * ----------------------
 */

TSharedPtr<CTexture2D> CCookPackagesCommandlet::ConvertTexture2D( const std::wstring& InPath, const std::wstring& InName /* = TEXT( "" ) */ )
{
	// Loading data from image
	int				numComponents = 0;
	uint32			sizeX = 0;
	uint32			sizeY = 0;
	void*			data = stbi_load( TCHAR_TO_ANSI( InPath.c_str() ), ( int* ) &sizeX, ( int* ) &sizeY, &numComponents, 4 );
	if ( !data )
	{
		return nullptr;
	}

	// Getting file name from path if InName is empty
	std::wstring		filename = InName;
	if ( filename.empty() )
	{
		filename = InPath;

		appNormalizePathSeparators( filename );
		std::size_t			pathSeparatorPos = filename.find_last_of( PATH_SEPARATOR );
		if ( pathSeparatorPos != std::string::npos )
		{
			filename.erase( 0, pathSeparatorPos + 1 );
		}

		std::size_t dotPos = filename.find_last_of( TEXT( "." ) );
		if ( dotPos != std::string::npos )
		{
			filename.erase( dotPos, filename.size() + 1 );
		}
	}

	// Create texture 2D and init him
	TSharedPtr<CTexture2D>		texture2DRef = MakeSharedPtr<CTexture2D>();
	texture2DRef->SetAssetName( filename );
	texture2DRef->SetAssetSourceFile( InPath );
	{
		std::vector< byte >		tempData;
		tempData.resize( sizeX * sizeY * GPixelFormats[ PF_A8R8G8B8 ].blockBytes );
		memcpy( tempData.data(), data, tempData.size() );
		texture2DRef->SetData( PF_A8R8G8B8, sizeX, sizeY, tempData );
	}

	// Clean up all data
	stbi_image_free( data );
	return texture2DRef;
}

bool CCookPackagesCommandlet::CookTexture2D( const SResourceInfo& InTexture2DInfo, TAssetHandle<CTexture2D>& OutTexture2D )
{
	LE_LOG( LT_Log, LC_Commandlet, TEXT( "Cooking texture 2D '%s:%s'" ), InTexture2DInfo.packageName.c_str(), InTexture2DInfo.filename.c_str() );
	
	TSharedPtr<CTexture2D>		texture2DRef = ConvertTexture2D( InTexture2DInfo.path, InTexture2DInfo.filename );
	OutTexture2D				= TAssetHandle<CTexture2D>( texture2DRef, MakeSharedPtr<SAssetReference>( AT_Texture2D, texture2DRef->GetGUID() ) );
	return OutTexture2D.IsAssetValid() && SaveToPackage( InTexture2DInfo, OutTexture2D );
}

/**
 * ---------------------
 * Cook all resources
 * ---------------------
 */

void CCookPackagesCommandlet::CookAllResources( bool InIsOnlyAlwaysCook /* = false */ )
{
	// Compile all global shaders
	{
		LE_LOG( LT_Log, LC_Commandlet, TEXT( "Compiling global shaders" ) );
		
		CShaderCompiler		shaderCompiler;
		bool		result = shaderCompiler.CompileAll( shaderCache, cookedShaderPlatform, true );
		if ( !result )
		{
			appErrorf( TEXT( "Failed compiling global shaders" ) );
			return;
		}
	}

	// Cook textures
	for ( auto itPackage = texturesMap.begin(), itPackageEnd = texturesMap.end(); itPackage != itPackageEnd; ++itPackage )
	{
		for ( auto itAsset = itPackage->second.begin(), itAssetEnd = itPackage->second.end(); itAsset != itAssetEnd; ++itAsset )
		{
			if ( InIsOnlyAlwaysCook && !itAsset->second.bAlwaysCook )
			{
				continue;
			}

			TAssetHandle<CTexture2D>		texture2D;
			bool	result = CookTexture2D( itAsset->second, texture2D );
			if ( !result )
			{
				appErrorf( TEXT( "Failed cooking texture 2D '%s'" ), itAsset->second.filename.c_str() );
				return;
			}
		}
	}

	// Cook materials
	for ( auto itPackage = materialsMap.begin(), itPackageEnd = materialsMap.end(); itPackage != itPackageEnd; ++itPackage )
	{
		for ( auto itAsset = itPackage->second.begin(), itAssetEnd = itPackage->second.end(); itAsset != itAssetEnd; ++itAsset )
		{
			if ( InIsOnlyAlwaysCook && !itAsset->second.bAlwaysCook )
			{
				continue;
			}

			TAssetHandle<CMaterial>		material;
			bool	result = CookMaterial( itAsset->second, material );
			if ( !result )
			{
				appErrorf( TEXT( "Failed cooking material '%s'" ), itAsset->second.filename.c_str() );
				return;
			}
		}
	}

	// Cook audio
	for ( auto itPackage = audiosMap.begin(), itPackageEnd = audiosMap.end(); itPackage != itPackageEnd; ++itPackage )
	{
		for ( auto itAsset = itPackage->second.begin(), itAssetEnd = itPackage->second.end(); itAsset != itAssetEnd; ++itAsset )
		{
			if ( InIsOnlyAlwaysCook && !itAsset->second.bAlwaysCook )
			{
				continue;
			}

			TAssetHandle<CAudioBank>	audioBank;
			bool	result = CookAudioBank( itAsset->second, audioBank );
			if ( !result )
			{
				appErrorf( TEXT( "Failed cooking audio bank '%s'" ), itAsset->second.filename.c_str() );
				return;
			}
		}
	}

	// Cook physics material
	for ( auto itPackage = physMaterialsMap.begin(), itPackageEnd = physMaterialsMap.end(); itPackage != itPackageEnd; ++itPackage )
	{
		for ( auto itAsset = itPackage->second.begin(), itAssetEnd = itPackage->second.end(); itAsset != itAssetEnd; ++itAsset )
		{
			if ( InIsOnlyAlwaysCook && !itAsset->second.bAlwaysCook )
			{
				continue;
			}

			TAssetHandle<CPhysicsMaterial>		physMaterial;
			bool					result = CookPhysMaterial( itAsset->second, physMaterial );
			if ( !result )
			{
				appErrorf( TEXT( "Failed cooking physics material '%s'" ), itAsset->second.filename.c_str() );
				return;
			}
		}
	}
}

/**
 * ----------------------
 * Cook audio bank
 * ----------------------
 */

TSharedPtr<CAudioBank> CCookPackagesCommandlet::ConvertAudioBank( const std::wstring& InPath, const std::wstring& InName /* = TEXT( "" ) */ )
{
	// Create audio bank and init him
	TSharedPtr<CAudioBank>		audioBankRef = MakeSharedPtr<CAudioBank>();

	// Getting file name from path if InName is empty
	std::wstring		filename = InName;
	if ( filename.empty() )
	{
		filename = InPath;

		appNormalizePathSeparators( filename );
		std::size_t			pathSeparatorPos = filename.find_last_of( PATH_SEPARATOR );
		if ( pathSeparatorPos != std::string::npos )
		{
			filename.erase( 0, pathSeparatorPos + 1 );
		}

		std::size_t dotPos = filename.find_last_of( TEXT( "." ) );
		if ( dotPos != std::string::npos )
		{
			filename.erase( dotPos, filename.size() + 1 );
		}
	}

	audioBankRef->SetAssetName( filename );
	audioBankRef->SetAssetSourceFile( InPath );
	audioBankRef->SetOGGFile( InPath );
	return audioBankRef;
}

bool CCookPackagesCommandlet::CookAudioBank( const SResourceInfo& InAudioBankInfo, TAssetHandle<CAudioBank>& OutAudioBank )
{
	LE_LOG( LT_Log, LC_Commandlet, TEXT( "Cooking audio bank '%s:%s'" ), InAudioBankInfo.packageName.c_str(), InAudioBankInfo.filename.c_str() );
	
	TSharedPtr<CAudioBank>		audioBankRef = ConvertAudioBank( InAudioBankInfo.path, InAudioBankInfo.filename );
	OutAudioBank				= TAssetHandle<CAudioBank>( audioBankRef, MakeSharedPtr<SAssetReference>( AT_AudioBank, audioBankRef->GetGUID() ) );
	return OutAudioBank.IsAssetValid() && SaveToPackage( InAudioBankInfo, OutAudioBank );
}

/**
 * --------------------
 * Cooking physics material
 * --------------------
 */

bool CCookPackagesCommandlet::CookPhysMaterial( const SResourceInfo& InPhysMaterialInfo, TAssetHandle<CPhysicsMaterial>& OutPhysMaterial )
{
	LE_LOG( LT_Log, LC_Commandlet, TEXT( "Cooking physics material '%s:%s'" ), InPhysMaterialInfo.packageName.c_str(), InPhysMaterialInfo.filename.c_str() );
	
	// Parse physics material in JSON format
	CConfig		pmtMaterial;
	{
		CArchive*	arMaterial = GFileSystem->CreateFileReader( InPhysMaterialInfo.path, AR_NoFail );
		pmtMaterial.Serialize( *arMaterial );
		delete arMaterial;
	}

	// Getting general data
	float			staticFriction	= pmtMaterial.GetValue( TEXT( "PhysicsMaterial" ), TEXT( "StaticFriction" ) ).GetNumber();
	float			dynamicFriction	= pmtMaterial.GetValue( TEXT( "PhysicsMaterial" ), TEXT( "DynamicFriction" ) ).GetNumber();
	float			restitution		= pmtMaterial.GetValue( TEXT( "PhysicsMaterial" ), TEXT( "Restitution" ) ).GetNumber();
	float			density			= pmtMaterial.GetValue( TEXT( "PhysicsMaterial" ), TEXT( "Density" ) ).GetNumber();
	std::wstring	surfaceTypeName = pmtMaterial.GetValue( TEXT( "PhysicsMaterial" ), TEXT( "Surface" ) ).GetString();

	// Create physics material and saving to package
	TSharedPtr<CPhysicsMaterial>		physMaterialRef = MakeSharedPtr<CPhysicsMaterial>();
	physMaterialRef->SetAssetName( InPhysMaterialInfo.filename );
	physMaterialRef->SetStaticFriction( staticFriction );
	physMaterialRef->SetDynamicFriction( dynamicFriction );
	physMaterialRef->SetRestitution( restitution );
	physMaterialRef->SetDensity( density );
	physMaterialRef->SetSurfaceType( appTextToESurfaceType( surfaceTypeName ) );

	// Save to package
	OutPhysMaterial = TAssetHandle<CPhysicsMaterial>( physMaterialRef, MakeSharedPtr<SAssetReference>( AT_PhysicsMaterial, physMaterialRef->GetGUID() ) );
	return SaveToPackage( InPhysMaterialInfo, OutPhysMaterial );
}

/**
 * --------------------
 * General methods
 * --------------------
 */

bool CCookPackagesCommandlet::SaveToPackage( const SResourceInfo& InResourceInfo, const TAssetHandle<CAsset>& InAsset )
{
	std::wstring		outputPackage = CString::Format( TEXT( "%s" ) PATH_SEPARATOR TEXT( "%s.%s" ), GCookedDir.c_str(), InResourceInfo.packageName.c_str(), extensionInfo.package.c_str() );
	PackageRef_t			package = GPackageManager->LoadPackage( outputPackage, true );
	package->Add( InAsset );

	bool	result = package->Save( outputPackage );
	if ( !result )
	{
		appErrorf( TEXT( "Failed saving package '%s'" ), InResourceInfo.packageName.c_str() );
		return false;
	}

	return true;
}

void CCookPackagesCommandlet::InsertResourceToList( ResourceMap_t& InOutResourceMap, const std::wstring& InPackageName, const std::wstring& InFilename, const SResourceInfo& InResourceInfo )
{
	auto		itPackage = InOutResourceMap.find( InPackageName );
	if ( itPackage == InOutResourceMap.end() )
	{
		InOutResourceMap[ InPackageName ][ InFilename ] = InResourceInfo;
		return;
	}

	auto		itAsset = itPackage->second.find( InFilename );
	if ( itAsset == itPackage->second.end() )
	{
		InOutResourceMap[ InPackageName ][ InFilename ] = InResourceInfo;
		return;
	}
}

void CCookPackagesCommandlet::IndexingResources( const std::wstring& InRootDir, bool InIsRootDir /* = false */, bool InIsAlwaysCookDir /* = false */, const std::wstring& InParentDirName /* = TEXT( "" ) */ )
{
	// Getting package name from dir name
	std::wstring		packageName = InParentDirName;
	if ( !InIsRootDir )
	{
		packageName = InRootDir;
		appNormalizePathSeparators( packageName );
		std::size_t			pathSeparatorPos = packageName.find_last_of( PATH_SEPARATOR );
		if ( pathSeparatorPos != std::string::npos )
		{
			packageName.erase( 0, pathSeparatorPos + 1 );
		}

		// Split package names in format: <ParentDirName>_<CurrentDirName>. Example: PlayerCharacters_Materials
		if ( !InParentDirName.empty() )
		{
			packageName = InParentDirName + TEXT( "_" ) + packageName;
		}
	}

	std::vector< std::wstring >		files = GFileSystem->FindFiles( InRootDir, true, true );
	for ( uint32 index = 0, count = files.size(); index < count; ++index )
	{
		std::wstring		file		= files[ index ];
		std::size_t			dotPos		= file.find_last_of( TEXT( "." ) );
		std::wstring		fullPath	= InRootDir + PATH_SEPARATOR + file;

		if ( dotPos == std::wstring::npos )
		{
			IndexingResources( fullPath, false, InIsAlwaysCookDir, packageName );
			continue;
		}

		// Getting extension for check resource type
		std::wstring		extension = file;
		extension.erase( 0, dotPos + 1 );

		// Getting filename
		std::wstring		filename = file;
		{
			appNormalizePathSeparators( filename );
			std::size_t			pathSeparatorPos = filename.find_last_of( PATH_SEPARATOR );
			if ( pathSeparatorPos != std::string::npos )
			{
				filename.erase( 0, pathSeparatorPos + 1 );
			}

			dotPos = filename.find_last_of( TEXT( "." ) );
			if ( dotPos != std::string::npos )
			{
				filename.erase( dotPos, filename.size() + 1 );
			}
		}

		// If this resource is texture
		SResourceInfo			resourceInfo = SResourceInfo{ packageName, filename, fullPath, InIsAlwaysCookDir };
		if ( IsSupportedTextureExtension( extension ) )
		{
			InsertResourceToList( texturesMap, packageName, filename, resourceInfo );
		}
		// If this resource is material
		else if ( IsSupportedMaterialExtension( extension ) )
		{
			InsertResourceToList( materialsMap, packageName, filename, resourceInfo );
		}
		// If this resource is audio
		else if ( IsSupportedAudioExtension( extension ) )
		{
			InsertResourceToList( audiosMap, packageName, filename, resourceInfo );
		}
		// If this resource is physics material
		else if ( IsSupportedPhysMaterialExtension( extension ) )
		{
			InsertResourceToList( physMaterialsMap, packageName, filename, resourceInfo );
		}
		// If this resource is map
		else if ( IsSupportedMapExtension( extension ) )
		{
			mapsMap[ filename ]	= resourceInfo;
		}
	}
}

bool CCookPackagesCommandlet::Main( const CCommandLine& InCommandLine )
{
	GIsCooker = true;
	std::vector< std::wstring >			mapsToCook;

	// Parse arguments
	{
		std::wstring		platformName = InCommandLine.GetFirstValue( TEXT( "platform" ) );
		if ( !platformName.empty() )
		{
			cookedPlatform = appPlatformStringToType( platformName );
			check( cookedPlatform != PLATFORM_Unknown );
		}

		// Getting all maps from commands
		mapsToCook = InCommandLine.GetValues( TEXT( "maps" ) );
	}

	checkMsg( !mapsToCook.empty(), TEXT( "Mpas to cook not entered" ) );

	// Set shader cooked platform by cooked platform
	switch ( cookedPlatform )
	{
	case PLATFORM_Windows:		cookedShaderPlatform = SP_PCD3D_SM5;			break;
	default:					appErrorf( TEXT( "Unknown platform" ) );		break;
	}

	// Indexing resources in content dirs
	{
		// Always cook dirs
		std::vector< CConfigValue >			configVarAlwaysCookDirs = GConfig.GetValue( CT_Editor, TEXT( "Editor.CookPackages" ), TEXT( "AlwaysCookDirs" ) ).GetArray();
		for ( uint32 index = 0, count = configVarAlwaysCookDirs.size(); index < count; ++index )
		{
			const CConfigValue&		configAlwaysCookDirItem = configVarAlwaysCookDirs[ index ];
			check( configAlwaysCookDirItem.GetType() == CConfigValue::T_Object );
			CConfigObject			objectAlwaysCookDir = configAlwaysCookDirItem.GetObject();

			std::wstring		packageSufix	= objectAlwaysCookDir.GetValue( TEXT( "PackageSufix" ) ).GetString();
			std::wstring		path			= objectAlwaysCookDir.GetValue( TEXT( "Path" ) ).GetString();
			if ( !path.empty() )
			{
				IndexingResources( appBaseDir() + PATH_SEPARATOR + path, true, true, packageSufix );
			}
		}

		// Cooked dirs
		std::vector< CConfigValue >			configVarCookDirs = GConfig.GetValue( CT_Editor, TEXT( "Editor.CookPackages" ), TEXT( "CookDirs" ) ).GetArray();
		for ( uint32 index = 0, count = configVarCookDirs.size(); index < count; ++index )
		{
			const CConfigValue&		configCookDirItem = configVarCookDirs[ index ];
			check( configCookDirItem.GetType() == CConfigValue::T_Object );
			CConfigObject			objectCookDir = configCookDirItem.GetObject();

			std::wstring		packageSufix	= objectCookDir.GetValue( TEXT( "PackageSufix" ) ).GetString();
			std::wstring		path			= objectCookDir.GetValue( TEXT( "Path" ) ).GetString();
			if ( !path.empty() )
			{
				IndexingResources( appBaseDir() + PATH_SEPARATOR + path, true, false, packageSufix );
			}
		}
	}

	// Getting output cooked directory and extensions for saving
	{
		CConfigObject		configObjExtensions = GConfig.GetValue( CT_Editor, TEXT( "Editor.CookPackages" ), TEXT( "Extensions" ) ).GetObject();
		extensionInfo.package	= configObjExtensions.GetValue( TEXT( "Package" ) ).GetString();
		if ( extensionInfo.package.empty() )
		{
			extensionInfo.package = DEFAULT_PACKAGE_EXTENSION;
		}

		extensionInfo.map		= configObjExtensions.GetValue( TEXT( "Map" ) ).GetString();
		if ( extensionInfo.map.empty() )
		{
			extensionInfo.map = DEFAULT_MAP_EXTENSION;
		}
	}

	// Clear table of content and if cooked dir already created remove it
	GTableOfContents.Clear();
	if ( GFileSystem->IsExistFile( GCookedDir, true ) )
	{
		GFileSystem->DeleteDirectory( GCookedDir, true );
	}

	// Cook all resource with flag bAlwaysCook = true
	CookAllResources( true );

	// Cook maps
	for ( uint32 index = 0, count = mapsToCook.size(); index < count; ++index )
	{
		const std::wstring&		mapName		= mapsToCook[ index ];
		auto					itResource	= mapsMap.find( mapsToCook[ index ] );
		if ( itResource == mapsMap.end() )
		{
			LE_LOG( LT_Warning, LC_Commandlet, TEXT( "Map '%s' not found, cook it map skiped" ), mapName.c_str() );
			continue;
		}

		bool	result = CookMap( itResource->second );
		if ( !result )
		{
			appErrorf( TEXT( "Failed cooking map '%s'" ), mapName.c_str() );
			return false;
		}
	}

	// Serialize shader cache
	{
		CArchive*		archive = GFileSystem->CreateFileWriter( GCookedDir + PATH_SEPARATOR + GShaderManager->GetShaderCacheFilename( cookedShaderPlatform ), AW_NoFail );
		archive->SetType( AT_ShaderCache );
		archive->SerializeHeader();
		shaderCache.Serialize( *archive );
		delete archive;
	}

	// Serialize table of contents
	{
		CArchive*		archive = GFileSystem->CreateFileWriter( GCookedDir + PATH_SEPARATOR + CTableOfContets::GetNameTOC(), AW_NoFail );
		GTableOfContents.Serialize( *archive );
		delete archive;
	}

	GIsCooker = false;
	return true;
}