/**
 *  @file   PandoraSDK/src/Persistency/XmlFileWriter.cc
 *
 *  @brief  Implementation of the xml file writer class.
 *
 *  $Log: $
 */

#include "Api/PandoraContentApi.h"
#include "Api/PandoraContentApiImpl.h"

#include "Geometry/DetectorGap.h"
#include "Geometry/LArTPC.h"
#include "Geometry/SubDetector.h"

#include "Objects/CaloHit.h"
#include "Objects/MCParticle.h"
#include "Objects/Track.h"

#include "Persistency/XmlFileWriter.h"

#include <cstdint>
#include <cstring>
#include <sstream>

namespace pandora
{

namespace
{

//------------------------------------------------------------------------------------------------------------------------------------------
// FieldMap -> XML text helpers
//
// The FieldMap stores everything as raw bytes using the same layout as
// BinaryFileWriter. To emit human-readable XML we reinterpret those bytes
// back into typed values. We dispatch on byte count, which is unambiguous
// for the SDK field set:
//   1  byte  -> uint8  (bool stored as 1-byte int)
//   4  bytes -> uint32 (float, int, unsigned int, enum - stored as bit pattern)
//   8  bytes -> uint64 (uintptr_t for addresses)
//  12  bytes -> three floats (CartesianVector)
//  24  bytes -> six floats (TrackState)
//  4+N bytes -> length-prefixed string
//------------------------------------------------------------------------------------------------------------------------------------------

std::string Uint32ToString(const std::vector<unsigned char> &bytes)
{
    uint32_t v = 0;
    std::memcpy(&v, bytes.data(), sizeof(uint32_t));
    return std::to_string(v);
}

std::string Uint64ToString(const std::vector<unsigned char> &bytes)
{
    uint64_t v = 0;
    std::memcpy(&v, bytes.data(), sizeof(uint64_t));
    return std::to_string(v);
}

std::string CartesianVectorToString(const std::vector<unsigned char> &bytes)
{
    float x = 0.f, y = 0.f, z = 0.f;
    std::memcpy(&x, bytes.data() + 0 * sizeof(float), sizeof(float));
    std::memcpy(&y, bytes.data() + 1 * sizeof(float), sizeof(float));
    std::memcpy(&z, bytes.data() + 2 * sizeof(float), sizeof(float));
    return TypeToStringPrecision(x) + " " + TypeToStringPrecision(y) + " " + TypeToStringPrecision(z);
}

std::string TrackStateToString(const std::vector<unsigned char> &bytes)
{
    float f[6] = {};
    for (int i = 0; i < 6; ++i)
        std::memcpy(&f[i], bytes.data() + i * sizeof(float), sizeof(float));
    std::string s;
    for (int i = 0; i < 6; ++i)
        s += TypeToStringPrecision(f[i]) + (i < 5 ? " " : "");
    return s;
}

std::string StringFieldToString(const std::vector<unsigned char> &bytes)
{
    if (bytes.size() < sizeof(uint32_t))
        return {};
    uint32_t len = 0;
    std::memcpy(&len, bytes.data(), sizeof(uint32_t));
    if (bytes.size() != sizeof(uint32_t) + len)
        return {};
    return std::string(reinterpret_cast<const char *>(bytes.data() + sizeof(uint32_t)), len);
}

std::string FieldBytesToString(const std::vector<unsigned char> &bytes)
{
    switch (bytes.size())
    {
        case 1:  { uint8_t v = 0; std::memcpy(&v, bytes.data(), 1); return std::to_string(v); }
        case 4:  return Uint32ToString(bytes);
        case 8:  return Uint64ToString(bytes);
        case 12: return CartesianVectorToString(bytes);
        case 24: return TrackStateToString(bytes);
        default:
        {
            if (bytes.size() >= sizeof(uint32_t))
            {
                uint32_t len = 0;
                std::memcpy(&len, bytes.data(), sizeof(uint32_t));
                if (bytes.size() == sizeof(uint32_t) + len)
                    return StringFieldToString(bytes);
            }
            std::ostringstream oss;
            oss << std::hex;
            for (unsigned char b : bytes)
                oss << static_cast<unsigned int>(b);
            return oss.str();
        }
    }
}

} // anonymous namespace

//------------------------------------------------------------------------------------------------------------------------------------------
// Constructor / destructor
//------------------------------------------------------------------------------------------------------------------------------------------

XmlFileWriter::XmlFileWriter(const pandora::Pandora &pandora, const std::string &fileName,
    const FileMode fileMode) :
    FileWriter(pandora, fileName),
    m_pXmlDocument(nullptr),
    m_pContainerXmlElement(nullptr),
    m_pCurrentXmlElement(nullptr)
{
    m_fileType = XML;

    if (APPEND == fileMode)
    {
        m_pXmlDocument = new TiXmlDocument(fileName);

        if (!m_pXmlDocument->LoadFile())
        {
            // File doesn't exist yet — start a fresh document.
            delete m_pXmlDocument;
            m_pXmlDocument = new TiXmlDocument;
            m_pXmlDocument->LinkEndChild(new TiXmlElement("PandoraFile"));
        }
    }
    else if (OVERWRITE == fileMode)
    {
        m_pXmlDocument = new TiXmlDocument;
        m_pXmlDocument->LinkEndChild(new TiXmlElement("PandoraFile"));
    }
    else
    {
        throw StatusCodeException(STATUS_CODE_INVALID_PARAMETER);
    }

    const ComponentId allComponents[] = {
        CALO_HIT_COMPONENT, TRACK_COMPONENT, MC_PARTICLE_COMPONENT,
        RELATIONSHIP_COMPONENT, SUB_DETECTOR_COMPONENT, LINE_GAP_COMPONENT,
        BOX_GAP_COMPONENT, CONCENTRIC_GAP_COMPONENT, LAR_TPC_COMPONENT,
        EVENT_INFO_COMPONENT
    };

    for (const ComponentId id : allComponents)
    {
        ComponentSchemaVersion entry;
        entry.m_componentId   = id;
        entry.m_schemaVersion = GetSchemaVersion(id);
        m_schemaRegistry.push_back(entry);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

XmlFileWriter::~XmlFileWriter()
{
    m_pXmlDocument->SaveFile(m_fileName);
    delete m_pXmlDocument;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Schema version table
//------------------------------------------------------------------------------------------------------------------------------------------

unsigned int XmlFileWriter::GetSchemaVersion(const ComponentId componentId)
{
    switch (componentId)
    {
        case CALO_HIT_COMPONENT:       return 1;
        case TRACK_COMPONENT:          return 1;
        case MC_PARTICLE_COMPONENT:    return 1;
        case RELATIONSHIP_COMPONENT:   return 1;
        case SUB_DETECTOR_COMPONENT:   return 1;
        case LINE_GAP_COMPONENT:       return 1;
        case BOX_GAP_COMPONENT:        return 1;
        case CONCENTRIC_GAP_COMPONENT: return 1;
        case LAR_TPC_COMPONENT:        return 1;
        case EVENT_INFO_COMPONENT:     return 1;
        default:                       return 0;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Container framing
//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileWriter::WriteHeader(const ContainerId containerId)
{
    const std::string key(
        (HEADER_CONTAINER   == containerId) ? "Header"   :
        (GEOMETRY_CONTAINER == containerId) ? "Geometry" :
        (EVENT_CONTAINER    == containerId) ? "Event"    : "Unknown");

    TiXmlElement *const pRoot = m_pXmlDocument->RootElement();

    if (!pRoot)
        return STATUS_CODE_FAILURE;

    m_pContainerXmlElement = new TiXmlElement(key);
    pRoot->LinkEndChild(m_pContainerXmlElement);

    m_containerId = containerId;
    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileWriter::WriteFooter()
{
    if ((HEADER_CONTAINER   != m_containerId) &&
        (EVENT_CONTAINER    != m_containerId) &&
        (GEOMETRY_CONTAINER != m_containerId))
        return STATUS_CODE_FAILURE;

    m_pContainerXmlElement = nullptr;
    m_pCurrentXmlElement   = nullptr;
    m_containerId          = UNKNOWN_CONTAINER;
    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// WriteComponent
//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileWriter::WriteComponent(const std::string &elementName,
    const unsigned int schemaVersion, const FieldMap &fields)
{
    if (!m_pContainerXmlElement)
        return STATUS_CODE_FAILURE;

    TiXmlElement *const pComponentElement = new TiXmlElement(elementName);
    pComponentElement->SetAttribute("schemaVersion", static_cast<int>(schemaVersion));

    for (const auto &entry : fields.GetAllFields())
    {
        TiXmlElement *const pFieldElement = new TiXmlElement(entry.first);
        pFieldElement->LinkEndChild(new TiXmlText(FieldBytesToString(entry.second)));
        pComponentElement->LinkEndChild(pFieldElement);
    }

    m_pContainerXmlElement->LinkEndChild(pComponentElement);

    m_pCurrentXmlElement = pComponentElement;

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Global header
//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileWriter::WriteGlobalHeader()
{
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteHeader(HEADER_CONTAINER));

    if (HEADER_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteMetadata());
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteSchemaRegistry());
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteFooter());

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileWriter::WriteMetadata()
{
    if (HEADER_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    FieldMap fields;
    fields.Set("producerName",      m_metadata.m_producerName);
    fields.Set("producerVersion",   m_metadata.m_producerVersion);
    fields.Set("creationTimestamp", m_metadata.m_creationTimestamp);
    fields.Set("description",       m_metadata.m_description);

    for (const auto &kv : m_metadata.m_userParameters)
        fields.Set(std::string("userParam:") + kv.first, kv.second);

    return this->WriteComponent("Metadata", 1u, fields);
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileWriter::WriteSchemaRegistry()
{
    if (HEADER_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    FieldMap fields;

    for (const ComponentSchemaVersion &entry : m_schemaRegistry)
        fields.Set(std::to_string(static_cast<uint32_t>(entry.m_componentId)), entry.m_schemaVersion);

    return this->WriteComponent("SchemaRegistry", 0u, fields);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Geometry components
//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileWriter::WriteSubDetector(const SubDetector *const pSubDetector)
{
    if (GEOMETRY_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    FieldMap fields;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pSubDetectorFactory->Write(pSubDetector, fields));

    fields.Set("subDetectorName",      pSubDetector->GetSubDetectorName());
    fields.Set("subDetectorType",      pSubDetector->GetSubDetectorType());
    fields.Set("innerRCoordinate",     pSubDetector->GetInnerRCoordinate());
    fields.Set("innerZCoordinate",     pSubDetector->GetInnerZCoordinate());
    fields.Set("innerPhiCoordinate",   pSubDetector->GetInnerPhiCoordinate());
    fields.Set("innerSymmetryOrder",   pSubDetector->GetInnerSymmetryOrder());
    fields.Set("outerRCoordinate",     pSubDetector->GetOuterRCoordinate());
    fields.Set("outerZCoordinate",     pSubDetector->GetOuterZCoordinate());
    fields.Set("outerPhiCoordinate",   pSubDetector->GetOuterPhiCoordinate());
    fields.Set("outerSymmetryOrder",   pSubDetector->GetOuterSymmetryOrder());
    fields.Set("isMirroredInZ",        pSubDetector->IsMirroredInZ());

    const SubDetector::SubDetectorLayerVector &layers(pSubDetector->GetSubDetectorLayerVector());
    const unsigned int nLayers = static_cast<unsigned int>(layers.size());

    if (pSubDetector->GetNLayers() != nLayers)
        return STATUS_CODE_FAILURE;

    fields.Set("nLayers", nLayers);

    for (unsigned int i = 0; i < nLayers; ++i)
    {
        const std::string prefix("layer" + std::to_string(i) + "_");
        fields.Set(prefix + "closestDistanceToIp", layers[i].GetClosestDistanceToIp());
        fields.Set(prefix + "nRadiationLengths",   layers[i].GetNRadiationLengths());
        fields.Set(prefix + "nInteractionLengths", layers[i].GetNInteractionLengths());
    }

    return this->WriteComponent("SubDetector", GetSchemaVersion(SUB_DETECTOR_COMPONENT), fields);
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileWriter::WriteLArTPC(const LArTPC *const pLArTPC)
{
    if (GEOMETRY_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    FieldMap fields;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pLArTPCFactory->Write(pLArTPC, fields));

    fields.Set("larTPCVolumeId",     pLArTPC->GetLArTPCVolumeId());
    fields.Set("centerX",            pLArTPC->GetCenterX());
    fields.Set("centerY",            pLArTPC->GetCenterY());
    fields.Set("centerZ",            pLArTPC->GetCenterZ());
    fields.Set("widthX",             pLArTPC->GetWidthX());
    fields.Set("widthY",             pLArTPC->GetWidthY());
    fields.Set("widthZ",             pLArTPC->GetWidthZ());
    fields.Set("wirePitchU",         pLArTPC->GetWirePitchU());
    fields.Set("wirePitchV",         pLArTPC->GetWirePitchV());
    fields.Set("wirePitchW",         pLArTPC->GetWirePitchW());
    fields.Set("wireAngleU",         pLArTPC->GetWireAngleU());
    fields.Set("wireAngleV",         pLArTPC->GetWireAngleV());
    fields.Set("wireAngleW",         pLArTPC->GetWireAngleW());
    fields.Set("sigmaUVW",           pLArTPC->GetSigmaUVW());
    fields.Set("isDriftInPositiveX", pLArTPC->IsDriftInPositiveX());

    return this->WriteComponent("LArTPC", GetSchemaVersion(LAR_TPC_COMPONENT), fields);
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileWriter::WriteDetectorGap(const DetectorGap *const pDetectorGap)
{
    if (GEOMETRY_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    const LineGap       *const pLineGap       = dynamic_cast<const LineGap *>(pDetectorGap);
    const BoxGap        *const pBoxGap        = dynamic_cast<const BoxGap *>(pDetectorGap);
    const ConcentricGap *const pConcentricGap = dynamic_cast<const ConcentricGap *>(pDetectorGap);

    if (nullptr != pLineGap)
    {
        FieldMap fields;
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pLineGapFactory->Write(pLineGap, fields));
        fields.Set("lineGapType", pLineGap->GetLineGapType());
        fields.Set("lineStartX",  pLineGap->GetLineStartX());
        fields.Set("lineEndX",    pLineGap->GetLineEndX());
        fields.Set("lineStartZ",  pLineGap->GetLineStartZ());
        fields.Set("lineEndZ",    pLineGap->GetLineEndZ());
        return this->WriteComponent("LineGap", GetSchemaVersion(LINE_GAP_COMPONENT), fields);
    }
    else if (nullptr != pBoxGap)
    {
        FieldMap fields;
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pBoxGapFactory->Write(pBoxGap, fields));
        fields.Set("vertex", pBoxGap->GetVertex());
        fields.Set("side1",  pBoxGap->GetSide1());
        fields.Set("side2",  pBoxGap->GetSide2());
        fields.Set("side3",  pBoxGap->GetSide3());
        return this->WriteComponent("BoxGap", GetSchemaVersion(BOX_GAP_COMPONENT), fields);
    }
    else if (nullptr != pConcentricGap)
    {
        FieldMap fields;
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pConcentricGapFactory->Write(pConcentricGap, fields));
        fields.Set("minZCoordinate",     pConcentricGap->GetMinZCoordinate());
        fields.Set("maxZCoordinate",     pConcentricGap->GetMaxZCoordinate());
        fields.Set("innerRCoordinate",   pConcentricGap->GetInnerRCoordinate());
        fields.Set("innerPhiCoordinate", pConcentricGap->GetInnerPhiCoordinate());
        fields.Set("innerSymmetryOrder", pConcentricGap->GetInnerSymmetryOrder());
        fields.Set("outerRCoordinate",   pConcentricGap->GetOuterRCoordinate());
        fields.Set("outerPhiCoordinate", pConcentricGap->GetOuterPhiCoordinate());
        fields.Set("outerSymmetryOrder", pConcentricGap->GetOuterSymmetryOrder());
        return this->WriteComponent("ConcentricGap", GetSchemaVersion(CONCENTRIC_GAP_COMPONENT), fields);
    }

    return STATUS_CODE_FAILURE;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Event components
//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileWriter::WriteCaloHit(const CaloHit *const pCaloHit)
{
    if (EVENT_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    FieldMap fields;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pCaloHitFactory->Write(pCaloHit, fields));

    fields.Set("cellGeometry",            pCaloHit->GetCellGeometry());
    fields.Set("positionVector",          pCaloHit->GetPositionVector());
    fields.Set("expectedDirection",       pCaloHit->GetExpectedDirection());
    fields.Set("cellNormalVector",        pCaloHit->GetCellNormalVector());
    fields.Set("cellThickness",           pCaloHit->GetCellThickness());
    fields.Set("nCellRadiationLengths",   pCaloHit->GetNCellRadiationLengths());
    fields.Set("nCellInteractionLengths", pCaloHit->GetNCellInteractionLengths());
    fields.Set("time",                    pCaloHit->GetTime());
    fields.Set("inputEnergy",             pCaloHit->GetInputEnergy());
    fields.Set("mipEquivalentEnergy",     pCaloHit->GetMipEquivalentEnergy());
    fields.Set("electromagneticEnergy",   pCaloHit->GetElectromagneticEnergy());
    fields.Set("hadronicEnergy",          pCaloHit->GetHadronicEnergy());
    fields.Set("isDigital",               pCaloHit->IsDigital());
    fields.Set("hitType",                 pCaloHit->GetHitType());
    fields.Set("hitRegion",               pCaloHit->GetHitRegion());
    fields.Set("layer",                   pCaloHit->GetLayer());
    fields.Set("isInOuterSamplingLayer",  pCaloHit->IsInOuterSamplingLayer());
    fields.Set("parentAddress",           pCaloHit->GetParentAddress());
    fields.Set("cellSize0",               pCaloHit->GetCellSize0());
    fields.Set("cellSize1",               pCaloHit->GetCellSize1());

    return this->WriteComponent("CaloHit", GetSchemaVersion(CALO_HIT_COMPONENT), fields);
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileWriter::WriteTrack(const Track *const pTrack)
{
    if (EVENT_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    FieldMap fields;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pTrackFactory->Write(pTrack, fields));

    fields.Set("d0",                      pTrack->GetD0());
    fields.Set("z0",                      pTrack->GetZ0());
    fields.Set("particleId",              pTrack->GetParticleId());
    fields.Set("charge",                  pTrack->GetCharge());
    fields.Set("mass",                    pTrack->GetMass());
    fields.Set("momentumAtDca",           pTrack->GetMomentumAtDca());
    fields.Set("trackStateAtStart",       pTrack->GetTrackStateAtStart());
    fields.Set("trackStateAtEnd",         pTrack->GetTrackStateAtEnd());
    fields.Set("trackStateAtCalorimeter", pTrack->GetTrackStateAtCalorimeter());
    fields.Set("timeAtCalorimeter",       pTrack->GetTimeAtCalorimeter());
    fields.Set("reachesCalorimeter",      pTrack->ReachesCalorimeter());
    fields.Set("isProjectedToEndCap",     pTrack->IsProjectedToEndCap());
    fields.Set("canFormPfo",              pTrack->CanFormPfo());
    fields.Set("canFormClusterlessPfo",   pTrack->CanFormClusterlessPfo());
    fields.Set("parentAddress",           pTrack->GetParentAddress());

    return this->WriteComponent("Track", GetSchemaVersion(TRACK_COMPONENT), fields);
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileWriter::WriteMCParticle(const MCParticle *const pMCParticle)
{
    if (EVENT_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    FieldMap fields;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pMCParticleFactory->Write(pMCParticle, fields));

    fields.Set("energy",         pMCParticle->GetEnergy());
    fields.Set("momentum",       pMCParticle->GetMomentum());
    fields.Set("vertex",         pMCParticle->GetVertex());
    fields.Set("endpoint",       pMCParticle->GetEndpoint());
    fields.Set("particleId",     pMCParticle->GetParticleId());
    fields.Set("mcParticleType", pMCParticle->GetMCParticleType());
    fields.Set("uid",            pMCParticle->GetUid());

    return this->WriteComponent("MCParticle", GetSchemaVersion(MC_PARTICLE_COMPONENT), fields);
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileWriter::WriteRelationship(const RelationshipId relationshipId,
    const void *address1, const void *address2, const float weight)
{
    if (EVENT_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    FieldMap fields;
    fields.Set("relationshipId", static_cast<uint32_t>(relationshipId));
    fields.Set("address1",       reinterpret_cast<uintptr_t>(address1));
    fields.Set("address2",       reinterpret_cast<uintptr_t>(address2));
    fields.Set("weight",         weight);

    return this->WriteComponent("Relationship", GetSchemaVersion(RELATIONSHIP_COMPONENT), fields);
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileWriter::WriteEventInformation()
{
    if (EVENT_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    FieldMap fields;
    fields.Set("run",    m_pPandora->GetRun());
    fields.Set("subrun", m_pPandora->GetSubrun());
    fields.Set("event",  m_pPandora->GetEvent());

    return this->WriteComponent("EventInfo", GetSchemaVersion(EVENT_INFO_COMPONENT), fields);
}

} // namespace pandora
