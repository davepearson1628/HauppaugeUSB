#ifndef _TS_RANDOM_ACCESS_FIXER_H_
#define _TS_RANDOM_ACCESS_FIXER_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

class TSRandomAccessFixer
{
  public:
    TSRandomAccessFixer();

    void Reset();

    std::vector<uint8_t> Process(const void *data, size_t len);
    std::vector<uint8_t> Flush();

    uint64_t RecoveryPointsMarked() const { return m_recovery_points_marked; }
    uint64_t ExtraPacketsInserted() const { return m_extra_packets_inserted; }

  private:
    static const size_t TS_SIZE = 188;
    using packet_t = std::array<uint8_t, TS_SIZE>;

    std::vector<uint8_t> m_input;
    std::vector<packet_t> m_segment;
    bool                  m_synced;
    bool                  m_have_segment;

    int                   m_pmt_pid;
    int                   m_video_pid;
    uint8_t               m_video_cc_offset;

    uint64_t              m_recovery_points_marked;
    uint64_t              m_extra_packets_inserted;

    void ProcessPacket(const packet_t &packet,
                       std::vector<uint8_t> &output);
    void FinalizeSegment(std::vector<uint8_t> &output,
                         bool allow_rai_fix = true);
    void EmitPacket(packet_t packet,
                    std::vector<uint8_t> &output,
                    uint8_t cc_offset) const;

    void ParsePAT(const packet_t &packet);
    void ParsePMT(const packet_t &packet);

    bool SegmentHasRecoveryPoint() const;
    bool MarkSegmentRandomAccess(packet_t &extra_packet,
                                 bool &have_extra,
                                 size_t &insert_after);

    static int PacketPID(const packet_t &packet);
    static bool PacketPUSI(const packet_t &packet);
    static bool HasPayload(const packet_t &packet);
    static size_t PayloadOffset(const packet_t &packet);
    static uint8_t ContinuityCounter(const packet_t &packet);
    static void SetContinuityCounter(packet_t &packet, uint8_t cc);

    static size_t AdaptationStuffing(const packet_t &packet);
    static bool RemoveAdaptationStuffing(packet_t &packet, size_t amount);
    static bool AddRAI(packet_t &packet, size_t &payload_loss);

    static bool FindRecoveryPointSEI(const std::vector<uint8_t> &pes_payload);
    static bool SEINALHasRecoveryPoint(const uint8_t *data, size_t len);
    static size_t FindStartCode(const uint8_t *data,
                                size_t len,
                                size_t from,
                                size_t &prefix_len);

    static void AppendPacket(const packet_t &packet,
                             std::vector<uint8_t> &output);
};

#endif
