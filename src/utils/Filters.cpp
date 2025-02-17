#include "Filters.hpp"

LowPassFilter::LowPassFilter(float cutoff)
    : m_b(0.0f, 0.0f, 0.0f)
    , m_cutoff(cutoff)
{
}

wb::FloatVector3D LowPassFilter::filter(const wb::FloatVector3D& input)
{
    m_b.x = (1.0f - m_cutoff) * m_b.x + m_cutoff * input.x;
    m_b.y = (1.0f - m_cutoff) * m_b.y + m_cutoff * input.y;
    m_b.z = (1.0f - m_cutoff) * m_b.z + m_cutoff * input.z;
    return input - m_b;
}

void LowPassFilter::reset()
{
    m_b = wb::FloatVector3D(0.0f, 0.0f, 0.0f);
}
