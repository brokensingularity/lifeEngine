#include <vector>

#include "Misc/Class.h"
#include "Misc/Misc.h"
#include "Misc/CoreGlobals.h"
#include "Misc/TableOfContents.h"
#include "Logger/LoggerMacros.h"
#include "System/BaseFileSystem.h"
#include "System/Package.h"
#include "Containers/StringConv.h"
#include "Render/StaticMesh.h"
#include "Commandlets/CookerSyncCommandlet.h"

IMPLEMENT_CLASS( CCookerSyncCommandlet )

void CCookerSyncCommandlet::AddContentEntries( const std::wstring& InRootDir )
{
	std::vector< std::wstring >		files = GFileSystem->FindFiles( InRootDir, true, true );
	for ( uint32 index = 0, count = files.size(); index < count; ++index )
	{
		std::wstring		file = files[ index ];
		std::size_t			dotPos = file.find_last_of( TEXT( "." ) );
		std::wstring		fullPath = InRootDir + TEXT( "/" ) + file;
		
		if ( dotPos == std::wstring::npos )
		{
			AddContentEntries( fullPath );
			continue;
		}

		std::wstring		extension = file;
		extension.erase( 0, dotPos + 1 );
		if ( extension == TEXT( "pak" ) )
		{
			PackageRef_t		package = GPackageManager->LoadPackage( fullPath );
			check( package );

			LE_LOG( LT_Log, LC_Commandlet, TEXT( "Added package '%s'" ), fullPath.c_str() );
			GTableOfContents.AddEntry( package->GetGUID(), package->GetName(), fullPath );
			GPackageManager->UnloadPackage( fullPath );
		}
	}
}

bool CCookerSyncCommandlet::Main( const CCommandLine& InCommandLine )
{
	GTableOfContents.Clear();
	AddContentEntries( appBaseDir() );

	// Serialize table of contents
	CArchive*		archiveTOC = GFileSystem->CreateFileWriter( GCookedDir + CTableOfContets::GetNameTOC(), AW_NoFail );
	GTableOfContents.Serialize( *archiveTOC );
	delete archiveTOC;

	return true;
}