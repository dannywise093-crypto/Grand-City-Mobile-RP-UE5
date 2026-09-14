#include "GrandCityGlobalRoutingSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ConfigCacheIni.h"

FString UGrandCityGlobalRoutingSubsystem::GetBaseUrl() const
{
    FString Value;
    GConfig->GetString(TEXT("/Script/GrandCityMobile.GrandCityGlobalRoutingSettings"), TEXT("BaseUrl"), Value, GGameIni);
    return Value.IsEmpty() ? TEXT("http://127.0.0.1:8090") : Value;
}

void UGrandCityGlobalRoutingSubsystem::FindBestServer(const TMap<FString, int32>& LatencyMsByRegion, FGrandCityRouteCallback Callback)
{
    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(GetBaseUrl() / TEXT("v1/route"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Latencies = MakeShared<FJsonObject>();
    for (const TPair<FString, int32>& Pair : LatencyMsByRegion)
    {
        Latencies->SetNumberField(Pair.Key, Pair.Value);
    }
    Body->SetObjectField(TEXT("latencyMsByRegion"), Latencies);

    FString Payload;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
    Request->SetContentAsString(Payload);

    Request->OnProcessRequestComplete().BindLambda([Callback](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
    {
        FGrandCityRouteResult Result;
        if (!bConnected || !Response.IsValid())
        {
            Result.Error = TEXT("network_error");
            Callback(Result);
            return;
        }

        TSharedPtr<FJsonObject> Json;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
        FJsonSerializer::Deserialize(Reader, Json);
        if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()) || !Json.IsValid())
        {
            Result.Error = Json.IsValid() && Json->HasField(TEXT("error")) ? Json->GetStringField(TEXT("error")) : TEXT("routing_failed");
            Callback(Result);
            return;
        }

        Json->TryGetStringField(TEXT("regionId"), Result.RegionId);
        Json->TryGetStringField(TEXT("serverId"), Result.ServerId);
        Json->TryGetStringField(TEXT("endpoint"), Result.Endpoint);
        Result.bSuccess = !Result.Endpoint.IsEmpty() && !Result.RegionId.IsEmpty();
        if (!Result.bSuccess) Result.Error = TEXT("invalid_route_response");
        Callback(Result);
    });

    Request->ProcessRequest();
}
