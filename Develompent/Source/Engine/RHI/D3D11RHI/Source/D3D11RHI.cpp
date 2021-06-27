#include "Core.h"
#include "Logger/LoggerMacros.h"
#include "D3D11RHI.h"
#include "D3D11Viewport.h"
#include "D3D11DeviceContext.h"
#include "D3D11Surface.h"
#include "D3D11Shader.h"
#include "D3D11Buffer.h"

#include "D3D11ImGUI.h"

/**
 * Constructor
 */
FD3D11RHI::FD3D11RHI() :
	isInitialize( false ),
	immediateContext( nullptr ),
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

#if !SHIPPING_BUILD
	deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // !SHIPPING_BUILD

	// Create DXGI factory and adapter
	HRESULT		result = CreateDXGIFactory( __uuidof( IDXGIFactory ), ( void** ) &dxgiFactory );
	check( result == S_OK );

	uint32			currentAdapter = 0;
	while ( dxgiFactory->EnumAdapters( currentAdapter, &dxgiAdapter ) == DXGI_ERROR_NOT_FOUND )
	{
		++currentAdapter;
	}
	checkMsg( dxgiAdapter, "GPU adapter not found" );

	D3D_FEATURE_LEVEL				maxFeatureLevel = D3D_FEATURE_LEVEL_11_0;
	D3D_FEATURE_LEVEL				featureLevel;
	ID3D11DeviceContext*			d3d11DeviceContext = nullptr;
	result = D3D11CreateDevice( dxgiAdapter, driverType, nullptr, deviceFlags, &maxFeatureLevel, 1, D3D11_SDK_VERSION, &d3d11Device, &featureLevel, &d3d11DeviceContext );
	check( result == S_OK );

	immediateContext = new FD3D11DeviceContext( d3d11DeviceContext );
	
	// Print info adapter
	DXGI_ADAPTER_DESC				adapterDesc;
	dxgiAdapter->GetDesc( &adapterDesc );

	LE_LOG( LT_Log, LC_Init, TEXT( "Found D3D11 adapter: %s" ), adapterDesc.Description );
	LE_LOG( LT_Log, LC_Init, TEXT( "Adapter has %uMB of dedicated video memory, %uMB of dedicated system memory, and %uMB of shared system memory" ),
			adapterDesc.DedicatedVideoMemory / ( 1024 * 1024 ),
			adapterDesc.DedicatedSystemMemory / ( 1024 * 1024 ),
			adapterDesc.SharedSystemMemory / ( 1024 * 1024 ) );

	isInitialize = true;
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
class FBaseViewportRHI* FD3D11RHI::CreateViewport( void* InWindowHandle, uint32 InWidth, uint32 InHeight )
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
 * Lock vertex buffer
 */
void FD3D11RHI::LockVertexBuffer( class FBaseDeviceContextRHI* InDeviceContext, const FVertexBufferRHIRef InVertexBuffer, uint32 InSize, FLockedData& OutLockedData )
{
	check( OutLockedData.data == nullptr );

	D3D11_MAP						writeMode = D3D11_MAP_WRITE_DISCARD;
	D3D11_MAPPED_SUBRESOURCE		mappedSubresource;
	
	static_cast< FD3D11DeviceContext* >( InDeviceContext )->GetD3D11DeviceContext()->Map( static_cast< FD3D11VertexBufferRHI* >( InVertexBuffer.GetPtr() )->GetD3D11Buffer(), 0, writeMode, 0, &mappedSubresource );
	OutLockedData.data			= ( byte* )mappedSubresource.pData;
	OutLockedData.pitch			= mappedSubresource.RowPitch;
	OutLockedData.isNeedFree	= false;
}

/**
 * Unlock vertex buffer
 */
void FD3D11RHI::UnlockVertexBuffer( class FBaseDeviceContextRHI* InDeviceContext, const FVertexBufferRHIRef InVertexBuffer, FLockedData& InLockedData )
{}

/**
 * Begin drawing viewport
 */
void FD3D11RHI::BeginDrawingViewport( class FBaseDeviceContextRHI* InDeviceContext, class FBaseViewportRHI* InViewport )
{
	check( InDeviceContext && InViewport );
	FD3D11DeviceContext*			deviceContext = ( FD3D11DeviceContext* )InDeviceContext;
	FD3D11Viewport*					viewport = ( FD3D11Viewport* )InViewport;

	ID3D11RenderTargetView*			d3d11RenderTargetView = viewport->GetBackBuffer()->GetD3D11RenderTargetView();
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

/**
 * Initialize ImGUI
 */
bool FD3D11RHI::InitImGUI( class FBaseDeviceContextRHI* InDeviceContext )
{
	check( d3d11Device && InDeviceContext );
	FD3D11DeviceContext*		deviceContext = ( FD3D11DeviceContext* )InDeviceContext;
	return ImGui_ImplDX11_Init( d3d11Device, deviceContext->GetD3D11DeviceContext() );
}

/**
 * Shutdown render of ImGUI
 */
void FD3D11RHI::ShutdownImGUI( class FBaseDeviceContextRHI* InDeviceContext )
{
	ImGui_ImplDX11_Shutdown();
}

/**
 * Begin drawing ImGUI
 */
void FD3D11RHI::BeginDrawingImGUI( class FBaseDeviceContextRHI* InDeviceContext )
{
	ImGui_ImplDX11_NewFrame();
}

/**
 * End drawing ImGUI
 */
void FD3D11RHI::EndDrawingImGUI( class FBaseDeviceContextRHI* InDeviceContext )
{
	ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData() );
}
#endif // WITH_EDITOR

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