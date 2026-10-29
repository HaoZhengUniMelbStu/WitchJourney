// Shared look of every UMG element in the plugin: one font family (Project Settings > Hex Sokoban > UI),
// one palette, and the FantasyRPG UI kit skins (/Game/FantasyRPG/PNG) drawn as 9-slice brushes.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

class UTexture2D;
class UWidgetTree;
class UBorder;

/** Skinned brushes from the FantasyRPG kit. */
enum class EHexSkin : uint8
{
	Popup,			// stone-framed dark popup (menus, result card, editor panel)
	ButtonYellow,	// primary
	ButtonPurple,	// secondary
	ButtonGray,		// white plate (title_btn_white) meant to be tinted
	ButtonBlueGray,
	ButtonDark,
	ButtonRed,
	ButtonGreen,
	FrameDefault,	// square slot frames (level cards, palette, HUD card)
	FrameCurrent,	// gold (hovered / focused)
	FrameComplete,	// navy (cleared)
	FrameDim,
	Ribbon,			// purple title ribbon
	RibbonResult,	// big "stage clear" ribbon
	RibbonRed,		// red title ribbon (main menu subtitle)
	ChipDim,		// compact rounded plates for small buttons (pass_reward_bg_*)
	ChipNormal,
	ChipYellow,
	Count
};

struct HEXSOKOBAN_API FHexUIStyle
{
	/** UI font at Size (points). Falls back to the engine font when no UI font is configured. */
	static FSlateFontInfo Font(int32 Size, bool bBold = false);

	/** Bold font with the kit's dark outline (titles, numbers). */
	static FSlateFontInfo OutlinedFont(int32 Size, int32 OutlineSize = 2, const FLinearColor& OutlineColor = FLinearColor(0.02f, 0.015f, 0.04f, 1.f));

	/** Rounded panel brush (Slate rounded box, no texture). */
	static FSlateBrush Panel(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f);

	/** A kit texture by its path under /Game/FantasyRPG/PNG/, e.g. TEXT("06_Stage/stage_star"). Kept loaded. */
	static UTexture2D* Texture(const TCHAR* KitPath);

	/** Plain image brush of a kit texture (DrawSize zero = the texture's size). */
	static FSlateBrush Image(const TCHAR* KitPath, const FVector2D& DrawSize = FVector2D::ZeroVector, const FLinearColor& Tint = FLinearColor::White);

	/** 9-slice skin; Scale shrinks / grows the frame thickness (1 = the art's pixels). */
	static FSlateBrush Skin(EHexSkin Which, float Scale = 1.f, const FLinearColor& Tint = FLinearColor::White);

	/** Padding that keeps content inside a skin's frame (and above a button's drop shadow) at Scale. */
	static FMargin SkinPadding(EHexSkin Which, float Scale = 1.f);

	/** Compose the character-select frame and fixed-size corner ornaments around existing content. */
	static void DecoratePanel(UWidgetTree* Tree, UBorder* Border);

	// Palette (dark fantasy, matches the kit)
	static const FLinearColor Ink;			// main text (ivory)
	static const FLinearColor InkSoft;		// labels (muted lavender)
	static const FLinearColor InkDark;		// text on yellow / light buttons
	static const FLinearColor PanelLight;	// panel fill (dark slate violet)
	static const FLinearColor PanelDark;	// deeper translucent fill (bars, pills)
	static const FLinearColor PanelInset;	// recessed wells (trays, text fields)
	static const FLinearColor Accent;		// gold
	static const FLinearColor Purple;		// kit violet
	static const FLinearColor Good;			// success green
	static const FLinearColor Bad;			// error red
	static const FLinearColor TextLight;	// text on dark panels
	static const FLinearColor ScreenDim;	// full-screen dim behind popups
};
