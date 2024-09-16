#pragma once
//-----------------------------------------------------------------------------------------------
// ErrorWarningAssert.hpp
//
// Summary of error and assertion macros:
//	#define ERROR_AND_DIE( errorText )						// "MUST not reach this point"; Show error dialogue, then shut down the app
//	#define ERROR_RECOVERABLE( errorText )					// "SHOULD not reach this point"; Show warning dialogue, then proceed
//	#define GUARANTEE_OR_DIE( condition, errorText )		// "MUST be true"; If condition is false, show error dialogue then shut down the app
//	#define GUARANTEE_RECOVERABLE( condition, errorText )	// "SHOULD be true"; If condition is false, show warning dialogue then proceed
//	#define ASSERT_OR_DIE( condition, errorText )			// Same as GUARANTEE_OR_DIE, but removed if DISABLE_ASSERTS is #defined
//	#define ASSERT_RECOVERABLE( condition, errorText )		// Same as GUARANTEE_RECOVERABLE, but removed if DISABLE_ASSERTS is #defined
// 


//-----------------------------------------------------------------------------------------------
#include <string>


//-----------------------------------------------------------------------------------------------
enum class MsgSeverityLevel
{
	INFORMATION,
	QUESTION,
	WARNING,
	FATAL
};


//-----------------------------------------------------------------------------------------------
void DebuggerPrintf( char const* messageFormat, ... );
bool IsDebuggerAvailable();
__declspec( noreturn ) void FatalError( char const* filePath, char const* functionName, int lineNum, std::string const& reasonForError, char const* conditionText=nullptr );
void RecoverableWarning( char const* filePath, char const* functionName, int lineNum, std::string const& reasonForWarning, char const* conditionText=nullptr );
void SystemDialogue_Okay( std::string const& messageTitle, std::string const& messageText, MsgSeverityLevel severity );
bool SystemDialogue_YesNo( std::string const& messageTitle, std::string const& messageText, MsgSeverityLevel severity );
bool SystemDialogue_OkayCancel( std::string const& messageTitle, std::string const& messageText, MsgSeverityLevel severity );
int SystemDialogue_YesNoCancel( std::string const& messageTitle, std::string const& messageText, MsgSeverityLevel severity );



//-----------------------------------------------------------------------------------------------
// ERROR_AND_DIE
//
// Present in all builds.
// No condition; always triggers if reached.
// Depending on the platform, this typically:
//	- Logs an error message to the console and/or log file
//	- Opens an error/message dialogue box
//	- Triggers a debug breakpoint (if appropriate development suite is present)
//	- Shuts down the app
//
// Use this when reaching a certain line of code should never happen under any circumstances,
// and continued execution is dangerous or impossible.
//
#define ERROR_AND_DIE( errorMessageText )															\
{																									\
	FatalError( __FILE__,  __FUNCTION__, __LINE__, errorMessageText );								\
}


//-----------------------------------------------------------------------------------------------
// ERROR_RECOVERABLE
//
// Present in all builds.
// No condition; always triggers if reached.
// Depending on the platform, this typically:
//	- Logs a warning message to the console and/or log file
//	- Opens an warning/message dialogue box
//	- Triggers a debug breakpoint (if appropriate development suite is present)
//	- Continues execution
//
#define ERROR_RECOVERABLE( errorMessageText )														\
{																									\
	RecoverableWarning( __FILE__,  __FUNCTION__, __LINE__, errorMessageText );						\
}


//-----------------------------------------------------------------------------------------------
// GUARANTEE_OR_DIE
//
// Present in all builds.
// Triggers if condition is false.
// Depending on the platform, this typically:
//	- Logs an error message to the console and/or log file
//	- Opens an error/message dialogue box
//	- Triggers a debug breakpoint (if appropriate development suite is present)
//	- Shuts down the app
//
#define GUARANTEE_OR_DIE( condition, errorMessageText )												\
{																									\
	if( !(condition) )																				\
	{																								\
		char const* conditionText = #condition;														\
		FatalError( __FILE__,  __FUNCTION__, __LINE__, errorMessageText, conditionText );			\
	}																								\
}


//-----------------------------------------------------------------------------------------------
// GUARANTEE_RECOVERABLE
//
// Present in all builds.
// Triggers if condition is false.
// Depending on the platform, this typically:
//	- Logs a warning message to the console and/or log file
//	- Opens an warning/message dialogue box
//	- Triggers a debug breakpoint (if appropriate development suite is present)
//	- Continues execution
//
#define GUARANTEE_RECOVERABLE( condition, errorMessageText )										\
{																									\
	if( !(condition) )																				\
	{																								\
		char const* conditionText = #condition;														\
		RecoverableWarning( __FILE__,  __FUNCTION__, __LINE__, errorMessageText, conditionText );	\
	}																								\
}


//-----------------------------------------------------------------------------------------------
// ASSERT_OR_DIE
//
// Removed if DISABLE_ASSERTS is defined, typically in a Final build configuration.
// Triggers if condition is false.
// Depending on the platform, this typically:
//	- Logs an error message to the console and/or log file
//	- Opens an error/message dialogue box
//	- Triggers a debug breakpoint (if appropriate development suite is present)
//	- Shuts down the app
//
#if defined( DISABLE_ASSERTS )
#define ASSERT_OR_DIE( condition, errorMessageText ) { (void)( condition ); }
#else
#define ASSERT_OR_DIE( condition, errorMessageText )												\
{																									\
	if( !(condition) )																				\
	{																								\
		char const* conditionText = #condition;														\
		FatalError( __FILE__,  __FUNCTION__, __LINE__, errorMessageText, conditionText );			\
	}																								\
}
#endif


//-----------------------------------------------------------------------------------------------
// ASSERT_RECOVERABLE
//
// Removed if DISABLE_ASSERTS is defined, typically in a Final build configuration.
// Triggers if condition is false.
// Depending on the platform, this typically:
//	- Logs a warning message to the console and/or log file
//	- Opens an warning/message dialogue box
//	- Triggers a debug breakpoint (if appropriate development suite is present)
//	- Continues execution
//
#if defined( DISABLE_ASSERTS )
#define ASSERT_RECOVERABLE( condition, errorMessageText ) { (void)( condition ); }
#else
#define ASSERT_RECOVERABLE( condition, errorMessageText )											\
{																									\
	if( !(condition) )																				\
	{																								\
		char const* conditionText = #condition;														\
		RecoverableWarning( __FILE__,  __FUNCTION__, __LINE__, errorMessageText, conditionText );	\
	}																								\
}
#endif


