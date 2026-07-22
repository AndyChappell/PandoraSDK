/**
 *  @file   PandoraSDK/include/Geometry/LArReadoutChannel.h
 *
 *  @brief  This class describes a readout channel for a LArTPC. For example, in a horizontal drift TPC, this class represents a wire
 *          associated with a particular wire plane.
 *
 *  $Log: $
 */
#ifndef PANDORA_LAR_READOUT_CHANNEL_H
#define PANDORA_LAR_READOUT_CHANNEL_H 1

#include "Pandora/PandoraEnumeratedTypes.h"

#include <unordered_map>
#include <utility>

namespace pandora
{
/**
 *  @brief  LArReadoutChannel class. This class describes a readout channel for a LARTPC. For example, in a horizontal drift TPC, this class
 *          represents a wire associated with a particular wire plane.
 */
class LArReadoutChannel
{
public:
    typedef std::pair<unsigned int, unsigned int> ChannelInterval;
    typedef std::unordered_map<pandora::HitType, ChannelInterval> ViewChannelIntervalMap;

    /**
     *  @brief  Constructor
     *
     *  @param  id the readout unit id (e,g, plane id in a horizontal drift TPC)
     *  @param  channelIntervalMap a map, keyed by view, describing the channel id interval for channels in other views that this channel can
     *          'intersect'
     */
    LArReadoutChannel(unsigned int id, const ViewChannelIntervalMap &channelIntervalMap);

    /**
     *  @brief  Destructor
     */
    virtual ~LArReadoutChannel();

    /**
     *  @brief  Get the id of the readout unit.
     *
     *  @return the readout unit id
     */
    unsigned int GetId() const;

    /**
     *  @brief  Get the channel interval for the specified view.
     *
     *  @param  view the view for which to retrieve the channel interval
     *
     *  @return the channel interval [min, max] for the specified view
     *  @throws StatusCodeException if the specified view is not present in the channel interval map
     */
    const ChannelInterval &GetChannelInterval(const pandora::HitType view) const;

private:
    unsigned int m_id;                              ///< The id of the readout volume
    ViewChannelIntervalMap m_channelIntervalMap;    ///< A map, keyed by view, describing the channel id 'intersection' interval
};

//------------------------------------------------------------------------------------------------------------------------------------------

inline unsigned int LArReadoutChannel::GetId() const
{
    return m_id;
}

} // namespace pandora

#endif // #ifndef PANDORA_LAR_READOUT_CHANNEL_H

