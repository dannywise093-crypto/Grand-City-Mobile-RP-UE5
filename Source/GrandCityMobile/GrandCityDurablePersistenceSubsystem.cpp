#include "GrandCityDurablePersistenceSubsystem.h"

#include "HttpModule.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
    bool JsonString(const TSharedPtr<FJsonObject>& Json, const TCHAR* Name, FString& Out)
    {
        return Json.IsValid() && Json->TryGetStringField(Name, Out);
    }

    void ReadProfile(const TSharedPtr<FJsonObject>& Json, FGrandCityPlayerProfile& Out)
    {
        Json->TryGetNumberField(TEXT("schemaVersion"), Out.SchemaVersion);
        JsonString(Json, TEXT("accountId"), Out.AccountId);
        JsonString(Json, TEXT("characterId"), Out.CharacterId);
        JsonString(Json, TEXT("characterName"), Out.CharacterName);
        JsonString(Json, TEXT("regionId"), Out.RegionId);
        Json->TryGetNumberField(TEXT("characterLevel"), Out.CharacterLevel);
        Json->TryGetNumberField(TEXT("cash"), Out.Cash);
        Json->TryGetNumberField(TEXT("bankBalance"), Out.BankBalance);
        Json->TryGetNumberField(TEXT("reputation"), Out.Reputation);
        Json->TryGetNumberField(TEXT("totalPlayTimeSeconds"), Out.TotalPlayTimeSeconds);
        Json->TryGetNumberField(TEXT("lastSaveUnixSeconds"), Out.LastSaveUnixSeconds);
    }
}

FString UGrandCityDurablePersistenceSubsystem::GetBaseUrl() const
{
    static const FString DefaultBaseUrl(TEXT("http://127.0.0.1:8080"));

    FString Value;
    GConfig->GetString(TEXT("/Script/GrandCityMobile.GrandCityPersistenceSettings"), TEXT("BaseUrl"), Value, GGameIni);
    Value.TrimStartAndEndInline();

    if (Value.Len() >= 2 && Value.StartsWith(TEXT("\"")) && Value.EndsWith(TEXT("\"")))
    {
        Value = Value.Mid(1, Value.Len() - 2);
        Value.TrimStartAndEndInline();
    }

    while (Value.EndsWith(TEXT("/")))
    {
        Value.LeftChopInline(1);
    }

    if (Value.IsEmpty())
    {
        return DefaultBaseUrl;
    }

    const bool bHasHttpScheme = Value.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase)
        || Value.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase);
    if (!bHasHttpScheme)
    {
        UE_LOG(LogTemp, Error,
            TEXT("Grand City persistence BaseUrl '%s' is invalid; using %s."),
            *Value,
            *DefaultBaseUrl);
        return DefaultBaseUrl;
    }

    return Value;
}

FString UGrandCityDurablePersistenceSubsystem::GetApiKey() const
{
    FString Value;
    GConfig->GetString(TEXT("/Script/GrandCityMobile.GrandCityPersistenceSettings"), TEXT("ApiKey"), Value, GGameIni);
    return Value;
}

void UGrandCityDurablePersistenceSubsystem::LoadProfile(const FString& AccountId, FGrandCityProfileLoadResult Callback)
{
    if (AccountId.IsEmpty())
    {
        Callback(false, false, FGrandCityPlayerProfile());
        return;
    }

    const FString Url = FString::Printf(TEXT("%s/v1/profiles/%s"), *GetBaseUrl(), *FGenericPlatformHttp::UrlEncode(AccountId));
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *GetApiKey()));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->OnProcessRequestComplete().BindLambda([Callback](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
    {
        FGrandCityPlayerProfile Profile;
        const bool bFound = bSucceeded && Response.IsValid() && Response->GetResponseCode() == 200;
        if (bFound)
        {
            TSharedPtr<FJsonObject> Json;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
            if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
            {
                ReadProfile(Json, Profile);
                Callback(true, true, Profile);
                return;
            }
        }
        Callback(bSucceeded && Response.IsValid() && (Response->GetResponseCode() == 404), false, Profile);
    });
    Request->ProcessRequest();
}

void UGrandCityDurablePersistenceSubsystem::SaveProfile(const FGrandCityPlayerProfile& Profile, FGrandCityProfileSaveResult Callback)
{
    if (Profile.AccountId.IsEmpty())
    {
        Callback(false, Profile);
        return;
    }

    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("schemaVersion"), Profile.SchemaVersion);
    Json->SetStringField(TEXT("accountId"), Profile.AccountId);
    Json->SetStringField(TEXT("characterId"), Profile.CharacterId);
    Json->SetStringField(TEXT("characterName"), Profile.CharacterName);
    Json->SetStringField(TEXT("regionId"), Profile.RegionId);
    Json->SetNumberField(TEXT("characterLevel"), Profile.CharacterLevel);
    Json->SetNumberField(TEXT("cash"), Profile.Cash);
    Json->SetNumberField(TEXT("bankBalance"), Profile.BankBalance);
    Json->SetNumberField(TEXT("reputation"), Profile.Reputation);
    Json->SetNumberField(TEXT("totalPlayTimeSeconds"), Profile.TotalPlayTimeSeconds);
    Json->SetNumberField(TEXT("lastSaveUnixSeconds"), Profile.LastSaveUnixSeconds);

    FString Body;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
    FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);

    const FString Url = FString::Printf(TEXT("%s/v1/profiles/%s"), *GetBaseUrl(), *FGenericPlatformHttp::UrlEncode(Profile.AccountId));
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(TEXT("PUT"));
    Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *GetApiKey()));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(Body);
    Request->OnProcessRequestComplete().BindLambda([Callback, Profile](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
    {
        const bool bSuccess = bSucceeded && Response.IsValid() && Response->GetResponseCode() == 200;
        Callback(bSuccess, Profile);
    });
    Request->ProcessRequest();
}
