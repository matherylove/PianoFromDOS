/*************************************************************************************************
*
* File: MetaData.h
*
* Description: Small, dependency-free implementation of the Piano From Above metadata schema.
*              It implements the subset of the generated Protocol Buffers 2.5 API used by PFA and
*              reads/writes the original protobuf wire format, removing the libprotobuf dependency.
*
* PianoFromDOS compatibility layer.
*
*************************************************************************************************/
#pragma once

#include <string>
#include <vector>
#include <stddef.h>

namespace PFAData
{

template <class T>
class RepeatedOwned
{
public:
    RepeatedOwned() {}
    ~RepeatedOwned() { Clear(); }

    int size() const { return static_cast<int>(m_Items.size()); }
    const T &Get(int i) const { return *m_Items[i]; }
    T *Mutable(int i) { return m_Items[i]; }
    T *Add() { T *p = new T(); m_Items.push_back(p); return p; }

    void SwapElements(int a, int b)
    {
        T *tmp = m_Items[a];
        m_Items[a] = m_Items[b];
        m_Items[b] = tmp;
    }

    void RemoveLast()
    {
        if (m_Items.empty()) return;
        delete m_Items.back();
        m_Items.pop_back();
    }

    void Clear()
    {
        for (size_t i = 0; i < m_Items.size(); ++i) delete m_Items[i];
        m_Items.clear();
    }

private:
    RepeatedOwned(const RepeatedOwned &);
    RepeatedOwned &operator=(const RepeatedOwned &);
    std::vector<T*> m_Items;
};

class Label
{
public:
    Label() : m_Pos(0) {}

    int pos() const { return m_Pos; }
    void set_pos(int v) { m_Pos = v; }

    const std::string &label() const { return m_Label; }
    std::string *mutable_label() { return &m_Label; }
    void set_label(const std::string &v) { m_Label = v; }

private:
    int m_Pos;
    std::string m_Label;

    friend class MetaDataCodec;
};

class Score
{
public:
    Score() { Clear(); }

    void Clear()
    {
        m_HasScore = false;
        m_Score = 0;
        m_Mult = 10;
        m_Missed = m_Incorrect = m_Ok = m_Good = m_Great = 0;
        m_CurStreak = m_GoodStreak = m_BadStreak = 0;
        m_Date = 0;
    }

    void CopyFrom(const Score &o)
    {
        m_HasScore = o.m_HasScore;
        m_Score = o.m_Score;
        m_Mult = o.m_Mult;
        m_Missed = o.m_Missed;
        m_Incorrect = o.m_Incorrect;
        m_Ok = o.m_Ok;
        m_Good = o.m_Good;
        m_Great = o.m_Great;
        m_CurStreak = o.m_CurStreak;
        m_GoodStreak = o.m_GoodStreak;
        m_BadStreak = o.m_BadStreak;
        m_Date = o.m_Date;
    }

    bool has_score() const { return m_HasScore; }
    int score() const { return m_Score; }
    void set_score(int v) { m_HasScore = true; m_Score = v; }

    int mult() const { return m_Mult; }
    void set_mult(int v) { m_Mult = v; }
    void clear_mult() { m_Mult = 10; }
    int missed() const { return m_Missed; }
    void set_missed(int v) { m_Missed = v; }
    int incorrect() const { return m_Incorrect; }
    void set_incorrect(int v) { m_Incorrect = v; }
    int ok() const { return m_Ok; }
    void set_ok(int v) { m_Ok = v; }
    int good() const { return m_Good; }
    void set_good(int v) { m_Good = v; }
    int great() const { return m_Great; }
    void set_great(int v) { m_Great = v; }
    int curstreak() const { return m_CurStreak; }
    void set_curstreak(int v) { m_CurStreak = v; }
    int goodstreak() const { return m_GoodStreak; }
    void set_goodstreak(int v) { m_GoodStreak = v; }
    int badstreak() const { return m_BadStreak; }
    void set_badstreak(int v) { m_BadStreak = v; }
    int date() const { return m_Date; }
    void set_date(int v) { m_Date = v; }

private:
    bool m_HasScore;
    int m_Score, m_Mult;
    int m_Missed, m_Incorrect, m_Ok, m_Good, m_Great;
    int m_CurStreak, m_GoodStreak, m_BadStreak;
    int m_Date;

    friend class MetaDataCodec;
};

class SongInfo
{
public:
    SongInfo() : m_Division(0), m_Notes(0), m_Beats(0), m_Seconds(0), m_Tracks(0), m_Plays(0) {}

    const std::string &md5() const { return m_Md5; }
    void set_md5(const std::string &v) { m_Md5 = v; }
    int division() const { return m_Division; }
    void set_division(int v) { m_Division = v; }
    int notes() const { return m_Notes; }
    void set_notes(int v) { m_Notes = v; }
    int beats() const { return m_Beats; }
    void set_beats(int v) { m_Beats = v; }
    int seconds() const { return m_Seconds; }
    void set_seconds(int v) { m_Seconds = v; }
    int tracks() const { return m_Tracks; }
    void set_tracks(int v) { m_Tracks = v; }
    int plays() const { return m_Plays; }
    void set_plays(int v) { m_Plays = v; }

private:
    std::string m_Md5;
    int m_Division, m_Notes, m_Beats, m_Seconds, m_Tracks, m_Plays;

    friend class MetaDataCodec;
};

class FileInfo
{
public:
    FileInfo() {}

    const SongInfo &info() const { return m_Info; }
    SongInfo *mutable_info() { return &m_Info; }

    int label_size() const { return m_Labels.size(); }
    const Label &label(int i) const { return m_Labels.Get(i); }
    Label *mutable_label(int i) { return m_Labels.Mutable(i); }
    Label *add_label() { return m_Labels.Add(); }

    int top10_size() const { return m_Top10.size(); }
    const Score &top10(int i) const { return m_Top10.Get(i); }
    Score *mutable_top10(int i) { return m_Top10.Mutable(i); }
    RepeatedOwned<Score> *mutable_top10() { return &m_Top10; }
    Score *add_top10() { return m_Top10.Add(); }

private:
    SongInfo m_Info;
    RepeatedOwned<Label> m_Labels;
    RepeatedOwned<Score> m_Top10;

    friend class MetaDataCodec;
};

class File
{
public:
    File() : m_FileSize(0), m_InfoPos(0) {}

    const std::string &filename() const { return m_FileName; }
    void set_filename(const std::string &v) { m_FileName = v; }
    int filesize() const { return m_FileSize; }
    void set_filesize(int v) { m_FileSize = v; }
    int infopos() const { return m_InfoPos; }
    void set_infopos(int v) { m_InfoPos = v; }

private:
    std::string m_FileName;
    int m_FileSize, m_InfoPos;

    friend class MetaDataCodec;
};

class MetaData
{
public:
    MetaData() {}
    ~MetaData() { Clear(); }

    int file_size() const { return m_Files.size(); }
    const File &file(int i) const { return m_Files.Get(i); }
    File *mutable_file(int i) { return m_Files.Mutable(i); }
    File *add_file() { return m_Files.Add(); }

    int fileinfo_size() const { return m_FileInfos.size(); }
    const FileInfo &fileinfo(int i) const { return m_FileInfos.Get(i); }
    FileInfo *mutable_fileinfo(int i) { return m_FileInfos.Mutable(i); }
    FileInfo *add_fileinfo() { return m_FileInfos.Add(); }

    void Clear() { m_Files.Clear(); m_FileInfos.Clear(); }
    bool ParseFromArray(const void *data, int size);
    bool SerializeToString(std::string *out) const;

private:
    MetaData(const MetaData &);
    MetaData &operator=(const MetaData &);
    RepeatedOwned<File> m_Files;
    RepeatedOwned<FileInfo> m_FileInfos;

    friend class MetaDataCodec;
};

} // namespace PFAData
