#include "Core.h"
#include "System/ThreadingBase.h"
#include "WindowsThreading.h"
#include "Containers/StringConv.h"

/* Global factory for creating threads */
CThreadFactory*			GThreadFactory = new CThreadFactoryWindows();

/* Global factory for creating synchronization objects */
CSynchronizeFactory*	GSynchronizeFactory = new CSynchronizeFactoryWindows();

/**
 * Code setting the thread name for use in the debugger.
 *
 * http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
 */
#define MS_VC_EXCEPTION				0x406D1388

#pragma pack( push, 8 )
typedef struct tagTHREADNAME_INFO
{
	DWORD		dwType;			// Must be 0x1000.
	LPCSTR		szName;			// Pointer to name (in user addr space).
	DWORD		dwThreadID;		// Thread ID (-1=caller thread).
	DWORD		dwFlags;		// Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack( pop )

void SetThreadName( HANDLE InThreadHandle, const achar* InThreadName )
{
	THREADNAME_INFO			threadNameInfo;
	threadNameInfo.dwType		= 0x1000;
	threadNameInfo.szName		= InThreadName;
	threadNameInfo.dwThreadID	= ::GetThreadId( InThreadHandle );
	threadNameInfo.dwFlags		= 0;

	__try
	{
		RaiseException( MS_VC_EXCEPTION, 0, sizeof( threadNameInfo ) / sizeof( ULONG_PTR ), ( ULONG_PTR* ) &threadNameInfo ) ;
	}
	__except ( EXCEPTION_EXECUTE_HANDLER )
	{
	}
}

CRunnableThreadWindows::CRunnableThreadWindows() :
	threadId( 0 ),
	thread( nullptr ),
	runnable( nullptr ),
	threadInitSyncEvent( nullptr ),
	threadPriority( TP_Normal ),
	isAutoDeleteSelf( false ),
	isAutoDeleteRunnable( false )
{}

CRunnableThreadWindows::~CRunnableThreadWindows()
{
	if ( thread != nullptr )
	{
		Kill( true );
	}
}

void CRunnableThreadWindows::SetProcessorAffinity( uint32 InProcessorNum )
{}

void CRunnableThreadWindows::SetProcessorAffinityMask( uint32 InProcessorMask )
{}

void CRunnableThreadWindows::Suspend( bool InIsShouldPause /*= true*/ )
{
	check( thread );
	if ( InIsShouldPause )
	{
		SuspendThread( thread );
	}
	else
	{
		ResumeThread( thread );
	}
}

bool CRunnableThreadWindows::Kill( bool InIsShouldWait /*= false*/ )
{
	check( thread );
	bool		didExitOK = true;

	// Let the runnable have a chance to stop without brute force killing
	if ( runnable )
	{
		runnable->Stop();
	}

	// If waiting was specified, wait the amount of time. If that fails,
	// brute force kill that thread. Very bad as that might leak.
	if ( InIsShouldWait )
	{
		// Wait indefinitely for the thread to finish.  IMPORTANT:  It's not safe to just go and
		// kill the thread with TerminateThread() as it could have a mutex lock that's shared
		// with a thread that's continuing to run, which would cause that other thread to
		// dead-lock.  (This can manifest itself in code as simple as the synchronization
		// object that is used by our logging output classes.  Trust us, we've seen it!)
		WaitForSingleObject( thread, INFINITE );
	}

	// Now clean up the thread handle so we don't leak
	CloseHandle( thread );
	thread = nullptr;

	// delete the runnable if requested and we didn't shut down gracefully already.
	if ( runnable && isAutoDeleteRunnable )
	{
		delete runnable;
		runnable = nullptr;
	}

	// Delete ourselves if requested and we didn't shut down gracefully already.
	// This check prevents a double-delete of self when we shut down gracefully.
	if ( !didExitOK && isAutoDeleteSelf )
	{
		GThreadFactory->Destroy( this );
	}

	return didExitOK;
}

void CRunnableThreadWindows::WaitForCompletion()
{
	// Block until this thread exits
	WaitForSingleObject( thread, INFINITE );
}

uint32 CRunnableThreadWindows::GetThreadID() const
{
	return threadId;
}

bool CRunnableThreadWindows::Create( CRunnable* InRunnable, const tchar* InThreadName, bool InIsAutoDeleteSelf /*= false*/, bool InIsAutoDeleteRunnable /*= false*/, uint32 InStackSize /*= 0*/, EThreadPriority InThreadPriority /*= TP_Normal*/ )
{
	// Remember our inputs
	runnable = InRunnable;
	isAutoDeleteSelf = InIsAutoDeleteSelf;
	isAutoDeleteRunnable = InIsAutoDeleteRunnable;
	threadPriority = InThreadPriority;

	// Create a sync event to guarantee the Init() function is called first
	threadInitSyncEvent = GSynchronizeFactory->CreateSynchEvent( true );

	// Create the new thread
	thread = CreateThread( nullptr, InStackSize, &CRunnableThreadWindows::StaticMainProc, this, 0, ( LPDWORD ) &threadId );
	
	// If it fails, clear all the vars
	if ( !thread )
	{		
		if ( isAutoDeleteRunnable )
		{
			delete runnable;
		}

		runnable = nullptr;
	}
	else
	{
		// Let the thread start up, then set the name for debug purposes
		threadInitSyncEvent->Wait( INFINITE );
		SetThreadName( thread, InThreadName ? TCHAR_TO_ANSI( InThreadName ) : "Unnamed LE" );
	}

	// Cleanup the sync event
	GSynchronizeFactory->Destroy( threadInitSyncEvent );
	threadInitSyncEvent = nullptr;
	return thread != nullptr;
}

DWORD CRunnableThreadWindows::StaticMainProc( LPVOID InThis )
{
	CRunnableThreadWindows*		thisThread = ( CRunnableThreadWindows* )InThis;
	return thisThread->Run();
}

uint32 CRunnableThreadWindows::Run()
{
	check( runnable );
	appSetThreadPriority( thread, threadPriority );

	// Initialize the runnable object
	bool		initReturn = runnable->Init();
	check( initReturn );
	UNUSED_VAR( initReturn );

	// Initialization has completed, release the sync event
	threadInitSyncEvent->Trigger();

	// Now run the task that needs to be done
	uint32		exitCode = runnable->Run();

	// Allow any allocated resources to be cleaned up
	runnable->Exit();

	// Should we delete the runnable?
	if ( isAutoDeleteRunnable )
	{
		delete runnable;
		runnable = nullptr;
	}

	// Clean ourselves up without waiting
	if ( isAutoDeleteSelf )
	{
		// Now clean up the thread handle so we don't leak
		CloseHandle( thread );
		thread = nullptr;
		GThreadFactory->Destroy( this );
	}

	// Return from the thread with the exit code
	return exitCode;
}

CRunnableThread* CThreadFactoryWindows::CreateThread( CRunnable* InRunnable, const tchar* InThreadName, bool InIsAutoDeleteSelf /*= false*/, bool InIsAutoDeleteRunnable /*= false*/, uint32 InStackSize /*= 0*/, EThreadPriority InThreadPriority /*= TP_Normal*/ )
{
	CRunnableThreadWindows*		newThread = new CRunnableThreadWindows();
	
#if DO_CHECK
	check( newThread->Create( InRunnable, InThreadName, InIsAutoDeleteSelf, InIsAutoDeleteRunnable, InStackSize, InThreadPriority ) );
#else
	newThread->Create( InRunnable, InThreadName, InIsAutoDeleteSelf, InIsAutoDeleteRunnable, InStackSize, InThreadPriority );
#endif // DO_CHECK
	
	return newThread;
}

void CThreadFactoryWindows::Destroy( CRunnableThread* InThread )
{
	delete ( CRunnableThreadWindows* )InThread;
}

CEventWindows::CEventWindows() :
	event( nullptr )
{}

CEventWindows::~CEventWindows()
{
	if ( event )
	{
		CloseHandle( event );
	}
}

bool CEventWindows::Create( bool InIsManualReset /*= false*/, const tchar* InName /*= nullptr*/ )
{
	event = CreateEventW( nullptr, InIsManualReset, 0, InName );
	return event != nullptr;
}

void CEventWindows::Trigger()
{
	check( event );
	SetEvent( event );
}

void CEventWindows::Reset()
{
	check( event );
	ResetEvent( event );
}

void CEventWindows::Pulse()
{
	check( event );
	PulseEvent( event );
}

bool CEventWindows::Wait( uint32 InWaitTime /*= (uint32)-1*/ )
{
	check( event );
	return WaitForSingleObject( event, InWaitTime) == WAIT_OBJECT_0;
}

CSemaphoreWindows::CSemaphoreWindows() :
	semaphore( nullptr )
{}

CSemaphoreWindows::~CSemaphoreWindows()
{
	if ( semaphore )
	{
		CloseHandle( semaphore );
	}
}

bool CSemaphoreWindows::Create( uint32 InMaxCount, uint32 InInitialCount, const tchar* InName /*= nullptr*/ )
{
	semaphore = CreateSemaphoreW(
		nullptr,						// default security attributes
		InInitialCount,					// initial count
		InMaxCount,						// maximum count
		InName );						// named semaphore

	bool success = semaphore != nullptr;

#if !SHIPPING_BUILD
	if ( !success )
	{
		uint32			error = GetLastError();
		appErrorf( TEXT( "Failed in CSemaphoreWindows::Create() with ERROR CODE: %u" ), error );
	}
#endif

	return success;
}

bool CSemaphoreWindows::Signal()
{
	return Post( 1 );
}

bool CSemaphoreWindows::Post( uint32 InCount )
{
	check( semaphore );
	bool success = ReleaseSemaphore( semaphore, InCount, nullptr );

#if !SHIPPING_BUILD
	if ( !success )
	{
		uint32				error = GetLastError();
		appErrorf( TEXT( "Failed in CSemaphoreWindows::Post() with ERROR CODE: %u" ), error );
	}
#endif

	return success;
}

bool CSemaphoreWindows::TryWait()
{
	check( semaphore );
	uint32		ret = ( uint32 )WaitForSingleObject( semaphore, 0 );

	check( ret != WAIT_FAILED );
	return ret != WAIT_TIMEOUT;
}

void CSemaphoreWindows::Wait()
{
	check( semaphore );
	
	uint32		ret = ( uint32 )WaitForSingleObject( semaphore, INFINITE );
	check( ret == WAIT_OBJECT_0 );
	UNUSED_VAR( ret );
}

bool CSemaphoreWindows::WaitTimeoutMs( uint32 InMilliseconds )
{
	check( semaphore );
	uint32		ret = ( uint32 )WaitForSingleObject( semaphore, InMilliseconds );

	check( ret != WAIT_FAILED );
	return ret != WAIT_TIMEOUT;
}

CCriticalSection* CSynchronizeFactoryWindows::CreateCriticalSection()
{
	return new CCriticalSection();
}

CEvent* CSynchronizeFactoryWindows::CreateSynchEvent( bool InIsManualReset /*= false*/, const tchar* InName /*= nullptr*/ )
{
	// Allocate the new object
	CEvent*			newEvent = new CEventWindows();

	// If the internal create fails, delete the instance and return nullptr
	if ( !newEvent->Create( InIsManualReset, InName ) )
	{
		delete newEvent;
		newEvent = nullptr;
	}

	return newEvent;
}

CSemaphore* CSynchronizeFactoryWindows::CreateSemaphore( uint32 InMaxCount, uint32 InInitialCount, const tchar* InName /*= nullptr*/ )
{
	// Allocate the new object
	CSemaphore*			newSemaphore = new CSemaphoreWindows();

	// If the internal create fails, delete the instance and return NULL
	if ( !newSemaphore->Create( InMaxCount, InInitialCount, InName ) )
	{
		delete newSemaphore;
		newSemaphore = nullptr;
	}

	return newSemaphore;
}

void CSynchronizeFactoryWindows::Destroy( CSynchronize* InSynchObj )
{
	delete InSynchObj;
}