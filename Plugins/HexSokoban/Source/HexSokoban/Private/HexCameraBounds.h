#pragma once
#include "CoreMinimal.h"
namespace HexCameraBounds
{
inline TArray<FVector2D> Hull(TArray<FVector2D> Points)
{
	Points.Sort([](const FVector2D& A,const FVector2D& B){return A.X==B.X ? A.Y<B.Y : A.X<B.X;});
	TArray<FVector2D> Unique;
	for (const auto& P:Points) if (Unique.IsEmpty() || P!=Unique.Last()) Unique.Add(P);
	if (Unique.Num()<3) return Unique;
	TArray<FVector2D> H;
	auto Cross=[](const FVector2D& A,const FVector2D& B,const FVector2D& C){return FVector2D::CrossProduct(B-A,C-B);};
	for (const auto& P:Unique) {while(H.Num()>1 && Cross(H[H.Num()-2],H.Last(),P)<=0) H.Pop(); H.Add(P);}
	const int32 Lower=H.Num();
	for(int32 I=Unique.Num()-2;I>=0;--I) {while(H.Num()>Lower && Cross(H[H.Num()-2],H.Last(),Unique[I])<=0) H.Pop(); H.Add(Unique[I]);}
	H.Pop(); return H;
}
inline FVector2D Clamp(const FVector2D& P,const TArray<FVector2D>& H)
{
	if (H.IsEmpty()) return P;
	if (H.Num()==1) return H[0];
	bool Inside=H.Num()>2;
	FVector2D Best=H[0]; double Distance=TNumericLimits<double>::Max();
	for(int32 I=0;I<H.Num();++I)
	{
		const FVector2D A=H[I], B=H[(I+1)%H.Num()], Edge=B-A;
		if(FVector2D::CrossProduct(Edge,P-A)<-0.001) Inside=false;
		const FVector2D Q=A+Edge*FMath::Clamp(FVector2D::DotProduct(P-A,Edge)/FMath::Max(Edge.SizeSquared(),0.001),0.0,1.0);
		const double D=(Q-P).SizeSquared(); if(D<Distance){Distance=D;Best=Q;}
	}
	return Inside?P:Best;
}
inline FVector2D Rubber(const FVector2D& P,const FVector2D& Edge,double Limit)
{
	const FVector2D Offset=P-Edge;
	return Edge+Offset/(1.0+Offset.Size()/FMath::Max(1.0,Limit));
}
}
