#include "GrandCityMobileAccountClientSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
    TSharedPtr<FJsonObject> ParseJson(const FString& Body)
    {
        TSharedPtr<FJsonObject> Json;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
        FJsonSerializer::Deserialize(Reader, Json);
        return Json;
    }

    FString ReadError(const TSharedPtr<FJsonObject>& Json)
    {
        FString Error;
        if (Json.IsValid()) Json->TryGetStringField(TEXT("error"), Error);
        return Error.IsEmpty() ? TEXT("request_failed") : Error;
    }
}

FString UGrandCityMobileAccountClientSubsystem::GetBaseUrl() const
{
    FString Value;
    GConfig->GetString(TEXT("/Script/GrandCityMobile.GrandCityAccountClientSettings"), TEXT("BaseUrl"), Value, GGameIni);
    return Value.IsEmpty() ? TEXT("http://127.0.0.1:8080") : Value;
}

void UGrandCityMobileAccountClientSubsystem::RegisterAccount(const FString& DisplayName, const FString& Password, FGrandCityAccountClientResult Callback)
{
    SendCredentialsRequest(TEXT("v1/accounts/register"), DisplayName, Password, Callback);
}

void UGrandCityMobileAccountClientSubsystem::LoginAccount(const FString& DisplayName, const FString& Password, FGrandCityAccountClientResult Callback)
{
    SendCredentialsRequest(TEXT("v1/accounts/login"), DisplayName, Password, Callback);
}

void UGrandCityMobileAccountClientSubsystem::SendCredentialsRequest(const TCHAR* Endpoint, const FString& DisplayName, const FString& Password, FGrandCityAccountClientResult Callback)
{
    if (DisplayName.IsEmpty() || Password.IsEmpty())
    {
        Callback(false, FGrandCityAccountIdentity(), FString(), TEXT("invalid_input"));
        return;
    }

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(GetBaseUrl() / Endpoint);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("displayName"), DisplayName);
    Body->SetStringField(TEXT("password"), Password);

    FString Payload;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
    Request->SetContentAsString(Payload);

    Request->OnProcessRequestComplete().BindLambda([this, Callback](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
    {
        if (!bConnected || !Response.IsValid())
        {
            Callback(false, FGrandCityAccountIdentity(), FString(), TEXT("network_error"));
            return;
        }

        const TSharedPtr<FJsonObject> Json = ParseJson(Response->GetContentAsString());
        if (!Json.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()))
        {
            Callback(false, FGrandCityAccountIdentity(), FString(), ReadError(Json));
            return;
        }

        FGrandCityAccountIdentity NewIdentity;
        FString NewToken;
        Json->TryGetStringField(TEXT("token"), NewToken);

        const TSharedPtr<FJsonObject>* Account = nullptr;
        if (Json->TryGetObjectField(TEXT("account"), Account) && Account && Account->IsValid())
        {
            (*Account)->TryGetStringField(TEXT("account_id"), NewIdentity.AccountId);
            (*Account)->TryGetStringField(TEXT("display_name"), NewIdentity.DisplayName);
            (*Account)->TryGetStringField(TEXT("region_id"), NewIdentity.RegionId);
        }

        NewIdentity.bAuthenticated = !NewIdentity.AccountId.IsEmpty() && !NewToken.IsEmpty();
        if (NewIdentity.bAuthenticated)
        {
            Identity = NewIdentity;
            AuthToken = NewToken;
        }

        Callback(NewIdentity.bAuthenticated, NewIdentity, NewToken, NewIdentity.bAuthenticated ? FString() : TEXT("invalid_auth_response"));
    });

    Request->ProcessRequest();
}

void UGrandCityMobileAccountClientSubsystem::RequestConnectionTicket(const FString& TargetServerId, FGrandCityConnectionTicketResult Callback)
{
    if (AuthToken.IsEmpty() || TargetServerId.IsEmpty())
    {
        Callback(false, FString(), TEXT("not_authenticated"));
        return;
    }

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(GetBaseUrl() / TEXT("v1/connection-tickets"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("targetServerId"), TargetServerId);

    FString Payload;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
    Request->SetContentAsString(Payload);

    Request->OnProcessRequestComplete().BindLambda([Callback](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
    {
        if (!bConnected || !Response.IsValid())
        {
            Callback(false, FString(), TEXT("network_error"));
            return;
        }

        const TSharedPtr<FJsonObject> Json = ParseJson(Response->GetContentAsString());
        if (!Json.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()))
        {
            Callback(false, FString(), ReadError(Json));
            return;
        }

        FString Ticket;
        Json->TryGetStringField(TEXT("ticket"), Ticket);
        Callback(!Ticket.IsEmpty(), Ticket, Ticket.IsEmpty() ? TEXT("invalid_ticket_response") : FString());
    });

    Request->ProcessRequest();
}

void UGrandCityMobileAccountClientSubsystem::ClearSession()
{
    Identity = FGrandCityAccountIdentity();
    AuthToken.Empty();
}
