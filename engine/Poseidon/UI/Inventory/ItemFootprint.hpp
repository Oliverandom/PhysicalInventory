#pragma once

namespace Poseidon
{

struct GridSize
{
    int w = 1;
    int h = 1;
};

class WeaponType;
class MagazineType;

namespace ItemFootprint
{

GridSize Of(const WeaponType *weapon);
GridSize Of(const MagazineType *type);

} // namespace ItemFootprint
} // namespace Poseidon
