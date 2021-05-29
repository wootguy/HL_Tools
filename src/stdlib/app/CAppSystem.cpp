#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING

#include <cassert>

#include <experimental/filesystem>

#include "core/shared/Logging.h"

#include "core/shared/Platform.h"

#include "utility/PlatUtils.h"

#include "CAppSystem.h"

namespace app
{
bool CAppSystem::Run( int iArgc, wchar_t* pszArgV[] )
{
	bool bResult = Start();

	if( bResult )
	{
		RunApp( iArgc, pszArgV );
	}

	OnShutdown();

	return bResult;
}

bool CAppSystem::Start()
{
	bool bResult = Startup();

	if( bResult )
	{
		m_State = AppState::RUNNING;
	}

	return bResult;
}

void CAppSystem::OnShutdown()
{
	m_State = AppState::SHUTTING_DOWN;

	ShutdownApp();

	Shutdown();

	m_State = AppState::SHUT_DOWN;
}

bool CAppSystem::Startup()
{
	//Configure the working directory to be the exe directory.
	{
		bool bSuccess;
		auto szExePath = plat::GetExeFileName( &bSuccess );

		if( !bSuccess )
		{
			Error( "CAppSystem::Startup: Failed to get executable filename!\n" );
			return false;
		}

		std::experimental::filesystem::path exePath( szExePath );

		std::error_code canonicalError;

		auto exeDir = exePath.parent_path();

		exeDir = std::experimental::filesystem::canonical( exeDir, canonicalError );

		if( canonicalError )
		{
			Error(
				"CAppSystem::Startup: Failed to canonicalize  \"%s\" with error \"%s\"!\n",
				exeDir.string().c_str(), canonicalError.message().c_str() );
			return false;
		}

		std::error_code cwdError;

		std::experimental::filesystem::current_path( exeDir, cwdError );

		if( cwdError )
		{
			Error( 
				"CAppSystem::Startup: Failed to set current working directory \"%s\" with error \"%s\"!\n", 
				exeDir.string().c_str(), cwdError.message().c_str() );
			return false;
		}
	}

	m_State = AppState::STARTING_UP;

	if( !StartupApp() )
	{
		Error( "CAppSystem::Startup: Failed to start up app!\n" );
		return false;
	}

	m_State = AppState::LOADING_LIBS;

	if( !LoadAppLibraries() )
	{
		Error( "CAppSystem::Startup: Failed to load one or more libraries!\n" );
		return false;
	}

	m_State = AppState::LOADING_IFACES;

	//Create the list of functions for ease of use.
	std::vector<CreateInterfaceFn> factories;

	//Include this library's factory.
	factories.reserve( m_Libraries.size() + 1 );

	factories.push_back( &::CreateInterface );

	for( auto& lib : m_Libraries )
	{
		if( auto pFunc = lib.GetFunctionAddress( CREATEINTERFACE_NAME ) )
		{
			factories.push_back( reinterpret_cast<CreateInterfaceFn>( pFunc ) );
		}
	}

	factories.shrink_to_fit();

	if( !Connect( factories.data(), factories.size() ) )
	{
		Error( "CAppSystem::Startup: Failed to connect one or more interfaces!\n" );
		return false;
	}

	m_State = AppState::INITIALIZING;

	if( !Initialize() )
	{
		Error( "CAppSystem::Startup: Failed to initialize app!\n" );
		return false;
	}

	return true;
}

void CAppSystem::Shutdown()
{
	for( auto& lib : m_Libraries )
	{
		lib.Free();
	}

	m_Libraries.clear();
	m_Libraries.shrink_to_fit();
}

const CLibrary* CAppSystem::GetLibraryByName( const char* const pszName ) const
{
	assert( pszName );

	for( auto& lib : m_Libraries )
	{
		if( lib.GetName() == pszName )
			return &lib;
	}

	return nullptr;
}

bool CAppSystem::IsLibraryLoaded( const char* const pszName ) const
{
	return GetLibraryByName( pszName ) != nullptr;
}

bool CAppSystem::LoadLibrary( const CLibArgs& args )
{
	if( IsLibraryLoaded( args.GetFilename() ) )
		return true;

	CLibrary lib;

	if( !lib.Load( args ) )
	{
		FatalError( "CAppSystem::LoadLibrary: Failed to load library \"%s\"\nThe error given was \"%s\"\n", args.GetFilename(), CLibrary::GetLoadErrorDescription() );
		return false;
	}

	m_Libraries.emplace_back( std::move( lib ) );

	return true;
}
}