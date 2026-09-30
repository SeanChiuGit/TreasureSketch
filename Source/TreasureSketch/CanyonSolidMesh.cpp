#include "CanyonSolidMesh.h"

#include "ProceduralMeshComponent.h"

void BuildCanyonSolidMesh(UProceduralMeshComponent* Mesh, const FCanyonSolidBounds& Bounds,
    TFunctionRef<FCanyonSolidColumn(float, float)> SampleColumn)
{
    Mesh->ClearAllMeshSections();
    const int32 NX = FMath::FloorToInt((Bounds.Max.X - Bounds.Min.X) / Bounds.StepXY + 0.001f);
    const int32 NY = FMath::FloorToInt((Bounds.Max.Y - Bounds.Min.Y) / Bounds.StepXY + 0.001f);
    if (NX < 2 || NY < 2) return;
    const int32 StrideY = NX + 1;
    const int32 StrideZ = (NX + 1) * (NY + 1);
    auto ColumnIndex = [&](int32 X, int32 Y) { return Y * StrideY + X; };
    TArray<FCanyonSolidColumn> Columns;
    Columns.SetNumUninitialized(StrideZ);
    float Highest = Bounds.BottomZ + Bounds.StepZ;
    for (int32 Y = 0; Y <= NY; ++Y)
        for (int32 X = 0; X <= NX; ++X)
        {
            const float WX = Bounds.Min.X + X * Bounds.StepXY;
            const float WY = Bounds.Min.Y + Y * Bounds.StepXY;
            FCanyonSolidColumn& Column = Columns[ColumnIndex(X, Y)];
            Column = SampleColumn(WX, WY);
            Highest = FMath::Max(Highest, Column.SurfaceZ);
        }
    const int32 NZ = FMath::CeilToInt((Highest + Bounds.StepZ - Bounds.BottomZ) / Bounds.StepZ);
    auto Index = [&](int32 X, int32 Y, int32 Z)
    { return Z * StrideZ + ColumnIndex(X, Y); };
    auto Position = [&](int32 X, int32 Y, int32 Z)
    { return FVector(Bounds.Min.X + X * Bounds.StepXY,
        Bounds.Min.Y + Y * Bounds.StepXY, Bounds.BottomZ + Z * Bounds.StepZ); };
    auto Solid = [&](const FCanyonSolidColumn& Column, float Z)
    {
        const float Q = FMath::Clamp((Z - Column.FloorZ) / Column.Clearance, 0.f, 1.f);
        const float ArchWidth = Column.HalfWidth
            * FMath::Sqrt(FMath::Max(0.f, 1.f - Q * Q));
        const float Void = FMath::Min(FMath::Min(ArchWidth - FMath::Abs(Column.Lateral),
            Z - Column.FloorZ), Column.FloorZ + Column.Clearance - Z);
        return FMath::Min(FMath::Min(Column.SurfaceZ - Z,
            Z - Bounds.BottomZ + Bounds.StepZ), -Void);
    };
    TArray<float> Values;
    Values.SetNumUninitialized(StrideZ * (NZ + 1));
    for (int32 Z = 0; Z <= NZ; ++Z)
        for (int32 Y = 0; Y <= NY; ++Y)
            for (int32 X = 0; X <= NX; ++X)
                Values[Index(X, Y, Z)] = Solid(Columns[ColumnIndex(X, Y)],
                    Bounds.BottomZ + Z * Bounds.StepZ);

    TArray<FVector> Vertices, Normals;
    TArray<int32> Triangles;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    const int32 Corners[8][3] = { {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
        {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1} };
    const int32 Tets[6][4] = { {0,5,1,6}, {0,1,2,6}, {0,2,3,6},
        {0,3,7,6}, {0,7,4,6}, {0,4,5,6} };
    auto AddTriangle = [&](FVector A, FVector B, FVector C, const FVector& RockWitness)
    {
        FVector Geometric = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
        if (Geometric.IsNearlyZero()) return;
        const FVector Center = (A + B + C) / 3.f;
        if (FVector::DotProduct(Geometric, RockWitness - Center) < 0.f)
        {
            Swap(B, C);
            Geometric *= -1.f;
        }
        const FVector Outward = -Geometric;
        const int32 Base = Vertices.Num();
        for (const FVector& Vertex : { A, B, C })
        {
            Vertices.Add(Vertex);
            Normals.Add(Outward);
            UVs.Add(FVector2D(Vertex.X / 500.f, Vertex.Y / 500.f));
            Colors.Add(FLinearColor::White);
            Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
        }
        Triangles.Append({ Base, Base + 1, Base + 2 });
    };

    for (int32 Z = 0; Z < NZ; ++Z)
        for (int32 Y = 0; Y < NY; ++Y)
            for (int32 X = 0; X < NX; ++X)
            {
                FVector P[8]; float F[8];
                for (int32 C = 0; C < 8; ++C)
                {
                    const int32 CX = X + Corners[C][0];
                    const int32 CY = Y + Corners[C][1];
                    const int32 CZ = Z + Corners[C][2];
                    P[C] = Position(CX, CY, CZ);
                    F[C] = Values[Index(CX, CY, CZ)];
                }
                for (const auto& Tet : Tets)
                {
                    int32 Rock[4], Air[4], RockCount = 0, AirCount = 0;
                    for (int32 I = 0; I < 4; ++I)
                    {
                        const int32 Corner = Tet[I];
                        if (F[Corner] > 0.f) Rock[RockCount++] = Corner;
                        else Air[AirCount++] = Corner;
                    }
                    if (RockCount == 0 || AirCount == 0) continue;
                    auto Cut = [&](int32 A, int32 B)
                    { return FMath::Lerp(P[A], P[B], F[A] / (F[A] - F[B])); };
                    if (RockCount == 1)
                        AddTriangle(Cut(Rock[0], Air[0]), Cut(Rock[0], Air[1]),
                            Cut(Rock[0], Air[2]), P[Rock[0]]);
                    else if (AirCount == 1)
                        AddTriangle(Cut(Air[0], Rock[0]), Cut(Air[0], Rock[1]),
                            Cut(Air[0], Rock[2]), P[Rock[0]]);
                    else
                    {
                        const FVector A = Cut(Rock[0], Air[0]);
                        const FVector B = Cut(Rock[0], Air[1]);
                        const FVector C = Cut(Rock[1], Air[1]);
                        const FVector D = Cut(Rock[1], Air[0]);
                        AddTriangle(A, B, C, P[Rock[0]]);
                        AddTriangle(A, C, D, P[Rock[0]]);
                    }
                }
            }
    Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals,
        UVs, Colors, Tangents, true);
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh->SetCollisionResponseToAllChannels(ECR_Block);
}
