#include "Containers/String.h"
#include "System/EditorFrame.h"
#include "System/Config.h"
#include "Logger/BaseLogger.h"
#include "Logger/LoggerMacros.h"
#include "Misc/CoreGlobals.h"
#include "Misc/EngineGlobals.h"
#include "Misc/WorldEdGlobals.h"
#include "RHI/BaseRHI.h"
#include "System/WorldEdApp.h"

WxWorldEdApp::WxWorldEdApp() 
	: editorFrame( nullptr )
{}

bool WxWorldEdApp::OnInit()
{
	GApp = this;
	WxLaunchApp::OnInit();

	// Get the editor
	editorFrame = CreateEditorFrame();
	check( editorFrame );

	// Init UI and show editor frame
	editorFrame->Show();
	editorFrame->SetUp();
	return true;
}

WxEditorFrame* WxWorldEdApp::CreateEditorFrame()
{
	// Look up the name of the frame we are creating
	std::wstring		editorFrameName = GEditorConfig.GetValue( TEXT( "Editor.EditorFrame" ), TEXT( "Class" ) ).GetString();

	// In case the config file is messed up
	if ( editorFrameName.empty() )
	{
		editorFrameName = TEXT( "WxEditorFrame" );
	}

	// Use the wxWindows' RTTI system to create the window
	wxObject*		newObject = wxCreateDynamicObject( editorFrameName.c_str() );
	if ( !newObject )
	{
		LE_LOG( LT_Warning, LC_Editor, TEXT( "Failed to create the editor frame class %s" ), editorFrameName.c_str() );
		LE_LOG( LT_Warning, LC_Editor, TEXT( "Falling back to WxEditorFrame" ) );

		// Fallback to the default frame
		newObject = new WxEditorFrame();
	}

	// Make sure it's the right type too
	if ( !wxDynamicCast( newObject, WxEditorFrame ) )
	{
		LE_LOG( LT_Warning, LC_Editor, TEXT( "Class %s is not derived from WxEditorFrame" ), editorFrameName.c_str() );
		LE_LOG( LT_Warning, LC_Editor, TEXT( "Falling back to WxEditorFrame" ) );

		delete newObject;
		newObject = new WxEditorFrame();
	}

	WxEditorFrame*		frame = wxDynamicCast( newObject, WxEditorFrame );
	check( frame );

	// Now do the window intialization
	frame->Create();
	return frame;
}