#include "Misc/CoreGlobals.h"
#include "Misc/Misc.h"
#include "Containers/StringConv.h"
#include "System/BaseFileSystem.h"
#include "Windows/ContentBrowserWindow.h"
#include "ImGUI/imgui_internal.h"
#include "ImGUI/imgui_stdlib.h"

/** Border size for buttons in asset viewer */
#define CONTENTBROWSER_ASSET_BORDERSIZE		1.f

/** Table of color buttons by asset type */
static ImVec4		GAssetBorderColors[] =
{
	ImVec4( 1.f, 1.f, 1.f, 1.f ),				// AT_Unknown
	ImVec4( 0.75f, 0.25f, 0.25f, 1.f ),			// AT_Texture2D
	ImVec4( 0.25f, 0.75f, 0.25f, 1.f ),			// AT_Material
	ImVec4( 0.f, 0.f, 0.f, 0.f ),				// AT_Script
	ImVec4( 0.f, 1.f, 1.f, 1.f ),				// AT_StaticMesh
	ImVec4( 0.38f, 0.33f, 0.83f, 1.f ),			// AT_AudioBank
	ImVec4( 0.78f, 0.75f, 0.5f, 1.f )			// AT_PhysicsMaterial
};
static_assert( ARRAY_COUNT( GAssetBorderColors ) == AT_Count, "Need full init GAssetBorderColors array" );

CContentBrowserWindow::CContentBrowserWindow( const std::wstring& InName )
	: CImGUILayer( InName )
	, padding( 16.f )
	, thumbnailSize( 64.f )
{
	memset( filterAssetType, 1, ARRAY_COUNT( filterAssetType ) * sizeof( bool ) );
}

void CContentBrowserWindow::OnTick()
{
	ImGui::Columns( 2 );

	// Draw list of packages in file system
	ImGui::InputTextWithHint( "##FilterPackage", "Search", &filterPackage );
	ImGui::SameLine( 0, 0 );
	if ( ImGui::Button( "X##0" ) )
	{
		filterPackage.clear();
	}

	ImGui::BeginChild( "##Packages" );
	ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.f, 0.f, 0.f, 0.f ) );
	
	// Engine category packages
	if ( ImGui::TreeNode( "Engine" ) )
	{
		DrawPackageList( appBaseDir() + PATH_SEPARATOR TEXT( "Engine/Content/" ) );
		ImGui::TreePop();
	}

	// Game category packages
	if ( ImGui::TreeNode( "Game" ) )
	{
		DrawPackageList( appGameDir() + PATH_SEPARATOR TEXT( "Content/" ) );
		ImGui::TreePop();
	}

	ImGui::PopStyleColor();
	ImGui::EndChild();
	ImGui::NextColumn();

	// Draw assets in current package
	if ( package )
	{
		// Section of filter assets
		ImGui::InputTextWithHint( "##FilterAsset", "Search", &filterAsset );
		ImGui::SameLine( 0, 0 );
		if ( ImGui::Button( "X##1" ) )
		{
			filterAsset.clear();
		}

		ImGui::SameLine();
		ImGui::PushItemWidth( -1 );
		if ( ImGui::BeginCombo( "##FilterAssetTypes", GetPreviewFilterAssetType().c_str() ) )
		{
			bool		bEnabledAllTypes = IsShowAllAssetTypes();
			if ( ImGui::Selectable( "All", &bEnabledAllTypes, ImGuiSelectableFlags_DontClosePopups ) )
			{
				if ( bEnabledAllTypes )
				{
					memset( filterAssetType, 1, ARRAY_COUNT( filterAssetType ) * sizeof( bool ) );
				}
				else
				{
					memset( filterAssetType, 0, ARRAY_COUNT( filterAssetType ) * sizeof( bool ) );
				}
			}

			for ( uint32 index = AT_FirstType; index < AT_Count; ++index )
			{
				ImGui::Selectable( TCHAR_TO_ANSI( ConvertAssetTypeToText( ( EAssetType )index ).c_str() ), &filterAssetType[index-1], ImGuiSelectableFlags_DontClosePopups );
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
		ImGui::Separator();

		// Section of assets
		ImGui::BeginChild( "##Assets" );
		float	panelWidth		= ImGui::GetContentRegionAvail().x;
		int32	columnCount		= panelWidth / ( thumbnailSize + padding );
		if ( columnCount < 1 )
		{
			columnCount = 1;
		}
		ImGui::Columns( columnCount, 0, false );	
		
		SAssetInfo		assetInfo;
		for ( uint32 index = 0, count = package->GetNumAssets(); index < count; ++index )
		{
			package->GetAssetInfo( index, assetInfo );
			if ( filterAssetType[assetInfo.type-1] && CString::InString( assetInfo.name, ANSI_TO_TCHAR( filterAsset.c_str() ), true ) )
			{
				ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, CONTENTBROWSER_ASSET_BORDERSIZE );
				ImGui::PushStyleColor( ImGuiCol_Border, GAssetBorderColors[assetInfo.type] );
				ImGui::Button( TCHAR_TO_ANSI( assetInfo.name.c_str() ), { thumbnailSize, thumbnailSize } );
				ImGui::PopStyleVar();
				ImGui::PopStyleColor();

				ImGui::Text( TCHAR_TO_ANSI( assetInfo.name.c_str() ) );
				ImGui::NextColumn();
			}
		}	
		ImGui::EndChild();
	}

	ImGui::EndColumns();
}

void CContentBrowserWindow::DrawPackageList( const std::wstring& InRootDir )
{
	std::vector<std::wstring>	files = GFileSystem->FindFiles( InRootDir, true, true );
	for ( uint32 index = 0, count = files.size(); index < count; ++index )
	{
		std::wstring		file		= files[index];
		std::size_t			dotPos		= file.find_last_of( TEXT( "." ) );
		std::wstring		fullPath	= InRootDir + TEXT( "/" ) + file;

		// Draw tree in sub directory
		if ( dotPos == std::wstring::npos )
		{
			if ( ImGui::TreeNode( TCHAR_TO_ANSI( file.c_str() ) ) )
			{
				DrawPackageList( fullPath );
				ImGui::TreePop();
			}
			continue;
		}

		std::wstring		extension = file;
		extension.erase( 0, dotPos + 1 );
		if ( extension == TEXT( "pak" ) && CString::InString( file, ANSI_TO_TCHAR( filterPackage.c_str() ), true ) )
		{
			ImGui::Button( TCHAR_TO_ANSI( file.c_str() ) );
			if ( ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
			{
				package = GPackageManager->LoadPackage( fullPath );
				check( package );
			}
		}
	}
}

std::string CContentBrowserWindow::GetPreviewFilterAssetType() const
{
	if ( IsShowAllAssetTypes() )
	{
		return "All";
	}

	std::wstring	result;
	for ( uint32 index = AT_FirstType; index < AT_Count; ++index )
	{
		if ( filterAssetType[index - 1] )
		{
			result += CString::Format( TEXT( "%s%s" ), result.empty() ? TEXT( "" ) : TEXT( ", " ), ConvertAssetTypeToText( ( EAssetType )index ).c_str() );
		}
	}

	return TCHAR_TO_ANSI( result.c_str() );
}