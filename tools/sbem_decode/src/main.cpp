#include <iostream>
#include <string>
#include <filesystem>

#include "sbem/sbem.hpp"
#include "samples.hpp"
#include "utils.hpp"

constexpr const char* CMD_INFO = "info";
constexpr const char* CMD_CSV = "csv";

void usage()
{
    printf("Usage: sbem_decode [command] [measurement?] [sbem_file_path]\n");
    printf("  Commands:\n");
    printf("    %8s - Prints information about the SBEM file.\n", CMD_INFO);
    printf("    %8s - Prints decoded samples in CSV format.\n", CMD_CSV);
    printf("\n");
    printf("  Args:\n");
    printf("    measurement - Select measurement to show when using the '%s' command.\n", CMD_CSV);
    printf("        valid values: acc|gyro|magn|ecg|hr|temp|activity|tap\n");
    printf("\n");
    printf("  Examples:\n");
    printf("    sbem_decode info samples.sbem\n");
    printf("        Parse and show information about the SBEM file.\n");
    printf("    sbem_decode csv acc samples.sbem\n");
    printf("        Parse and show decoded acceleration samples.\n");
    printf("\n");
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("Error: Missing command\n");
        usage();
        return EXIT_FAILURE;
    }

    std::string cmd(argv[1]);

    if (cmd != CMD_INFO && cmd != CMD_CSV)
    {
        printf("Error: Unknown command: %10s\n", cmd.c_str());
        usage();
        return EXIT_FAILURE;
    }
    else if (cmd == CMD_INFO && argc != 3)
    {
        printf("Error: Unexpected arguments\n");
        usage();
        return EXIT_FAILURE;
    }
    else if (cmd == CMD_CSV && argc != 4)
    {
        printf("Error: Unexpected arguments\n");
        usage();
        return EXIT_FAILURE;
    }

    std::filesystem::path filepath(argv[argc - 1]);
    SbemDocument sbem;
    auto parseResult = sbem.parse(filepath.c_str());
    if (parseResult != SbemDocument::ParseResult::Success)
    {
        printf("Failed to parse: (%u)\n", parseResult);
        return EXIT_FAILURE;
    }
    Samples samples(sbem);

    if (cmd == CMD_INFO)
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
        utils::printActivitySamples(samples);
        utils::printTapDetectionSamples(samples);
    }

    if (cmd == CMD_CSV)
    {
        std::string meas(argv[2]);

        if (meas == "acc")
            utils::printAccSamplesCSV(samples, std::cout);
        else if (meas == "gyro")
            utils::printGyroSamplesCSV(samples, std::cout);
        else if (meas == "magn")
            utils::printMagnSamplesCSV(samples, std::cout);
        else if (meas == "hr")
            utils::printHRSamplesCSV(samples, std::cout);
        else if (meas == "ecg")
            utils::printECGSamplesCSV(samples, std::cout);
        else if (meas == "temp")
            utils::printTempSamplesCSV(samples, std::cout);
        else if (meas == "activity")
            utils::printActivitySamplesCSV(samples, std::cout);
        else if (meas == "tap")
            utils::printTapDetectionSamplesCSV(samples, std::cout);
        else
        {
            printf("Error: Unknown measurement '%s'.\n", meas.c_str());
            usage();
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
