/*************************************************************************************************
*
* File: MetaData.cpp
*
* Description: Dependency-free reader/writer for the original MetaData.proto wire format.
*
*************************************************************************************************/
#include "MetaData.h"

namespace PFAData
{

class MetaDataCodec
{
public:
    typedef unsigned long long U64;

    static void WriteVarint(std::string &out, U64 v)
    {
        while (v >= 0x80) {
            out.push_back(static_cast<char>((v & 0x7f) | 0x80));
            v >>= 7;
        }
        out.push_back(static_cast<char>(v));
    }

    static void WriteTag(std::string &out, unsigned field, unsigned wire)
    {
        WriteVarint(out, static_cast<U64>((field << 3) | wire));
    }

    static void WriteInt32(std::string &out, unsigned field, int v)
    {
        WriteTag(out, field, 0);
        if (v < 0)
            WriteVarint(out, static_cast<U64>(static_cast<long long>(v)));
        else
            WriteVarint(out, static_cast<U64>(static_cast<unsigned int>(v)));
    }

    static U64 ZigZagEncode32(int v)
    {
        unsigned int uv = static_cast<unsigned int>(v);
        return static_cast<U64>((uv << 1) ^ static_cast<unsigned int>(v >> 31));
    }

    static int ZigZagDecode32(U64 v)
    {
        unsigned int u = static_cast<unsigned int>(v);
        return static_cast<int>((u >> 1) ^ static_cast<unsigned int>(-static_cast<int>(u & 1)));
    }

    static void WriteSInt32(std::string &out, unsigned field, int v)
    {
        WriteTag(out, field, 0);
        WriteVarint(out, ZigZagEncode32(v));
    }

    static void WriteBytes(std::string &out, unsigned field, const std::string &s)
    {
        WriteTag(out, field, 2);
        WriteVarint(out, static_cast<U64>(s.size()));
        out.append(s);
    }

    static void WriteMessage(std::string &out, unsigned field, const std::string &msg)
    {
        WriteTag(out, field, 2);
        WriteVarint(out, static_cast<U64>(msg.size()));
        out.append(msg);
    }

    static bool ReadVarint(const unsigned char *&p, const unsigned char *end, U64 &v)
    {
        v = 0;
        unsigned shift = 0;
        while (p < end && shift < 64) {
            unsigned char c = *p++;
            v |= static_cast<U64>(c & 0x7f) << shift;
            if ((c & 0x80) == 0) return true;
            shift += 7;
        }
        return false;
    }

    static bool ReadLength(const unsigned char *&p, const unsigned char *end,
                           const unsigned char *&sub, const unsigned char *&subEnd)
    {
        U64 len;
        if (!ReadVarint(p, end, len)) return false;
        if (len > static_cast<U64>(end - p)) return false;
        sub = p;
        subEnd = p + static_cast<size_t>(len);
        p = subEnd;
        return true;
    }

    static bool SkipField(const unsigned char *&p, const unsigned char *end, unsigned wire)
    {
        U64 tmp;
        switch (wire) {
            case 0: return ReadVarint(p, end, tmp);
            case 1:
                if (end - p < 8) return false;
                p += 8; return true;
            case 2: {
                const unsigned char *s, *e;
                return ReadLength(p, end, s, e);
            }
            case 5:
                if (end - p < 4) return false;
                p += 4; return true;
            default:
                return false;
        }
    }

    static bool NextTag(const unsigned char *&p, const unsigned char *end,
                        unsigned &field, unsigned &wire)
    {
        U64 tag;
        if (!ReadVarint(p, end, tag) || tag == 0) return false;
        field = static_cast<unsigned>(tag >> 3);
        wire = static_cast<unsigned>(tag & 7);
        return field != 0;
    }

    static bool ReadString(const unsigned char *&p, const unsigned char *end, std::string &s)
    {
        const unsigned char *b, *e;
        if (!ReadLength(p, end, b, e)) return false;
        s.assign(reinterpret_cast<const char*>(b), reinterpret_cast<const char*>(e));
        return true;
    }

    static std::string EncodeLabel(const Label &v)
    {
        std::string out;
        WriteInt32(out, 1, v.m_Pos);
        WriteBytes(out, 2, v.m_Label);
        return out;
    }

    static bool DecodeLabel(Label &v, const unsigned char *p, const unsigned char *end)
    {
        while (p < end) {
            unsigned field, wire;
            if (!NextTag(p, end, field, wire)) return false;
            U64 n;
            if (field == 1 && wire == 0) {
                if (!ReadVarint(p, end, n)) return false;
                v.m_Pos = static_cast<int>(n);
            } else if (field == 2 && wire == 2) {
                if (!ReadString(p, end, v.m_Label)) return false;
            } else if (!SkipField(p, end, wire)) return false;
        }
        return true;
    }

    static std::string EncodeScore(const Score &v)
    {
        std::string out;
        if (v.m_HasScore) WriteSInt32(out, 1, v.m_Score);
        if (v.m_Mult != 10) WriteInt32(out, 2, v.m_Mult);
        if (v.m_Missed) WriteInt32(out, 3, v.m_Missed);
        if (v.m_Incorrect) WriteInt32(out, 4, v.m_Incorrect);
        if (v.m_Ok) WriteInt32(out, 5, v.m_Ok);
        if (v.m_Good) WriteInt32(out, 6, v.m_Good);
        if (v.m_Great) WriteInt32(out, 7, v.m_Great);
        if (v.m_CurStreak) WriteSInt32(out, 8, v.m_CurStreak);
        if (v.m_GoodStreak) WriteInt32(out, 9, v.m_GoodStreak);
        if (v.m_BadStreak) WriteInt32(out, 10, v.m_BadStreak);
        if (v.m_Date) WriteInt32(out, 11, v.m_Date);
        return out;
    }

    static bool DecodeScore(Score &v, const unsigned char *p, const unsigned char *end)
    {
        while (p < end) {
            unsigned field, wire;
            if (!NextTag(p, end, field, wire)) return false;
            U64 n;
            if (wire == 0 && field >= 1 && field <= 11) {
                if (!ReadVarint(p, end, n)) return false;
                switch (field) {
                    case 1: v.m_HasScore = true; v.m_Score = ZigZagDecode32(n); break;
                    case 2: v.m_Mult = static_cast<int>(n); break;
                    case 3: v.m_Missed = static_cast<int>(n); break;
                    case 4: v.m_Incorrect = static_cast<int>(n); break;
                    case 5: v.m_Ok = static_cast<int>(n); break;
                    case 6: v.m_Good = static_cast<int>(n); break;
                    case 7: v.m_Great = static_cast<int>(n); break;
                    case 8: v.m_CurStreak = ZigZagDecode32(n); break;
                    case 9: v.m_GoodStreak = static_cast<int>(n); break;
                    case 10: v.m_BadStreak = static_cast<int>(n); break;
                    case 11: v.m_Date = static_cast<int>(n); break;
                }
            } else if (!SkipField(p, end, wire)) return false;
        }
        return true;
    }

    static std::string EncodeSongInfo(const SongInfo &v)
    {
        std::string out;
        WriteBytes(out, 1, v.m_Md5);
        if (v.m_Division) WriteInt32(out, 2, v.m_Division);
        if (v.m_Notes) WriteInt32(out, 3, v.m_Notes);
        if (v.m_Beats) WriteInt32(out, 4, v.m_Beats);
        if (v.m_Seconds) WriteInt32(out, 5, v.m_Seconds);
        if (v.m_Tracks) WriteInt32(out, 6, v.m_Tracks);
        if (v.m_Plays) WriteInt32(out, 7, v.m_Plays);
        return out;
    }

    static bool DecodeSongInfo(SongInfo &v, const unsigned char *p, const unsigned char *end)
    {
        while (p < end) {
            unsigned field, wire;
            if (!NextTag(p, end, field, wire)) return false;
            U64 n;
            if (field == 1 && wire == 2) {
                if (!ReadString(p, end, v.m_Md5)) return false;
            } else if (wire == 0 && field >= 2 && field <= 7) {
                if (!ReadVarint(p, end, n)) return false;
                switch (field) {
                    case 2: v.m_Division = static_cast<int>(n); break;
                    case 3: v.m_Notes = static_cast<int>(n); break;
                    case 4: v.m_Beats = static_cast<int>(n); break;
                    case 5: v.m_Seconds = static_cast<int>(n); break;
                    case 6: v.m_Tracks = static_cast<int>(n); break;
                    case 7: v.m_Plays = static_cast<int>(n); break;
                }
            } else if (!SkipField(p, end, wire)) return false;
        }
        return true;
    }

    static std::string EncodeFileInfo(const FileInfo &v)
    {
        std::string out;
        WriteMessage(out, 1, EncodeSongInfo(v.m_Info));
        for (int i = 0; i < v.m_Labels.size(); ++i)
            WriteMessage(out, 2, EncodeLabel(v.m_Labels.Get(i)));
        for (int i = 0; i < v.m_Top10.size(); ++i)
            WriteMessage(out, 3, EncodeScore(v.m_Top10.Get(i)));
        return out;
    }

    static bool DecodeFileInfo(FileInfo &v, const unsigned char *p, const unsigned char *end)
    {
        while (p < end) {
            unsigned field, wire;
            if (!NextTag(p, end, field, wire)) return false;
            if (wire == 2 && field >= 1 && field <= 3) {
                const unsigned char *b, *e;
                if (!ReadLength(p, end, b, e)) return false;
                if (field == 1) {
                    if (!DecodeSongInfo(v.m_Info, b, e)) return false;
                } else if (field == 2) {
                    Label *x = v.m_Labels.Add();
                    if (!DecodeLabel(*x, b, e)) return false;
                } else {
                    Score *x = v.m_Top10.Add();
                    if (!DecodeScore(*x, b, e)) return false;
                }
            } else if (!SkipField(p, end, wire)) return false;
        }
        return true;
    }

    static std::string EncodeFile(const File &v)
    {
        std::string out;
        WriteBytes(out, 1, v.m_FileName);
        WriteInt32(out, 2, v.m_FileSize);
        WriteInt32(out, 3, v.m_InfoPos);
        return out;
    }

    static bool DecodeFile(File &v, const unsigned char *p, const unsigned char *end)
    {
        while (p < end) {
            unsigned field, wire;
            if (!NextTag(p, end, field, wire)) return false;
            U64 n;
            if (field == 1 && wire == 2) {
                if (!ReadString(p, end, v.m_FileName)) return false;
            } else if ((field == 2 || field == 3) && wire == 0) {
                if (!ReadVarint(p, end, n)) return false;
                if (field == 2) v.m_FileSize = static_cast<int>(n);
                else v.m_InfoPos = static_cast<int>(n);
            } else if (!SkipField(p, end, wire)) return false;
        }
        return true;
    }

    static bool DecodeMetaData(MetaData &v, const unsigned char *p, const unsigned char *end)
    {
        while (p < end) {
            unsigned field, wire;
            if (!NextTag(p, end, field, wire)) return false;
            if ((field == 1 || field == 2) && wire == 2) {
                const unsigned char *b, *e;
                if (!ReadLength(p, end, b, e)) return false;
                if (field == 1) {
                    File *x = v.m_Files.Add();
                    if (!DecodeFile(*x, b, e)) return false;
                } else {
                    FileInfo *x = v.m_FileInfos.Add();
                    if (!DecodeFileInfo(*x, b, e)) return false;
                }
            } else if (!SkipField(p, end, wire)) return false;
        }
        return true;
    }

    static void EncodeMetaData(const MetaData &v, std::string &out)
    {
        out.clear();
        for (int i = 0; i < v.m_Files.size(); ++i)
            WriteMessage(out, 1, EncodeFile(v.m_Files.Get(i)));
        for (int i = 0; i < v.m_FileInfos.size(); ++i)
            WriteMessage(out, 2, EncodeFileInfo(v.m_FileInfos.Get(i)));
    }
};

bool MetaData::ParseFromArray(const void *data, int size)
{
    Clear();
    if (!data || size < 0) return false;
    const unsigned char *p = static_cast<const unsigned char*>(data);
    const unsigned char *end = p + size;
    if (!MetaDataCodec::DecodeMetaData(*this, p, end)) {
        Clear();
        return false;
    }
    return true;
}

bool MetaData::SerializeToString(std::string *out) const
{
    if (!out) return false;
    MetaDataCodec::EncodeMetaData(*this, *out);
    return true;
}

} // namespace PFAData
