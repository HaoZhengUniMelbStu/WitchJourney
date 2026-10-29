#include "HexAudioSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"
#include "GameFramework/GameModeBase.h"

UHexAudioSubsystem* UHexAudioSubsystem::Get(const UObject* Context)
{
    UWorld* World = Context ? Context->GetWorld() : nullptr;
    return World && World->IsGameWorld() && World->GetGameInstance()
        ? World->GetGameInstance()->GetSubsystem<UHexAudioSubsystem>() : nullptr;
}

void UHexAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Random.GenerateNewSeed(); // Separate from the puzzle/deck RNG.
    auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FAssetData> Assets;
    Registry.GetAssetsByPath(TEXT("/Game/SFX/UI"), Assets, true);
    for (const FAssetData& Asset : Assets)
    {
        if (USoundBase* Sound = Cast<USoundBase>(Asset.GetAsset()))
        {
            FString Family = Asset.AssetName.ToString();
            while (!Family.IsEmpty() && FChar::IsDigit(Family[Family.Len()-1])) Family.LeftChopInline(1);
            Family.RemoveFromEnd(TEXT("_"));
            const int32 Index = Sounds.Add(Sound);
            ExactSounds.Add(Asset.AssetName, Index);
            // Pop is intentionally restricted to the three approved variants.
            if (Family != TEXT("Pop") || Asset.AssetName == TEXT("Pop_01") || Asset.AssetName == TEXT("Pop_02") || Asset.AssetName == TEXT("Pop_03"))
                Families.FindOrAdd(FName(*Family)).Add(Index);
        }
    }
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickAudio));
}

void UHexAudioSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    if (Music) { Music->Stop(); Music->DestroyComponent(); Music = nullptr; }
    Super::Deinitialize();
}

void UHexAudioSubsystem::Play(const UObject* Context, FName Family, float Volume)
{
    if (auto* Audio = Get(Context)) Audio->PlayFamily(Family, Volume);
}

void UHexAudioSubsystem::PlayExact(const UObject* Context, FName AssetName, float Volume)
{
    if (auto* Audio = Get(Context))
    {
        if (const int32* Index = Audio->ExactSounds.Find(AssetName))
        {
            ++Audio->SoundPlayCounts.FindOrAdd(AssetName);
            // Navigation can travel on the same click: finish the one-shot across
            // that transition instead of destroying it with the outgoing world.
            UGameplayStatics::SpawnSound2D(Audio->GetWorld(), Audio->Sounds[*Index], Volume, 1.f, 0.f, nullptr, true, true);
        }
    }
}

void UHexAudioSubsystem::Deal(const UObject* Context, int32 Count)
{
    if (auto* Audio = Get(Context)) { Audio->PendingDraws = Count; Audio->DrawDelay = 0.15f; Audio->DrawInterval = Count>30 ? .12f : .075f; }
}

void UHexAudioSubsystem::PlayFamily(FName Family, float Volume)
{
    const TArray<int32>* Choices = Families.Find(Family);
    if (!Choices || Choices->IsEmpty() || !GetWorld()) return;
    int32 Pick = Random.RandRange(0, Choices->Num()-1);
    if (const int32* Last = LastChoice.Find(Family); Last && Choices->Num() > 1 && Pick == *Last)
        Pick = (Pick + Random.RandRange(1, Choices->Num()-1)) % Choices->Num();
    LastChoice.Add(Family, Pick);
    ++SoundPlayCounts.FindOrAdd(Sounds[(*Choices)[Pick]]->GetFName());
    UGameplayStatics::PlaySound2D(GetWorld(), Sounds[(*Choices)[Pick]], Volume);
}

bool UHexAudioSubsystem::TickAudio(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld() || !World->HasBegunPlay()) return true;
    if (LastWorld.Get() != World)
    {
        LastWorld = World;
        // Selection applies only to the menu world; it must not leak into gameplay.
        if (!World->GetMapName().Contains(TEXT("HexMenu"))) bQuietMenu = false;
    }
    if (!Music)
    {
        if (USoundWave* Track = LoadObject<USoundWave>(nullptr, TEXT("/Game/SFX/Music/MainMusic.MainMusic")))
        {
            Track->bLooping = true;
            Music = UGameplayStatics::CreateSound2D(World, Track, 0.f, 1.f, 0.f, nullptr, true, false);
            if (Music) { Music->bIsUISound = true; Music->Play(); }
        }
    }
    const AGameModeBase* Mode = World->GetAuthGameMode();
    const bool bEditor = Mode && Mode->GetClass()->GetName().Contains(TEXT("EditorGameMode"));
    const float Target = (bQuietMenu || bEditor || UGameplayStatics::IsGamePaused(World)) ? 0.22f : 0.4f;
    MusicVolume = FMath::FInterpTo(MusicVolume, Target, FMath::Min(DeltaTime, 0.1f), 3.f);
    if (Music) Music->SetVolumeMultiplier(MusicVolume);
    if (PendingDraws > 0)
    {
        DrawDelay -= DeltaTime;
        if (DrawDelay <= 0.f) { PlayFamily(TEXT("draw_card"), 0.45f); --PendingDraws; DrawDelay = DrawInterval; }
    }
    return true;
}
