#include "overlay.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <windows.h>
#include <tlhelp32.h>
#include <imgui.h>

#define M_PI 3.14159265358979323846f

namespace GameState {
    bool espEnabled = true;
    bool boxEsp = true;
    bool skeletonEsp = true;
    bool snaplines = false;
    bool healthEsp = true;
    bool distanceEsp = true;
    bool aimbot = true;
    bool silentAim = false;
    bool ignoreDead = true;
    bool teamCheck = true;
    bool aimPrediction = true;
    bool autoShoot = false;
    bool wallCheck = false;
    float aimSmooth = 5.0f;
    float aimFov = 300.0f;
    int aimBone = 8;
}

// Memory Utilities
struct FVector { float X, Y, Z; };
struct FVector2D { float X, Y; };
struct FRotator { float Pitch, Yaw, Roll; };
struct FMatrix { float M[4][4]; };
struct FTransform {
    struct { float X, Y, Z, W; } Rotation;
    FVector Translation;
    uint32_t pad1[1];
    FVector Scale3D;
    uint32_t pad2[1];
};

struct TArray {
    uintptr_t Data;
    int32_t Count;
    int32_t Max;
};

// Offsets
constexpr uintptr_t GWorld = 0x44869E0; // From user's old UpGun_GDI_ESP.cpp
constexpr uintptr_t UWorld_PersistentLevel = 0x30;
constexpr uintptr_t UWorld_OwningGameInstance = 0x180;
constexpr uintptr_t UGameInstance_LocalPlayers = 0x38;
constexpr uintptr_t ULocalPlayer_PlayerController = 0x30;
constexpr uintptr_t APlayerController_AcknowledgedPawn = 0x2A0;
constexpr uintptr_t APlayerController_PlayerCameraManager = 0x2B8;
constexpr uintptr_t APlayerCameraManager_CameraCachePrivate = 0x1AA0;

constexpr uintptr_t ULevel_Actors = 0x98;
constexpr uintptr_t AActor_RootComponent = 0x130;
constexpr uintptr_t USceneComponent_RelativeLocation = 0x11C;
constexpr uintptr_t USceneComponent_ComponentToWorld = 0x1C0; // Standard 4.26 offset

constexpr uintptr_t APawn_PlayerState = 0x240;
constexpr uintptr_t APlayerState_PawnPrivate = 0x280;

constexpr uintptr_t ACharacter_Mesh = 0x280; // from SDK Dump
constexpr uintptr_t USkeletalMeshComponent_CachedBoneSpaceTransforms = 0x730; 
constexpr uintptr_t USkinnedMeshComponent_BoneArray = 0x4A0; 
constexpr uintptr_t USkinnedMeshComponent_bRecentlyRendered = 0x5F7; // bit index 0x06

constexpr uintptr_t APlayerController_ControlRotation = 0x288;
constexpr uintptr_t APlayerCameraManager_POVRotation = 0x1ABC; // 0x1AA0 (Cache) + 0x10 (POV) + 0x0C (Rot)

HANDLE hProcess = nullptr;
uintptr_t baseAddr = 0;

template <typename T>
T ReadMem(uintptr_t address) {
    T buffer{};
    ReadProcessMemory(hProcess, (LPCVOID)address, &buffer, sizeof(T), nullptr);
    return buffer;
}

void WriteMemRot(uintptr_t address, FRotator rot) {
    WriteProcessMemory(hProcess, (LPVOID)address, &rot, sizeof(FRotator), nullptr);
}

FMatrix MakeMatrix(FRotator rot, FVector origin) {
    float radPitch = (rot.Pitch * M_PI / 180.f);
    float radYaw = (rot.Yaw * M_PI / 180.f);
    float radRoll = (rot.Roll * M_PI / 180.f);

    float SP = sinf(radPitch);
    float CP = cosf(radPitch);
    float SY = sinf(radYaw);
    float CY = cosf(radYaw);
    float SR = sinf(radRoll);
    float CR = cosf(radRoll);

    FMatrix matrix;
    matrix.M[0][0] = CP * CY;
    matrix.M[0][1] = CP * SY;
    matrix.M[0][2] = SP;
    matrix.M[0][3] = 0.f;

    matrix.M[1][0] = SR * SP * CY - CR * SY;
    matrix.M[1][1] = SR * SP * SY + CR * CY;
    matrix.M[1][2] = -SR * CP;
    matrix.M[1][3] = 0.f;

    matrix.M[2][0] = -(CR * SP * CY + SR * SY);
    matrix.M[2][1] = CY * SR - CR * SP * SY;
    matrix.M[2][2] = CR * CP;
    matrix.M[2][3] = 0.f;

    matrix.M[3][0] = origin.X;
    matrix.M[3][1] = origin.Y;
    matrix.M[3][2] = origin.Z;
    matrix.M[3][3] = 1.f;
    return matrix;
}

FMatrix MatrixMultiplication(FMatrix pM1, FMatrix pM2) {
    FMatrix pOut;
    pOut.M[0][0] = pM1.M[0][0] * pM2.M[0][0] + pM1.M[0][1] * pM2.M[1][0] + pM1.M[0][2] * pM2.M[2][0] + pM1.M[0][3] * pM2.M[3][0];
    pOut.M[0][1] = pM1.M[0][0] * pM2.M[0][1] + pM1.M[0][1] * pM2.M[1][1] + pM1.M[0][2] * pM2.M[2][1] + pM1.M[0][3] * pM2.M[3][1];
    pOut.M[0][2] = pM1.M[0][0] * pM2.M[0][2] + pM1.M[0][1] * pM2.M[1][2] + pM1.M[0][2] * pM2.M[2][2] + pM1.M[0][3] * pM2.M[3][2];
    pOut.M[0][3] = pM1.M[0][0] * pM2.M[0][3] + pM1.M[0][1] * pM2.M[1][3] + pM1.M[0][2] * pM2.M[2][3] + pM1.M[0][3] * pM2.M[3][3];
    pOut.M[1][0] = pM1.M[1][0] * pM2.M[0][0] + pM1.M[1][1] * pM2.M[1][0] + pM1.M[1][2] * pM2.M[2][0] + pM1.M[1][3] * pM2.M[3][0];
    pOut.M[1][1] = pM1.M[1][0] * pM2.M[0][1] + pM1.M[1][1] * pM2.M[1][1] + pM1.M[1][2] * pM2.M[2][1] + pM1.M[1][3] * pM2.M[3][1];
    pOut.M[1][2] = pM1.M[1][0] * pM2.M[0][2] + pM1.M[1][1] * pM2.M[1][2] + pM1.M[1][2] * pM2.M[2][2] + pM1.M[1][3] * pM2.M[3][2];
    pOut.M[1][3] = pM1.M[1][0] * pM2.M[0][3] + pM1.M[1][1] * pM2.M[1][3] + pM1.M[1][2] * pM2.M[2][3] + pM1.M[1][3] * pM2.M[3][3];
    pOut.M[2][0] = pM1.M[2][0] * pM2.M[0][0] + pM1.M[2][1] * pM2.M[1][0] + pM1.M[2][2] * pM2.M[2][0] + pM1.M[2][3] * pM2.M[3][0];
    pOut.M[2][1] = pM1.M[2][0] * pM2.M[0][1] + pM1.M[2][1] * pM2.M[1][1] + pM1.M[2][2] * pM2.M[2][1] + pM1.M[2][3] * pM2.M[3][1];
    pOut.M[2][2] = pM1.M[2][0] * pM2.M[0][2] + pM1.M[2][1] * pM2.M[1][2] + pM1.M[2][2] * pM2.M[2][2] + pM1.M[2][3] * pM2.M[3][2];
    pOut.M[2][3] = pM1.M[2][0] * pM2.M[0][3] + pM1.M[2][1] * pM2.M[1][3] + pM1.M[2][2] * pM2.M[2][3] + pM1.M[2][3] * pM2.M[3][3];
    pOut.M[3][0] = pM1.M[3][0] * pM2.M[0][0] + pM1.M[3][1] * pM2.M[1][0] + pM1.M[3][2] * pM2.M[2][0] + pM1.M[3][3] * pM2.M[3][0];
    pOut.M[3][1] = pM1.M[3][0] * pM2.M[0][1] + pM1.M[3][1] * pM2.M[1][1] + pM1.M[3][2] * pM2.M[2][1] + pM1.M[3][3] * pM2.M[3][1];
    pOut.M[3][2] = pM1.M[3][0] * pM2.M[0][2] + pM1.M[3][1] * pM2.M[1][2] + pM1.M[3][2] * pM2.M[2][2] + pM1.M[3][3] * pM2.M[3][2];
    pOut.M[3][3] = pM1.M[3][0] * pM2.M[0][3] + pM1.M[3][1] * pM2.M[1][3] + pM1.M[3][2] * pM2.M[2][3] + pM1.M[3][3] * pM2.M[3][3];
    return pOut;
}

FMatrix TransformToMatrix(FTransform transform) {
    FMatrix m;
    m.M[3][0] = transform.Translation.X;
    m.M[3][1] = transform.Translation.Y;
    m.M[3][2] = transform.Translation.Z;
    m.M[3][3] = 1.0f;

    float x2 = transform.Rotation.X + transform.Rotation.X;
    float y2 = transform.Rotation.Y + transform.Rotation.Y;
    float z2 = transform.Rotation.Z + transform.Rotation.Z;

    float xx2 = transform.Rotation.X * x2;
    float yy2 = transform.Rotation.Y * y2;
    float zz2 = transform.Rotation.Z * z2;

    float yz2 = transform.Rotation.Y * z2;
    float wx2 = transform.Rotation.W * x2;

    float xy2 = transform.Rotation.X * y2;
    float wz2 = transform.Rotation.W * z2;

    float xz2 = transform.Rotation.X * z2;
    float wy2 = transform.Rotation.W * y2;

    m.M[0][0] = (1.0f - (yy2 + zz2)) * transform.Scale3D.X;
    m.M[0][1] = (xy2 + wz2) * transform.Scale3D.X;
    m.M[0][2] = (xz2 - wy2) * transform.Scale3D.X;
    m.M[0][3] = 0.0f;

    m.M[1][0] = (xy2 - wz2) * transform.Scale3D.Y;
    m.M[1][1] = (1.0f - (xx2 + zz2)) * transform.Scale3D.Y;
    m.M[1][2] = (yz2 + wx2) * transform.Scale3D.Y;
    m.M[1][3] = 0.0f;

    m.M[2][0] = (xz2 + wy2) * transform.Scale3D.Z;
    m.M[2][1] = (yz2 - wx2) * transform.Scale3D.Z;
    m.M[2][2] = (1.0f - (xx2 + yy2)) * transform.Scale3D.Z;
    m.M[2][3] = 0.0f;

    return m;
}

FVector GetBoneWithRotation(uintptr_t mesh, int id) {
    uintptr_t boneArray = ReadMem<uintptr_t>(mesh + USkinnedMeshComponent_BoneArray);
    if (!boneArray) boneArray = ReadMem<uintptr_t>(mesh + 0x4B0); // Fallbacks
    if (!boneArray) return {0, 0, 0};

    FTransform bone = ReadMem<FTransform>(boneArray + (id * 0x30));
    FTransform ComponentToWorld = ReadMem<FTransform>(mesh + USceneComponent_ComponentToWorld);
    
    FMatrix Matrix = MatrixMultiplication(TransformToMatrix(bone), TransformToMatrix(ComponentToWorld));
    return FVector{Matrix.M[3][0], Matrix.M[3][1], Matrix.M[3][2]};
}

bool WorldToScreen(FVector WorldLocation, FVector CameraLocation, FRotator CameraRotation, float FOV, int ScreenWidth, int ScreenHeight, FVector& OutScreen) {
    FMatrix vMatrix = MakeMatrix(CameraRotation, FVector{0, 0, 0});
    FVector vAxisX = { vMatrix.M[0][0], vMatrix.M[0][1], vMatrix.M[0][2] };
    FVector vAxisY = { vMatrix.M[1][0], vMatrix.M[1][1], vMatrix.M[1][2] };
    FVector vAxisZ = { vMatrix.M[2][0], vMatrix.M[2][1], vMatrix.M[2][2] };

    FVector vDelta = { WorldLocation.X - CameraLocation.X, WorldLocation.Y - CameraLocation.Y, WorldLocation.Z - CameraLocation.Z };
    FVector vTransformed = {
        vDelta.X * vAxisY.X + vDelta.Y * vAxisY.Y + vDelta.Z * vAxisY.Z,
        vDelta.X * vAxisZ.X + vDelta.Y * vAxisZ.Y + vDelta.Z * vAxisZ.Z,
        vDelta.X * vAxisX.X + vDelta.Y * vAxisX.Y + vDelta.Z * vAxisX.Z
    };

    if (vTransformed.Z < 1.0f) vTransformed.Z = 1.0f;

    float ScreenCenterX = ScreenWidth / 2.0f;
    float ScreenCenterY = ScreenHeight / 2.0f;

    OutScreen.X = ScreenCenterX + vTransformed.X * (ScreenCenterX / tanf(FOV * M_PI / 360.0f)) / vTransformed.Z;
    OutScreen.Y = ScreenCenterY - vTransformed.Y * (ScreenCenterX / tanf(FOV * M_PI / 360.0f)) / vTransformed.Z;
    // Store Z in distance for filtering
    OutScreen.Z = vTransformed.Z;

    return (OutScreen.X >= 0.0f && OutScreen.X <= ScreenWidth && OutScreen.Y >= 0.0f && OutScreen.Y <= ScreenHeight);
}

float GetDistance2D(float x1, float y1, float x2, float y2) {
    return sqrtf(powf(x2 - x1, 2.0f) + powf(y2 - y1, 2.0f));
}

// Win32 Helpers
DWORD GetProcId(const wchar_t* procName) {
    DWORD procId = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W procEntry;
        procEntry.dwSize = sizeof(procEntry);
        if (Process32FirstW(hSnap, &procEntry)) {
            do {
                if (!_wcsicmp(procEntry.szExeFile, procName)) {
                    procId = procEntry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnap, &procEntry));
        }
    }
    CloseHandle(hSnap);
    return procId;
}

uintptr_t GetModuleBaseAddress(DWORD procId, const wchar_t* modName) {
    uintptr_t modBaseAddr = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
    if (hSnap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W modEntry;
        modEntry.dwSize = sizeof(modEntry);
        if (Module32FirstW(hSnap, &modEntry)) {
            do {
                if (!_wcsicmp(modEntry.szModule, modName)) {
                    modBaseAddr = (uintptr_t)modEntry.modBaseAddr;
                    break;
                }
            } while (Module32NextW(hSnap, &modEntry));
        }
    }
    CloseHandle(hSnap);
    return modBaseAddr;
}

void DrawLineC(ImDrawList* draw, FVector2D from, FVector2D to, ImColor color, float thickness) {
    if (from.X > 0 && from.Y > 0 && to.X > 0 && to.Y > 0)
        draw->AddLine(ImVec2(from.X, from.Y), ImVec2(to.X, to.Y), color, thickness);
}

int main() {
    std::cout << "[*] Starting oxem.gg ESP..." << std::endl;
    
    DWORD procId = GetProcId(L"UpGun-Win64-Shipping.exe");
    if (!procId) { std::cout << "[-] UpGun not found. Launch game first.\n"; return 1; }

    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId);
    if (!hProcess) { std::cout << "[-] Failed to open process. Run as Administrator.\n"; return 1; }

    baseAddr = GetModuleBaseAddress(procId, L"UpGun-Win64-Shipping.exe");
    if (!baseAddr) { std::cout << "[-] Failed to find Base Address.\n"; return 1; }

    std::cout << "[+] Found UpGun Process ID: " << procId << "\n";
    std::cout << "[+] Base Address: 0x" << std::hex << baseAddr << std::dec << "\n";

    if (!Overlay::Initialize()) {
        std::cout << "[-] Failed to initialize overlay.\n";
        return 1;
    }

    std::cout << "[+] Overlay initialized successfully!\n";
    std::cout << "[+] Press INSERT to toggle Menu.\n";

    while (!Overlay::ShouldClose()) {
        Overlay::BeginFrame();

        HWND hwndGame = FindWindow(nullptr, L"UpGun");
        if (hwndGame) {
            RECT rect;
            GetWindowRect(hwndGame, &rect);
            int ww = rect.right - rect.left;
            int wh = rect.bottom - rect.top;
            SetWindowPos(Overlay::hwnd, HWND_TOPMOST, rect.left, rect.top, ww, wh, 0);
        }

        int width = GetSystemMetrics(SM_CXSCREEN);
        int height = GetSystemMetrics(SM_CYSCREEN);
        if (hwndGame) {
            RECT rc;
            GetClientRect(hwndGame, &rc);
            width = rc.right - rc.left;
            height = rc.bottom - rc.top;
        }

        uintptr_t UWorld = ReadMem<uintptr_t>(baseAddr + GWorld);
        if (UWorld) {
            uintptr_t gameInst = ReadMem<uintptr_t>(UWorld + UWorld_OwningGameInstance);
            uintptr_t localPlayersArray = ReadMem<uintptr_t>(gameInst + UGameInstance_LocalPlayers);
            uintptr_t localPlayer = ReadMem<uintptr_t>(localPlayersArray);
            uintptr_t playerController = ReadMem<uintptr_t>(localPlayer + ULocalPlayer_PlayerController);
            uintptr_t localPawn = ReadMem<uintptr_t>(playerController + APlayerController_AcknowledgedPawn);

            uintptr_t cameraManager = ReadMem<uintptr_t>(playerController + APlayerController_PlayerCameraManager);
            
            FVector camLoc = ReadMem<FVector>(cameraManager + APlayerCameraManager_CameraCachePrivate + 0x10 + 0x0);
            FRotator camRot = ReadMem<FRotator>(cameraManager + APlayerCameraManager_CameraCachePrivate + 0x10 + 0xC);
            float fov = ReadMem<float>(cameraManager + APlayerCameraManager_CameraCachePrivate + 0x10 + 0x18);

            uintptr_t gameState = ReadMem<uintptr_t>(UWorld + 0x120);
            if (!gameState) continue; // Fallback or wait
            
            TArray playerArray = ReadMem<TArray>(gameState + 0x238);
            int actorsCount = playerArray.Count;

            ImDrawList* drawList = ImGui::GetBackgroundDrawList();

            float screenCenterX = width / 2.0f;
            float screenCenterY = height / 2.0f;

            uint8_t myTeam = 255;
            if (localPawn) {
                myTeam = ReadMem<uint8_t>(localPawn + 0x0518);
            }

            float closestDistance = FLT_MAX;
            FRotator targetRotation = {0};
            bool hasTarget = false;

            if (GameState::aimbot) {
                drawList->AddCircle(ImVec2(screenCenterX, screenCenterY), GameState::aimFov, ImColor(255, 255, 255, 100), 64, 1.0f);
            }

            float bulletSpeed = 0.0f;
            if (GameState::aimbot && GameState::aimPrediction && localPawn) {
                uintptr_t ps = ReadMem<uintptr_t>(localPawn + APawn_PlayerState);
                if (ps) {
                    uintptr_t weaponAttrSet = ReadMem<uintptr_t>(ps + 0x0350);
                    if (weaponAttrSet) {
                        bulletSpeed = ReadMem<float>(weaponAttrSet + 0x0054);
                    }
                }
            }

            for (int i = 0; i < actorsCount; ++i) {
                uintptr_t playerState = ReadMem<uintptr_t>(playerArray.Data + (i * 8));
                if (!playerState) continue;

                uintptr_t actor = ReadMem<uintptr_t>(playerState + 0x280); // PawnPrivate
                if (!actor || actor == localPawn) continue;

                uintptr_t rootComponent = ReadMem<uintptr_t>(actor + AActor_RootComponent);
                if (!rootComponent) continue;

                bool isDead = ReadMem<bool>(actor + 0x0508);
                uint8_t team = ReadMem<uint8_t>(actor + 0x0518);

                if (GameState::ignoreDead && isDead) continue;
                if (GameState::teamCheck && localPawn && myTeam != 255 && myTeam == team) continue;

                uintptr_t mesh = ReadMem<uintptr_t>(actor + ACharacter_Mesh);
                if (!mesh) continue;

                if (GameState::wallCheck) {
                    uint8_t bRecentlyRendered = ReadMem<uint8_t>(mesh + USkinnedMeshComponent_bRecentlyRendered);
                    bool isVisible = (bRecentlyRendered & (1 << 6)) != 0;
                    if (!isVisible) continue;
                }

                // Read Health
                float health = 100.0f, maxHealth = 100.0f;
                uintptr_t attrSet = ReadMem<uintptr_t>(playerState + 0x0348);
                if (attrSet) {
                    health = ReadMem<float>(attrSet + 0x0068 + 4);
                    maxHealth = ReadMem<float>(attrSet + 0x0088 + 4);
                }


                if (GameState::espEnabled) {
                    FVector headLoc = GetBoneWithRotation(mesh, GameState::aimBone);
                    FVector rootLoc = GetBoneWithRotation(mesh, 0);

                    FVector sHead, sRoot;
                    if (WorldToScreen(headLoc, camLoc, camRot, fov, width, height, sHead) &&
                        WorldToScreen(rootLoc, camLoc, camRot, fov, width, height, sRoot)) {
                        
                        // Bounding Box Dynamic
                        float boxHeight = abs(sRoot.Y - sHead.Y) * 1.3f; // add space for actual head
                        float boxWidth = boxHeight * 0.6f;
                        float boxLeft = sRoot.X - (boxWidth / 2.0f);
                        float boxTop = sHead.Y - (boxHeight * 0.15f); // slight offset over head

                        FVector vDelta = { headLoc.X - camLoc.X, headLoc.Y - camLoc.Y, headLoc.Z - camLoc.Z };
                        float distance3D = sqrtf(vDelta.X * vDelta.X + vDelta.Y * vDelta.Y + vDelta.Z * vDelta.Z);

                        if (GameState::boxEsp) {
                            drawList->AddRect(ImVec2(boxLeft, boxTop), ImVec2(boxLeft + boxWidth, boxTop + boxHeight), ImColor(0, 255, 0), 0, 0, 1.5f);
                        }

                        if (GameState::healthEsp) {
                            float hpPercent = (maxHealth > 0.0f) ? (health / maxHealth) : 1.0f;
                            if (hpPercent < 0.0f) hpPercent = 0.0f;
                            if (hpPercent > 1.0f) hpPercent = 1.0f;
                            
                            float barWidth = 4.0f;
                            float barLeft = boxLeft - barWidth - 4.0f;
                            
                            drawList->AddRectFilled(ImVec2(barLeft, boxTop), ImVec2(barLeft + barWidth, boxTop + boxHeight), ImColor(0, 0, 0, 150));
                            drawList->AddRectFilled(ImVec2(barLeft, boxTop + boxHeight - (boxHeight * hpPercent)), ImVec2(barLeft + barWidth, boxTop + boxHeight), ImColor((int)(255 * (1.0f - hpPercent)), (int)(255 * hpPercent), 0, 255));
                        }

                        if (GameState::distanceEsp) {
                            char distStr[32];
                            snprintf(distStr, sizeof(distStr), "%.1fm", distance3D / 100.0f);
                            drawList->AddText(ImVec2(boxLeft + (boxWidth / 2.0f) - 10.0f, boxTop + boxHeight + 2.0f), ImColor(255, 255, 255), distStr);
                        }

                        if (GameState::snaplines) {
                            drawList->AddLine(ImVec2(screenCenterX, height), ImVec2(sRoot.X, sRoot.Y), ImColor(255, 0, 0), 1.0f);
                        }

                        if (GameState::skeletonEsp) {
                            // Quick Bone Mappings (UpGun UE4 Mannequin)
                            int bones[] = { 
                                8, 7, // Head -> Neck
                                7, 4, // Neck -> Spine
                                4, 1, // Spine -> Pelvis
                                // Arms
                                7, 34, 34, 35, 35, 36, // Left Arm
                                7, 11, 11, 12, 12, 13, // Right Arm
                                // Legs
                                1, 55, 55, 56, 56, 57, // Left Leg
                                1, 51, 51, 52, 52, 53  // Right Leg
                            };

                            for (int b = 0; b < sizeof(bones)/sizeof(int); b += 2) {
                                FVector b1 = GetBoneWithRotation(mesh, bones[b]);
                                FVector b2 = GetBoneWithRotation(mesh, bones[b + 1]);
                                FVector sb1, sb2;
                                if (WorldToScreen(b1, camLoc, camRot, fov, width, height, sb1) &&
                                    WorldToScreen(b2, camLoc, camRot, fov, width, height, sb2)) {
                                    drawList->AddLine(ImVec2(sb1.X, sb1.Y), ImVec2(sb2.X, sb2.Y), ImColor(255, 255, 255), 1.0f);
                                }
                            }
                        }

                        if (GameState::aimbot) {
                            float dist = GetDistance2D(screenCenterX, screenCenterY, sHead.X, sHead.Y);
                            if (dist < GameState::aimFov && dist < closestDistance) {
                                
                                FVector aimPos = headLoc;
                                
                                if (GameState::aimPrediction && bulletSpeed > 100.0f) {
                                    uintptr_t movementComp = ReadMem<uintptr_t>(actor + 0x0288);
                                    if (movementComp) {
                                        FVector targetVel = ReadMem<FVector>(movementComp + 0x00C4);
                                        float travelTime = distance3D / bulletSpeed;
                                        aimPos.X += targetVel.X * travelTime;
                                        aimPos.Y += targetVel.Y * travelTime;
                                        aimPos.Z += targetVel.Z * travelTime;
                                    }
                                }

                                FVector delta = { aimPos.X - camLoc.X, aimPos.Y - camLoc.Y, aimPos.Z - camLoc.Z };
                                float dist3D = sqrtf(delta.X * delta.X + delta.Y * delta.Y + delta.Z * delta.Z);

                                closestDistance = dist;
                                targetRotation.Pitch = asinf(delta.Z / dist3D) * (180.0f / M_PI);
                                targetRotation.Yaw = atan2f(delta.Y, delta.X) * (180.0f / M_PI);
                                targetRotation.Roll = 0;

                                hasTarget = true;
                            }
                        }
                    }
                }
            }

            bool isShooting = (GetAsyncKeyState(VK_LBUTTON) & 0x8000);
            bool isAiming = (GetAsyncKeyState(VK_RBUTTON) & 0x8000);

            // Apply Aimbot
            if (GameState::aimbot && hasTarget) {
                if (GameState::silentAim) {
                    // Write Target Rotation to ControlRotation (Server knows we look here)
                    WriteMemRot(playerController + APlayerController_ControlRotation, targetRotation);
                    
                    // Freeze Camera Rotation (Player's screen stays at camRot)
                    uintptr_t cameraManager = ReadMem<uintptr_t>(playerController + APlayerController_PlayerCameraManager);
                    if (cameraManager) {
                        WriteMemRot(cameraManager + APlayerCameraManager_POVRotation, camRot);
                    }

                    // AutoShoot if Target in Sight
                    if (GameState::autoShoot) {
                        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                        // Briefly hold then release if needed or just spam
                        // Sleep(10); // Not good in overlay loop
                        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    }
                } else if (isAiming) {
                    // Smoothly move view (Normal Aimbot)
                    FRotator currentRot = camRot;
                    FRotator delta = { targetRotation.Pitch - currentRot.Pitch, targetRotation.Yaw - currentRot.Yaw, 0 };
                    
                    // Normalize angles
                    if (delta.Yaw > 180.f) delta.Yaw -= 360.f;
                    if (delta.Yaw < -180.f) delta.Yaw += 360.f;

                    float smooth = (GameState::aimSmooth > 1.0f) ? GameState::aimSmooth : 1.0f;
                    FRotator finalRot = { currentRot.Pitch + delta.Pitch / smooth, currentRot.Yaw + delta.Yaw / smooth, 0 };
                    
                    WriteMemRot(playerController + APlayerController_ControlRotation, finalRot);
                }
            } else if (GameState::autoShoot && !hasTarget && isShooting) {
                // If we want to allow manual shooting while autoShoot is on, we don't need to do anything.
                // But we should ensure we release mouse button if target lost during autoShoot?
                // Actually, our autoShoot logic above uses click-per-frame.
            }

        }

        Overlay::RenderMenu();
        Overlay::EndFrame();
    }

    Overlay::Shutdown();
    CloseHandle(hProcess);
    return 0;
}
