#include "Containers/String.h"
#include "Render/SceneRendering.h"
#include "Render/Scene.h"
#include "Misc/EngineGlobals.h"
#include "System/World.h"
#include "System/BaseEngine.h"
#include "RHI/BaseRHI.h"
#include "RHI/BaseBufferRHI.h"
#include "RHI/BaseSurfaceRHI.h"
#include "RHI/BaseDeviceContextRHI.h"
#include "RHI/TypesRHI.h"
#include "RHI/StaticStatesRHI.h"
#include "Render/RenderUtils.h"
#include "Render/Shaders/ScreenShader.h"
#include "Render/RenderingThread.h"
#include "Render/SceneUtils.h"
#include "Render/Scene.h"
#include "Render/Texture.h"
#include "Render/VertexFactory/SimpleElementVertexFactory.h"
#include "Render/SceneRenderTargets.h"

CSceneRenderer::CSceneRenderer( CSceneView* InSceneView, class CScene* InScene /* = nullptr */ )
	: scene( InScene )
	, sceneView( InSceneView )
{}

void CSceneRenderer::BeginRenderViewTarget( ViewportRHIParamRef_t InViewportRHI )
{
	CBaseDeviceContextRHI*	immediateContext	= GRHI->GetImmediateContext();

	SCOPED_DRAW_EVENT( EventBeginRenderViewTarget, DEC_SCENE_ITEMS, TEXT( "Begin Render View Target" ) );
	GSceneRenderTargets.Allocate( InViewportRHI->GetWidth(), InViewportRHI->GetHeight() );

	// If SHOW_Lights setted and disabled wireframe mode, we rendering to the GBuffer
	ShowFlags_t		showFlags = sceneView->GetShowFlags();
	if ( showFlags & SHOW_Lights 
#if WITH_EDITOR
		 && !( showFlags & SHOW_Wireframe )
#endif // WITH_EDITOR
		 )
	{
		GSceneRenderTargets.BeginRenderingGBuffer( immediateContext );
		GSceneRenderTargets.ClearGBufferTargets( immediateContext );
	}
	// Otherwise to the scene color
	else
	{
		GSceneRenderTargets.BeginRenderingSceneColor( immediateContext );
	}
	
	immediateContext->ClearSurface( GSceneRenderTargets.GetSceneColorSurface(), sceneView->GetBackgroundColor() );
	immediateContext->ClearDepthStencil( GSceneRenderTargets.GetSceneDepthZSurface() );
	GRHI->SetDepthState( immediateContext, TStaticDepthStateRHI<true>::GetRHI() );
	GRHI->SetBlendState( immediateContext, TStaticBlendStateRHI<>::GetRHI() );
	GRHI->SetViewParameters( immediateContext, *sceneView );

	// Build visible view on scene
	if ( scene )
	{
		scene->BuildView( *sceneView );
	}
}

void CSceneRenderer::Render( ViewportRHIParamRef_t InViewportRHI )
{
	if ( !scene )
	{
		return;
	}

	CBaseDeviceContextRHI*	immediateContext	= GRHI->GetImmediateContext();
	ShowFlags_t				showFlags			= sceneView->GetShowFlags();
	bool					bDirty				= false;

	// Render world layer
	{
		bDirty |= RenderSDG( immediateContext, SDG_World );
	}
	
	// Render lights
	if ( bDirty && !scene->GetVisibleLights().empty() && showFlags & SHOW_Lights
#if WITH_EDITOR
		 && !( showFlags & SHOW_Wireframe )
#endif // WITH_EDITOR
		 )
	{
		RenderLights( immediateContext );
	}

#if WITH_EDITOR
	// If we in editor draw highlight layer (gizmo and etc)
	if ( GIsEditor )
	{
		RenderHighlight( immediateContext );
	}
#endif // WITH_EDITOR

	// Render post process
	RenderPostProcess( immediateContext );

	// Render UI
	RenderUI( immediateContext );
}

#if WITH_EDITOR
void CSceneRenderer::RenderHighlight( class CBaseDeviceContextRHI* InDeviceContext )
{
	SCOPED_DRAW_EVENT( EventUI, DEC_SCENE_ITEMS, TEXT( "Highlight" ) );
	GRHI->SetDepthState( InDeviceContext, TStaticDepthStateRHI<true>::GetRHI() );
	GRHI->SetBlendState( InDeviceContext, TStaticBlendStateRHI<>::GetRHI() );
	
	RenderSDG( InDeviceContext, SDG_Highlight );
}
#endif // WITH_EDITOR

bool CSceneRenderer::RenderSDG( class CBaseDeviceContextRHI* InDeviceContext, uint32 InSDGIndex )
{
	ShowFlags_t			showFlags	= sceneView->GetShowFlags();
	SSceneDepthGroup&	SDG			= scene->GetSDG( ( ESceneDepthGroup )InSDGIndex );
	
	if ( SDG.IsEmpty() )
	{
		return false;
	}

	SCOPED_DRAW_EVENT( EventSDG, DEC_SCENE_ITEMS, CString::Format( TEXT( "SDG %s" ), GetSceneSDGName( ( ESceneDepthGroup )InSDGIndex ) ).c_str() );

#if WITH_EDITOR
	// Draw simple elements
	if ( showFlags & SHOW_SimpleElements && !SDG.simpleElements.IsEmpty() )
	{
		SCOPED_DRAW_EVENT( EventSimpleElements, DEC_SIMPLEELEMENTS, TEXT( "Simple elements" ) );
		SDG.simpleElements.Draw( InDeviceContext, *sceneView );
	}

	// Draw gizmos
	if ( showFlags & SHOW_Gizmo && SDG.gizmoDrawList.GetNum() > 0 )
	{
		SCOPED_DRAW_EVENT( EventGizmos, DEC_SPRITE, TEXT( "Gizmos" ) );
		SDG.gizmoDrawList.Draw( InDeviceContext, *sceneView );
	}
#endif // WITH_EDITOR

	// Draw static meshes
	if ( showFlags & SHOW_StaticMesh && SDG.staticMeshDrawList.GetNum() > 0 )
	{
		SCOPED_DRAW_EVENT( EventStaticMeshes, DEC_STATIC_MESH, TEXT( "Static meshes" ) );
		SDG.staticMeshDrawList.Draw( InDeviceContext, *sceneView );
	}

	// Draw sprites
	if ( showFlags & SHOW_Sprite && SDG.spriteDrawList.GetNum() > 0 )
	{
		SCOPED_DRAW_EVENT( EventSprites, DEC_SPRITE, TEXT( "Sprites" ) );
		SDG.spriteDrawList.Draw( InDeviceContext, *sceneView );
	}

	// Draw dynamic meshes
	if ( showFlags & SHOW_DynamicElements && ( SDG.dynamicMeshElements.GetNum() > 0 ||
#if WITH_EDITOR
											   !SDG.dynamicMeshBuilders.empty()
#else
											   false
#endif // WITH_EDITOR
											   ) )
	{
		SCOPED_DRAW_EVENT( EventDynamicElements, DEC_DYNAMICELEMENTS, TEXT( "Dynamic elements" ) );

		// Draw dynamic mesh elements
		if ( SDG.dynamicMeshElements.GetNum() > 0 )
		{
			SDG.dynamicMeshElements.Draw( InDeviceContext, *sceneView );
		}

		// Draw dynamic mesh builders
#if WITH_EDITOR
		if ( !SDG.dynamicMeshBuilders.empty() )
		{
			for ( auto it = SDG.dynamicMeshBuilders.begin(), itEnd = SDG.dynamicMeshBuilders.end(); it != itEnd; ++it )
			{
				const SDynamicMeshBuilderElement& element = *it;
				if ( element.dynamicMeshBuilder )
				{
					element.dynamicMeshBuilder->Draw<CMeshDrawingPolicy>( InDeviceContext, element.localToWorldMatrix, element.material, *sceneView );
				}
			}
		}
#endif // WITH_EDITOR
	}

	return true;
}

void CSceneRenderer::FinishRenderViewTarget( ViewportRHIParamRef_t InViewportRHI )
{
	SCOPED_DRAW_EVENT( EventFinishRenderViewTarget, DEC_SCENE_ITEMS, TEXT( "Finish Render View Target" ) );

	// Clear visible view on finish of the scene render
	if ( scene )
	{
		scene->ClearView();
	}

	CBaseDeviceContextRHI*					immediateContext	= GRHI->GetImmediateContext();
	Texture2DRHIRef_t						sceneColorTexture	= GSceneRenderTargets.GetSceneColorTexture();
	const uint32							sceneColorSizeX		= sceneColorTexture->GetSizeX();
	const uint32							sceneColorSizeY		= sceneColorTexture->GetSizeY();
	const uint32							viewportSizeX		= InViewportRHI->GetWidth();
	const uint32							viewportSizeY		= InViewportRHI->GetHeight();
	CScreenVertexShader<SVST_Default>*		screenVertexShader	= GShaderManager->FindInstance< CScreenVertexShader<SVST_Default>, CSimpleElementVertexFactory >();
	CScreenPixelShader*						screenPixelShader	= GShaderManager->FindInstance< CScreenPixelShader, CSimpleElementVertexFactory >();
	check( screenVertexShader && screenPixelShader );
	
	GRHI->SetRenderTarget( immediateContext, InViewportRHI->GetSurface(), nullptr );
	GRHI->SetDepthState( immediateContext, TStaticDepthStateRHI<false>::GetRHI() );
	GRHI->SetRasterizerState( immediateContext, TStaticRasterizerStateRHI<>::GetRHI() );
	GRHI->SetBlendState( immediateContext, TStaticBlendStateRHI<>::GetRHI() );
	GRHI->SetBoundShaderState( immediateContext, GRHI->CreateBoundShaderState( TEXT( "FinishRenderViewTarget" ), GSimpleElementVertexDeclaration.GetVertexDeclarationRHI(), screenVertexShader->GetVertexShader(), screenPixelShader->GetPixelShader() ) );

	screenPixelShader->SetTexture( immediateContext, sceneColorTexture );
	screenPixelShader->SetSamplerState( immediateContext, TStaticSamplerStateRHI<>::GetRHI() );
	DrawDenormalizedQuad( immediateContext, 1, 1, viewportSizeX, viewportSizeY, 0, 0, viewportSizeX, viewportSizeY, viewportSizeX, viewportSizeY, sceneColorSizeX, sceneColorSizeY, 1.f );
}
