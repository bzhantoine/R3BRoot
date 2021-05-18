/******************************************************************************
 *   Copyright (C) 2019 GSI Helmholtzzentrum für Schwerionenforschung GmbH    *
 *   Copyright (C) 2019 Members of R3B Collaboration                          *
 *                                                                            *
 *             This software is distributed under the terms of the            *
 *                 GNU General Public Licence (GPL) version 3,                *
 *                    copied verbatim in the file "LICENSE".                  *
 *                                                                            *
 * In applying this license GSI does not waive the privileges and immunities  *
 * granted to it by virtue of its status as an Intergovernmental Organization *
 * or submit itself to any jurisdiction.                                      *
 ******************************************************************************/

#include "R3BTofiReader.h"
#include "FairLogger.h"
#include "FairRootManager.h"
#include "R3BTofiMappedData.h"
#include "TClonesArray.h"

extern "C"
{
#include "ext_data_client.h"
#include "ext_h101_tofd.h"
}

#define MAX_TOFI_CARDS (sizeof data->TOFD_TRIGCLI / sizeof data->TOFD_TRIGCLI[0])
#define MAX_TOFI_PLANES (sizeof data->TOFD_P / sizeof data->TOFD_P[0])

R3BTofiReader::R3BTofiReader(EXT_STR_h101_TOFD* data, UInt_t offset)
    : R3BReader("R3BTofiReader")
    , fData(data)
    , fOffset(offset)
    , fLogger(FairLogger::GetLogger())
    , fArray(new TClonesArray("R3BTofiMappedData"))
    , fArrayTrigger(new TClonesArray("R3BTofiMappedData"))
{
}

R3BTofiReader::~R3BTofiReader() {}

Bool_t R3BTofiReader::Init(ext_data_struct_info* a_struct_info)
{
    int ok;

    EXT_STR_h101_TOFD_ITEMS_INFO(ok, *a_struct_info, fOffset, EXT_STR_h101_TOFD, 0);

    if (!ok)
    {
        perror("ext_data_struct_info_item");
        LOG(error) << "Failed to setup structure information.";
        return kFALSE;
    }
    puts("1");
    // Register output array in tree
    FairRootManager::Instance()->Register("TofiMapped", "Land", fArray, kTRUE);
    FairRootManager::Instance()->Register("TofiTriggerMapped", "Land", fArrayTrigger, kTRUE);
    puts("2");

    // initial clear (set number of hits to 0)
    EXT_STR_h101_TOFD_onion* data = (EXT_STR_h101_TOFD_onion*)fData;
    for (int d = 0; d < MAX_TOFI_PLANES; d++)
    {
        for (int t = 0; t < 2; t++)
        {
            data->TOFD_P[d].T[t].TFLM = 0;
            data->TOFD_P[d].T[t].TFTM = 0;
        }
    }

    return kTRUE;
}

Bool_t R3BTofiReader::Read()
{
    // Convert plain raw data to multi-dimensional array
    EXT_STR_h101_TOFD_onion* data = (EXT_STR_h101_TOFD_onion*)fData;

    // puts("Event");
    for (uint32_t d = 0; d < MAX_TOFI_PLANES; d++)
    {
        for (uint32_t t = 0; t < 2; t++)
        {
            auto const& side = data->TOFD_P[d].T[t];

            //
            // TAMEX3.
            //

            // int32_t first = -1;
            // bool do_print = false;
            // Leading.
            auto numChannels = side.TCLM;
            uint32_t curChannelStart = 0;
            for (uint32_t i = 0; i < numChannels; i++)
            {
                uint32_t channel = side.TCLMI[i];
                uint32_t nextChannelStart = side.TCLME[i];
                for (uint32_t j = curChannelStart; j < nextChannelStart; j++)
                {
                    // printf("Lead %8u %8u %8u %8u\n", d, t, channel, side.TCLv[j] * 5);
                    new ((*fArray)[fArray->GetEntriesFast()])
                        R3BTofiMappedData(d + 1, t + 1, channel, 1, side.TCLv[j], side.TFLv[j]);
                    // if (-1 == first) { first = side.TCLv[j]; }
                    // else if (fabs((int32_t)((side.TCLv[j] - first + 2048 + 1024) & 2047) - 1024) > 400) {
                    //  std::cout << first << '\n';
                    //  std::cout << side.TCLv[j] - first << '\n';
                    //  std::cout << ((side.TCLv[j] - first + 2048 + 1024) & 2047) << '\n';
                    //  std::cout << (int32_t)((side.TCLv[j] - first + 2048 + 1024) & 2047) - 1024 << '\n';
                    //  std::cout << fabs((int32_t)((side.TCLv[j] - first + 2048 + 1024) & 2047) - 1024) << '\n';
                    //  do_print = true;
                    //}
                }
                curChannelStart = nextChannelStart;
            }
            // if (do_print) {
            // numChannels = side.TCLM;
            // curChannelStart = 0;
            // for (uint32_t i = 0; i < numChannels; i++)
            //{
            //    uint32_t channel = side.TCLMI[i];
            //    uint32_t nextChannelStart = side.TCLME[i];
            //    for (uint32_t j = curChannelStart; j < nextChannelStart; j++)
            //    {
            // printf("Lead %8u %8u %8u %8u\n", d, t, channel, side.TCLv[j]);
            //    }
            //    curChannelStart = nextChannelStart;
            //}
            //}

            // Trailing.
            numChannels = side.TCTM;
            curChannelStart = 0;
            for (uint32_t i = 0; i < numChannels; i++)
            {
                uint32_t channel = side.TCTMI[i];
                uint32_t nextChannelStart = side.TCTME[i];
                for (uint32_t j = curChannelStart; j < nextChannelStart; j++)
                {
                    // printf("Tail %8u %8u %8u %8u\n", d, t, channel, side.TCTv[j] * 5);
                    new ((*fArray)[fArray->GetEntriesFast()])
                        R3BTofiMappedData(d + 1, t + 1, channel, 2, side.TCTv[j], side.TFTv[j]);
                }
                curChannelStart = nextChannelStart;
            }

        } // for side
    }     // for planes

    // Leading TAMEX trigger times.
    {
        auto numChannels = data->TOFD_TRIGFL;
        for (uint32_t i = 0; i < numChannels; i++)
        {
            uint32_t channel = data->TOFD_TRIGFLI[i];
            new ((*fArrayTrigger)[fArrayTrigger->GetEntriesFast()])
                R3BTofiMappedData(MAX_TOFI_PLANES + 1, 1, channel, 1, data->TOFD_TRIGCLv[i], data->TOFD_TRIGFLv[i]);
        }
    }

    return kTRUE;
}

void R3BTofiReader::Reset()
{
    // Reset the output array
    fArray->Clear();
    fArrayTrigger->Clear();
}

ClassImp(R3BTofiReader)