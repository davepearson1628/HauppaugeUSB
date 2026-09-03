#include "TSRandomAccessFixer.h"

#include <algorithm>
#include <cstring>

TSRandomAccessFixer::TSRandomAccessFixer()
{
    Reset();
}

void TSRandomAccessFixer::Reset()
{
    m_input.clear();
    m_segment.clear();
    m_synced = false;
    m_have_segment = false;

    m_pmt_pid = -1;
    m_video_pid = -1;
    m_video_cc_offset = 0;

    m_recovery_points_marked = 0;
    m_extra_packets_inserted = 0;
}

std::vector<uint8_t> TSRandomAccessFixer::Process(const void *data, size_t len)
{
    std::vector<uint8_t> output;

    if (!data || len == 0)
        return output;

    const uint8_t *src = static_cast<const uint8_t *>(data);
    m_input.insert(m_input.end(), src, src + len);

    size_t consumed = 0;

    for (;;)
    {
        const size_t available = m_input.size() - consumed;

        if (!m_synced)
        {
            if (available < TS_SIZE * 2)
                break;

            size_t sync = m_input.size();

            for (size_t i = consumed;
                 i + TS_SIZE < m_input.size();
                 ++i)
            {
                if (m_input[i] == 0x47 &&
                    m_input[i + TS_SIZE] == 0x47)
                {
                    sync = i;
                    break;
                }
            }

            if (sync == m_input.size())
            {
                /*
                 * Keep enough bytes to find a sync pair across the next
                 * callback boundary. Everything before that is passed
                 * through unchanged.
                 */
                if (available > (TS_SIZE * 2 - 1))
                {
                    const size_t keep = TS_SIZE * 2 - 1;
                    const size_t emit_end = m_input.size() - keep;

                    FinalizeSegment(output, false);
                    output.insert(output.end(),
                                  m_input.begin() + consumed,
                                  m_input.begin() + emit_end);
                    consumed = emit_end;
                }
                break;
            }

            if (sync > consumed)
            {
                FinalizeSegment(output, false);
                output.insert(output.end(),
                              m_input.begin() + consumed,
                              m_input.begin() + sync);
                consumed = sync;
            }

            m_synced = true;
        }

        if (m_input.size() - consumed < TS_SIZE)
            break;

        if (m_input[consumed] != 0x47)
        {
            m_synced = false;
            continue;
        }

        packet_t packet;
        std::copy(m_input.begin() + consumed,
                  m_input.begin() + consumed + TS_SIZE,
                  packet.begin());
        consumed += TS_SIZE;

        ProcessPacket(packet, output);
    }

    if (consumed > 0)
        m_input.erase(m_input.begin(), m_input.begin() + consumed);

    return output;
}

std::vector<uint8_t> TSRandomAccessFixer::Flush()
{
    std::vector<uint8_t> output;

    FinalizeSegment(output, true);

    if (!m_input.empty())
    {
        output.insert(output.end(), m_input.begin(), m_input.end());
        m_input.clear();
    }

    m_synced = false;
    return output;
}

void TSRandomAccessFixer::ProcessPacket(const packet_t &packet,
                                        std::vector<uint8_t> &output)
{
    const int pid = PacketPID(packet);

    if (pid == 0x0000)
        ParsePAT(packet);
    else if (m_pmt_pid >= 0 && pid == m_pmt_pid)
        ParsePMT(packet);

    if (m_video_pid >= 0 &&
        pid == m_video_pid &&
        HasPayload(packet) &&
        PacketPUSI(packet))
    {
        if (m_have_segment)
            FinalizeSegment(output, true);

        m_segment.clear();
        m_segment.push_back(packet);
        m_have_segment = true;
        return;
    }

    if (m_have_segment)
    {
        m_segment.push_back(packet);
        return;
    }

    EmitPacket(packet, output, m_video_cc_offset);
}

void TSRandomAccessFixer::FinalizeSegment(std::vector<uint8_t> &output,
                                          bool allow_rai_fix)
{
    if (!m_have_segment)
        return;

    packet_t extra_packet;
    bool have_extra = false;
    size_t insert_after = 0;

    if (allow_rai_fix && SegmentHasRecoveryPoint())
    {
        if (MarkSegmentRandomAccess(extra_packet,
                                    have_extra,
                                    insert_after))
        {
            ++m_recovery_points_marked;
        }
    }

    uint8_t cc_offset = m_video_cc_offset;

    for (size_t i = 0; i < m_segment.size(); ++i)
    {
        packet_t packet = m_segment[i];

        EmitPacket(packet, output, cc_offset);

        if (have_extra && i == insert_after)
        {
            const uint8_t previous_cc =
                static_cast<uint8_t>((ContinuityCounter(packet) +
                                      cc_offset) & 0x0f);

            SetContinuityCounter(extra_packet,
                                 static_cast<uint8_t>((previous_cc + 1) & 0x0f));
            AppendPacket(extra_packet, output);

            cc_offset = static_cast<uint8_t>((cc_offset + 1) & 0x0f);
            ++m_extra_packets_inserted;
        }
    }

    m_video_cc_offset = cc_offset;
    m_segment.clear();
    m_have_segment = false;
}

void TSRandomAccessFixer::EmitPacket(packet_t packet,
                                     std::vector<uint8_t> &output,
                                     uint8_t cc_offset) const
{
    if (m_video_pid >= 0 && PacketPID(packet) == m_video_pid)
    {
        SetContinuityCounter(
            packet,
            static_cast<uint8_t>((ContinuityCounter(packet) + cc_offset) & 0x0f)
        );
    }

    AppendPacket(packet, output);
}

void TSRandomAccessFixer::ParsePAT(const packet_t &packet)
{
    if (!HasPayload(packet) || !PacketPUSI(packet))
        return;

    size_t off = PayloadOffset(packet);
    if (off >= TS_SIZE)
        return;

    const uint8_t pointer = packet[off++];
    off += pointer;

    if (off + 8 > TS_SIZE || packet[off] != 0x00)
        return;

    const size_t section_length =
        static_cast<size_t>(((packet[off + 1] & 0x0f) << 8) |
                            packet[off + 2]);
    const size_t section_end = off + 3 + section_length;

    if (section_end > TS_SIZE || section_length < 9)
        return;

    const size_t entries_end = section_end - 4; // CRC

    for (size_t pos = off + 8; pos + 4 <= entries_end; pos += 4)
    {
        const uint16_t program_number =
            static_cast<uint16_t>((packet[pos] << 8) |
                                  packet[pos + 1]);

        if (program_number == 0)
            continue;

        m_pmt_pid = static_cast<int>(
            ((packet[pos + 2] & 0x1f) << 8) |
             packet[pos + 3]
        );
        return;
    }
}

void TSRandomAccessFixer::ParsePMT(const packet_t &packet)
{
    if (!HasPayload(packet) || !PacketPUSI(packet))
        return;

    size_t off = PayloadOffset(packet);
    if (off >= TS_SIZE)
        return;

    const uint8_t pointer = packet[off++];
    off += pointer;

    if (off + 12 > TS_SIZE || packet[off] != 0x02)
        return;

    const size_t section_length =
        static_cast<size_t>(((packet[off + 1] & 0x0f) << 8) |
                            packet[off + 2]);
    const size_t section_end = off + 3 + section_length;

    if (section_end > TS_SIZE || section_length < 13)
        return;

    const size_t program_info_length =
        static_cast<size_t>(((packet[off + 10] & 0x0f) << 8) |
                            packet[off + 11]);

    size_t pos = off + 12 + program_info_length;
    const size_t streams_end = section_end - 4; // CRC

    while (pos + 5 <= streams_end)
    {
        const uint8_t stream_type = packet[pos];
        const int elementary_pid =
            static_cast<int>(((packet[pos + 1] & 0x1f) << 8) |
                              packet[pos + 2]);
        const size_t es_info_length =
            static_cast<size_t>(((packet[pos + 3] & 0x0f) << 8) |
                                packet[pos + 4]);

        if (stream_type == 0x1b)
        {
            m_video_pid = elementary_pid;
            return;
        }

        pos += 5 + es_info_length;
    }
}

bool TSRandomAccessFixer::SegmentHasRecoveryPoint() const
{
    if (m_video_pid < 0 || m_segment.empty())
        return false;

    std::vector<uint8_t> pes_payload;

    for (size_t i = 0; i < m_segment.size(); ++i)
    {
        const packet_t &packet = m_segment[i];

        if (PacketPID(packet) != m_video_pid || !HasPayload(packet))
            continue;

        const size_t off = PayloadOffset(packet);
        if (off < TS_SIZE)
            pes_payload.insert(pes_payload.end(),
                               packet.begin() + off,
                               packet.end());
    }

    return FindRecoveryPointSEI(pes_payload);
}

bool TSRandomAccessFixer::MarkSegmentRandomAccess(packet_t &extra_packet,
                                                  bool &have_extra,
                                                  size_t &insert_after)
{
    have_extra = false;

    /*
     * Work on a copy. If any structural check fails, the original segment
     * is emitted byte-for-byte unchanged.
     */
    std::vector<packet_t> modified = m_segment;
    std::vector<size_t> payload_packets;
    std::vector<uint8_t> payload;

    for (size_t i = 0; i < modified.size(); ++i)
    {
        packet_t &packet = modified[i];

        if (PacketPID(packet) != m_video_pid || !HasPayload(packet))
            continue;

        payload_packets.push_back(i);

        const size_t off = PayloadOffset(packet);
        payload.insert(payload.end(),
                       packet.begin() + off,
                       packet.end());
    }

    if (payload_packets.empty())
        return false;

    packet_t &first = modified[payload_packets.front()];

    size_t payload_loss = 0;
    if (!AddRAI(first, payload_loss))
        return false;

    size_t recovered = 0;

    /*
     * Prefer reclaiming stuffing from the end of the PES. This keeps the
     * change as local as possible and matches the normal Hauppauge packet
     * layout, where the final payload packet usually carries stuffing.
     */
    for (size_t n = payload_packets.size();
         n > 1 && recovered < payload_loss;
         --n)
    {
        packet_t &packet = modified[payload_packets[n - 1]];
        const size_t available = AdaptationStuffing(packet);
        const size_t take = std::min(available,
                                     payload_loss - recovered);

        if (take > 0 && RemoveAdaptationStuffing(packet, take))
            recovered += take;
    }

    const size_t deficit = payload_loss - recovered;

    if (deficit > 0)
    {
        if (deficit > 182)
            return false;

        extra_packet.fill(0xff);
        extra_packet[0] = 0x47;
        extra_packet[1] = static_cast<uint8_t>((m_video_pid >> 8) & 0x1f);
        extra_packet[2] = static_cast<uint8_t>(m_video_pid & 0xff);
        extra_packet[3] = 0x30; // adaptation + payload, CC filled later

        const size_t adaptation_length = 183 - deficit;
        extra_packet[4] = static_cast<uint8_t>(adaptation_length);

        if (adaptation_length > 0)
        {
            extra_packet[5] = 0x00;
            for (size_t i = 6; i < 5 + adaptation_length; ++i)
                extra_packet[i] = 0xff;
        }

        have_extra = true;
        insert_after = payload_packets.back();
    }

    size_t source = 0;

    for (size_t n = 0; n < payload_packets.size(); ++n)
    {
        packet_t &packet = modified[payload_packets[n]];
        const size_t off = PayloadOffset(packet);
        const size_t capacity = TS_SIZE - off;

        if (source + capacity > payload.size())
            return false;

        std::copy(payload.begin() + source,
                  payload.begin() + source + capacity,
                  packet.begin() + off);
        source += capacity;
    }

    if (have_extra)
    {
        const size_t off = PayloadOffset(extra_packet);
        const size_t capacity = TS_SIZE - off;

        if (capacity != deficit || source + capacity != payload.size())
            return false;

        std::copy(payload.begin() + source,
                  payload.end(),
                  extra_packet.begin() + off);
        source += capacity;
    }

    if (source != payload.size())
        return false;

    m_segment.swap(modified);
    return true;
}

int TSRandomAccessFixer::PacketPID(const packet_t &packet)
{
    return static_cast<int>(((packet[1] & 0x1f) << 8) | packet[2]);
}

bool TSRandomAccessFixer::PacketPUSI(const packet_t &packet)
{
    return (packet[1] & 0x40) != 0;
}

bool TSRandomAccessFixer::HasPayload(const packet_t &packet)
{
    const uint8_t afc = static_cast<uint8_t>((packet[3] >> 4) & 0x03);
    return afc == 1 || afc == 3;
}

size_t TSRandomAccessFixer::PayloadOffset(const packet_t &packet)
{
    const uint8_t afc = static_cast<uint8_t>((packet[3] >> 4) & 0x03);

    if (afc == 1)
        return 4;

    if (afc == 3)
    {
        const size_t off = 5 + packet[4];
        return off <= TS_SIZE ? off : TS_SIZE;
    }

    return TS_SIZE;
}

uint8_t TSRandomAccessFixer::ContinuityCounter(const packet_t &packet)
{
    return static_cast<uint8_t>(packet[3] & 0x0f);
}

void TSRandomAccessFixer::SetContinuityCounter(packet_t &packet, uint8_t cc)
{
    packet[3] = static_cast<uint8_t>((packet[3] & 0xf0) | (cc & 0x0f));
}

size_t TSRandomAccessFixer::AdaptationStuffing(const packet_t &packet)
{
    const uint8_t afc = static_cast<uint8_t>((packet[3] >> 4) & 0x03);
    if (afc != 3)
        return 0;

    const size_t afl = packet[4];
    if (afl == 0 || 5 + afl > TS_SIZE)
        return 0;

    const uint8_t flags = packet[5];
    size_t used = 1; // flags byte

    if (flags & 0x10) used += 6; // PCR
    if (flags & 0x08) used += 6; // OPCR
    if (flags & 0x04) used += 1; // splice countdown

    if (flags & 0x02)
    {
        if (used >= afl)
            return 0;
        const size_t private_len = packet[5 + used];
        used += 1 + private_len;
    }

    if (flags & 0x01)
    {
        if (used >= afl)
            return 0;
        const size_t ext_len = packet[5 + used];
        used += 1 + ext_len;
    }

    if (used > afl)
        return 0;

    for (size_t i = 5 + used; i < 5 + afl; ++i)
    {
        if (packet[i] != 0xff)
            return 0;
    }

    return afl - used;
}

bool TSRandomAccessFixer::RemoveAdaptationStuffing(packet_t &packet,
                                                   size_t amount)
{
    if (amount == 0)
        return true;

    const size_t stuffing = AdaptationStuffing(packet);
    if (stuffing < amount)
        return false;

    const size_t old_afl = packet[4];
    const size_t new_afl = old_afl - amount;

    packet[4] = static_cast<uint8_t>(new_afl);

    /*
     * Only trailing stuffing is removed. The caller subsequently rewrites
     * the complete payload area, so no memmove of the old payload is needed.
     */
    return true;
}

bool TSRandomAccessFixer::AddRAI(packet_t &packet, size_t &payload_loss)
{
    payload_loss = 0;

    if (!HasPayload(packet))
        return false;

    const uint8_t afc = static_cast<uint8_t>((packet[3] >> 4) & 0x03);

    if (afc == 1)
    {
        packet[3] = static_cast<uint8_t>((packet[3] & 0xcf) | 0x30);
        packet[4] = 1;
        packet[5] = 0x40;
        payload_loss = 2;
        return true;
    }

    if (afc == 3)
    {
        const size_t afl = packet[4];

        if (afl == 0)
        {
            packet[4] = 1;
            packet[5] = 0x40;
            payload_loss = 1;
            return true;
        }

        if (5 + afl > TS_SIZE)
            return false;

        packet[5] |= 0x40;
        return true;
    }

    return false;
}

bool TSRandomAccessFixer::FindRecoveryPointSEI(
    const std::vector<uint8_t> &pes_payload)
{
    if (pes_payload.size() < 10)
        return false;

    size_t es = 0;

    if (pes_payload[0] == 0x00 &&
        pes_payload[1] == 0x00 &&
        pes_payload[2] == 0x01)
    {
        if (pes_payload.size() < 9)
            return false;

        es = 9 + pes_payload[8];
        if (es >= pes_payload.size())
            return false;
    }

    const uint8_t *data = pes_payload.data() + es;
    const size_t len = pes_payload.size() - es;
    size_t pos = 0;

    while (pos < len)
    {
        size_t prefix = 0;
        const size_t start = FindStartCode(data, len, pos, prefix);
        if (start == len)
            break;

        const size_t nal = start + prefix;
        if (nal >= len)
            break;

        size_t next_prefix = 0;
        const size_t next = FindStartCode(data, len, nal + 1, next_prefix);
        const size_t nal_end = (next == len) ? len : next;

        if ((data[nal] & 0x1f) == 6 && nal + 1 < nal_end)
        {
            if (SEINALHasRecoveryPoint(data + nal + 1,
                                       nal_end - (nal + 1)))
                return true;
        }

        if (next == len)
            break;
        pos = next;
    }

    return false;
}

bool TSRandomAccessFixer::SEINALHasRecoveryPoint(const uint8_t *data,
                                                  size_t len)
{
    std::vector<uint8_t> rbsp;
    rbsp.reserve(len);

    int zero_count = 0;

    for (size_t i = 0; i < len; ++i)
    {
        const uint8_t b = data[i];

        if (zero_count >= 2 && b == 0x03)
        {
            zero_count = 0;
            continue;
        }

        rbsp.push_back(b);

        if (b == 0x00)
            ++zero_count;
        else
            zero_count = 0;
    }

    size_t pos = 0;

    while (pos < rbsp.size())
    {
        if (rbsp[pos] == 0x80)
            break;

        unsigned payload_type = 0;
        while (pos < rbsp.size() && rbsp[pos] == 0xff)
        {
            payload_type += 255;
            ++pos;
        }
        if (pos >= rbsp.size())
            break;
        payload_type += rbsp[pos++];

        size_t payload_size = 0;
        while (pos < rbsp.size() && rbsp[pos] == 0xff)
        {
            payload_size += 255;
            ++pos;
        }
        if (pos >= rbsp.size())
            break;
        payload_size += rbsp[pos++];

        if (pos + payload_size > rbsp.size())
            break;

        if (payload_type == 6)
            return true;

        pos += payload_size;
    }

    return false;
}

size_t TSRandomAccessFixer::FindStartCode(const uint8_t *data,
                                           size_t len,
                                           size_t from,
                                           size_t &prefix_len)
{
    prefix_len = 0;

    for (size_t i = from; i + 3 <= len; ++i)
    {
        if (i + 4 <= len &&
            data[i] == 0x00 && data[i + 1] == 0x00 &&
            data[i + 2] == 0x00 && data[i + 3] == 0x01)
        {
            prefix_len = 4;
            return i;
        }

        if (data[i] == 0x00 && data[i + 1] == 0x00 &&
            data[i + 2] == 0x01)
        {
            prefix_len = 3;
            return i;
        }
    }

    return len;
}

void TSRandomAccessFixer::AppendPacket(const packet_t &packet,
                                        std::vector<uint8_t> &output)
{
    output.insert(output.end(), packet.begin(), packet.end());
}
