#pragma once
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "CoreGlobals.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"

// Shared lifecycle of the opt-in runtime check suites (-ErlingTest, -ErlingTest_Latest, -ErlingUITest).
// Begin() deletes the stale report and arms a real-time watchdog (-ErlingTestTimeout=<s>, default 900 s).
// Finish() saves the report, logs ERLING_<SUITE>_CHECKS_COMPLETE and force-exits: 0 pass, 1 fail, 2 watchdog.
namespace ErlingTestRun
{
struct FState
{
	bool bStarted = false;
	double StartSeconds = 0.0;
	uint64 StartFrame = 0;
	double TimeoutSeconds = 900.0;
	FString ReportPath;
};

inline void Begin(FState& State, const TCHAR* Suite, const FString& ReportPath)
{
	if (State.bStarted)
		return;
	State.bStarted = true;
	State.StartSeconds = FPlatformTime::Seconds();
	State.StartFrame = GFrameCounter;
	State.ReportPath = ReportPath;
	FParse::Value(FCommandLine::Get(), TEXT("ErlingTestTimeout="), State.TimeoutSeconds);
	IFileManager::Get().Delete(*ReportPath, false, true, true);
	UE_LOG(LogTemp, Display, TEXT("ERLING_TEST_ENV: suite=%s fixed_step=%d fixed_dt=%.6f engine=%s"), Suite,
	    FApp::UseFixedTimeStep() ? 1 : 0, FApp::GetFixedDeltaTime(), *FEngineVersion::Current().ToString());
}

inline void Finish(const FState& State, const TCHAR* Marker, const FString& Report, bool bFailed, uint8 ExitCode)
{
	FFileHelper::SaveStringToFile(Report, *State.ReportPath);
	const double Seconds = FPlatformTime::Seconds() - State.StartSeconds;
	const uint64 Frames = GFrameCounter - State.StartFrame;
	UE_LOG(LogTemp, Display, TEXT("ERLING_TEST_FRAMES: frames=%llu seconds=%.3f avg_fps=%.2f"), Frames, Seconds,
	    Seconds > 0.0 ? double(Frames) / Seconds : 0.0);
	UE_LOG(LogTemp, Display, TEXT("%s: %s"), Marker, bFailed ? TEXT("FAIL") : TEXT("PASS"));
	if (GLog)
		GLog->Flush();
	// Force: on Windows a non-forced exit only posts WM_QUIT and the status code is lost.
	FPlatformMisc::RequestExitWithStatus(true, ExitCode);
}

inline void Finish(const FState& State, const TCHAR* Marker, const FString& Report, bool bFailed)
{
	Finish(State, Marker, Report, bFailed, bFailed ? 1 : 0);
}

// Call at the top of every suite tick, before any early return. True once the time limit has fired.
inline bool Watchdog(const FState& State, const TCHAR* Marker, FString& Report)
{
	if (!State.bStarted || FPlatformTime::Seconds() - State.StartSeconds < State.TimeoutSeconds)
		return false;
	Report += TEXT("watchdog_timeout: FAIL\n");
	UE_LOG(LogTemp, Error, TEXT("ERLING_TEST_WATCHDOG: suite did not finish within %.0f s"), State.TimeoutSeconds);
	Finish(State, Marker, Report, true, 2);
	return true;
}
} // namespace ErlingTestRun
#endif // !UE_BUILD_SHIPPING
