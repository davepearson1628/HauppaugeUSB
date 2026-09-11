/*  -*- Mode: c++ -*-
 *
 * Copyright (C) John Poet 2018
 *
 * This file is part of HauppaugeUSB.
 *
 * HauppaugeUSB is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HauppaugeUSB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HauppaugeUSB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include <thread>
#include <math.h>

#include "registryif.h"
#include "audio_CX2081x.h"
#include "device_EDID.h"
#include "hauppauge2_EDID.h"

// Common/Rx/audio_CS8416.h
#include "audio_CS8416.h"


// ---------------------------------------------------------------------
// CS8416 registers/bits used for automatic audio-format detection.
// ---------------------------------------------------------------------

static const uint8_t CS8416_DEVICE_ADDR =
    0x22;

static const uint8_t CS8416_AFMTD_REG =
    0x0B;

static const uint8_t CS8416_RXERR_REG =
    0x0C;

static const uint8_t CS8416_AFMTD_IEC61937 =
    0x20;

static const uint8_t CS8416_AFMTD_PCM =
    0x40;

static const uint8_t CS8416_RXERR_UNLOCK =
    0x10;


#include "HauppaugeDev.h"
#include "FlipInterlacedFields.h"
#include "Logger.h"


using namespace std;


// =====================================================================
// Construction / destruction
// =====================================================================

HauppaugeDev::HauppaugeDev(
    const Parameters &params)
    :
      m_fd(-1),
      m_rxDev(nullptr),
      m_encDev(nullptr),
      m_fx2(nullptr),
      m_params(params),
      m_video_initialized(-1),
      m_err(false)
{
    configure();
}


HauppaugeDev::~HauppaugeDev(void)
{
    Close();
}


// =====================================================================
// Configure encoder registry settings
// =====================================================================

void HauppaugeDev::configure(void)
{
#if 0
    switch (m_params.audio_rate)
    {
        case HAPI_AUDIO_SAMPLE_RATE_FOLLOW_INPUT:

            m_params.audioSamplerate = -1;

            break;


        case HAPI_AUDIO_SAMPLE_RATE_NONE:

            m_params.audioSamplerate = 0;

            break;


        case HAPI_AUDIO_SAMPLE_RATE_32_KHZ:

            m_params.audioSamplerate = 32000;

            break;


        case HAPI_AUDIO_SAMPLE_RATE_44_1_KHZ:

            m_params.audioSamplerate = 44100;

            break;


        case HAPI_AUDIO_SAMPLE_RATE_48_KHZ:

            m_params.audioSamplerate = 48000;

            break;


        case HAPI_AUDIO_SAMPLE_RATE_96_KHZ:

            m_params.audioSamplerate = 96000;

            break;


        case HAPI_AUDIO_SAMPLE_RATE_192_KHZ:

            m_params.audioSamplerate = 192000;

            break;


        case HAPI_AUDIO_SAMPLE_RATE_16_KHZ:

            m_params.audioSamplerate = 16000;

            break;
    }
#endif


    RegistryAccess::writeDword(
        "VideoCapSource",
        m_params.videoInput
    );


    // RegistryAccess::writeDword(
    //     "RateControl",
    //     m_params.videoRateControl
    // );


    RegistryAccess::writeDword(
        "VideoRateControl",
        m_params.videoRateControl
    );


    RegistryAccess::writeDword(
        "VideoTSBitrate",
        m_params.tsBitrate
    );


    RegistryAccess::writeDword(
        "VideoOutputBitrate",
        m_params.videoBitrate
    );


    RegistryAccess::writeDword(
        "VBRMin",
        m_params.videoVBRMin
    );


    RegistryAccess::writeDword(
        "VBRMax",
        m_params.videoVBRMax
    );


    RegistryAccess::writeDword(
        "VideoProfile",
        m_params.videoProfile
    );


    RegistryAccess::writeDword(
        "Profile",
        m_params.videoProfile
    );


#if 0
    RegistryAccess::writeDword(
        "VideoCodecOutputFormat",
        4
    );

    RegistryAccess::writeDword(
        "Level",
        m_params.videoH264Level
    );

    RegistryAccess::writeDword(
        "VideoLatency",
        m_params.videoLatency
    );

    RegistryAccess::writeDword(
        "Latency",
        m_params.videoLatency
    );
#endif


    RegistryAccess::writeDword(
        "VideoLevel",
        m_params.videoH264Level
    );


    RegistryAccess::writeDword(
        "AudioCapSource",
        m_params.audioInput
    );


    // RegistryAccess::writeDword(
    //     "AudioCapSPDIFInput",
    //     3
    // );


    /*
     * AUTO is resolved after the CS8416 has locked onto the
     * incoming S/PDIF signal.
     *
     * Use AAC as the safe initial encoder value until then.
     */
    HAPI_AUDIO_CODEC configuredAudioCodec =
        (
            m_params.audioCodec ==
            HAPI_AUDIO_CODEC_AUTO
        )
        ?
        HAPI_AUDIO_CODEC_AAC
        :
        m_params.audioCodec;


    RegistryAccess::writeDword(
        "AudioCodecOutputFormat",
        configuredAudioCodec
    );


    // RegistryAccess::writeDword(
    //     "AudioCapMode",
    //     5
    // );


    RegistryAccess::writeDword(
        "AudioOutputSamplingRate",
        m_params.audioSamplerate
    );


    RegistryAccess::writeDword(
        "AudioCapSampleRate",
        m_params.audioSamplerate
    );


    RegistryAccess::writeDword(
        "AudioOutputBitrate",
        m_params.audioBitrate
    );


    // AudioOutputMode
    // -
}


// =====================================================================
// Digital audio setup
// =====================================================================

bool HauppaugeDev::set_digital_audio(
    bool optical)
{
    try
    {
        /*
         * Initialise CS8416 optical receiver
         * if detected.
         */
        audio_CS8416 audio_CS8416(
            *m_fx2
        );


        if (optical)
        {
            audio_CS8416.reset(
                audio_CS8416::
                AudioInput::OPTICAL
            );
        }
        else
        {
            audio_CS8416.reset(
                audio_CS8416::
                AudioInput::HDMI
            );
        }


        INFOLOG
            << "CS8416 initialized.";
    }
    catch (std::runtime_error &e)
    {
        WARNLOG
            << "This device does not have "
            << "a CS8416. "
            << "AC3 not available.";

        return false;
    }


    return true;
}


// =====================================================================
// Audio format
// =====================================================================

bool HauppaugeDev::set_audio_format(
    encoderAudioInFormat_t audioFormat)
{
    bool spdif =
        (
            m_params.audioInput ==
            HAPI_AUDIO_CAPTURE_SOURCE_SPDIF
        );


    if (spdif)
    {
        if (set_digital_audio(true))
        {
            m_fx2->setPortStateBits(
                FX2_PORT_E,
                0x10,
                0x00
            );


            DEBUGLOG
                << "Audio Input: S/PDIF";
        }
        else
        {
            WARNLOG
                << "Can't initialize CS8416 part. "
                << "S/PDIF input is not available";


            spdif = false;
        }
    }


    if (!spdif)
    {
        switch (m_params.audioInput)
        {
            case HAPI_AUDIO_CAPTURE_SOURCE_HDMI:
            {
                if (set_digital_audio(false))
                {
                    /*
                     * Device has a CS8416 chip.
                     */
                    if (audioFormat == ENCAIF_AC3)
                    {
                        m_fx2->setPortStateBits(
                            FX2_PORT_E,
                            0x10,
                            0x00
                        );


                        INFOLOG
                            << "'SPDIF' from HDMI "
                            << "via 8416";
                    }
                    else
                    {
                        m_fx2->setPortStateBits(
                            FX2_PORT_E,
                            0x00,
                            0x00
                        );


                        INFOLOG
                            << "I2S audio "
                            << "from ADV7842";
                    }
                }
                else
                {
                    /*
                     * Older device without CS8416.
                     */
                    m_fx2->setPortStateBits(
                        FX2_PORT_E,
                        0,
                        0x18
                    );


                    INFOLOG
                        << "I2S audio "
                        << "from ADV7842";
                }


                INFOLOG
                    << "Audio Input: HDMI";


                break;
            }


            case HAPI_AUDIO_CAPTURE_SOURCE_LR:

            default:
            {
                if (m_params.audioBoost)
                {
                    /*
                     * Set audio input via RCA
                     * with audio boost.
                     */
                    m_fx2->setPortStateBits(
                        FX2_PORT_E,
                        0x18,
                        0
                    );


                    INFOLOG
                        << "Audio Input: RCA "
                        << "with boost";
                }
                else
                {
                    /*
                     * Set audio input via RCA.
                     */
                    m_fx2->setPortStateBits(
                        FX2_PORT_E,
                        0x08,
                        0x10
                    );


                    INFOLOG
                        << "Audio Input: RCA";
                }


                break;
            }
        }
    }


    INFOLOG
        << "Audio codec: "
        << (
            audioFormat == ENCAIF_AC3
            ?
            "AC3"
            :
            "AUTO"
        );


    return true;
}


// =====================================================================
// Configure audio format
// =====================================================================

encoderAudioInFormat_t HauppaugeDev::configure_audio_format(void)
{
    encoderAudioInFormat_t audioFormat =
        (
            m_params.audioCodec ==
            HAPI_AUDIO_CODEC_AC3
        )
        ?
        ENCAIF_AC3
        :
        ENCAIF_AUTO;


    /*
     * HAPI_AUDIO_CODEC_AUTO:
     *
     * The TV360 is configured for "Follow Content".
     *
     * On its S/PDIF output the CS8416 sees ordinary stereo as
     * PCM and Dolby Digital as an IEC61937 compressed stream.
     *
     * Wait for the CS8416 receiver to lock, then select the
     * existing Hauppauge AAC or AC3 path automatically.
     */
    if (
        m_params.audioCodec ==
            HAPI_AUDIO_CODEC_AUTO
        &&
        m_params.audioInput ==
            HAPI_AUDIO_CAPTURE_SOURCE_SPDIF
    )
    {
        HAPI_AUDIO_CODEC detectedCodec =
            HAPI_AUDIO_CODEC_AAC;


        bool formatDetected =
            false;


        try
        {
            audio_CS8416 cs8416(
                *m_fx2
            );


            cs8416.reset(
                audio_CS8416::
                AudioInput::OPTICAL
            );


            for (
                unsigned attempt = 0;
                attempt < 20;
                ++attempt
            )
            {
                uint8_t afmtdReg =
                    CS8416_AFMTD_REG;


                uint8_t rxerrReg =
                    CS8416_RXERR_REG;


                uint8_t afmtd =
                    0;


                uint8_t rxerr =
                    0;


                bool afmtdOK =
                    m_fx2->I2CWriteRead(
                        CS8416_DEVICE_ADDR,
                        &afmtdReg,
                        1,
                        &afmtd,
                        1
                    );


                bool rxerrOK =
                    m_fx2->I2CWriteRead(
                        CS8416_DEVICE_ADDR,
                        &rxerrReg,
                        1,
                        &rxerr,
                        1
                    );


                if (
                    afmtdOK
                    &&
                    rxerrOK
                    &&
                    !(
                        rxerr
                        &
                        CS8416_RXERR_UNLOCK
                    )
                )
                {
                    if (
                        afmtd
                        &
                        CS8416_AFMTD_IEC61937
                    )
                    {
                        detectedCodec =
                            HAPI_AUDIO_CODEC_AC3;


                        audioFormat =
                            ENCAIF_AC3;


                        formatDetected =
                            true;


                        INFOLOG
                            << "CS8416 detected "
                            << "IEC61937 audio - "
                            << "selecting AC3 encoder";


                        break;
                    }


                    if (
                        afmtd
                        &
                        CS8416_AFMTD_PCM
                    )
                    {
                        detectedCodec =
                            HAPI_AUDIO_CODEC_AAC;


                        audioFormat =
                            ENCAIF_AUTO;


                        formatDetected =
                            true;


                        INFOLOG
                            << "CS8416 detected "
                            << "PCM audio - "
                            << "selecting AAC encoder";


                        break;
                    }
                }


                std::this_thread::
                    sleep_for(
                        std::chrono::
                        milliseconds(100)
                    );
            }
        }
        catch (std::runtime_error &e)
        {
            WARNLOG
                << "Unable to initialise "
                << "CS8416 for automatic "
                << "audio detection.";
        }


        if (!formatDetected)
        {
            detectedCodec =
                HAPI_AUDIO_CODEC_AAC;


            audioFormat =
                ENCAIF_AUTO;


            WARNLOG
                << "Unable to determine "
                << "S/PDIF audio format - "
                << "falling back to AAC";
        }


        RegistryAccess::writeDword(
            "AudioCodecOutputFormat",
            detectedCodec
        );
    }


    /*
     * HDMI AUTO detection.
     *
     * On the Colossus 2 the ADV7842 receiver reports:
     *
     *     nonPCM=false  -> stereo PCM
     *     nonPCM=true   -> compressed Dolby Digital
     *
     * Use that indication to select the same existing
     * AAC / AC3 paths.
     */
    else if (
        m_params.audioCodec ==
            HAPI_AUDIO_CODEC_AUTO
        &&
        m_params.audioInput ==
            HAPI_AUDIO_CAPTURE_SOURCE_HDMI
    )
    {
        HAPI_AUDIO_CODEC detectedCodec =
            HAPI_AUDIO_CODEC_AAC;


        bool formatDetected =
            false;


        for (
            unsigned attempt = 0;
            attempt < 20;
            ++attempt
        )
        {
            receiverAudioParams_t ap;


            ap.sampleRate =
                0;


            ap.nonPCM =
                false;


            ap.audioType =
                0;


            for (
                unsigned i = 0;
                i < 5;
                ++i
            )
            {
                ap.channelStatus[i] =
                    0;
            }


            if (
                m_rxDev->getAudioParams(
                    &ap
                )
                &&
                ap.sampleRate > 0
            )
            {
                if (ap.nonPCM)
                {
                    detectedCodec =
                        HAPI_AUDIO_CODEC_AC3;


                    audioFormat =
                        ENCAIF_AC3;


                    formatDetected =
                        true;


                    INFOLOG
                        << "ADV7842 detected "
                        << "non-PCM HDMI audio - "
                        << "selecting AC3 encoder";


                    break;
                }


                detectedCodec =
                    HAPI_AUDIO_CODEC_AAC;


                audioFormat =
                    ENCAIF_AUTO;


                formatDetected =
                    true;


                INFOLOG
                    << "ADV7842 detected "
                    << "PCM HDMI audio - "
                    << "selecting AAC encoder";


                break;
            }


            std::this_thread::
                sleep_for(
                    std::chrono::
                    milliseconds(100)
                );
        }


        if (!formatDetected)
        {
            detectedCodec =
                HAPI_AUDIO_CODEC_AAC;


            audioFormat =
                ENCAIF_AUTO;


            WARNLOG
                << "Unable to determine "
                << "HDMI audio format - "
                << "falling back to AAC";
        }


        RegistryAccess::writeDword(
            "AudioCodecOutputFormat",
            detectedCodec
        );
    }


    set_audio_format(
        audioFormat
    );



    return audioFormat;
}


// =====================================================================
// Set input format
// =====================================================================

bool HauppaugeDev::set_input_format(
    encoderSource_t source,
    unsigned width,
    unsigned height,
    bool interlaced,
    float vFreq,
    float aspectRatio,
    float audioSampleRate)
{
    INFOLOG
        << "Input width: "
        << width
        << " height: "
        << height
        << " interlaced: "
        << interlaced
        << " vFreq: "
        << vFreq
        << " audio SR: "
        << audioSampleRate;


    encoderAudioInFormat_t audioFormat =
        configure_audio_format();


    // -----------------------------------------------------------------
    // Video coding mode
    // -----------------------------------------------------------------

    if (
        m_params.videoCodingMode >=
            HAPI_CODING_MODE_FIELD
        &&
        m_params.videoCodingMode <=
            HAPI_CODING_MODE_PAFF
    )
    {
        string desc;


        switch (
            m_params.videoCodingMode
        )
        {
            case HAPI_CODING_MODE_FRAME:

                desc =
                    "Frame";

                break;


            case HAPI_CODING_MODE_FIELD:

                desc =
                    "Field";

                break;


            case HAPI_CODING_MODE_MBAFF:

                desc =
                    "MBAFF";

                break;


            case HAPI_CODING_MODE_PAFF:

                desc =
                    "PAFF";

                break;


            default:

                desc =
                    "Unknown";
        }


        if (interlaced)
        {
            INFOLOG
                << "Setting video coding "
                << "mode to "
                << desc;


            RegistryAccess::writeDword(
                "VideoCodingMode",
                m_params.videoCodingMode
            );
        }
        else
        {
            INFOLOG
                << "Progressive video, "
                << "ignoring requested "
                << "video coding mode "
                << desc;
        }
    }
    else
    {
        DEBUGLOG
            << "Progressive video";
    }


    if (
        interlaced
        &&
        m_params.flipFields
    )
    {
        FlipHDMIFields();
    }


    if (
        !m_encDev->setInputFormat(
            source,
            audioFormat,
            width,
            height,
            interlaced,
            vFreq,
            aspectRatio,
            audioSampleRate
        )
    )
    {
        CRITLOG
            << "Cannot set video mode";


        return false;
    }


    return true;
}


// =====================================================================
// Validate video resolution
// =====================================================================

bool HauppaugeDev::valid_resolution(
    int width,
    int height)
{
    if (
        width == 0
        ||
        height == 0
    )
    {
        return false;
    }


    if (
        width != 1920
        &&
        width != 1280
        &&
        width != 720
    )
    {
        return false;
    }


    if (
        height != 1080
        &&
        height != 720
        &&
        height != 480
        &&
        height != 576
    )
    {
        return false;
    }


    return true;
}


// =====================================================================
// Composite input
// =====================================================================

bool HauppaugeDev::init_cvbs(void)
{
    if (
        m_video_initialized !=
        HAPI_VIDEO_CAPTURE_SOURCE_CVBS
    )
    {
        m_rxDev->setInput(
            RXI_CVBS
        );


        std::this_thread::
            sleep_for(
                std::chrono::
                milliseconds(500)
            );


        m_rxDev->showInfo();
        m_encDev->showInfo();
    }


    int idx;


    for (
        idx = 0;
        idx < MAX_RETRY;
        ++idx
    )
    {
        if (
            m_rxDev->hasInputSignal()
        )
        {
            break;
        }


        std::this_thread::
            sleep_for(
                std::chrono::
                milliseconds(50)
            );
    }


    if (idx == MAX_RETRY)
    {
        m_errmsg =
            "No input signal.";


        CRITLOG
            << m_errmsg;


        return false;
    }


    receiverOutputParams_t vp;


    for (
        idx = 0;
        idx < MAX_RETRY;
        ++idx
    )
    {
        /*
         * getOutputParams can return garbage.
         */
        if (
            m_rxDev->getOutputParams(
                &vp
            )
            &&
            valid_resolution(
                vp.width,
                vp.height
            )
        )
        {
            break;
        }


        INFOLOG
            << "Invalid Output Params, "
            << "retrying.";


        std::this_thread::
            sleep_for(
                std::chrono::
                milliseconds(50)
            );
    }


    if (idx == MAX_RETRY)
    {
        m_errmsg =
            "Cannot determine video mode.";


        CRITLOG
            << m_errmsg;


        return false;
    }


    float aspectRatio =
        vp.aspectRatio;


#if 0
    if (enforceAR != 0)
        aspectRatio = enforceAR;
#endif


    if (
        !set_input_format(
            ENCS_CVBS,
            vp.width,
            vp.height,
            vp.interlaced,
            vp.vFreq,
            aspectRatio,
            0
        )
    )
    {
        return false;
    }


    m_rxDev->setOutputBusMode(
        RXOBM_656_10
    );


    INFOLOG
        << "Composite video input "
        << "initialized.";


    m_video_initialized =
        HAPI_VIDEO_CAPTURE_SOURCE_CVBS;


    return true;
}


// =====================================================================
// Component input
// =====================================================================

bool HauppaugeDev::init_component(void)
{
    if (
        m_video_initialized !=
        HAPI_VIDEO_CAPTURE_SOURCE_COMPONENT
    )
    {
        m_rxDev->setInput(
            RXI_COMP
        );


        std::this_thread::
            sleep_for(
                std::chrono::
                milliseconds(500)
            );


        m_rxDev->showInfo();
        m_encDev->showInfo();
    }


    int idx;


    for (
        idx = 0;
        idx < MAX_RETRY;
        ++idx
    )
    {
        if (
            m_rxDev->hasInputSignal()
        )
        {
            break;
        }


        std::this_thread::
            sleep_for(
                std::chrono::
                milliseconds(50)
            );
    }


    if (idx == MAX_RETRY)
    {
        m_errmsg =
            "No input signal.";


        CRITLOG
            << m_errmsg;


        return false;
    }


    receiverOutputParams_t vp;


    for (
        idx = 0;
        idx < MAX_RETRY;
        ++idx
    )
    {
        /*
         * getOutputParams can return garbage.
         */
        if (
            m_rxDev->getOutputParams(
                &vp
            )
            &&
            valid_resolution(
                vp.width,
                vp.height
            )
        )
        {
            break;
        }


        INFOLOG
            << "Invalid Output Params ("
            << vp.width
            << "x"
            << vp.height
            << "), retrying.";


        std::this_thread::
            sleep_for(
                std::chrono::
                milliseconds(50)
            );
    }


    if (idx == MAX_RETRY)
    {
        m_errmsg =
            "Cannot determine video mode.";


        CRITLOG
            << m_errmsg;


        return false;
    }


    float aspectRatio =
        vp.aspectRatio;


#if 0
    if (enforceAR != 0)
        aspectRatio = enforceAR;
#endif


    if (
        !set_input_format(
            ENCS_COMP,
            vp.width,
            vp.height,
            vp.interlaced,
            vp.vFreq,
            aspectRatio,
            0
        )
    )
    {
        return false;
    }


    if (
        (
            vp.height == 480
            &&
            roundf(vp.vFreq) == 60
            &&
            vp.interlaced
        )
        ||
        (
            vp.height == 576
            &&
            roundf(vp.vFreq) == 50
            &&
            vp.interlaced
        )
    )
    {
        m_rxDev->setOutputBusMode(
            RXOBM_656_10_DC
        );
    }
    else
    {
        m_rxDev->setOutputBusMode(
            RXOBM_422_10x2
        );
    }


    audio_CX2081x audio_CX2081x(
        *m_fx2
    );


    audio_CX2081x.init();


    INFOLOG
        << "Component video input "
        << "initialized.";


    m_video_initialized =
        HAPI_VIDEO_CAPTURE_SOURCE_COMPONENT;


    return true;
}


// =====================================================================
// SDI input
// =====================================================================

bool HauppaugeDev::init_sdi(void)
{
    m_errmsg =
        "SDI not supported.";


    CRITLOG
        << m_errmsg;


    return false;
}


// =====================================================================
// HDMI input
// =====================================================================

bool HauppaugeDev::init_hdmi(void)
{
    if (
        m_video_initialized !=
        HAPI_VIDEO_CAPTURE_SOURCE_HDMI
    )
    {
        m_rxDev->setInput(
            RXI_HDMI
        );


        std::this_thread::
            sleep_for(
                std::chrono::
                milliseconds(500)
            );


        m_rxDev->showInfo();
        m_encDev->showInfo();
    }


    int idx;


    for (
        idx = 0;
        idx < MAX_RETRY;
        ++idx
    )
    {
        if (
            m_rxDev->hasInputSignal()
        )
        {
            break;
        }


        std::this_thread::
            sleep_for(
                std::chrono::
                milliseconds(50)
            );
    }


    if (idx == MAX_RETRY)
    {
        m_errmsg =
            "No input signal.";


        CRITLOG
            << m_errmsg;


        return false;
    }


    receiverAudioParams_t ap;


    ap.sampleRate =
        0;


    m_rxDev->getAudioParams(
        &ap
    );


    encoderAudioInFormat_t audioFormat =
        configure_audio_format();


    int vic =
        m_rxDev->getHDMIFormat();


    if (
        vic <= 0
        ||
        !m_encDev->setHDMIFormat(
            vic,
            audioFormat,
            ap.sampleRate
        )
    )
    {
        receiverHDMIParams_t vp;


        for (
            idx = 0;
            idx < MAX_RETRY;
            ++idx
        )
        {
            /*
             * getHDMIParams can return garbage.
             */
            if (
                m_rxDev->getHDMIParams(
                    &vp
                )
                &&
                valid_resolution(
                    vp.width,
                    vp.height
                )
            )
            {
                break;
            }


            INFOLOG
                << "Invalid Output Params, "
                << "retrying.";


            std::this_thread::
                sleep_for(
                    std::chrono::
                    milliseconds(50)
                );
        }


        if (idx == MAX_RETRY)
        {
            m_errmsg =
                "Cannot determine video mode.";


            CRITLOG
                << m_errmsg;


            return false;
        }


        if (
            !set_input_format(
                ENCS_HDMI,
                vp.width,
                vp.height,
                vp.interlaced,
                vp.vFreq,
                16.0f / 9,
                ap.sampleRate
            )
        )
        {
            return false;
        }


        /*
         * There should not be any such modes in DMT...
         * Or maybe bad InfoFrame?
         */
        m_rxDev->setOutputBusMode(
            RXOBM_422_10x2
        );
    }
    else
    {
        switch (vic)
        {
            /*
             * TODO:
             * will be moved into device_t class soon.
             */
            case 6:
            case 7:
            case 21:
            case 22:

                m_rxDev->setOutputBusMode(
                    RXOBM_656_10_DC
                );

                break;


            default:

                m_rxDev->setOutputBusMode(
                    RXOBM_422_10x2
                );
        }
    }


#if 0
    if (enforceAR != 0)
        m_encDev->setHDMIAR(enforceAR);
#endif


    INFOLOG
        << "HDMI video input initialized.";


    m_video_initialized =
        HAPI_VIDEO_CAPTURE_SOURCE_HDMI;


    return true;
}


// =====================================================================
// Open output file
// =====================================================================

bool HauppaugeDev::open_file(
    const string &file_name)
{
    if (file_name == "stdout")
    {
        m_fd =
            1;
    }
    else
    {
        m_fd =
            open(
                file_name.c_str(),
                O_WRONLY
                |
                O_TRUNC
                |
                O_CREAT,
                0666
            );
    }


    if (m_fd < 0)
    {
        m_errmsg =
            "Failed to open '"
            +
            file_name
            +
            "' :"
            +
            strerror(errno);


        ERRORLOG
            << m_errmsg;


        return false;
    }


    INFOLOG
        << "Output to '"
        << file_name
        << "'";


    return true;
}


// =====================================================================
// Port debugging
// =====================================================================

void HauppaugeDev::log_ports(void)
{
    DEBUGLOG
        << hex

        << "\nPORT_A: [0x"
        << m_fx2->getPortDir(
            FX2_PORT_A
        )
        << "] 0x"
        << m_fx2->getPortState(
            FX2_PORT_A
        )

        << "\n\tPORT_B: [0x"
        << m_fx2->getPortDir(
            FX2_PORT_B
        )
        << "] 0x"
        << m_fx2->getPortState(
            FX2_PORT_B
        )

        << "\n\tPORT_C: [0x"
        << m_fx2->getPortDir(
            FX2_PORT_C
        )
        << "] 0x"
        << m_fx2->getPortState(
            FX2_PORT_C
        )

        << "\n\tPORT_D: [0x"
        << m_fx2->getPortDir(
            FX2_PORT_D
        )
        << "] 0x"
        << m_fx2->getPortState(
            FX2_PORT_D
        )

        << "\n\tPORT_E: [0x"
        << m_fx2->getPortDir(
            FX2_PORT_E
        )
        << "] 0x"
        << m_fx2->getPortState(
            FX2_PORT_E
        )

        << "\n\tCTL_PORTS: [0x"
        << m_fx2->getPortDir(
            FX2_CTL_PORTS
        )
        << "] 0x"
        << m_fx2->getPortState(
            FX2_CTL_PORTS
        )

        << dec;
}


// =====================================================================
// Open Hauppauge device
// =====================================================================

bool HauppaugeDev::Open(
    USBWrapper_t &usbio,
    bool ac3,
    DataTransfer::callback_t *cb)
{
    INFOLOG
        << "Opening Hauppauge USB device.";


    if (m_params.verbose)
    {
        string desc;


        usbio.USBDevDesc(
            desc
        );


        DEBUGLOG
            << desc;
    }


    // -----------------------------------------------------------------
    // FX2
    // -----------------------------------------------------------------

    m_fx2 =
        new FX2Device_t(
            usbio
        );


    int idx = 0;


    for (
        ;
        !m_fx2->isUSBHighSpeed()
            &&
        idx < MAX_RETRY;
        ++idx
    )
    {
        m_fx2->stopCPU();


        std::this_thread::
            sleep_for(
                std::chrono::
                milliseconds(1)
            );


        m_fx2->loadFirmware();


        std::this_thread::
            sleep_for(
                std::chrono::
                milliseconds(1)
            );


        m_fx2->startCPU();


        std::this_thread::
            sleep_for(
                std::chrono::
                milliseconds(100)
            );
    }


    INFOLOG
        << "FX2 ready after "
        << idx
        << " tries.";


    log_ports();


    /*
     * Reset CS5340.
     *
     * It will be set back by m_encDev->init().
     */
    m_fx2->setPortStateBits(
        FX2_CTL_PORTS,
        0,
        0x10
    );


    // -----------------------------------------------------------------
    // Encoder
    // -----------------------------------------------------------------

    m_encDev =
        new encoderDev_DXT_t(
            *m_fx2
        );


    if (!m_encDev->init())
    {
        m_errmsg =
            "Encoder init failed.";


        CRITLOG
            << m_errmsg;


        return false;
    }


    // -----------------------------------------------------------------
    // MDP one-shot snapshot support
    //
    // This configures the DataTransfer side-channel.
    //
    // It does NOT alter either the normal output FD or MythTV callback.
    // -----------------------------------------------------------------

    m_encDev->setMDP(
        m_params.mdp,
        m_params.mdpImage,
        m_params.mdpDelay
    );


    if (m_params.mdp)
    {
        INFOLOG
            << "MDP snapshot support enabled. "
            << "Image='"
            << m_params.mdpImage
            << "', delay="
            << m_params.mdpDelay
            << "ms, request='"
            << m_params.mdpImage
            << ".request'";
    }


    DEBUGLOG
        << hex

        << "\nPORT_A: 0x"
        << m_fx2->getPortState(
            FX2_PORT_A
        )

        << "\nPORT_E: 0x"
        << m_fx2->getPortState(
            FX2_PORT_E
        )

        << dec;


    std::this_thread::
        sleep_for(
            std::chrono::
            milliseconds(10)
        );


    INFOLOG
        << "encDev ready";


    // -----------------------------------------------------------------
    // Receiver
    // -----------------------------------------------------------------

    m_rxDev =
        new receiver_ADV7842_t(
            *m_fx2
        );


    /*
     * AUTO must advertise AC3 capability so that the TV360
     * is free to send Dolby Digital when "Follow Content"
     * is selected.
     */
    bool ac3Capable =
        ac3
        ||
        (
            m_params.audioCodec ==
            HAPI_AUDIO_CODEC_AUTO
        );


    if (ac3Capable)
    {
#if 1
        m_rxDev->setEDID(
            edidHDPVR2_1080p6050_ac3_fix_rgb,
            sizeof(
                edidHDPVR2_1080p6050_ac3_fix_rgb
            ),
            edidHDPVR2_1080p6050_ac3_fix_rgbSpaLoc
        );


        INFOLOG
            << "Using 1080p6050 "
            << "w/AC3 EDID.";
#else
        m_rxDev->setEDID(
            edidHDPVR2_1080p6050_atmos,
            sizeof(
                edidHDPVR2_1080p6050_ac3_fix_rgb
            ),
            edidHDPVR2_1080p6050_atmos_SPAloc
        );


        INFOLOG
            << "Using 1080p6050 "
            << "w/Atmos EDID.";
#endif
    }
    else
    {
#if 0
        /*
         * Original EDID.
         */
        m_rxDev->setEDID(
            EDID_default,
            sizeof(
                EDID_default
            ),
            EDID_default_SPAloc
        );


        INFOLOG
            << "Using 1080p6050 "
            << "stereo EDID.";
#else
        /*
         * Updated EDID from Hauppauge.
         */
        m_rxDev->setEDID(
            edidHDPVR2_1080p6050_pcm_fix_rgb,
            sizeof(
                edidHDPVR2_1080p6050_pcm_fix_rgb
            ),
            edidHDPVR2_1080p6050_pcm_fix_rgbSpaLoc
        );


        INFOLOG
            << "Using 1080p6050 "
            << "stereo RGB EDID.";
#endif
    }


    m_rxDev->init();


    INFOLOG
        << "rxDev ready";


    // -----------------------------------------------------------------
    // Existing output FD
    // -----------------------------------------------------------------

    if (!m_params.output.empty())
    {
        if (
            !open_file(
                m_params.output
            )
        )
        {
            return false;
        }


        m_encDev->setOutputFD(
            m_fd
        );
    }


    // -----------------------------------------------------------------
    // Existing MythTV callback
    // -----------------------------------------------------------------

    if (cb)
    {
        m_encDev->setWriteCallback(
            *cb
        );
    }


    // audio_CX2081x _audio_CX2081x(*m_fx2);


    INFOLOG
        << "Hauppauge USB device ready.";


    return true;
}


// =====================================================================
// Close device
// =====================================================================

void HauppaugeDev::Close(void)
{
    if (m_rxDev)
    {
        delete m_rxDev;

        m_rxDev =
            nullptr;
    }


    if (m_encDev)
    {
        delete m_encDev;

        m_encDev =
            nullptr;
    }


    if (m_fx2)
    {
        delete m_fx2;

        m_fx2 =
            nullptr;
    }
}


// =====================================================================
// Start encoding
// =====================================================================

bool HauppaugeDev::StartEncoding(void)
{
    switch (
        m_params.videoInput
    )
    {
        case HAPI_VIDEO_CAPTURE_SOURCE_CVBS:

            if (!init_cvbs())
                return false;

            break;


        case HAPI_VIDEO_CAPTURE_SOURCE_COMPONENT:

            if (!init_component())
                return false;

            break;


        case HAPI_VIDEO_CAPTURE_SOURCE_SDI:

            if (!init_sdi())
                return false;

            break;


        case HAPI_VIDEO_CAPTURE_SOURCE_HDMI:

        default:

            if (!init_hdmi())
                return false;

            break;
    }


    if (!m_encDev->startCapture())
    {
        m_errmsg =
            "Encoder start capture failed.";


        ERRORLOG
            << m_errmsg;


        return false;
    }


    log_ports();


    return true;
}


// =====================================================================
// Stop encoding
// =====================================================================

bool HauppaugeDev::StopEncoding(void)
{
    if (!m_encDev->stopCapture())
    {
        m_errmsg =
            "Encoder stop capture failed.";


        WARNLOG
            << m_errmsg;


        return false;
    }


    log_ports();


    return true;
}
