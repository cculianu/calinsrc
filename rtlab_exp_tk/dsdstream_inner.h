/***************************************************************************
                          dsdstream_inner.h  -  description
                             -------------------
    begin                : Mon Feb 11 2002
    copyright            : (C) 2002 by Calin Culianu
    email                : calin@rtlab.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef _DSDSTREAM_INNER_H
#define _DSDSTREAM_INNER_H

#ifndef _INSIDE_DSDSTREAM
#error Please do not include dsdstream_inner.h directly! Instead include dsdstream.h!
#endif

private:

struct Serializeable {
    virtual ~Serializeable() {};
    virtual void clear() = 0;

    virtual void serialize (Settings & settings, const QString & section_name) const = 0;
    virtual void unserialize (const Settings & settings, const QString & section_name) = 0;
};

struct ChannelMask : public Serializeable {
    ChannelMask(uint size = SHD_MAX_CHANNELS) { mask.resize(size); clear(); };
    ChannelMask(const ChannelMask & m) : Serializeable() { *this = m; };
    void clear();
    ChannelMask & operator= ( const ChannelMask & m ) { count = m.count; mask = m.mask.copy(); return *this; };

    virtual void serialize (Settings & settings, const QString & section_name) const;
    virtual void unserialize (const Settings & settings, const QString & section_name) ;
    // the following two are needed by class DSDStream for doing the MASK_CHANGED_INSN
    size_t serialize(DSDStream & s) const throw (Exception);
    size_t unserialize(DSDStream &s) throw (Exception);

    bool isOn(uint chan) const      { return mask[chan]; };

    void setOn(uint chan, bool onoroff) {
      if ( isOn(chan) == onoroff ) return;
      count += -1 + 2*onoroff;
      mask.setBit(chan, onoroff);
    };

    uint numOn() const      { return count; };


/*
   Sorry for the macro used to define methods.. but I thought this might illustrate the similarity
   between all implementations of the operatorXX code :)

   Your basic operators to determine if one mask is >, <, >=, <=, == to another mask.
   A ChannelMask is defined as being greater than another mask if and only if it contains more channels,
   and equal if it contains the same number of channels.

   Use method compare() to determine actual bitwise equality.
*/
#define __CM_OP_TEMPL(OP) \
    bool operator OP (const ChannelMask & m) const {  return count OP m.count;  }
   __CM_OP_TEMPL(>);
   __CM_OP_TEMPL(<);
   __CM_OP_TEMPL(==);
   __CM_OP_TEMPL(<=);
   __CM_OP_TEMPL(>=);
#undef __CM_OP_TEMPL

   int compare (const ChannelMask & m) const;
   bool equals (const ChannelMask &m) const    { return !compare(m); };

  private:
    QBitArray mask;
    uint count;

    static const QString key_mask, key_count;
};

struct MaskState : public Serializeable {
    MaskState() { clear(); };
    void clear();

    virtual void serialize (Settings & settings, const QString & section_name) const;
    virtual void unserialize (const Settings & settings, const QString & section_name);

    ChannelMask mask;
    scan_index_t startIndex; // startIndex --> endIndex is an inclusive range
    scan_index_t endIndex;

    vector<uint> channels_on; // needs to be recomputed each time channel mask changes
    map<uint, uint> id_to_pos_map;
    void computeChannelsOn();
private:
    static const QString key_mask, key_startIndex, key_endIndex;


};

struct RateState : public Serializeable {
    RateState() { clear(); };
    void clear();

    virtual void serialize (Settings & settings, const QString & section_name) const;
    virtual void unserialize (const Settings & settings, const QString & section_name) ;

    sampling_rate_t rate;
    scan_index_t startIndex;        // startIndex --> endIndex is an inclusive range
    scan_index_t endIndex;
private:
    static const QString key_rate, key_startIndex, key_endIndex;

};

struct StateHistory : public Serializeable {
    StateHistory() { clear(); };
    void clear();

    void serialize (Settings & settings, const QString & section_name) const;
    void unserialize (const Settings & settings, const QString & section_name) ;
    void computeMaxUniqueChannelsUsed(); // runs through the maskStates in the history and computes num of unique channels used
    bool isChanOn(uint chan, scan_index_t atIndex) const;
    vector<MaskState>::const_iterator maskStateAt(scan_index_t atIndex) const;

    uint max_unique_channels_used;
    vector<MaskState> maskStates;
    vector<RateState> rateStates;
    vector<scan_index_t> skippedRanges; // all the scan indices that were skipped, as pairs of inclusive ranges
    scan_index_t startIndex; // global start and stop indices.. inclusive range
    scan_index_t endIndex;   // "
    uint64 sampleCount;
    scan_index_t scanCount;
    time_t timeStarted;

private:
    static const QString key_max_unique_channels_used,
                         key_num_maskStates,
                         key_num_rateStates,
                         key_num_skippedRanges,
                         key_maskStates,
                         key_rateStates,
                         key_skippedRanges,
                         key_startIndex,
                         key_endIndex,
                         key_sampleCount,
                         key_scanCount,
                         key_timeStarted;
};


#endif
