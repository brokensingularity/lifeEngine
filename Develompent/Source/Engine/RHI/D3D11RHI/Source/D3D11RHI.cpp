#include "Core.h"
#include "Logger/LoggerMacros.h"
#include "Render/RenderUtils.h"
#include "D3D11RHI.h"
#include "D3D11Viewport.h"
#include "D3D11DeviceContext.h"
#include "D3D11Surface.h"
#include "D3D11Shader.h"
#include "D3D11Buffer.h"
#include "D3D11ImGUI.h"
#include "D3D11State.h"

/**
 * Get vertex count for primitive count
 */
static FORCEINLINE uint32 GetVertexCountForPrimitiveCount( uint32 InNumPrimitives, EPrimitiveType InPrimitiveType )
{
	uint32		vertexCount = 0;
	switch ( InPrimitiveType )
	{
	case PT_PointList:			vertexCount = InNumPrimitives;		break;
	case PT_TriangleList:		vertexCount = InNumPrimitives * 3;	break;
	case PT_TriangleStrip:		vertexCount = InNumPrimitives + 2;	break;
	case PT_LineList:			vertexCount = InNumPrimitives * 2;	break;

	default:
		appErrorf( TEXT( "Unknown primitive type: %u" ), ( uint32 )InPrimitiveType );
	}

	return vertexCount;
}

/**
 * Get D3D11 primitive type from engine primitive type
 */
static FORCEINLINE D3D11_PRIMITIVE_TOPOLOGY GetD3D11PrimitiveType( uint32 InPrimitiveType, bool InIsUsingTessellation )
{
	checkMsg( !InIsUsingTessellation, TEXT( "Tessellation not supported!" ) );
	switch ( InPrimitiveType )
	{
	case PT_PointList:			return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	case PT_TriangleList:		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case PT_TriangleStrip:		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case PT_LineList:			return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;

	default: 
		appErrorf( TEXT( "Unknown primitive type: %u" ), ( uint32 )InPrimitiveType );
	};

	return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

/**
 * Constructor
 */
FD3D11RHI::FD3D11RHI() :
	isInitialize( false ),
	immediateContext( nullptr ),
	psConstantBuffer( nullptr ),
	d3d11Device( nullptr )
{}

/**
 * Destructor
 */
FD3D11RHI::~FD3D11RHI()
{
	Destroy();
}

/**
 * Initialize RHI
 */
void FD3D11RHI::Init( bool InIsEditor )
{
	if ( IsInitialize() )			return;

	uint32					deviceFlags = 0;

	// In Direct3D 11, if you are trying to create a hardware or a software device, set pAdapter != NULL which constrains the other inputs to be:
	//		DriverType must be D3D_DRIVER_TYPE_UNKNOWN 
	//		Software must be NULL. 
	D3D_DRIVER_TYPE			driverType = D3D_DRIVER_TYPE_UNKNOWN;

#if DEBUG
	deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // DEBUG

	// Create DXGI factory and adapter
	HRESULT		result = CreateDXGIFactory( __uuidof( IDXGIFactory ), ( void** ) &dxgiFactory );
	check( result == S_OK );

	uint32			currentAdapter = 0;
	while ( dxgiFactory->EnumAdapters( currentAdapter, &dxgiAdapter ) == DXGI_ERROR_NOT_FOUND )
	{
		++currentAdapter;
	}
	checkMsg( dxgiAdapter, TEXT( "GPU adapter not found" ) );

	D3D_FEATURE_LEVEL				maxFeatureLevel = D3D_FEATURE_LEVEL_11_0;
	D3D_FEATURE_LEVEL				featureLevel;
	ID3D11DeviceContext*			d3d11DeviceContext = nullptr;
	result = D3D11CreateDevice( dxgiAdapter, driverType, nullptr, deviceFlags, &maxFeatureLevel, 1, D3D11_SDK_VERSION, &d3d11Device, &featureLevel, &d3d11DeviceContext );
	check( result == S_OK );

	immediateContext = new FD3D11DeviceContext( d3d11DeviceContext );
	
	// Create constant buffers for shaders
	psConstantBuffer = new FD3D11ConstantBuffer( sizeof( float ) * 4, TEXT( "PixelCB" ) );
	psConstantBuffer->Bind( immediateContext );

	// Print info adapter
	DXGI_ADAPTER_DESC				adapterDesc;
	dxgiAdapter->GetDesc( &adapterDesc );

	LE_LOG( LT_Log, LC_Init, TEXT( "Found D3D11 adapter: %s" ), adapterDesc.Description );
	LE_LOG( LT_Log, LC_Init, TEXT( "Adapter has %uMB of dedicated video memory, %uMB of dedicated system memory, and %uMB of shared system memory" ),
			adapterDesc.DedicatedVideoMemory / ( 1024 * 1024 ),
			adapterDesc.DedicatedSystemMemory / ( 1024 * 1024 ),
			adapterDesc.SharedSystemMemory / ( 1024 * 1024 ) );

	// Initialize the platform pixel format map
	GPixelFormats[ PF_Unknown ].platformFormat		= DXGI_FORMAT_UNKNOWN;
	GPixelFormats[ PF_A8R8G8B8 ].platformFormat		= DXGI_FORMAT_R8G8B8A8_UNORM;

	isInitialize = true;
}

/**
 * Acquire thread ownership
 */
void FD3D11RHI::AcquireThreadOwnership()
{}

/**
 * Release thread ownership
 */
void FD3D11RHI::ReleaseThreadOwnership()
{
}

/**
 * Is initialized RHI
 */
bool FD3D11RHI::IsInitialize() const
{
	return isInitialize;
}

/**
 * Destroy RHI
 */
void FD3D11RHI::Destroy()
{
	if ( !isInitialize )		return;

	delete immediateContext;
	d3d11Device->Release();
	dxgiAdapter->Release();
	dxgiFactory->Release();

	immediateContext = nullptr;
	d3d11Device = nullptr;
	dxgiAdapter = nullptr;
	dxgiFactory = nullptr;
}

/**
 * Create viewport
 */
FViewportRHIRef FD3D11RHI::CreateViewport( void* InWindowHandle, uint32 InWidth, uint32 InHeight )
{
	return new FD3D11Viewport( InWindowHandle, InWidth, InHeight );
}

/**
 * Create vertex shader
 */
FVertexShaderRHIRef FD3D11RHI::CreateVertexShader( const tchar* InShaderName, const byte* InData, uint32 InSize )
{
	return new FD3D11VertexShaderRHI( InData, InSize, InShaderName );
}

/**
 * Create hull shader
 */
FHullShaderRHIRef FD3D11RHI::CreateHullShader( const tchar* InShaderName, const byte* InData, uint32 InSize )
{
	return new FD3D11HullShaderRHI( InData, InSize, InShaderName );
}

/**
 * Create domain shader
 */
FDomainShaderRHIRef FD3D11RHI::CreateDomainShader( const tchar* InShaderName, const byte* InData, uint32 InSize )
{
	return new FD3D11DomainShaderRHI( InData, InSize, InShaderName );
}

/**
 * Create pixel shader
 */
FPixelShaderRHIRef FD3D11RHI::CreatePixelShader( const tchar* InShaderName, const byte* InData, uint32 InSize )
{
	return new FD3D11PixelShaderRHI( InData, InSize, InShaderName );
}

/**
 * Create geometry shader
 */
FGeometryShaderRHIRef FD3D11RHI::CreateGeometryShader( const tchar* InShaderName, const byte* InData, uint32 InSize )
{
	return new FD3D11GeometryShaderRHI( InData, InSize, InShaderName );
}

/**
 * Create vertex buffer
 */
FVertexBufferRHIRef FD3D11RHI::CreateVertexBuffer( const tchar* InBufferName, uint32 InSize, const byte* InData, uint32 InUsage )
{
	return new FD3D11VertexBufferRHI( InUsage, InSize, InData, InBufferName );
}

/**
 * Create index buffer
 */
FIndexBufferRHIRef FD3D11RHI::CreateIndexBuffer( const tchar* InBufferName, uint32 InStride, uint32 InSize, const byte* InData, uint32 InUsage )
{
	return new FD3D11IndexBufferRHI( InUsage, InStride, InSize, InData, InBufferName );
}

/**
 * Create vertex declaration
 */
FVertexDeclarationRHIRef FD3D11RHI::CreateVertexDeclaration( const FVertexDeclarationElementList& InElementList )
{
	return new FD3D11VertexDeclarationRHI( InElementList );
}

/**
 * Create bound shader state
 */
FBoundShaderStateRHIRef FD3D11RHI::CreateBoundShaderState( const tchar* InBoundShaderStateName, FVertexDeclarationRHIRef InVertexDeclaration, FVertexShaderRHIRef InVertexShader, FPixelShaderRHIRef InPixelShader, FHullShaderRHIRef InHullShader /*= nullptr*/, FDomainShaderRHIRef InDomainShader /*= nullptr*/, FGeometryShaderRHIRef InGeometryShader /*= nullptr*/ )
{
	FBoundShaderStateKey		key( InVertexDeclaration, InVertexShader, InPixelShader, InHullShader, InDomainShader, InGeometryShader );
	FBoundShaderStateRHIRef		boundShaderStateRHI = boundShaderStateHistory.Find( key );
	if ( !boundShaderStateRHI )
	{
		boundShaderStateRHI = new FD3D11BoundShaderStateRHI( InBoundShaderStateName, key, InVertexDeclaration, InVertexShader, InPixelShader, InHullShader, InDomainShader, InGeometryShader );
		boundShaderStateHistory.Add( key, boundShaderStateRHI );
	}

	return boundShaderStateRHI;
}

/**
 * Create rasterizer state
 */
FRasterizerStateRHIRef FD3D11RHI::CreateRasterizerState( const FRasterizerStateInitializerRHI& InInitializer )
{
	return new FD3D11RasterizerStateRHI( InInitializer );
}

FSamplerStateRHIRef FD3D11RHI::CreateSamplerState( const FSamplerStateInitializerRHI& InInitializer )
{
	return new FD3D11SamplerStateRHI( InInitializer );
}

FTexture2DRHIRef FD3D11RHI::CreateTexture2D( const tchar* InDebugName, uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, uint32 InNumMips, uint32 InFlags, void* InData /*= nullptr*/ )
{
	return new FD3D11Texture2DRHI( InDebugName, InSizeX, InSizeY, InNumMips, InFormat, InFlags, InData );
}

/**
 * Get device context
 */
class FBaseDeviceContextRHI* FD3D11RHI::GetImmediateContext() const
{
	return immediateContext;
}

/**
 * Set viewport
 */
void FD3D11RHI::SetViewport( class FBaseDeviceContextRHI* InDeviceContext, uint32 InMinX, uint32 InMinY, float InMinZ, uint32 InMaxX, uint32 InMaxY, float InMaxZ )
{
	D3D11_VIEWPORT			d3d11Viewport = { ( float )InMinX, ( float )InMinY, ( float )InMaxX - InMinX, ( float )InMaxY - InMinY, ( float )InMinZ, InMaxZ };
	if ( d3d11Viewport.Width > 0 && d3d11Viewport.Height > 0 )
	{
		static_cast< FD3D11DeviceContext* >( InDeviceContext )->GetD3D11DeviceContext()->RSSetViewports( 1, &d3d11Viewport );
	}
}

/**
 * Set bound shader state
 */
void FD3D11RHI::SetBoundShaderState( class FBaseDeviceContextRHI* InDeviceContext, FBoundShaderStateRHIParamRef InBoundShaderState )
{
	ID3D11DeviceContext*			d3d11DeviceContext = ( ( FD3D11DeviceContext* )InDeviceContext )->GetD3D11DeviceContext();
	FD3D11BoundShaderStateRHI*		boundShaderState = ( FD3D11BoundShaderStateRHI* )InBoundShaderState;
	FD3D11VertexShaderRHI*			vertexShader = ( FD3D11VertexShaderRHI* )boundShaderState->GetVertexShader().GetPtr();
	FD3D11PixelShaderRHI*			pixelShader = ( FD3D11PixelShaderRHI* )boundShaderState->GetPixelShader().GetPtr();
	FD3D11HullShaderRHI*			hullShader = ( FD3D11HullShaderRHI* )boundShaderState->GetHullShader().GetPtr();
	FD3D11DomainShaderRHI*			domainShader = ( FD3D11DomainShaderRHI* )boundShaderState->GetDomainShader().GetPtr();
	FD3D11GeometryShaderRHI*		geometryShader = ( FD3D11GeometryShaderRHI* )boundShaderState->GetGeometryShader().GetPtr();

	d3d11DeviceContext->IASetInputLayout( boundShaderState->GetD3D11InputLayout() );
	d3d11DeviceContext->VSSetShader( vertexShader ? vertexShader->GetResource() : nullptr, nullptr, 0 );
	d3d11DeviceContext->PSSetShader( pixelShader ? pixelShader->GetResource() : nullptr, nullptr, 0 );
	d3d11DeviceContext->GSSetShader( geometryShader ? geometryShader->GetResource() : nullptr, nullptr, 0 );
	d3d11DeviceContext->HSSetShader( hullShader ? hullShader->GetResource() : nullptr, nullptr, 0 );
	d3d11DeviceContext->DSSetShader( domainShader ? domainShader->GetResource() : nullptr, nullptr, 0 );
}

/**
 * Set stream source
 */
void FD3D11RHI::SetStreamSource( class FBaseDeviceContextRHI* InDeviceContext, uint32 InStreamIndex, FVertexBufferRHIParamRef InVertexBuffer, uint32 InStride, uint32 InOffset )
{
	ID3D11DeviceContext*			d3d11DeviceContext = ( ( FD3D11DeviceContext* )InDeviceContext )->GetD3D11DeviceContext();
	ID3D11Buffer*					d3d11Buffer = ( ( FD3D11VertexBufferRHI* )InVertexBuffer )->GetD3D11Buffer();
	
	d3d11DeviceContext->IASetVertexBuffers( InStreamIndex, 1, &d3d11Buffer, &InStride, &InOffset );
}

/**
 * Set rasterizer state
 */
void FD3D11RHI::SetRasterizerState( class FBaseDeviceContextRHI* InDeviceContext, FRasterizerStateRHIParamRef InNewState )
{
	ID3D11DeviceContext*			d3d11DeviceContext = ( ( FD3D11DeviceContext* )InDeviceContext )->GetD3D11DeviceContext();
	d3d11DeviceContext->RSSetState( ( ( FD3D11RasterizerStateRHI* )InNewState )->GetResource() );
}

void FD3D11RHI::SetSamplerState( class FBaseDeviceContextRHI* InDeviceContext, FPixelShaderRHIParamRef InPixelShader, FSamplerStateRHIParamRef InNewState, uint32 InStateIndex )
{
	ID3D11DeviceContext*			d3d11DeviceContext = ( ( FD3D11DeviceContext* )InDeviceContext )->GetD3D11DeviceContext();
	ID3D11SamplerState*				d3d11SamplerState = ( ( FD3D11SamplerStateRHI* )InNewState )->GetResource();
	d3d11DeviceContext->PSSetSamplers( InStateIndex, 1, &d3d11SamplerState );
}

void FD3D11RHI::SetTextureParameter( class FBaseDeviceContextRHI* InDeviceContext, FPixelShaderRHIParamRef InPixelShader, FTextureRHIParamRef InTexture, uint32 InTextureIndex )
{
	ID3D11ShaderResourceView*		d3d11ShaderResourceView = nullptr;
	if ( InTexture )
	{
		d3d11ShaderResourceView = ( ( FD3D11TextureRHI* )InTexture )->GetShaderResourceView();
	}

	// Trying to set a valid texture with a nullptr shader resource view is probably a code mistake
	check( !InTexture || d3d11ShaderResourceView );
	ID3D11DeviceContext*			d3d11DeviceContext = ( ( FD3D11DeviceContext* )InDeviceContext )->GetD3D11DeviceContext();
	d3d11DeviceContext->PSSetShaderResources( InTextureIndex, 1, &d3d11ShaderResourceView );
}

//void FD3D11RHI::SetShaderParameter( class FBaseDeviceContextRHI* InDeviceContext, FVertexShaderRHIParamRef InVertexShader, uint32 InBufferIndex, uint32 InBaseIndex, uint32 InNumBytes, const void* InNewValue )
//{
//	check( false );
//}

void FD3D11RHI::SetShaderParameter( class FBaseDeviceContextRHI* InDeviceContext, FPixelShaderRHIParamRef InPixelShader, uint32 InBufferIndex, uint32 InBaseIndex, uint32 InNumBytes, const void* InNewValue )
{
	check( psConstantBuffer );
	psConstantBuffer->Update( ( byte* )InNewValue, InBaseIndex, InNumBytes );
	psConstantBuffer->CommitConstantsToDevice( ( FD3D11DeviceContext* )InDeviceContext );
}

/**
 * Lock vertex buffer
 */
void FD3D11RHI::LockVertexBuffer( class FBaseDeviceContextRHI* InDeviceContext, const FVertexBufferRHIRef InVertexBuffer, uint32 InSize, uint32 InOffset, FLockedData& OutLockedData )
{
	check( OutLockedData.data == nullptr && InOffset < InSize );

	D3D11_MAP						writeMode = D3D11_MAP_WRITE_DISCARD;
	D3D11_MAPPED_SUBRESOURCE		mappedSubresource;
	
	static_cast< FD3D11DeviceContext* >( InDeviceContext )->GetD3D11DeviceContext()->Map( static_cast< FD3D11VertexBufferRHI* >( InVertexBuffer.GetPtr() )->GetD3D11Buffer(), 0, writeMode, 0, &mappedSubresource );
	OutLockedData.data			= ( byte* )mappedSubresource.pData + InOffset;
	OutLockedData.pitch			= mappedSubresource.RowPitch;
	OutLockedData.isNeedFree	= false;
}

/**
 * Unlock vertex buffer
 */
void FD3D11RHI::UnlockVertexBuffer( class FBaseDeviceContextRHI* InDeviceContext, const FVertexBufferRHIRef InVertexBuffer, FLockedData& InLockedData )
{
	check( InLockedData.data );

	static_cast< FD3D11DeviceContext* >( InDeviceContext )->GetD3D11DeviceContext()->Unmap( static_cast< FD3D11VertexBufferRHI* >( InVertexBuffer.GetPtr() )->GetD3D11Buffer(), 0 );
	InLockedData.data = nullptr;
}

/**
 * Lock index buffer
 */
void FD3D11RHI::LockIndexBuffer( class FBaseDeviceContextRHI* InDeviceContext, const FIndexBufferRHIRef InIndexBuffer, uint32 InSize, uint32 InOffset, FLockedData& OutLockedData )
{
	check( OutLockedData.data == nullptr && InOffset < InSize );

	D3D11_MAP						writeMode = D3D11_MAP_WRITE_DISCARD;
	D3D11_MAPPED_SUBRESOURCE		mappedSubresource;

	static_cast< FD3D11DeviceContext* >( InDeviceContext )->GetD3D11DeviceContext()->Map( static_cast< FD3D11IndexBufferRHI* >( InIndexBuffer.GetPtr() )->GetD3D11Buffer(), 0, writeMode, 0, &mappedSubresource );
	OutLockedData.data = ( byte* )mappedSubresource.pData + InOffset;
	OutLockedData.pitch = mappedSubresource.RowPitch;
	OutLockedData.isNeedFree = false;
}

/**
 * Unlock index buffer
 */
void FD3D11RHI::UnlockIndexBuffer( class FBaseDeviceContextRHI* InDeviceContext, const FIndexBufferRHIRef InIndexBuffer, FLockedData& InLockedData )
{
	check( InLockedData.data );

	static_cast< FD3D11DeviceContext* >( InDeviceContext )->GetD3D11DeviceContext()->Unmap( static_cast< FD3D11IndexBufferRHI* >( InIndexBuffer.GetPtr() )->GetD3D11Buffer(), 0 );
	InLockedData.data = nullptr;
}

void FD3D11RHI::LockTexture2D( class FBaseDeviceContextRHI* InDeviceContext, FTexture2DRHIParamRef InTexture, uint32 InMipIndex, bool InIsDataWrite, FLockedData& OutLockedData, bool InIsUseCPUShadow /*= false*/ )
{
	( ( FD3D11Texture2DRHI* )InTexture )->Lock( InDeviceContext, InMipIndex, InIsDataWrite, InIsUseCPUShadow, OutLockedData );
}

void FD3D11RHI::UnlockTexture2D( class FBaseDeviceContextRHI* InDeviceContext, FTexture2DRHIParamRef InTexture, uint32 InMipIndex, FLockedData& InLockedData )
{
	( ( FD3D11Texture2DRHI* )InTexture )->Unlock( InDeviceContext, InMipIndex, InLockedData );
}

/**
 * Draw primitive
 */
void FD3D11RHI::DrawPrimitive( class FBaseDeviceContextRHI* InDeviceContext, EPrimitiveType InPrimitiveType, uint32 InBaseVertexIndex, uint32 InNumPrimitives )
{
	ID3D11DeviceContext*			d3d11DeviceContext = ( ( FD3D11DeviceContext* )InDeviceContext )->GetD3D11DeviceContext();
	uint32							vertexCount = GetVertexCountForPrimitiveCount( InNumPrimitives, InPrimitiveType );

	d3d11DeviceContext->IASetPrimitiveTopology( GetD3D11PrimitiveType( InPrimitiveType, false ) );
	d3d11DeviceContext->Draw( vertexCount, InBaseVertexIndex );
}

/**
 * Begin drawing viewport
 */
void FD3D11RHI::BeginDrawingViewport( class FBaseDeviceContextRHI* InDeviceContext, class FBaseViewportRHI* InViewport )
{
	check( InDeviceContext && InViewport );
	FD3D11DeviceContext*			deviceContext = ( FD3D11DeviceContext* )InDeviceContext;
	FD3D11Viewport*					viewport = ( FD3D11Viewport* )InViewport;

	ID3D11RenderTargetView*			d3d11RenderTargetView = ( ( FD3D11Surface* )viewport->GetSurface().GetPtr() )->GetD3D11RenderTargetView();
	deviceContext->GetD3D11DeviceContext()->OMSetRenderTargets( 1, &d3d11RenderTargetView, nullptr );
	SetViewport( InDeviceContext, 0, 0, 0.f, viewport->GetWidth(), viewport->GetHeight(), 1.f );
}

/**
 * End drawing viewport
 */
void FD3D11RHI::EndDrawingViewport( class FBaseDeviceContextRHI* InDeviceContext, class FBaseViewportRHI* InViewport, bool InIsPresent, bool InLockToVsync )
{
	check( InViewport );

	if ( InIsPresent )
	{
		InViewport->Present( InLockToVsync );
	}
}

#if WITH_EDITOR
#include <d3dcompiler.h>

#include "Containers/StringConv.h"
#include "Misc/CoreGlobals.h"
#include "System/BaseArchive.h"
#include "System/BaseFileSystem.h"

/**
 * TranslateCompilerFlag - Translates the platform-independent compiler flags into D3DX defines
 * @param[in] CompilerFlag - The platform-independent compiler flag to translate
 * @return DWORD - The value of the appropriate D3DX enum
 */
static DWORD TranslateCompilerFlagD3D11( ECompilerFlags CompilerFlag )
{
	switch ( CompilerFlag )
	{
	case CF_PreferFlowControl:			return D3D10_SHADER_PREFER_FLOW_CONTROL;
	case CF_Debug:						return D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION;
	case CF_AvoidFlowControl:			return D3D10_SHADER_AVOID_FLOW_CONTROL;
	default:							return 0;
	};
}

/**
 * Compile shader
 */
bool FD3D11RHI::CompileShader( const tchar* InSourceFileName, const tchar* InFunctionName, EShaderFrequency InFrequency, const FShaderCompilerEnvironment& InEnvironment, FShaderCompilerOutput& OutOutput, bool InDebugDump /* = false */, const tchar* InShaderSubDir /* = TEXT( "" ) */ )
{
	FBaseArchive*		shaderArchive = GFileSystem->CreateFileReader( InSourceFileName );
	if ( !shaderArchive )
	{
		return false;
	}

	// Create string buffer and fill '\0'
	uint32				archiveSize = shaderArchive->GetSize() + 1;
	byte*				buffer = new byte[ archiveSize ];
	memset( buffer, '\0', archiveSize );

	// Serialize data to string buffer
	shaderArchive->Serialize( buffer, archiveSize );

	// Getting shader profile
	std::string			shaderProfile;
	switch ( InFrequency )
	{
	case SF_Vertex:
		shaderProfile = "vs_5_0";
		break;

	case SF_Hull:
		shaderProfile = "hs_5_0";
		break;

	case SF_Domain:
		shaderProfile = "ds_5_0";
		break;

	case SF_Geometry:
		shaderProfile = "gs_5_0";
		break;

	case SF_Pixel:
		shaderProfile = "ps_5_0";
		break;

	case SF_Compute:
		shaderProfile = "cs_5_0";
		break;

	default:
		appErrorf( TEXT( "Unknown shader frequency %i" ), InFrequency );
		return false;
	}

	// Translate the input environment's definitions to D3DXMACROs
	std::vector< D3D_SHADER_MACRO >				macros;
	for ( auto it = InEnvironment.difinitions.begin(), itEnd = InEnvironment.difinitions.end(); it != itEnd; ++it )
	{
		const std::wstring&			name = it->first;
		const std::wstring&			difinition = it->second;

		// Create temp C-string of name and value difinition
		achar*						tName = new achar[ name.size() + 1 ];
		achar*						tDifinition = new achar[ difinition.size() + 1 ];
		strncpy( tName, TCHAR_TO_ANSI( name.c_str() ), name.size() + 1 );
		strncpy( tDifinition, TCHAR_TO_ANSI( difinition.c_str() ), difinition.size() + 1 );

		D3D_SHADER_MACRO			d3dMacro;
		d3dMacro.Name = tName;
		d3dMacro.Definition = tDifinition;
		macros.push_back( d3dMacro );
	}

	// Terminate the Macros list
	macros.push_back( D3D_SHADER_MACRO{ nullptr, nullptr } );

	// Getting compile flags
	DWORD				compileFlags = D3D10_SHADER_OPTIMIZATION_LEVEL3;
	for ( uint32 indexFlag = 0, countFlags = ( uint32 )InEnvironment.compilerFlags.size(); indexFlag < countFlags; ++indexFlag )
	{
		// Accumulate flags set by the shader
		compileFlags |= TranslateCompilerFlagD3D11( InEnvironment.compilerFlags[ indexFlag ] );
	}

	ID3DBlob*		shaderBlob = nullptr;
	ID3DBlob*		errorsBlob = nullptr;
	HRESULT			result = D3DCompile( buffer, archiveSize, TCHAR_TO_ANSI( InSourceFileName ), macros.data(), nullptr, TCHAR_TO_ANSI( InFunctionName ), shaderProfile.c_str(), compileFlags, 0, &shaderBlob, &errorsBlob );
	if ( FAILED( result ) )
	{
		// Copy the error text to the output.
		void*		errorBuffer = errorsBlob ? errorsBlob->GetBufferPointer() : nullptr;
		if ( errorBuffer )
		{
			LE_LOG( LT_Error, LC_Shader, ANSI_TO_TCHAR( errorBuffer ) );
			errorsBlob->Release();
		}
		else
		{
			LE_LOG( LT_Error, LC_Shader, TEXT( "Compile Failed without warnings!" ) );
		}

		return false;
	}

	// Save code of shader and getting reflector of shader
	uint32		numShaderBytes = ( uint32 )shaderBlob->GetBufferSize();
	OutOutput.code.resize( numShaderBytes );
	memcpy( OutOutput.code.data(), shaderBlob->GetBufferPointer(), numShaderBytes );

	ID3D11ShaderReflection*				reflector = nullptr;
	result = D3DReflect( OutOutput.code.data(), OutOutput.code.size(), IID_ID3D11ShaderReflection, ( void** ) &reflector );
	check( result == S_OK );

	// Read the constant table description.
	D3D11_SHADER_DESC					shaderDesc;
	reflector->GetDesc( &shaderDesc );

	// TODO BG yehor.pohuliaka - Added her getting reflection information by shader (constant buffers, etc)

	// Set the number of instructions
	OutOutput.numInstructions = shaderDesc.InstructionCount;

	// Reflector is a com interface, so it needs to be released.
	reflector->Release();

	// Free temporary strings allocated for the macros
	for ( uint32 indexMacro = 0, countMacros = ( uint32 ) macros.size(); indexMacro < countMacros; ++indexMacro )
	{
		D3D_SHADER_MACRO&			macro = macros[ indexMacro ];
		delete[] macro.Name;
		delete[] macro.Definition;
	}

	shaderBlob->Release();
	delete[] buffer;
	delete shaderArchive;
	return true;
}
#endif // WITH_EDITOR

#if WITH_IMGUI
/**
 * Initialize ImGUI
 */
void FD3D11RHI::InitImGUI( class FBaseDeviceContextRHI* InDeviceContext )
{
	check( d3d11Device && InDeviceContext );
	FD3D11DeviceContext* deviceContext = ( FD3D11DeviceContext* ) InDeviceContext;

	check( ImGui_ImplDX11_Init( d3d11Device, deviceContext->GetD3D11DeviceContext() ) );
	ImGui_ImplDX11_NewFrame();
}

/**
 * Shutdown render of ImGUI
 */
void FD3D11RHI::ShutdownImGUI( class FBaseDeviceContextRHI* InDeviceContext )
{
	ImGui_ImplDX11_Shutdown();
}

/**
 * Draw ImGUI
 */
void FD3D11RHI::DrawImGUI( class FBaseDeviceContextRHI* InDeviceContext, ImDrawData* InImGUIDrawData )
{
	ImGui_ImplDX11_RenderDrawData( InImGUIDrawData );
}
#endif // WITH_IMGUI

/**
 * Get RHI name
 */
const tchar* FD3D11RHI::GetRHIName() const
{
	return TEXT( "D3D11RHI" );
}

/**
 * Set debug name fore DirectX 11 resource
 */
void D3D11SetDebugName( ID3D11DeviceChild* InObject, achar* InName )
{
	if ( !InName )			return;
	InObject->SetPrivateData( WKPDID_D3DDebugObjectName, ( uint32 )strlen( InName ), InName );
}

/**
 * Set debug name fore DirectX 11 resource
 */
void D3D11SetDebugName( IDXGIObject* InObject, achar* InName )
{
	if ( !InName )			return;
	InObject->SetPrivateData( WKPDID_D3DDebugObjectName, ( uint32 )strlen( InName ), InName );
}