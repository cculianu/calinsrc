/***************************************************************************
                          dsdstream_inner.cpp  -  description
                             -------------------
    begin                : Tue Feb 12 2002
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

#include <vector>
#include <set>
#include <algorithm>
#include "dsdstream.h"
#include "settings.h"

#include <qtextstream.h>
#include <qstring.h>


#include <string.h>

#include <stdio.h>

template<> DSDStream & DSDStream::operator<<(const ChannelMask & m) //throw (FileException)
{
  m.serialize(*this);
  return *this;
}

template<> DSDStream & DSDStream::operator>>(ChannelMask & m) //throw (FileException)
{
  m.unserialize(*this);
  return *this;
}

const QString DSDStream::StateHistory::key_max_unique_channels_used("maxUniqueChannelsUsed"),
              DSDStream::StateHistory::key_num_maskStates("numMaskStates"),
              DSDStream::StateHistory::key_num_rateStates("numRateStates"),
              DSDStream::StateHistory::key_num_skippedRanges("skippedRanges"),
              DSDStream::StateHistory::key_maskStates("maskStates"),
              DSDStream::StateHistory::key_rateStates("rateStates"),
              DSDStream::StateHistory::key_skippedRanges("skippedRanges"),
              DSDStream::StateHistory::key_startIndex("startIndex"),
              DSDStream::StateHistory::key_endIndex("endIndex"),
              DSDStream::StateHistory::key_sampleCount("sampleCount"),
              DSDStream::StateHistory::key_scanCount("scanCount"),
              DSDStream::StateHistory::key_timeStarted("timeStarted"),
              DSDStream::RateState::   key_rate("rate"),
              DSDStream::RateState::   key_startIndex("startIndex"),
              DSDStream::RateState::   key_endIndex("endIndex"),
              DSDStream::MaskState::   key_mask("mask"),
              DSDStream::MaskState::   key_startIndex("startIndex"),
              DSDStream::MaskState::   key_endIndex("endIndex"),
              DSDStream::ChannelMask:: key_count("count"),
              DSDStream::ChannelMask:: key_mask("mask");


void DSDStream::StateHistory::clear()
{
  max_unique_channels_used=0; startIndex=0; endIndex=0; sampleCount=0; scanCount=0; timeStarted=0;
  maskStates.clear(); rateStates.clear(); skippedRanges.clear();
}

void DSDStream::RateState::clear() { rate = 0; startIndex = 0; endIndex = 0; };
void DSDStream::MaskState::clear() { startIndex = endIndex = 0; mask.clear(); channels_on.clear(); id_to_pos_map.clear(); }
void DSDStream::ChannelMask::clear()  { count = 0; mask.fill(false);}

int DSDStream::ChannelMask::compare (const ChannelMask & m) const
{
  if (mask.size() > m.mask.size()) return 1;
  if (mask.size() < m.mask.size()) return -1;
  int cmp = 0;
  for (uint i = 0; i < mask.size(); i++)  cmp += (mask[i] * 1) + (m.mask[i]*-1);

  return cmp;
}

void DSDStream::StateHistory::computeMaxUniqueChannelsUsed()
{
  set<uint> chans;

  for (vector<MaskState>::iterator it = maskStates.begin(); it != maskStates.end(); it++)
    for (uint i = 0; i < SHD_MAX_CHANNELS; i++)
      if (it->mask.isOn(i) && chans.find(i) == chans.end())
        chans.insert(i);
  max_unique_channels_used = chans.size();
#ifdef DEBUGGG
  cerr << "Unique channels computed: " << max_unique_channels_used << endl;
#endif
}

void DSDStream::MaskState::computeChannelsOn()
{
  channels_on.clear();
  for (uint i = 0; i < SHD_MAX_CHANNELS; i++)
    if (mask.isOn(i)) channels_on.push_back(i);

  id_to_pos_map.clear();
  for (uint i = 0; i < channels_on.size(); i++)
    id_to_pos_map[channels_on[i]] = i;

}

vector<DSDStream::MaskState>::const_iterator DSDStream::StateHistory::maskStateAt(scan_index_t atIndex) const
{
  vector<MaskState>::const_iterator it = maskStates.end();

  if ( (endIndex < atIndex) || (startIndex > atIndex) ) goto end;
  for (it = maskStates.begin(); it < maskStates.end(); it++)
    if ( (it->startIndex <= atIndex) && (it->endIndex >= atIndex) )
      break;
  end:
  return it;
}

bool DSDStream::StateHistory::isChanOn(uint chan, scan_index_t index) const
{
  vector<MaskState>::const_iterator it = maskStateAt(index);
  return (it!=maskStates.end()) && it->mask.isOn(chan);
}

void DSDStream::StateHistory::serialize(Settings & settings, const QString & section_name) const
{
    QString orig_section = settings.currentSection();
    settings.setSection(section_name);
    settings.put(key_max_unique_channels_used, QString::number(max_unique_channels_used));
    settings.put(key_num_maskStates, QString::number(maskStates.size()));
    settings.put(key_num_rateStates, QString::number(rateStates.size()));
    settings.put(key_num_skippedRanges, QString::number(skippedRanges.size()));
    settings.put(key_startIndex, uint64_to_cstr(static_cast<uint64>(startIndex)));
    settings.put(key_endIndex, uint64_to_cstr(static_cast<uint64>(endIndex)));
    settings.put(key_sampleCount, uint64_to_cstr(static_cast<uint64>(sampleCount)));
    settings.put(key_scanCount, uint64_to_cstr(static_cast<uint64>(scanCount)));
    settings.put(key_timeStarted, QString::number(timeStarted));

    uint i;
    for(i = 0; i < skippedRanges.size(); i++) {
      settings.put(key_skippedRanges + "_" + QString::number(i), uint64_to_cstr(skippedRanges[i]));
    }
    for (i = 0; i < rateStates.size(); i++) {
      rateStates[i].serialize(settings, section_name + "'s " + key_rateStates + "_" + QString::number(i));
    }
    for (i = 0; i < maskStates.size(); i++) {
      maskStates[i].serialize(settings, section_name + "'s " + key_maskStates + "_" + QString::number(i));
    }
    settings.setSection(orig_section);
}

void DSDStream::MaskState::serialize(Settings & settings, const QString & section_name) const
{
  QString orig_section = settings.currentSection();

  mask.serialize(settings, section_name + "'s " + key_mask);
  settings.setSection(section_name);
  settings.put(key_startIndex, uint64_to_cstr(static_cast<uint64>(startIndex)));
  settings.put(key_endIndex, uint64_to_cstr(static_cast<uint64>(endIndex)));

  settings.setSection(orig_section);
}

void DSDStream::ChannelMask::serialize(Settings & settings, const QString & section_name) const
{
  QString orig_section = settings.currentSection();

  settings.setSection(section_name);
  settings.put(key_count, QString::number(count));

  QString chanMaskStr("");
  uint i;
  for (i = 0; i < mask.size(); i++) {
    chanMaskStr += (mask[i] ? '1' : '0');
  }
  settings.put(key_mask, chanMaskStr);
  settings.setSection(orig_section);
}

// needed by class DSDStream for doing the MASK_CHANGED_INSN
size_t DSDStream::ChannelMask::serialize(DSDStream & s) const //throw (Exception)
{
  size_t ret = s.device()->at();

  s << mask << count;
  s.device()->flush();
  ret = s.device()->at() - ret;
  return ret;
}

void DSDStream::RateState::serialize(Settings & settings, const QString & section_name) const
{
  QString orig_section = settings.currentSection();

  settings.setSection(section_name);
  settings.put(key_rate, uint64_to_cstr(static_cast<uint64>(rate)));
  settings.put(key_startIndex, uint64_to_cstr(static_cast<uint64>(startIndex)));
  settings.put(key_endIndex, uint64_to_cstr(static_cast<uint64>(endIndex)));

  settings.setSection(orig_section);
}


void DSDStream::StateHistory::unserialize(const Settings & settings, const QString & section)
{
  uint i;

  QString maxchans (settings.get(section, key_max_unique_channels_used)),
          ms_sz    (settings.get(section, key_num_maskStates)),
          rs_sz    (settings.get(section, key_num_rateStates)),
          sr_sz    (settings.get(section, key_num_skippedRanges)),
          si       (settings.get(section, key_startIndex)),
          ei       (settings.get(section, key_endIndex)),
          sc       (settings.get(section, key_sampleCount)),
          scc      (settings.get(section, key_scanCount)),
          ts       (settings.get(section, key_timeStarted));

  max_unique_channels_used = maxchans.toUInt();
  startIndex = cstr_to_uint64(si.ascii());
  endIndex = cstr_to_uint64(ei.ascii());
  sampleCount = cstr_to_uint64(sc.ascii());
  scanCount = cstr_to_uint64(scc.ascii());
  timeStarted = ts.toLong();

  for (maskStates.resize(ms_sz.toUInt()), i = 0; i < maskStates.size(); i++) {
    maskStates[i].unserialize(settings, section + "'s " + key_maskStates + "_" + QString::number(i));
  }

  for (rateStates.resize(rs_sz.toUInt()), i = 0; i < rateStates.size(); i++) {
    rateStates[i].unserialize(settings, section + "'s " + key_rateStates + "_" + QString::number(i));
  }

  for (skippedRanges.resize(sr_sz.toUInt()), i = 0; i < skippedRanges.size(); i++) {
    skippedRanges[i] = cstr_to_uint64(settings.get(section, key_skippedRanges + "_" + QString::number(i)));
  }

}

void DSDStream::ChannelMask::unserialize(const Settings & settings, const QString & section)
{
  QString mask_string = settings.get(section, key_mask);
  uint mask_length = mask_string.length(), i;

  for (mask.resize(mask_length), i = 0; i < mask_length; i++) {
    mask[i] = (mask_string[i] == '1');
  }

  count = settings.get(section, key_count).toUInt();
}

// needed by class DSDStream for doing the MASK_CHANGED_INSN
size_t DSDStream::ChannelMask::unserialize(DSDStream & s) //throw (Exception)
{
  size_t readLen = s.device()->at();

  s >> mask >> count;
  readLen = s.device()->at() - readLen;
  return readLen;
}

void DSDStream::MaskState::unserialize(const Settings & settings, const QString & section)
{
  mask.unserialize(settings, section + "'s " + key_mask);
  startIndex = cstr_to_uint64(settings.get(section, key_startIndex).ascii());
  endIndex = cstr_to_uint64(settings.get(section, key_endIndex).ascii());
  computeChannelsOn();
}

/* this value represents that a particular channel id is off (no position) */
void DSDStream::MaskState::Id2PosMap::clear()
{
  erase(begin(), end());
}

void DSDStream::MaskState::Id2PosMap::erase(iterator begin, iterator end)
{
  for(; begin < end; begin++) *begin = Off;
}

DSDStream::MaskState::Id2PosMap::iterator 
DSDStream::MaskState::Id2PosMap::find(Pos chan)
{
  if (chan >= SHD_MAX_CHANNELS || data[chan] == Off)
    return end();
  
  return data + chan;
}


void DSDStream::RateState::unserialize(const Settings & settings, const QString & section)
{
  rate = settings.get(section, key_rate).toUInt();
  startIndex = cstr_to_uint64(settings.get(section, key_startIndex).ascii());
  endIndex = cstr_to_uint64(settings.get(section, key_endIndex).ascii());
}

