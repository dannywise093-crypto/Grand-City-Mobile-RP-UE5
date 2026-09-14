# Grand City OpenAI — UE5.6 / Android Port

This folder is the Grand City Mobile integration target for the LifeEXE UnrealOpenAIPlugin runtime source.

## Port rules

- Target Unreal Engine 5.6.
- Runtime module only for shipping builds.
- Supported targets: Win64 and Android.
- Do not ship the original Win64 DLL/PDB files.
- Do not ship editor/test modules in Android builds.
- Keep OpenAI credentials out of the Android package.
- Prefer the Grand City server/AI gateway for production requests and policy enforcement.

## Source provenance

The supplied plugin identifies as UnrealOpenAIPlugin 5.8.0 by LifeEXE. The upstream project is MIT licensed; the copyright and permission notice must remain with redistributed source.

The original plugin's example `.uasset`/`.umap` content is intentionally excluded from the mobile runtime port.

## Build expectation

The source port must be compiled with the UE5.6 toolchain. This repository does not contain a UE5.6 editor/build environment, so a successful Android package cannot honestly be claimed until the project is opened and built with UE5.6 + Android SDK/NDK.
