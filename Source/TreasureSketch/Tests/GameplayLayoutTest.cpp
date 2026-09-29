#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../TreasureGameplayLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayLayoutTest, "TreasureSketch.Canyon.TwentySeeds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameplayLayoutTest::RunTest(const FString& Parameters)
{
    TSet<uint32> Signatures;
    TSet<uint8> Topologies;
    TSet<uint8> Relationships;
    TSet<uint8> LandmarkTypes;
    for (int32 Seed = 1000; Seed < 1020; ++Seed)
    {
        const FTreasureGameplayLayout Layout = FTreasureGameplayLayoutGenerator::GenerateCanyon(Seed);
        FString Reason;
        const bool bValid = Layout.ValidateGraph(&Reason);
        TestTrue(*FString::Printf(TEXT("Seed %d satisfies gameplay constraints: %s"), Seed, *Reason),
            bValid);
        TestEqual(*FString::Printf(TEXT("Seed %d reproduces layout"), Seed),
            FTreasureGameplayLayoutGenerator::GenerateCanyon(Seed).GetSignature(), Layout.GetSignature());
        Signatures.Add(Layout.GetSignature());
        Topologies.Add(static_cast<uint8>(Layout.Topology));
        Relationships.Add(static_cast<uint8>(Layout.TreasureRelationship));
        for (const FTreasureLayoutNode& Node : Layout.Nodes)
            if (Node.Landmark != ETreasureLandmark::None) LandmarkTypes.Add(static_cast<uint8>(Node.Landmark));
        TestTrue(*FString::Printf(TEXT("Seed %d has an early landmark"), Seed), Layout.GetLandmarkCount() >= 3);
    }
    TestEqual(TEXT("All seven macro layouts occur in twenty seeds"), Topologies.Num(), 7);
    TestEqual(TEXT("All four treasure relationships occur in twenty seeds"), Relationships.Num(), 4);
    TestTrue(TEXT("At least six major landmark silhouettes occur"), LandmarkTypes.Num() >= 6);
    TestTrue(TEXT("Twenty seeds yield at least sixteen distinct structural maps"), Signatures.Num() >= 16);
    for (int32 Seed = 0; Seed < 256; ++Seed)
    {
        FString Reason;
        const bool bValid = FTreasureGameplayLayoutGenerator::GenerateCanyon(Seed).ValidateGraph(&Reason);
        TestTrue(*FString::Printf(TEXT("Seed %d validates: %s"), Seed, *Reason), bValid);
    }
    return true;
}

#endif
