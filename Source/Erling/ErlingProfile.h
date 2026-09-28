#pragma once
#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// Test runs use their own save slot so they never touch the player's profile.
inline const TCHAR* ProfileSlot()
{
	return (FParse::Param(FCommandLine::Get(), TEXT("ErlingTest")) || FParse::Param(FCommandLine::Get(), TEXT("ErlingTest_Latest")) ||
	           FParse::Param(FCommandLine::Get(), TEXT("ErlingUITest")))
	           ? TEXT("ErlingProfile_Test")
	           : TEXT("ErlingProfile");
}
