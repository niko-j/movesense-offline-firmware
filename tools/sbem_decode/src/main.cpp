#include <iostream>
#include <string>
#include <filesystem>

#include "sbem/sbem.hpp"
#include "samples.hpp"
#include "utils.hpp"

void usage()
{
    printf("Usage: sbem_decode [command] [sbem_file_path]\n");
    printf("  Commands:\n");
    printf("    info    - Prints information about the SBEM file.\n");
    printf("    export  - Exports samples to CSV files.\n");
    printf("\n");
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        printf("Invalid args\n");
        usage();
        return EXIT_FAILURE;
    }

    std::string cmd(argv[1]);
    std::filesystem::path filepath(argv[2]);

    if (cmd != "info" && cmd != "export")
    {
        printf("Unknown command: %s\n", cmd.c_str());
        usage();
        return EXIT_FAILURE;
    }

    SbemDocument sbem;
    auto parseResult = sbem.parse(filepath.c_str());
    if (parseResult != SbemDocument::ParseResult::Success)
    {
        printf("Failed to parse: (%u)\n", parseResult);
        return EXIT_FAILURE;
    }


    Samples samples(sbem);

    if (cmd == "info")
    {
        printf("== SBEM File Info ==\n");
        utils::printHeader(sbem);
        utils::printDescriptors(sbem);
        utils::printDataChunks(sbem);

        printf("== Decoded samples ==\n");
        utils::printAccSamples(samples);
        utils::printGyroSamples(samples);
        utils::printMagnSamples(samples);
        utils::printHRSamples(samples);
        utils::printECGSamples(samples);
        utils::printTempSamples(samples);
    }

    if (cmd == "export")
    {
        std::string basename = filepath.filename().replace_extension("");

        if (samples.acc.size() > 0)
            samples.exportAccSamples(basename + "_acc.csv");

        if (samples.gyro.size() > 0)
            samples.exportGyroSamples(basename + "_gyro.csv");

        if (samples.magn.size() > 0)
            samples.exportMagnSamples(basename + "_magn.csv");

        if (samples.hr.size() > 0)
            samples.exportHRSamples(basename + "_hr.csv");

        if (samples.ecg.size() > 0)
            samples.exportECGSamples(basename + "_ecg.csv");

        if (samples.temp.size() > 0)
            samples.exportTempSamples(basename + "_temp.csv");
    }

    return EXIT_SUCCESS;
}
