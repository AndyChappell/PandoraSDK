/**
 *  @file   PandoraSDK/src/Geometry/LArReadoutChannel.cc
 *
 *  @brief  Implementation of the LArReadoutChannel class.
 *
 *  $Log: $
 */

#include "Geometry/LArReadoutChannel.h"

#include "Pandora/StatusCodes.h"

namespace pandora
{

LArReadoutChannel::LArReadoutChannel(unsigned int id, const ViewChannelIntervalMap &channelIntervalMap) :
    m_id(id),
    m_channelIntervalMap(channelIntervalMap)
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

const LArReadoutChannel::ChannelInterval &LArReadoutChannel::GetChannelInterval(const pandora::HitType view) const
{
    ViewChannelIntervalMap::const_iterator iter{m_channelIntervalMap.find(view)};

    if (m_channelIntervalMap.end() == iter)
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);

    return iter->second;
}

//------------------------------------------------------------------------------------------------------------------------------------------

LArReadoutChannel::~LArReadoutChannel()
{
}

} // namespace pandora

