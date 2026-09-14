#include "GrandCityServerRegistrySubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "GrandCityMobileGameState.h"

void UGrandCityServerRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bRunning = false;
    bDraining = false;

    if (UWorld* World = GetWorld())
    {
        if (World->GetNetMode() != NM_Client)
        {
            StartServerRegistration();
        }
    }
}

void UGrandCityServerRegistrySubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(HeartbeatTimer);
    }
    bRunning = false;
    Super::Deinitialize();
}

void UGrandCityServerRegistrySubsystem::StartServerRegistration()
{
    if (bRunning || !GetWorld() || GetWorld()->GetNetMode() == NM_Client)
    {
        return;
    }

    bRunning = true;
    bDraining = false;
    SendHeartbeat();

    GetWorld()->GetTimerManager().SetTimer(
        HeartbeatTimer,
        this,
        &UGrandCityServerRegistrySubsystem::SendHeartbeat,
        5.0f,
        true,
        5.0f);
}

void UGrandCityServerRegistrySubsystem::BeginDraining()
{
    if (!bRunning || bDraining)
    {
        return;
    }

    bDraining = true;
    SendHeartbeatRequest(true);
}

void UGrandCityServerRegistrySubsystem::SendHeartbeat()
{
    if (!bRunning || !GetWorld() || GetWorld()->GetNetMode() == NM_Client)
    {
        return;
    }

    SendHeartbeatRequest(bDraining);
}

void UGrandCityServerRegistrySubsystem::SendHeartbeatRequest(bool bDrainingState)
{
    const FString BaseUrl = GetMasterRouterUrl();
    const FString ApiKey = GetInternalApiKey();
    if (BaseUrl.IsEmpty() || ApiKey.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Server registry is not configured: master URL or internal API key is missing."));
        return;
    }

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("regionId"), GetRegionId());
    Body->SetStringField(TEXT("serverId"), GetServerId());
    Body->SetStringField(TEXT("endpoint"), GetServerEndpoint());
    Body->SetNumberField(TEXT("players"), GetWorld() ? GetWorld()->GetNumPlayers() : 0);
    Body->SetNumberField(TEXT("capacity"), GetServerCapacity());
    Body->SetStringField(TEXT("state"), bDrainingState ? TEXT("DRAINING") : TEXT("READY"));

    FString Payload;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BaseUrl / TEXT("v1/servers/heartbeat"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("x-internal-api-key"), ApiKey);
    Request->SetContentAsString(Payload);
    Request->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
    {
        if (!bConnected || !Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()))
        {
            UE_LOG(LogTemp, Warning, TEXT("Global master heartbeat failed."));
        }
    });
    Request->ProcessRequest();
}

FString UGrandCityServerRegistrySubsystem::GetMasterRouterUrl() const
{
    FString Value;
    GConfig->GetString(TEXT("/Script/GrandCityMobile.GrandCityGlobalRoutingSettings"), TEXT("BaseUrl"), Value, GGameIni);
    return Value.IsEmpty() ? TEXT("http://127.0.0.1:8090") : Value;
}

FString UGrandCityServerRegistrySubsystem::GetInternalApiKey() const
{
    FString Value;
    GConfig->GetString(TEXT("/Script/GrandCityMobile.GrandCityGlobalRoutingSettings"), TEXT("InternalApiKey"), Value, GGameIni);
    return Value;
}

FString UGrandCityServerRegistrySubsystem::GetRegionId() const
{
    if (const AGrandCityMobileGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AGrandCityMobileGameState>() : nullptr)
    {
        return GameState->RegionId.ToString();
    }

    FString Value;
    GConfig->GetString(TEXT("/Script/GrandCityMobile.GrandCityRegionalServer"), TEXT("RegionId"), Value, GGameIni);
    return Value.IsEmpty() ? TEXT("AFRICA_WEST") : Value;
}

FString UGrandCityServerRegistrySubsystem::GetServerId() const
{
    if (const AGrandCityMobileGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AGrandCityMobileGameState>() : nullptr)
    {
        return GameState->ServerId.ToString();
    }

    FString Value;
    GConfig->GetString(TEXT("/Script/GrandCityMobile.GrandCityRegionalServer"), TEXT("ServerId"), Value, GGameIni);
    return Value.IsEmpty() ? TEXT("GC-AFRICA-01") : Value;
}

FString UGrandCityServerRegistrySubsystem::GetServerEndpoint() const
{
    FString Value;
    GConfig->GetString(TEXT("/Script/GrandCityMobile.GrandCityRegionalServer"), TEXT("Endpoint"), Value, GGameIni);
    return Value;
}

int32 UGrandCityServerRegistrySubsystem::GetServerCapacity() const
{
    if (const AGrandCityMobileGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AGrandCityMobileGameState>() : nullptr)
    {
        return GameState->ServerPopulationLimit;
    }

    int32 Value = 100;
    GConfig->GetInt(TEXT("/Script/GrandCityMobile.GrandCityRegionalServer"), TEXT("Capacity"), Value, GGameIni);
    return FMath::Max(1, Value);
}
