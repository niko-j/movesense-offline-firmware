#pragma once
#include "wb-resources/resources.h"

class LowPassFilter
{
private:
    wb::FloatVector3D m_b;
    float m_cutoff;

public:
    LowPassFilter(float cutoff = 0.1f);
    wb::FloatVector3D filter(const wb::FloatVector3D& input);
};
