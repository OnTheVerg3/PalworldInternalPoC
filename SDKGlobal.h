#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <vector>
#include <iostream>
#include <cstdint>
#include <string> 

// Undefine conflicting macros
#ifdef DrawText
#undef DrawText
#endif
#ifdef GetObject
#undef GetObject
#endif
#ifdef PlaySound
#undef PlaySound
#endif

// Include your Dumper-7 SDK Master Header
#include "SDK/Basic.hpp"
#include "SDK.hpp"

// Include Module Headers
#include "SDK/Pal_classes.hpp"
#include "SDK/Pal_parameters.hpp"
#include "SDK/Engine_classes.hpp"

namespace SDK {
    inline UWorld** pGWorld = nullptr;

    inline uintptr_t PatternScan(const char* signature) {
        static auto pattern_to_byte = [](const char* pattern) {
            auto bytes = std::vector<int>{};
            auto start = const_cast<char*>(pattern);
            auto end = const_cast<char*>(pattern) + strlen(pattern);
            for (auto current = start; current < end; ++current) {
                if (*current == '?') {
                    ++current;
                    if (*current == '?') ++current;
                    bytes.push_back(-1);
                }
                else {
                    bytes.push_back(strtoul(current, &current, 16));
                }
            }
            return bytes;
            };

        auto dosHeader = (PIMAGE_DOS_HEADER)GetModuleHandleA(NULL);
        auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)dosHeader + dosHeader->e_lfanew);
        auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
        auto patternBytes = pattern_to_byte(signature);
        auto scanBytes = reinterpret_cast<std::uint8_t*>(dosHeader);
        auto s = patternBytes.size();
        auto d = patternBytes.data();

        for (auto i = 0ul; i < sizeOfImage - s; ++i) {
            bool found = true;
            for (auto j = 0ul; j < s; ++j) {
                if (scanBytes[i + j] != d[j] && d[j] != -1) {
                    found = false;
                    break;
                }
            }
            if (found) return reinterpret_cast<uintptr_t>(&scanBytes[i]);
        }
        return 0;
    }

    inline void Init() {
        uintptr_t GWorldPtr = PatternScan("48 8B 1D ? ? ? ? 48 85 DB 74 33 41 B0 01");
        if (GWorldPtr) {
            uintptr_t GWorldOffset = *reinterpret_cast<int32_t*>(GWorldPtr + 3);
            pGWorld = reinterpret_cast<UWorld**>(GWorldPtr + 7 + GWorldOffset);
        }
        uintptr_t GObjectsPtr = PatternScan("48 8B 05 ? ? ? ? 48 8B 0C C8 48 8D 04 D1");
        if (GObjectsPtr) {
            uintptr_t GObjectsOffset = *reinterpret_cast<int32_t*>(GObjectsPtr + 3);
            uintptr_t GObjectsReal = GObjectsPtr + 7 + GObjectsOffset;
            *reinterpret_cast<void**>(&SDK::UObject::GObjects) = reinterpret_cast<void*>(GObjectsReal);
        }
    }

    inline APalPlayerCharacter* GetLocalPlayer() {
        if (!pGWorld || !*pGWorld) return nullptr;
        UGameInstance* pGameInstance = (*pGWorld)->OwningGameInstance;
        if (!pGameInstance || pGameInstance->LocalPlayers.Num() == 0) return nullptr;
        ULocalPlayer* pLocalPlayer = pGameInstance->LocalPlayers[0];
        if (!pLocalPlayer || !pLocalPlayer->PlayerController) return nullptr;
        APawn* pPawn = pLocalPlayer->PlayerController->Pawn;
        return static_cast<APalPlayerCharacter*>(pPawn);
    }

    inline void ExecuteConsoleCommand(const std::string& Command) {
        if (!pGWorld || !*pGWorld) return;

        static UFunction* fn = nullptr;
        if (!fn) fn = UObject::FindObject<UFunction>("Function Engine.KismetSystemLibrary.ExecuteConsoleCommand");

        // [FIX] Manually find the Default Object (CDO) since GetDefaultObj isn't available
        static UObject* defaultObj = nullptr;
        if (!defaultObj) defaultObj = UObject::FindObject<UObject>("Default__KismetSystemLibrary");

        if (fn && defaultObj) {
            std::wstring wCommand(Command.begin(), Command.end());
            FString fCommand(wCommand.c_str());

            struct {
                UObject* WorldContextObject;
                FString Command;
                APlayerController* SpecificPlayer;
            } params;

            params.WorldContextObject = *pGWorld;
            params.Command = fCommand;
            params.SpecificPlayer = nullptr;

            defaultObj->ProcessEvent(fn, &params);
        }
    }
}