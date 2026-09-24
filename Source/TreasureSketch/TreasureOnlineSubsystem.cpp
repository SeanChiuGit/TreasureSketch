#include "TreasureOnlineSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"

void UTreasureOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get())
    {
        SessionInterface = OnlineSubsystem->GetSessionInterface();
        ExternalUIInterface = OnlineSubsystem->GetExternalUIInterface();
        Status = FString::Printf(TEXT("在线服务：%s | H 创建，I 邀请，J 搜索"), *OnlineSubsystem->GetSubsystemName().ToString());
        FString UserLabel = TEXT("未识别 Steam 用户");
        if (IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface())
        {
            const TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(0);
            UserLabel = FString::Printf(TEXT("用户：%s | Steam ID：%s"),
                *Identity->GetPlayerNickname(0), UserId.IsValid() ? *UserId->ToString() : TEXT("无"));
        }
        AddDiagnostic(UserLabel);
        AddDiagnostic(FString::Printf(TEXT("在线服务：%s | 邀请界面：%s"),
            *OnlineSubsystem->GetSubsystemName().ToString(), ExternalUIInterface.IsValid() ? TEXT("可用") : TEXT("不可用")));
        if (SessionInterface.IsValid())
        {
            InviteAcceptedHandle = SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(
                FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UTreasureOnlineSubsystem::OnSessionUserInviteAccepted));
        }
        if (GEngine)
        {
            NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UTreasureOnlineSubsystem::OnNetworkFailure);
            TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UTreasureOnlineSubsystem::OnTravelFailure);
        }
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
        SessionInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
    }
    if (GEngine)
    {
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
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
    AddDiagnostic(TEXT("CreateSession：请求创建 2 人 Steam Lobby"));
    if (!SessionInterface->CreateSession(0, NAME_GameSession, *SessionSettings))
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
        Status = TEXT("创建请求未能启动");
        AddDiagnostic(TEXT("CreateSession：请求未启动"));
    }
}

void UTreasureOnlineSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
    if (!bWasSuccessful)
    {
        Status = TEXT("Steam 房间创建失败");
        AddDiagnostic(TEXT("CreateSession：回调失败"));
        UE_LOG(LogTemp, Error, TEXT("TREASURE_ONLINE_HOST failed"));
        return;
    }
    FString SessionId = TEXT("未知");
    if (const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName))
        if (Session->SessionInfo.IsValid()) SessionId = Session->SessionInfo->GetSessionId().ToString();
    Status = TEXT("房间已创建；按 I 邀请 Steam 好友，或等待公共搜索");
    AddDiagnostic(FString::Printf(TEXT("CreateSession：成功 | Lobby ID：%s"), *SessionId));
    UE_LOG(LogTemp, Display, TEXT("TREASURE_ONLINE_HOST created lobby=%s"), *SessionId);
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
    FindHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &UTreasureOnlineSubsystem::OnFindSessionsComplete));
    Status = TEXT("正在搜索 Steam 房间……");
    AddDiagnostic(TEXT("FindSessions：开始广泛搜索 Lobby，再在本机匹配 TreasureSketch 标签"));
    if (!SessionInterface->FindSessions(0, SessionSearch.ToSharedRef()))
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
        Status = TEXT("搜索请求未能启动");
        AddDiagnostic(TEXT("FindSessions：请求未启动"));
    }
}

void UTreasureOnlineSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
    SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
    if (!bWasSuccessful || !SessionSearch.IsValid())
    {
        Status = TEXT("Steam Lobby 搜索失败，请确认 Steam 在线后再按 J");
        AddDiagnostic(TEXT("FindSessions：异步回调失败"));
        UE_LOG(LogTemp, Error, TEXT("TREASURE_ONLINE_FIND failed"));
        return;
    }
    UE_LOG(LogTemp, Display, TEXT("TREASURE_ONLINE_FIND completed results=%d"), SessionSearch->SearchResults.Num());
    AddDiagnostic(FString::Printf(TEXT("FindSessions：Steam 返回 %d 个 Lobby"), SessionSearch->SearchResults.Num()));
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
        ? FString::Printf(TEXT("Steam 返回 %d 个 Lobby，但 TreasureSketch 匹配为 0；请改用好友邀请"), SessionSearch->SearchResults.Num())
        : FString::Printf(TEXT("找到 %d 个 TreasureSketch 房间；按 K 加入第一个房间"), RoomLines.Num());
    AddDiagnostic(FString::Printf(TEXT("标签匹配：%d 个 TreasureSketch 房间"), RoomLines.Num()));
}

void UTreasureOnlineSubsystem::JoinFirstFoundGame()
{
    if (!SessionInterface.IsValid() || !SessionSearch.IsValid() ||
        !SessionSearch->SearchResults.IsValidIndex(JoinableResultIndex))
    {
        Status = TEXT("没有可加入的房间，请先按 J 刷新列表");
        return;
    }
    JoinSearchResult(SessionSearch->SearchResults[JoinableResultIndex], TEXT("公共房间列表"));
}

void UTreasureOnlineSubsystem::OpenSteamInviteUI()
{
    if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(NAME_GameSession))
    {
        Status = TEXT("请先按 H 创建房间，再按 I 邀请好友");
        AddDiagnostic(TEXT("ShowInviteUI：没有已创建的 Session"));
        return;
    }
    if (!ExternalUIInterface.IsValid())
    {
        Status = TEXT("Steam 邀请界面不可用，请确认 Steam Overlay 已启用");
        AddDiagnostic(TEXT("ShowInviteUI：External UI 无效"));
        return;
    }
    const bool bOpened = ExternalUIInterface->ShowInviteUI(0, NAME_GameSession);
    Status = bOpened ? TEXT("Steam 好友邀请界面已请求打开") : TEXT("无法打开 Steam 好友邀请界面");
    AddDiagnostic(FString::Printf(TEXT("ShowInviteUI：%s"), bOpened ? TEXT("调用成功") : TEXT("调用失败")));
}

void UTreasureOnlineSubsystem::OnSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId,
    FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
    AddDiagnostic(FString::Printf(TEXT("好友邀请：已接受 | 成功=%s | Controller=%d | Session有效=%s"),
        bWasSuccessful ? TEXT("是") : TEXT("否"), ControllerId, InviteResult.IsValid() ? TEXT("是") : TEXT("否")));
    if (!bWasSuccessful || !InviteResult.IsValid())
    {
        Status = TEXT("Steam 好友邀请无法解析成可加入房间");
        return;
    }
    JoinSearchResult(InviteResult, TEXT("Steam 好友邀请"));
}

void UTreasureOnlineSubsystem::JoinSearchResult(const FOnlineSessionSearchResult& SearchResult, const FString& Source)
{
    if (!SessionInterface.IsValid()) return;
    SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
    JoinHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateUObject(this, &UTreasureOnlineSubsystem::OnJoinSessionComplete));
    Status = FString::Printf(TEXT("正在通过%s加入房间……"), *Source);
    AddDiagnostic(FString::Printf(TEXT("JoinSession：来源=%s | 房主=%s | Ping=%d"),
        *Source, *SearchResult.Session.OwningUserName, SearchResult.PingInMs));
    if (!SessionInterface->JoinSession(0, NAME_GameSession, SearchResult))
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
        Status = TEXT("加入请求未能启动");
        AddDiagnostic(TEXT("JoinSession：同步请求失败"));
    }
}

void UTreasureOnlineSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
    FString ConnectString;
    if (Result != EOnJoinSessionCompleteResult::Success || !SessionInterface->GetResolvedConnectString(SessionName, ConnectString))
    {
        Status = FString::Printf(TEXT("加入房间失败，结果代码 %d"), static_cast<int32>(Result));
        AddDiagnostic(FString::Printf(TEXT("JoinSession：失败 | 结果代码=%d | 连接地址=%s"),
            static_cast<int32>(Result), ConnectString.IsEmpty() ? TEXT("无") : TEXT("有")));
        UE_LOG(LogTemp, Error, TEXT("TREASURE_ONLINE_JOIN failed result=%d"), static_cast<int32>(Result));
        return;
    }
    Status = TEXT("已找到主机，正在进入游戏……");
    AddDiagnostic(FString::Printf(TEXT("JoinSession：成功 | ClientTravel=%s"), *ConnectString));
    UE_LOG(LogTemp, Display, TEXT("TREASURE_ONLINE_JOIN travel=%s"), *ConnectString);
    if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
        PC->ClientTravel(ConnectString, TRAVEL_Absolute);
}

void UTreasureOnlineSubsystem::OnNetworkFailure(UWorld* World, UNetDriver* NetDriver,
    ENetworkFailure::Type FailureType, const FString& ErrorString)
{
    Status = FString::Printf(TEXT("网络失败 [%d]：%s"), static_cast<int32>(FailureType), *ErrorString);
    AddDiagnostic(Status);
    UE_LOG(LogTemp, Error, TEXT("TREASURE_ONLINE_NETWORK_FAILURE type=%d error=%s"), static_cast<int32>(FailureType), *ErrorString);
}

void UTreasureOnlineSubsystem::OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
    Status = FString::Printf(TEXT("进入主机失败 [%d]：%s"), static_cast<int32>(FailureType), *ErrorString);
    AddDiagnostic(Status);
    UE_LOG(LogTemp, Error, TEXT("TREASURE_ONLINE_TRAVEL_FAILURE type=%d error=%s"), static_cast<int32>(FailureType), *ErrorString);
}

void UTreasureOnlineSubsystem::AddDiagnostic(const FString& Message)
{
    Diagnostics.Insert(Message, 0);
    if (Diagnostics.Num() > 9) Diagnostics.SetNum(9);
    UE_LOG(LogTemp, Display, TEXT("TREASURE_ONLINE_DIAG %s"), *Message);
}
