#include "TreasureOnlineSubsystem.h"

#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"

void UTreasureOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get())
    {
        SessionInterface = OnlineSubsystem->GetSessionInterface();
        Status = FString::Printf(TEXT("在线服务：%s | H 创建，J 加入"), *OnlineSubsystem->GetSubsystemName().ToString());
        UE_LOG(LogTemp, Display, TEXT("TREASURE_ONLINE_SERVICE %s"), *OnlineSubsystem->GetSubsystemName().ToString());
    }
    else
    {
        Status = TEXT("在线服务未启动。请确认 Steam 正在运行。");
        UE_LOG(LogTemp, Error, TEXT("TREASURE_ONLINE_SERVICE unavailable"));
    }
}

void UTreasureOnlineSubsystem::Deinitialize()
{
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
    }
    Super::Deinitialize();
}

void UTreasureOnlineSubsystem::HostGame()
{
    if (!SessionInterface.IsValid()) { Status = TEXT("无法创建：Steam 在线服务不可用"); return; }
    if (SessionInterface->GetNamedSession(NAME_GameSession))
    {
        Status = TEXT("正在清理旧房间……");
        bCreateAfterDestroy = true;
        DestroyHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
            FOnDestroySessionCompleteDelegate::CreateUObject(this, &UTreasureOnlineSubsystem::OnDestroySessionComplete));
        SessionInterface->DestroySession(NAME_GameSession);
        return;
    }
    CreateSession();
}

void UTreasureOnlineSubsystem::CreateSession()
{
    SessionSettings = MakeShared<FOnlineSessionSettings>();
    SessionSettings->bIsLANMatch = false;
    SessionSettings->NumPublicConnections = 2;
    SessionSettings->bShouldAdvertise = true;
    SessionSettings->bAllowJoinInProgress = true;
    SessionSettings->bAllowJoinViaPresence = true;
    SessionSettings->bUsesPresence = true;
    SessionSettings->bUseLobbiesIfAvailable = true;
    SessionSettings->Set(FName(TEXT("TREASURE_ROOM")), FString(TEXT("TreasureSketch")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    CreateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateUObject(this, &UTreasureOnlineSubsystem::OnCreateSessionComplete));
    Status = TEXT("正在创建 Steam 公网房间……");
    if (!SessionInterface->CreateSession(0, NAME_GameSession, *SessionSettings))
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
        Status = TEXT("创建请求未能启动");
    }
}

void UTreasureOnlineSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
    if (!bWasSuccessful)
    {
        Status = TEXT("Steam 房间创建失败");
        UE_LOG(LogTemp, Error, TEXT("TREASURE_ONLINE_HOST failed"));
        return;
    }
    Status = TEXT("房间已创建，等待另一名玩家加入……");
    UE_LOG(LogTemp, Display, TEXT("TREASURE_ONLINE_HOST created"));
    if (UWorld* World = GetWorld()) World->ServerTravel(TEXT("/Game/Maps/L_TreasureSketchDemo?listen"));
}

void UTreasureOnlineSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
    SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
    if (bCreateAfterDestroy) { bCreateAfterDestroy = false; CreateSession(); }
}

void UTreasureOnlineSubsystem::FindAndJoinGame()
{
    if (!SessionInterface.IsValid()) { Status = TEXT("无法搜索：Steam 在线服务不可用"); return; }
    SessionSearch = MakeShared<FOnlineSessionSearch>();
    RoomLines.Reset();
    JoinableResultIndex = INDEX_NONE;
    SessionSearch->bIsLanQuery = false;
    SessionSearch->MaxSearchResults = 500;
    SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
    SessionSearch->QuerySettings.Set(FName(TEXT("TREASURE_ROOM")), FString(TEXT("TreasureSketch")), EOnlineComparisonOp::Equals);
    FindHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &UTreasureOnlineSubsystem::OnFindSessionsComplete));
    Status = TEXT("正在搜索 Steam 房间……");
    if (!SessionInterface->FindSessions(0, SessionSearch.ToSharedRef()))
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
        Status = TEXT("搜索请求未能启动");
    }
}

void UTreasureOnlineSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
    SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
    if (!bWasSuccessful || !SessionSearch.IsValid())
    {
        Status = TEXT("Steam Lobby 搜索失败，请确认 Steam 在线后再按 J");
        UE_LOG(LogTemp, Error, TEXT("TREASURE_ONLINE_FIND failed"));
        return;
    }
    UE_LOG(LogTemp, Display, TEXT("TREASURE_ONLINE_FIND completed results=%d"), SessionSearch->SearchResults.Num());
    int32 ResultIndex = 0;
    for (const FOnlineSessionSearchResult& Result : SessionSearch->SearchResults)
    {
        FString RoomTag;
        if (Result.Session.SessionSettings.Get(FName(TEXT("TREASURE_ROOM")), RoomTag) && RoomTag == TEXT("TreasureSketch"))
        {
            if (JoinableResultIndex == INDEX_NONE) JoinableResultIndex = ResultIndex;
            const int32 CurrentPlayers = Result.Session.SessionSettings.NumPublicConnections - Result.Session.NumOpenPublicConnections;
            RoomLines.Add(FString::Printf(TEXT("房主：%s  |  玩家：%d/%d  |  延迟：%d ms"),
                *Result.Session.OwningUserName, CurrentPlayers, Result.Session.SessionSettings.NumPublicConnections, Result.PingInMs));
        }
        ++ResultIndex;
    }
    Status = RoomLines.IsEmpty()
        ? FString::Printf(TEXT("没有找到房间（结果 %d）。请让主机先按 H，看到等待提示后再按 J"), SessionSearch->SearchResults.Num())
        : FString::Printf(TEXT("找到 %d 个 TreasureSketch 房间；按 K 加入第一个房间"), RoomLines.Num());
}

void UTreasureOnlineSubsystem::JoinFirstFoundGame()
{
    if (!SessionInterface.IsValid() || !SessionSearch.IsValid() ||
        !SessionSearch->SearchResults.IsValidIndex(JoinableResultIndex))
    {
        Status = TEXT("没有可加入的房间，请先按 J 刷新列表");
        return;
    }
    JoinHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateUObject(this, &UTreasureOnlineSubsystem::OnJoinSessionComplete));
    Status = TEXT("正在加入列表中的第一个房间……");
    if (!SessionInterface->JoinSession(0, NAME_GameSession, SessionSearch->SearchResults[JoinableResultIndex]))
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
        Status = TEXT("加入请求未能启动，请重新按 J 搜索");
    }
}

void UTreasureOnlineSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
    FString ConnectString;
    if (Result != EOnJoinSessionCompleteResult::Success || !SessionInterface->GetResolvedConnectString(SessionName, ConnectString))
    {
        Status = TEXT("加入房间失败");
        UE_LOG(LogTemp, Error, TEXT("TREASURE_ONLINE_JOIN failed result=%d"), static_cast<int32>(Result));
        return;
    }
    Status = TEXT("已找到主机，正在进入游戏……");
    UE_LOG(LogTemp, Display, TEXT("TREASURE_ONLINE_JOIN travel=%s"), *ConnectString);
    if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
        PC->ClientTravel(ConnectString, TRAVEL_Absolute);
}
