#include "JoltSetup.h"
#include <Jolt/Jolt.h>
JPH_SUPPRESS_WARNINGS
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <iostream>
#include <cstdarg>
using namespace JPH;
using namespace std;

// Callback for traces, connect this to your own trace function if you have one
static void TraceImpl(const char* inFMT, ...)
{
	// Format the message
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);

	// Print to the TTY
	cout << buffer << endl;
}

#ifdef JPH_ENABLE_ASSERTS

// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint inLine)
{
	// Print to the TTY
	cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr ? inMessage : "") << endl;

	// Breakpoint
	return true;
};

#endif // JPH_ENABLE_ASSERTS

void InitJolt()
{
	RegisterDefaultAllocator();
	Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)
		Factory::sInstance = new Factory();
	RegisterTypes();
}
void ShutdownJolt()
{
	// Unregisters all types with the factory and cleans up the default material
	UnregisterTypes();
	// Destroy the factory
	delete Factory::sInstance;
	Factory::sInstance = nullptr;
}