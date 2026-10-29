#include "HexUIStyle.h"
#include "HexLocalization.h"
#include "HexSokobanSettings.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "Styling/CoreStyle.h"
#include "Engine/Texture2D.h"
#include "Misc/Paths.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

namespace
{
	/** sRGB 0..255 -> linear (Slate colours are linear). Plain pow: safe during static init. */
	FLinearColor SRGB(int32 R, int32 G, int32 B, float A = 1.f)
	{
		auto Channel = [](int32 C) { return FMath::Pow(static_cast<float>(C) / 255.f, 2.2f); };
		return FLinearColor(Channel(R), Channel(G), Channel(B), A);
	}
}

// Dark fantasy palette, sampled from the FantasyRPG kit.
const FLinearColor FHexUIStyle::Ink = SRGB(247, 241, 228);
const FLinearColor FHexUIStyle::InkSoft = SRGB(172, 164, 204);
const FLinearColor FHexUIStyle::InkDark = SRGB(78, 40, 6);
const FLinearColor FHexUIStyle::PanelLight = SRGB(46, 42, 64, 0.97f);
const FLinearColor FHexUIStyle::PanelDark = SRGB(18, 16, 30, 0.82f);
const FLinearColor FHexUIStyle::PanelInset = SRGB(24, 22, 36, 1.f);
const FLinearColor FHexUIStyle::Accent = SRGB(255, 196, 48);
const FLinearColor FHexUIStyle::Purple = SRGB(124, 72, 236);
const FLinearColor FHexUIStyle::Good = SRGB(104, 200, 40, 0.96f);
const FLinearColor FHexUIStyle::Bad = SRGB(226, 56, 56, 0.96f);
const FLinearColor FHexUIStyle::TextLight = SRGB(247, 241, 228, 0.97f);
const FLinearColor FHexUIStyle::ScreenDim = SRGB(8, 6, 16, 0.62f);

namespace
{
	TSharedPtr<const FCompositeFont> GHexUIFont;
	TWeakObjectPtr<UFontFace> GHexUIRegular;
	TWeakObjectPtr<UFontFace> GHexUIBold;
	int32 GHexFontLanguage=INDEX_NONE;

	TSharedPtr<const FCompositeFont> GetComposite()
	{
		// Rebuild if the faces were never loaded or got collected (e.g. after a settings change).
		const int32 Language=FHexLocalization::GetLanguage();
		if (GHexUIFont.IsValid() && GHexFontLanguage==Language && (Language==1 || Language==3 || GHexUIRegular.IsValid()))
		{
			return GHexUIFont;
		}
		GHexFontLanguage=Language;
		if(Language==1 || Language==3)
		{
			const FString Prefix=FPaths::ProjectContentDir()/TEXT("HexSokoban/Localization/Fonts")/(Language==1?TEXT("NotoSansCJKjp-"):TEXT("NotoSansCJKkr-"));
			TSharedRef<FCompositeFont> Composite=MakeShared<FCompositeFont>();
			for(const TCHAR* Weight:{TEXT("Regular"),TEXT("Bold")})
			{
				FTypefaceEntry& Entry=Composite->DefaultTypeface.Fonts.AddDefaulted_GetRef();
				Entry.Name=Weight;
				Entry.Font=FFontData(Prefix+Weight+TEXT(".otf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
			}
			GHexUIFont=Composite;
			return GHexUIFont;
		}
		const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
		UFontFace* Regular = Settings.UIFontRegular.IsNull() ? nullptr : Settings.UIFontRegular.LoadSynchronous();
		UFontFace* Bold = Settings.UIFontBold.IsNull() ? nullptr : Settings.UIFontBold.LoadSynchronous();
		if (!Regular)
		{
			GHexUIFont.Reset();
			return nullptr;
		}
		Regular->AddToRoot();	// referenced by a non-UObject composite font for the rest of the session
		if (Bold)
		{
			Bold->AddToRoot();
		}
		TSharedRef<FCompositeFont> Composite = MakeShared<FCompositeFont>();
		FTypefaceEntry& RegularEntry = Composite->DefaultTypeface.Fonts.AddDefaulted_GetRef();
		RegularEntry.Name = TEXT("Regular");
		RegularEntry.Font = FFontData(Regular);
		FTypefaceEntry& BoldEntry = Composite->DefaultTypeface.Fonts.AddDefaulted_GetRef();
		BoldEntry.Name = TEXT("Bold");
		BoldEntry.Font = FFontData(Bold ? Bold : Regular);
		GHexUIRegular = Regular;
		GHexUIBold = Bold;
		GHexUIFont = Composite;
		return GHexUIFont;
	}
}

FSlateFontInfo FHexUIStyle::Font(int32 Size, bool bBold)
{
	if (TSharedPtr<const FCompositeFont> Composite = GetComposite())
	{
		return FSlateFontInfo(Composite, static_cast<float>(Size), bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
	}
	return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
}

FSlateBrush FHexUIStyle::Panel(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(Fill);
	Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), OutlineWidth);
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	return Brush;
}

FSlateFontInfo FHexUIStyle::OutlinedFont(int32 Size, int32 OutlineSize, const FLinearColor& OutlineColor)
{
	FSlateFontInfo Info = Font(Size, true);
	Info.OutlineSettings.OutlineSize = OutlineSize;
	Info.OutlineSettings.OutlineColor = OutlineColor;
	return Info;
}

// ---------------------------------------------------------------- FantasyRPG kit skins

namespace
{
	struct FSkinDef
	{
		const TCHAR* Path;
		FMargin Slice;		// 9-slice margins in the art's pixels (L, T, R, B)
		FMargin Content;	// content padding in the art's pixels
		FVector2D Size;		// the art's design size. Never read from the texture: a freshly loaded texture can still be
							// compiling in the editor and report a 32x32 placeholder, which tore every frame on first open.
	};

	const FSkinDef& GetSkinDef(EHexSkin Which)
	{
		static const FSkinDef Defs[] = {
			{ TEXT("99_Popup/common_popup"),		FMargin(64.f, 64.f, 64.f, 64.f),	FMargin(44.f, 40.f, 44.f, 42.f),	FVector2D(980.0, 1430.0) },	// thin-frame hi-res version
			{ TEXT("00_Common/common_btn_yellow"),	FMargin(24.f, 22.f, 24.f, 30.f),	FMargin(16.f, 6.f, 16.f, 14.f),	FVector2D(180.0, 111.0) },
			{ TEXT("00_Common/common_btn_purple"),	FMargin(24.f, 22.f, 24.f, 30.f),	FMargin(16.f, 6.f, 16.f, 14.f),	FVector2D(180.0, 111.0) },
			{ TEXT("00_Title/title_btn_white"),	FMargin(22.f, 18.f, 22.f, 24.f),	FMargin(14.f, 6.f, 14.f, 12.f),	FVector2D(179.0, 111.0) },	// ButtonGray: white plate, tinted per use
			{ TEXT("00_Common/common_btn_bluegray"),FMargin(24.f, 22.f, 24.f, 30.f),	FMargin(16.f, 6.f, 16.f, 14.f),	FVector2D(180.0, 111.0) },
			{ TEXT("00_Common/common_btn_dark"),	FMargin(24.f, 22.f, 24.f, 30.f),	FMargin(16.f, 6.f, 16.f, 14.f),	FVector2D(180.0, 111.0) },
			{ TEXT("00_Common/common_btn_red"),		FMargin(24.f, 22.f, 24.f, 30.f),	FMargin(16.f, 6.f, 16.f, 14.f),	FVector2D(180.0, 111.0) },
			{ TEXT("00_Common/common_btn_green"),	FMargin(24.f, 22.f, 24.f, 30.f),	FMargin(16.f, 6.f, 16.f, 14.f),	FVector2D(180.0, 111.0) },
			{ TEXT("06_Stage/stage_frame_default"),	FMargin(38.f, 38.f, 38.f, 44.f),	FMargin(24.f, 22.f, 24.f, 30.f),	FVector2D(198.0, 205.0) },
			{ TEXT("06_Stage/stage_frame_current"),	FMargin(38.f, 38.f, 38.f, 44.f),	FMargin(24.f, 22.f, 24.f, 30.f),	FVector2D(198.0, 205.0) },
			{ TEXT("06_Stage/stage_frame_complete"),FMargin(38.f, 38.f, 38.f, 44.f),	FMargin(24.f, 22.f, 24.f, 30.f),	FVector2D(198.0, 205.0) },
			{ TEXT("06_Stage/stage_frame_dim"),		FMargin(38.f, 38.f, 38.f, 44.f),	FMargin(24.f, 22.f, 24.f, 30.f),	FVector2D(198.0, 205.0) },
			{ TEXT("99_Popup/popup_title_ribbon"),	FMargin(88.f, 0.f, 88.f, 0.f),		FMargin(100.f, 8.f, 100.f, 34.f),	FVector2D(262.0, 105.0) },
			{ TEXT("09_Result/result_title_ribbon"),FMargin(124.f, 0.f, 124.f, 0.f),	FMargin(136.f, 12.f, 136.f, 46.f),	FVector2D(348.0, 136.0) },
			{ TEXT("00_Title/title_ribbon_bg"),		FMargin(76.f, 0.f, 76.f, 0.f),		FMargin(92.f, 10.f, 92.f, 22.f),	FVector2D(481.0, 95.0) },
			{ TEXT("22_BattelPass/pass_reward_bg_dim"),		FMargin(18.f, 18.f, 18.f, 24.f),	FMargin(14.f, 6.f, 14.f, 16.f),	FVector2D(234.0, 239.0) },
			{ TEXT("22_BattelPass/pass_reward_bg_normal"),	FMargin(18.f, 18.f, 18.f, 24.f),	FMargin(14.f, 6.f, 14.f, 16.f),	FVector2D(234.0, 239.0) },
			{ TEXT("22_BattelPass/pass_reward_bg_yellow"),	FMargin(18.f, 18.f, 18.f, 24.f),	FMargin(14.f, 6.f, 14.f, 16.f),	FVector2D(234.0, 239.0) },
		};
		static_assert(UE_ARRAY_COUNT(Defs) == static_cast<int32>(EHexSkin::Count), "one skin def per EHexSkin");
		return Defs[FMath::Clamp(static_cast<int32>(Which), 0, static_cast<int32>(EHexSkin::Count) - 1)];
	}

	TMap<FString, TWeakObjectPtr<UTexture2D>>& TextureCache()
	{
		static TMap<FString, TWeakObjectPtr<UTexture2D>> Cache;
		return Cache;
	}
}

UTexture2D* FHexUIStyle::Texture(const TCHAR* KitPath)
{
	const FString Key(KitPath);
	if (UTexture2D* Cached = TextureCache().FindRef(Key).Get())
	{
		return Cached;
	}
	const FString Name = FPaths::GetBaseFilename(Key);
	const FString ObjectPath = FString::Printf(TEXT("/Game/FantasyRPG/PNG/%s.%s"), *Key, *Name);
	UTexture2D* Loaded = LoadObject<UTexture2D>(nullptr, *ObjectPath);
	if (Loaded)
	{
#if WITH_EDITOR
		// In the editor a texture loaded for the first time may still be compiling and draws as a placeholder.
		FTextureCompilingManager::Get().FinishCompilation({ Loaded });
#endif
		Loaded->AddToRoot();	// UI art: tiny, used everywhere, keep it resident
		TextureCache().Add(Key, Loaded);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("HexUIStyle: missing kit texture %s"), *ObjectPath);
	}
	return Loaded;
}

FSlateBrush FHexUIStyle::Image(const TCHAR* KitPath, const FVector2D& DrawSize, const FLinearColor& Tint)
{
	FSlateBrush Brush;
	UTexture2D* Tex = Texture(KitPath);
	Brush.SetResourceObject(Tex);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = !DrawSize.IsNearlyZero() ? DrawSize
		: (Tex ? FVector2D(Tex->GetSizeX(), Tex->GetSizeY()) : FVector2D(32.0, 32.0));
	Brush.TintColor = FSlateColor(Tint);
	return Brush;
}

FSlateBrush FHexUIStyle::Skin(EHexSkin Which, float Scale, const FLinearColor& Tint)
{
	const FSkinDef& Def = GetSkinDef(Which);
	FSlateBrush Brush;
	UTexture2D* Tex = Texture(Def.Path);
	Brush.SetResourceObject(Tex);
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.Tiling = ESlateBrushTileType::NoTile;
	Brush.TintColor = FSlateColor(Tint);
	const FVector2D Size = Def.Size;
	// Margins are fractions of the texture; ImageSize sets how many screen pixels the frame takes.
	Brush.Margin = FMargin(Def.Slice.Left / Size.X, Def.Slice.Top / Size.Y, Def.Slice.Right / Size.X, Def.Slice.Bottom / Size.Y);
	Brush.ImageSize = Size * FMath::Max(0.05f, Scale);
	return Brush;
}

FMargin FHexUIStyle::SkinPadding(EHexSkin Which, float Scale)
{
	const FMargin& Pad = GetSkinDef(Which).Content;
	return FMargin(Pad.Left * Scale, Pad.Top * Scale, Pad.Right * Scale, Pad.Bottom * Scale);
}

void FHexUIStyle::DecoratePanel(UWidgetTree* Tree, UBorder* Border)
{
	check(Tree && Border && Border->GetContent());
	UWidget* Content = Border->GetContent();
	const FMargin ContentPadding = Border->GetPadding();
	Border->SetContent(nullptr);
	FSlateBrush NoBrush;
	NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	Border->SetBrush(NoBrush);
	Border->SetPadding(FMargin(0.f));

	UOverlay* Layers = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Layers->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Border->SetContent(Layers);
	auto AddFillLayer = [Layers](UWidget* Widget, const FMargin& Padding = FMargin(0.f))
	{
		UOverlaySlot* Slot = Layers->AddChildToOverlay(Widget);
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetVerticalAlignment(VAlign_Fill);
		Slot->SetPadding(Padding);
	};

	constexpr float ShadowDepth = 7.f * 0.8f;
	// One silhouette gives the straight base and both rounded corners a continuous
	// shadow. Do not also draw the texture's baked shadow: overlaps darken the joins.
	UBorder* Shadow = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Shadow->SetBrush(Panel(FLinearColor(0.f, 0.f, 0.f, 0.2f), 28.f * 40.f / 64.f));
	Shadow->SetPadding(FMargin(0.f));
	Shadow->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddFillLayer(Shadow, FMargin(0.f, ShadowDepth, 0.f, 0.f));

	// The transparent frame needs a separate, opaque inset. Only the straight edges stretch.
	UBorder* Fill = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Fill->SetBrush(Panel(PanelLight.CopyWithNewOpacity(1.f), 12.f));
	Fill->SetPadding(FMargin(0.f));
	Fill->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddFillLayer(Fill, FMargin(7.f));

	// A complete frame shows through the ornaments' transparent cutouts. Leave
	// those regions to the corner caps and stretch only the four straight edges.
	constexpr float CornerSize = 40.f;
	constexpr float EdgeDepth = 22.4f;
	auto AddEdge = [&](const FBox2f& UV, const FVector2D& Size,
		EHorizontalAlignment Horizontal, EVerticalAlignment Vertical, const FMargin& Padding)
	{
		UImage* Edge = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush Art = Image(TEXT("03_CharacterSelect/character_select_frame"), Size);
		Art.SetUVRegion(UV);
		Edge->SetBrush(Art);
		Edge->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* Slot = Layers->AddChildToOverlay(Edge);
		Slot->SetHorizontalAlignment(Horizontal);
		Slot->SetVerticalAlignment(Vertical);
		Slot->SetPadding(Padding);
	};
	AddEdge(FBox2f(FVector2f(28.f / 190.f, 0.f), FVector2f(162.f / 190.f, 28.f / 253.f)),
		FVector2D(1.f, EdgeDepth), HAlign_Fill, VAlign_Top, FMargin(CornerSize, 0.f, CornerSize, 0.f));
	AddEdge(FBox2f(FVector2f(28.f / 190.f, 225.f / 253.f), FVector2f(162.f / 190.f, 246.f / 253.f)),
		FVector2D(1.f, EdgeDepth - ShadowDepth), HAlign_Fill, VAlign_Bottom, FMargin(CornerSize, 0.f, CornerSize, ShadowDepth));
	AddEdge(FBox2f(FVector2f(0.f, 28.f / 253.f), FVector2f(28.f / 190.f, 225.f / 253.f)),
		FVector2D(EdgeDepth, 1.f), HAlign_Left, VAlign_Fill, FMargin(0.f, CornerSize, 0.f, CornerSize));
	AddEdge(FBox2f(FVector2f(162.f / 190.f, 28.f / 253.f), FVector2f(1.f, 225.f / 253.f)),
		FVector2D(EdgeDepth, 1.f), HAlign_Right, VAlign_Fill, FMargin(0.f, CornerSize, 0.f, CornerSize));

	// Corner caps are the final, non-interactive foreground layer above the content.
	AddFillLayer(Content, ContentPadding);

	// Preserve the entire alpha silhouette, including both pointed tails. The RGB
	// plate in the transparent area is not visible and must not drive UV cropping.
	// Keep the previous root scale (40 / 64) while restoring the full source extent.
	auto AddCorner = [&](bool bBottom, bool bRight)
	{
		UImage* Corner = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		constexpr float ArtScale = 40.f / 64.f;
		FSlateBrush Art = Image(bBottom
			? TEXT("03_CharacterSelect/character_select_frame_deco_1")
			: TEXT("03_CharacterSelect/character_select_frame_deco_0"),
			FVector2D(113.f * ArtScale, (bBottom ? 113.f : 112.f) * ArtScale));
		Art.Mirroring = (bBottom != bRight) ? ESlateBrushMirrorType::Horizontal : ESlateBrushMirrorType::NoMirror;
		Corner->SetBrush(Art);
		Corner->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* Slot = Layers->AddChildToOverlay(Corner);
		Slot->SetHorizontalAlignment(bRight ? HAlign_Right : HAlign_Left);
		Slot->SetVerticalAlignment(bBottom ? VAlign_Bottom : VAlign_Top);
		// The frame's last 7 source rows are drop shadow, not the metal edge.
		// Align lower ornaments to the solid edge at the frame's 0.8 draw scale.
		Slot->SetPadding(FMargin(0.f, 0.f, 0.f, bBottom ? ShadowDepth : 0.f));
	};
	AddCorner(false, false);
	AddCorner(false, true);
	AddCorner(true, false);
	AddCorner(true, true);

}
