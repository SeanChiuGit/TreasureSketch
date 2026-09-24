#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TreasureOnlineSubsystem.generated.h"

UCLASS()
class TREASURESKETCH_API UTreasureOnlineSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void HostGame();
    void FindAndJoinGame();
    void JoinFirstFoundGame();
    FString GetStatus() const { return Status; }
    const TArray<FString>& GetRoomLines() const { return RoomLines; }
    bool HasJoinableRoom() const { return JoinableResultIndex != INDEX_NONE; }

private:
    IOnlineSessionPtr SessionInterface;
    TSharedPtr<FOnlineSessionSettings> SessionSettings;
    TSharedPtr<FOnlineSessionSearch> SessionSearch;
    FString Status = TEXT("按 H 创建 Steam 房间，或按 J 搜索并加入");
    TArray<FString> RoomLines;
    int32 JoinableResultIndex = INDEX_NONE;
    bool bCreateAfterDestroy = false;
    FDelegateHandle CreateHandle;
    FDelegateHandle DestroyHandle;
    FDelegateHandle FindHandle;
    FDelegateHandle JoinHandle;

    void CreateSession();
    void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
    void OnFindSessionsComplete(bool bWasSuccessful);
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
};
