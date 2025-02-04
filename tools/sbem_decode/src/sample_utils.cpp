#include "sample_utils.hpp"

template<typename T>
void printImuSamples(const std::string& name, const std::vector<T>& samples)
{
    uint16_t samplerate = 0;
    if (samples.size() > 1)
    {
        const auto& a = samples.at(0);
        const auto& b = samples.at(1);
        double interval = sample_utils::calculateSampleInterval(a.timestamp, a.measurements.size(), b.timestamp);
        samplerate = sample_utils::calculateSampleRate(interval);
    }

    std::cout << name << " SampleRate(" << samplerate << " Hz) {\n";
    for (const auto& sample : samples)
    {
        std::cout << " @" << sample.timestamp << " [ ";
        for (const auto& v : sample.measurements)
        {
            std::cout
                << "{ X(" << v.x.toFloat()
                << ") Y(" << v.y.toFloat()
                << ") Z(" << v.z.toFloat()
                << ") } ";
        }
        std::cout << "]\n";
    }
    std::cout << "}\n";
}

void sample_utils::printAccSamples(const Samples& samples)
{
    printImuSamples("Acc", samples.acc);
}

void sample_utils::printGyroSamples(const Samples& samples)
{
    printImuSamples("Gyro", samples.gyro);
}

void sample_utils::printMagnSamples(const Samples& samples)
{
    printImuSamples("Magn", samples.magn);
}

void sample_utils::printHRSamples(const Samples& samples)
{
    std::cout << "HR {\n";
    for (const auto& sample : samples.hr)
    {
        std::cout 
            << " @" << sample.timestamp
            << " Average(" << (int) sample.average
            << ")\n";
    }
    std::cout << "}\n";
}

void sample_utils::printRRSamples(const Samples& samples)
{
    std::cout << "R-to-R {\n";
    for (const auto& entries : samples.rr)
    {
        auto values = entries.unpack();
        std::cout << " @" << entries.timestamp << " [ ";
        for (const auto& rr : values)
        {
            std::cout << rr << " ";
        }
        std::cout << "]\n";
    }
    std::cout << "}\n";
}

void sample_utils::printECGSamples(const Samples& samples)
{
    uint16_t samplerate = 0;
    if (samples.ecg.size() > 1)
    {
        const auto& a = samples.ecg.at(0);
        const auto& b = samples.ecg.at(1);
        double interval = calculateSampleInterval(a.timestamp, a.sampleData.size(), b.timestamp);
        samplerate = calculateSampleRate(interval);
    }

    std::cout << "ECG SampleRate(" << samplerate << " Hz) {\n";
    for (const auto& sample : samples.ecg)
    {
        std::cout << " @" << sample.timestamp << " [ ";
        for (const auto& v : sample.sampleData)
        {
            std::cout << v << " ";
        }
        std::cout << "]\n";
    }
    std::cout << "}\n";
}

void sample_utils::printTempSamples(const Samples& samples)
{
    std::cout << "Temp {\n";
    for (const auto& sample : samples.temp)
    {
        std::cout 
            << " @" << sample.timestamp 
            << " Value(" << (int) sample.measurement 
            << ")\n";
    }
    std::cout << "}\n";
}

void sample_utils::printActivitySamples(const Samples& samples)
{
    std::cout << "Activity {\n";
    for (const auto& sample : samples.activity)
    {
        std::cout 
            << " @" << sample.timestamp 
            << " Value(" << sample.activity.toFloat() 
            << ")\n";
    }
    std::cout << "}\n";
}

void sample_utils::printTapDetectionSamples(const Samples& samples)
{
    std::cout << "TapDetection {\n";
    for (const auto& sample : samples.taps)
    {
        std::cout 
            << " @" << sample.timestamp 
            << " Count(" << (int) sample.count 
            << ")\n";
    }
    std::cout << "}\n";
}

