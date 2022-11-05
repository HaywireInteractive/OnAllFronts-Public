#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/AutomationTest.h"


#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMassMoveToCommandProcessorTest, "ProjectM.MassMoveToCommandProcessor", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)


bool FMassMoveToCommandProcessorTest::RunTest(const FString& Parameters)
{
	auto GetOffset = [](const float ForwardX, const float ForwardY) -> FVector {
		return FVector(UMassMoveToCommandProcessor::GetSoldierOffsetFromSquadLeaderUnscaledMeters(6, FVector(ForwardX, ForwardY, 0.f)), 0.f);
	};
	auto V3D = [](const float X, const float Y) -> FVector {
		return FVector(X, Y, 0.f);
	};
	{
		// All tests here assume (X,Y) of (0,1) is 0 degrees.
		TestEqual(TEXT("Rotate right 90 deg must create correct value"), GetOffset(1.f, 0.f), V3D(-30.f, 10.f));
		TestEqual(TEXT("Rotate left 90 deg must create correct value"), GetOffset(-1.f, 0.f), V3D(30.f, -10.f));
		TestEqual(TEXT("Rotate right 45 deg must create correct value"), GetOffset(1.f, 1.f), V3D(-28.28427f, -14.14214f));
		TestEqual(TEXT("Rotate 0 deg must create correct value"), GetOffset(0.f, 1.f), V3D(-10.f, -30.f));
		TestEqual(TEXT("Rotate 180 deg must create correct value"), GetOffset(0.f, -1.f), V3D(10.f, 30.f));
		TestEqual(TEXT("Rotate right 135 deg must create correct value"), GetOffset(1.f, -1.f), V3D(-14.14214f, 28.28427f));
	}

	return true;
}


#endif //WITH_DEV_AUTOMATION_TESTS
