#include "GrandCityAccountAuthSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
    TSharedPtr<FJsonObject> ParseObject(const FString& Body)
    {
        TSharedPtr<FJsonObject> Object;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
        FJsonSerializer::Deserialize(Reader, Object);
        return Object;
    }
}

FString UGrandCityAccountAuthSubsystem::GetBaseUrl() const
{
    FString Value;
    GConfig->GetString(TEXT("/Script/GrandCityMobile.GrandCityPersistenceSettings"), TEXT("BaseUrl"), Value, GGameIni);
    return Value.IsEmpty() ? TEXT("http://127.0.0.1:8080") : Value;
}

FString UGrandCityAccountAuthSubsystem::GetInternalApiKey() const
{
    FString Value;
    GConfig->GetString(TEXT("/Script/GrandCityMobile.GrandCityPersistenceSettings"), TEXT("ApiKey"), Value, GGameIni);
    return Value;
}

void UGrandCityAccountAuthSubsystem::SetIdentity(const FGrandCityAccountIdentity& InIdentity)
{
    Identity = InIdentity;
}

void UGrandCityAccountAuthSubsystem::VerifyToken(const FString& AuthToken, FGrandCityAuthResult Callback)
{
    if (AuthToken.IsEmpty())
    {
        Callback(false, FGrandCityAccountIdentity());
        return;
    }

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(GetBaseUrl() / TEXT("v1/auth/verify"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("x-internal-api-key"), GetInternalApiKey());

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("token"), AuthToken);
    FString Payload;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
    Request->SetContentAsString(Payload);

    Request->OnProcessRequestComplete().BindLambda(
        [this, Callback](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
        {
            FGrandCityAccountIdentity Result;
            if (!bConnected || !Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()))
            {
                Callback(false, Result);
                return;
            }

            const TSharedPtr<FJsonObject> Json = ParseObject(Response->GetContentAsString());
            const TSharedPtr<FJsonObject>* Account = nullptr;
            if (!Json.IsValid() || !Json->TryGetObjectField(TEXT("account"), Account) || !Account || !Account->IsValid())
            {
                Callback(false, Result);
                return;
            }

            (*Account)->TryGetStringField(TEXT("account_id"), Result.AccountId);
            (*Account)->TryGetStringField(TEXT("display_name"), Result.DisplayName);
            (*Account)->TryGetStringField(TEXT("region_id"), Result.RegionId);
            Result.bAuthenticated = !Result.AccountId.IsEmpty();
            if (!Result.bAuthenticated)
            {
                Callback(false, FGrandCityAccountIdentity());
                return;
            }

            SetIdentity(Result);
            Callback(true, Result);
        });

    Request->ProcessRequest();
}

void UGrandCityAccountAuthSubsystem::ClaimSession(const FString& AccountId, const FString& ServerId, const FString& TransferToken, FGrandCitySessionResult Callback)
{
    if (AccountId.IsEmpty() || ServerId.IsEmpty())
    {
        Callback(false, FString());
        return;
    }

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(GetBaseUrl() / TEXT("v1/sessions/claim"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("x-internal-api-key"), GetInternalApiKey());

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("accountId"), AccountId);
    Body->SetStringField(TEXT("serverId"), ServerId);
    if (!TransferToken.IsEmpty())
    {
        Body->SetStringField(TEXT("transferToken"), TransferToken);
    }

    FString Payload;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
    Request->SetContentAsString(Payload);

    Request->OnProcessRequestComplete().BindLambda([Callback](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
    {
        if (!bConnected || !Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()))
        {
            Callback(false, FString());
            return;
        }
        Callback(true, FString());
    });

    Request->ProcessRequest();
}

void UGrandCityAccountAuthSubsystem::ReleaseSession(const FString& AccountId, const FString& ServerId, TFunction<void(bool)> Callback)
{
    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(GetBaseUrl() / TEXT("v1/sessions/release"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("x-internal-api-key"), GetInternalApiKey());

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("accountId"), AccountId);
    Body->SetStringField(TEXT("serverId"), ServerId);
    FString Payload;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
    Request->SetContentAsString(Payload);

    Request->OnProcessRequestComplete().BindLambda([Callback](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
    {
        Callback(bConnected && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()));
    });
    Request->ProcessRequest();
}
