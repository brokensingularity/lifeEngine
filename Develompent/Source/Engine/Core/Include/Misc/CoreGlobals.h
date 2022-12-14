/**
 * @file
 * @addtogroup Core Core
 *
 * Copyright BSOD-Games, All Rights Reserved.
 * Authors: Yehor Pohuliaka (zombiHello)
 */

#ifndef COREGLOBALS_H
#define COREGLOBALS_H

#include <string>

/**
 * @ingroup Core
 * @brief Logger
 */
extern class CBaseLogger*			GLog;

/**
 * @ingroup Core
 * @brief File system
 */
extern class CBaseFileSystem*		GFileSystem;

/**
 * @ingroup Core
 * @brief Window
 */
extern class CBaseWindow*			GWindow;

/**
 * @ingroup Core
 * @brief Config manager
 */
extern class CConfigManager			GConfig;

/**
 * @ingroup Core
 * @brief Indicates that MainLoop() should be exited at the end of the current iteration
 */
extern bool							GIsRequestingExit;

#if WITH_EDITOR
/**
 * @ingroup Core
 * @brief Whether engine was launched for game
 */
extern bool							GIsGame;

/**
 * @ingroup Core
 * Whether engine was launched for editing
 */
extern bool							GIsEditor;

/**
 * @ingroup Core
 * Whether engine was launched for commandlet
 */
extern bool							GIsCommandlet;

/**
 * @ingroup Core
 * Whether engine was launched for cooked packages
 */
extern bool							GIsCooker;

/**
 * @ingroup Core
 * Whether we should pause before exiting. Used by commandlets
 */
extern bool							GShouldPauseBeforeExit;
#else
#define								GIsGame 1
#define								GIsEditor 0
#define								GIsCommandlet 0
#define								GIsCooker 0
#endif // WITH_EDITOR

/**
 * @ingroup Core
 * Seconds per CPU cycle for this PC 
 */
extern double						GSecondsPerCycle;

/**
* @ingroup Core
* Time at which appSeconds() was first initialized (very early on)
*/
extern double						GStartTime;

/**
 * @ingroup Core
 * Current delta time in seconds
 */
extern double						GDeltaTime;

/**
 * @ingroup Core
 * Current time in seconds
 */
extern double						GCurrentTime;

/**
 * @ingroup Core
 * Last time in seconds
 */
extern double						GLastTime;

/**
 * @ingroup Core
 * Package manager
 */
extern class CPackageManager*		GPackageManager;

/**
 * @ingroup Core
 * Table of contents
 */
extern class CTableOfContets		GTableOfContents;

/**
 * @ingroup Core
 * Game name
 */
extern std::wstring					GGameName;

/**
 * @ingroup Core
 * Platform type
 */
extern enum EPlatformType			GPlatform;

/**
 * @ingroup Core
 * @brief Command line
 */
extern class CCommandLine			GCommandLine;

/**
 * @ingroup Core
 * @brief Asset factory
 */
extern class CAssetFactory			GAssetFactory;

#endif // !COREGLOBALS_H
