# Grand City Mobile — OpenAI Unreal Plugin Integration

This document records the integration plan for the OpenAI Unreal plugin supplied as `OpenAI.zip`.

## Source

The supplied plugin identifies itself as **UnrealOpenAIPlugin 5.8.0**, created by LifeEXE. The package contains a Runtime `OpenAI` module plus Editor/Test modules and Windows binaries.

## Important compatibility note

Grand City Mobile currently targets Unreal Engine 5.6. The supplied plugin declares Unreal Engine 5.8.0 and its runtime/editor/test modules are allow-listed for Win64 in the supplied package.

Therefore the package must **not** be copied wholesale into the mobile game. The integration should be treated as a source-level port:

1. Preserve the plugin's original attribution/license information.
2. Port the Runtime OpenAI module to the Grand City UE5.6 toolchain.
3. Remove Win64-only binaries and editor/test modules from mobile shipping builds.
4. Verify Android-compatible HTTP, JSON, image and audio dependencies.
5. Keep the OpenAI API credential off the Android client.
6. Route production AI requests through a Grand City backend/service where appropriate.

## Intended Grand City architecture

```text
Android Client
    |
    v
Grand City Game Server / AI Gateway
    |
    v
OpenAI API
    |
    v
Validated AI response
    |
    v
Grand City gameplay systems
```

The AI layer must not directly become authoritative over player money, inventory, account identity, permissions, or other security-sensitive state. Game servers validate all gameplay actions.

## Planned gameplay integrations

- AI NPC dialogue/personality
- Dynamic mission and conversation generation
- Player help/assistant system
- Optional NPC voice generation/transcription
- Vision-powered features where performance and privacy permit
- AI-driven narrative/event suggestions with server-side validation

## Current status

The uploaded plugin has been inspected and this integration record has been added to the Grand City repository. The full 5.8.0 plugin source is intentionally **not** copied wholesale until its UE5.6/Android compatibility is ported and verified.

No claim is made that the plugin currently compiles for Grand City Mobile or Android; that requires an actual UE5.6 build/test environment.
