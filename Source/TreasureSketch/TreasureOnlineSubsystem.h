#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineExternalUIInterface.h"
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
    void OpenSteamInviteUI();
    FString GetStatus() const { return Status; }
    const TArray<FString>& GetRoomLines() const { return RoomLines; }
    const TArray<FString>& GetDiagnostics() const { return Diagnostics; }
    bool HasJoinableRoom() const { return JoinableResultIndex != INDEX_NONE; }

private:
    IOnlineSessionPtr SessionInterface;
    IOnlineExternalUIPtr ExternalUIInterface;
    TSharedPtr<FOnlineSessionSettings> SessionSettings;
    TSharedPtr<FOnlineSessionSearch> SessionSearch;
    FString Status = TEXT("按 H 创建 Steam 房间，或按 J 搜索并加入");
    TArray<FString> RoomLines;
    TArray<FString> Diagnostics;
    int32 JoinableResultIndex = INDEX_NONE;
    bool bCreateAfterDestroy = false;
    FDelegateHandle CreateHandle;
    FDelegateHandle DestroyHandle;
    FDelegateHandle FindHandle;
    FDelegateHandle JoinHandle;
    FDelegateHandle InviteAcceptedHandle;
    FDelegateHandle NetworkFailureHandle;
    FDelegateHandle TravelFailureHandle;

    void CreateSession();
    void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
    void OnFindSessionsComplete(bool bWasSuccessful);
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void OnSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
    void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
    void OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
    void JoinSearchResult(const FOnlineSessionSearchResult& SearchResult, const FString& Source);
    void AddDiagnostic(const FString& Message);
};
