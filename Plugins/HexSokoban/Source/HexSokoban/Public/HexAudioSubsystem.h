#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "HexAudioSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

/** Session-owned music and UI sound families. Survives map travel, ends with PIE/game. */
UCLASS()
class HEXSOKOBAN_API UHexAudioSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    static UHexAudioSubsystem* Get(const UObject* Context);
    static void Play(const UObject* Context, FName Family, float Volume = 0.6f);
    static void PlayExact(const UObject* Context, FName AssetName, float Volume = 0.6f);
    static void Deal(const UObject* Context, int32 Count);
    void SetMenuQuiet(bool bQuiet) { bQuietMenu = bQuiet; }
private:
    bool TickAudio(float DeltaTime);
    void PlayFamily(FName Family, float Volume);
    UPROPERTY(Transient, BlueprintReadOnly, Category="Audio", meta=(AllowPrivateAccess="true")) TObjectPtr<UAudioComponent> Music;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Audio", meta=(AllowPrivateAccess="true")) TArray<TObjectPtr<USoundBase>> Sounds;
    TMap<FName, int32> ExactSounds;
    TMap<FName, TArray<int32>> Families;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Audio", meta=(AllowPrivateAccess="true")) TMap<FName, int32> SoundPlayCounts;
    TMap<FName, int32> LastChoice;
    FTSTicker::FDelegateHandle TickHandle;
    TWeakObjectPtr<UWorld> LastWorld;
    FRandomStream Random;
    bool bQuietMenu = false;
    float MusicVolume = 0.f;
    int32 PendingDraws = 0;
    float DrawDelay = 0.f;
    float DrawInterval = .09f;
};
