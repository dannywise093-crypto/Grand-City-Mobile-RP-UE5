// Grand City Mobile - server-side AI gateway transport.
#include "AI/GrandCityAIGatewayClient.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"

FString UGrandCityAIGatewayClient::GetBaseUrl() const
{
    FString Value;
    GConfig->GetString(TEXT("/Script/GrandCityMobile.GrandCityAISettings"), TEXT("BaseUrl"), Value, GGameIni);
    return Value.IsEmpty() ? TEXT("http://127.0.0.1:8100") : Value;
}

FString UGrandCityAIGatewayClient::GetGatewayKey() const
{
    // Never store the gateway secret in DefaultGame.ini: that config can be packaged into the client.
    FString Value = FPlatformMisc::GetEnvironmentVariable(TEXT("GRANDCITY_AI_GATEWAY_KEY"));
    if (!Value.IsEmpty())
    {
        return Value;
    }

    FString CommandLineValue;
    if (FParse::Value(FCommandLine::Get(), TEXT("GrandCityAIGatewayKey="), CommandLineValue))
    {
        return CommandLineValue;
    }

    return FString();
}

void UGrandCityAIGatewayClient::SendChatRequest(const FString& RequestId, const FString& SystemPrompt, const FString& UserPrompt,
    FGrandCityAIGatewaySuccess OnSuccess, FGrandCityAIGatewayFailure OnFailure)
{
    if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
    {
        OnFailure.ExecuteIfBound(RequestId, TEXT("AI gateway transport is server-only."));
        return;
    }

    if (RequestId.IsEmpty() || SystemPrompt.IsEmpty() || UserPrompt.IsEmpty())
    {
        OnFailure.ExecuteIfBound(RequestId, TEXT("AI request fields are required."));
        return;
    }

    const FString GatewayKey = GetGatewayKey();
    if (GatewayKey.IsEmpty())
    {
        OnFailure.ExecuteIfBound(RequestId, TEXT("AI gateway server secret is not configured."));
        return;
    }

    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("request_id"), RequestId);
    Body->SetStringField(TEXT("system_prompt"), SystemPrompt);
    Body->SetStringField(TEXT("user_prompt"), UserPrompt);

    FString BodyString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
    FJsonSerializer::Serialize(Body, Writer);

    FString Url = GetBaseUrl();
    if (Url.EndsWith(TEXT("/")))
    {
        Url.LeftChopInline(1);
    }
    Url += TEXT("/v1/ai/chat");

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("x-ai-gateway-key"), GatewayKey);
    Request->SetContentAsString(BodyString);
    Request->SetTimeout(20.0f);

    Request->OnProcessRequestComplete().BindLambda(
        [RequestId, OnSuccess, OnFailure](FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (!bWasSuccessful || !Response.IsValid())
            {
                OnFailure.ExecuteIfBound(RequestId, TEXT("AI gateway connection failed."));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
            if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
            {
                OnFailure.ExecuteIfBound(RequestId, TEXT("AI gateway returned invalid JSON."));
                return;
            }

            if (Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
            {
                FString ErrorText;
                Json->TryGetStringField(TEXT("error"), ErrorText);
                OnFailure.ExecuteIfBound(RequestId, ErrorText.IsEmpty() ? TEXT("AI gateway request failed.") : ErrorText);
                return;
            }

            FString ResponseText;
            if (!Json->TryGetStringField(TEXT("text"), ResponseText))
            {
                OnFailure.ExecuteIfBound(RequestId, TEXT("AI gateway returned no response text."));
                return;
            }

            OnSuccess.ExecuteIfBound(RequestId, ResponseText);
        });

    if (!Request->ProcessRequest())
    {
        OnFailure.ExecuteIfBound(RequestId, TEXT("AI gateway request could not be started."));
    }
}
