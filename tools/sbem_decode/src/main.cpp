#include <iostream>
#include <string>

#include "sample_utils.hpp"
#include "sample_csv.hpp"
#include "sbem_loader.hpp"
#include "sbem_utils.hpp"

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
    printf("        valid values: acc|gyro|magn|ecg|hr|rr|temp|activity|tap\n");
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
        printf("Error: Unknown command: %s\n", cmd.c_str());
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
        printf("Failed to parse: %s (%u)\n", filepath.c_str(), parseResult);
        return EXIT_FAILURE;
    }
    Samples samples(sbem);

    if (cmd == CMD_INFO)
    {
        printf("== SBEM File Info ==\n");
        sbem_utils::printHeader(sbem, std::cout);
        sbem_utils::printDescriptors(sbem, std::cout);
        sbem_utils::printDataChunks(sbem, std::cout);

        printf("== Decoded samples ==\n");
        sample_utils::printAccSamples(samples);
        sample_utils::printGyroSamples(samples);
        sample_utils::printMagnSamples(samples);
        sample_utils::printHRSamples(samples);
        sample_utils::printRRSamples(samples);
        sample_utils::printECGSamples(samples);
        sample_utils::printTempSamples(samples);
        sample_utils::printActivitySamples(samples);
        sample_utils::printTapDetectionSamples(samples);
    }

    if (cmd == CMD_CSV)
    {
        std::string meas(argv[2]);

        if (meas == "acc")
            sample_csv::writeAccSamplesCSV(samples, std::cout);
        else if (meas == "gyro")
            sample_csv::writeGyroSamplesCSV(samples, std::cout);
        else if (meas == "magn")
            sample_csv::writeMagnSamplesCSV(samples, std::cout);
        else if (meas == "hr")
            sample_csv::writeHRSamplesCSV(samples, std::cout);
        else if (meas == "rr")
            sample_csv::writeRRSamplesCSV(samples, std::cout);
        else if (meas == "ecg")
            sample_csv::writeECGSamplesCSV(samples, std::cout);
        else if (meas == "temp")
            sample_csv::writeTempSamplesCSV(samples, std::cout);
        else if (meas == "activity")
            sample_csv::writeActivitySamplesCSV(samples, std::cout);
        else if (meas == "tap")
            sample_csv::writeTapDetectionSamplesCSV(samples, std::cout);
        else
        {
            printf("Error: Unknown measurement '%s'.\n", meas.c_str());
            usage();
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
