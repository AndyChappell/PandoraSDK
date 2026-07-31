/**
 *  @file   PandoraSDK/src/Persistency/XmlFileReader.cc
 *
 *  @brief  Implementation of the xml file reader class.
 *
 *  $Log: $
 */

#include "Api/PandoraApi.h"

#include "Objects/CaloHit.h"
#include "Objects/Track.h"

#include "Persistency/XmlFileReader.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>

namespace pandora
{

namespace
{

//------------------------------------------------------------------------------------------------------------------------------------------
// XML text -> FieldMap raw-byte helpers
//
// These are the exact inverses of the FieldBytesToString functions in
// XmlFileWriter.cc. Each takes a string from an XML text node and produces
// the same raw-byte layout that FieldMap::Set<T> would have stored.
// Dispatch is done by the tag name via a conventions table (see
// ReadComponentFields below); for tags whose type is unknown we store the
// text as a length-prefixed string so the FieldMap round-trips cleanly.
//------------------------------------------------------------------------------------------------------------------------------------------

std::vector<unsigned char> Uint32FromString(const std::string &s)
{
    uint32_t v = static_cast<uint32_t>(std::stoul(s));
    std::vector<unsigned char> bytes(sizeof(uint32_t));
    std::memcpy(bytes.data(), &v, sizeof(uint32_t));
    return bytes;
}

std::vector<unsigned char> Uint64FromString(const std::string &s)
{
    uint64_t v = static_cast<uint64_t>(std::stoull(s));
    std::vector<unsigned char> bytes(sizeof(uint64_t));
    std::memcpy(bytes.data(), &v, sizeof(uint64_t));
    return bytes;
}

std::vector<unsigned char> CartesianVectorFromString(const std::string &s)
{
    std::istringstream iss(s);
    float x = 0.f, y = 0.f, z = 0.f;
    iss >> x >> y >> z;
    std::vector<unsigned char> bytes(3 * sizeof(float));
    std::memcpy(bytes.data() + 0 * sizeof(float), &x, sizeof(float));
    std::memcpy(bytes.data() + 1 * sizeof(float), &y, sizeof(float));
    std::memcpy(bytes.data() + 2 * sizeof(float), &z, sizeof(float));
    return bytes;
}

std::vector<unsigned char> TrackStateFromString(const std::string &s)
{
    std::istringstream iss(s);
    float f[6] = {};
    for (int i = 0; i < 6; ++i) iss >> f[i];
    std::vector<unsigned char> bytes(6 * sizeof(float));
    for (int i = 0; i < 6; ++i)
        std::memcpy(bytes.data() + i * sizeof(float), &f[i], sizeof(float));
    return bytes;
}

std::vector<unsigned char> StringToBytes(const std::string &s)
{
    uint32_t len = static_cast<uint32_t>(s.size());
    std::vector<unsigned char> bytes(sizeof(uint32_t) + len);
    std::memcpy(bytes.data(), &len, sizeof(uint32_t));
    if (len > 0)
        std::memcpy(bytes.data() + sizeof(uint32_t), s.data(), len);
    return bytes;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Tag-to-encoding table.
//
// For every field tag written by the SDK writers we record whether it is:
//   UINT32  — 4-byte integer/enum/float/bool
//   UINT64  — 8-byte address (uintptr_t)
//   CVEC    — CartesianVector (12 bytes)
//   TSTATE  — TrackState (24 bytes)
//   STRING  — length-prefixed string
//
// This lets ReadComponentFields reconstruct the correct raw-byte layout
// without needing type information at the call site.
//------------------------------------------------------------------------------------------------------------------------------------------

enum FieldEncoding { UINT32, UINT64, CVEC, TSTATE, STRING };

FieldEncoding GetFieldEncoding(const std::string &tag)
{
    // Addresses
    if (tag == "address1" || tag == "address2" || tag == "parentAddress" || tag == "uid")
        return UINT64;

    // CartesianVectors
    if (tag == "positionVector"   || tag == "expectedDirection" ||
        tag == "cellNormalVector" || tag == "momentumAtDca"     ||
        tag == "momentum"         || tag == "vertex"            ||
        tag == "endpoint"         || tag == "side1"             ||
        tag == "side2"            || tag == "side3"             ||
        tag == "vertexGap")
        return CVEC;

    // TrackStates
    if (tag == "trackStateAtStart"       || tag == "trackStateAtEnd" ||
        tag == "trackStateAtCalorimeter")
        return TSTATE;

    // String fields
    if (tag == "subDetectorName"    || tag == "producerName"    ||
        tag == "producerVersion"    || tag == "creationTimestamp" ||
        tag == "description"        || tag.substr(0, 10) == "userParam:")
        return STRING;

    // Everything else: uint32 (covers floats, ints, enums, bool, unsigned)
    return UINT32;
}

} // anonymous namespace

//------------------------------------------------------------------------------------------------------------------------------------------
// Constructor / destructor
//------------------------------------------------------------------------------------------------------------------------------------------

XmlFileReader::XmlFileReader(const pandora::Pandora &pandora, const std::string &fileName) :
    FileReader(pandora, fileName),
    m_pXmlDocument(nullptr),
    m_pContainerXmlNode(nullptr),
    m_pCurrentXmlElement(nullptr),
    m_isAtFileStart(true)
{
    m_fileType = XML;
    m_pXmlDocument = new TiXmlDocument(fileName);

    if (!m_pXmlDocument->LoadFile())
    {
        std::cout << "XmlFileReader — invalid or missing XML file: " << fileName << std::endl;
        delete m_pXmlDocument;
        throw StatusCodeException(STATUS_CODE_FAILURE);
    }

    // Seed the container cursor at the root element's first child so that
    // GetNextContainerId() works correctly before the first GoToNextContainer.
    m_pContainerXmlNode = TiXmlHandle(m_pXmlDocument).FirstChildElement().FirstChild().Node();
}

//------------------------------------------------------------------------------------------------------------------------------------------

XmlFileReader::~XmlFileReader()
{
    delete m_pXmlDocument;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Migration registration
//------------------------------------------------------------------------------------------------------------------------------------------

void XmlFileReader::RegisterMigration(const ComponentId componentId,
    const unsigned int fromVersion, const unsigned int toVersion, MigrationFn fn)
{
    if (toVersion != fromVersion + 1)
        throw StatusCodeException(STATUS_CODE_INVALID_PARAMETER);

    MigrationKey key;
    key.m_componentId = componentId;
    key.m_fromVersion = fromVersion;
    m_migrations[key] = std::move(fn);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Container navigation — structurally identical to original
//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadHeader()
{
    m_pCurrentXmlElement = nullptr;
    m_containerId        = this->GetNextContainerId();

    if ((HEADER_CONTAINER   != m_containerId) &&
        (EVENT_CONTAINER    != m_containerId) &&
        (GEOMETRY_CONTAINER != m_containerId))
        return STATUS_CODE_FAILURE;

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::GoToNextContainer()
{
    m_pCurrentXmlElement = nullptr;

    if (m_isAtFileStart)
    {
        // First call: position at the first child of the root element.
        if (!m_pContainerXmlNode)
            m_pContainerXmlNode = TiXmlHandle(m_pXmlDocument).FirstChildElement().FirstChild().Node();

        m_isAtFileStart = false;
    }
    else
    {
        if (!m_pContainerXmlNode)
            throw StatusCodeException(STATUS_CODE_NOT_FOUND);

        m_pContainerXmlNode = m_pContainerXmlNode->NextSibling();
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

ContainerId XmlFileReader::GetNextContainerId()
{
    const std::string name((nullptr != m_pContainerXmlNode) ? m_pContainerXmlNode->ValueStr() : "");

    if      ("Header"   == name) return HEADER_CONTAINER;
    else if ("Event"    == name) return EVENT_CONTAINER;
    else if ("Geometry" == name) return GEOMETRY_CONTAINER;
    else                         return UNKNOWN_CONTAINER;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::GoToGeometry(const unsigned int geometryNumber)
{
    int nGeometriesRead(0);
    m_isAtFileStart      = true;
    m_pContainerXmlNode  = nullptr;
    m_pCurrentXmlElement = nullptr;

    if (GEOMETRY_CONTAINER != this->GetNextContainerId())
        --nGeometriesRead;

    while (nGeometriesRead < static_cast<int>(geometryNumber))
    {
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->GoToNextGeometry());
        ++nGeometriesRead;
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::GoToEvent(const unsigned int eventNumber)
{
    int nEventsRead(0);
    m_isAtFileStart      = true;
    m_pContainerXmlNode  = nullptr;
    m_pCurrentXmlElement = nullptr;

    if (EVENT_CONTAINER != this->GetNextContainerId())
        --nEventsRead;

    while (nEventsRead < static_cast<int>(eventNumber))
    {
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->GoToNextEvent());
        ++nEventsRead;
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Unified component read path
//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadComponentFields(unsigned int &schemaVersion, FieldMap &fields) const
{
    if (!m_pCurrentXmlElement)
        return STATUS_CODE_FAILURE;

    // Read schemaVersion attribute.
    schemaVersion = 0;
    const char *const pAttr = m_pCurrentXmlElement->Attribute("schemaVersion");

    if (nullptr != pAttr)
    {
        try { schemaVersion = static_cast<unsigned int>(std::stoul(pAttr)); }
        catch (...) { schemaVersion = 0; }
    }

    // Each child element is a field: its tag name and text content.
    for (TiXmlElement *pChild = m_pCurrentXmlElement->FirstChildElement();
         nullptr != pChild;
         pChild = pChild->NextSiblingElement())
    {
        const std::string tag(pChild->ValueStr());
        const std::string text(pChild->GetText() ? pChild->GetText() : "");

        // Reconstruct raw bytes according to the known encoding for this tag.
        std::vector<unsigned char> bytes;

        switch (GetFieldEncoding(tag))
        {
            case UINT32: bytes = Uint32FromString(text.empty() ? "0" : text); break;
            case UINT64: bytes = Uint64FromString(text.empty() ? "0" : text); break;
            case CVEC:   bytes = CartesianVectorFromString(text);              break;
            case TSTATE: bytes = TrackStateFromString(text);                   break;
            case STRING: bytes = StringToBytes(text);                          break;
        }

        fields.SetRawBytes(tag, std::move(bytes));
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void XmlFileReader::ApplyMigrations(const ComponentId componentId,
    const unsigned int fileSchemaVersion, FieldMap &fields) const
{
    unsigned int version = fileSchemaVersion;

    while (true)
    {
        MigrationKey key;
        key.m_componentId = componentId;
        key.m_fromVersion = version;

        auto it = m_migrations.find(key);
        if (it == m_migrations.end())
            break;

        it->second(fields);
        ++version;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadNextComponent([[maybe_unused]] const ContainerId expectedContainer)
{
    // Advance to the next sibling element within the current container.
    if (!m_pCurrentXmlElement)
    {
        TiXmlHandle localHandle(m_pContainerXmlNode);
        m_pCurrentXmlElement = localHandle.FirstChild().Element();
    }
    else
    {
        m_pCurrentXmlElement = m_pCurrentXmlElement->NextSiblingElement();
    }

    // No more siblings — end of container.
    if (!m_pCurrentXmlElement)
    {
        m_containerId = UNKNOWN_CONTAINER;
        return STATUS_CODE_NOT_FOUND;
    }

    const std::string elementName(m_pCurrentXmlElement->ValueStr());

    // Populate FieldMap.
    unsigned int schemaVersion = 0;
    FieldMap fields;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->ReadComponentFields(schemaVersion, fields));

    // Dispatch by element name. Unknown names are logged and skipped.
    // Global header components
    if ("Metadata" == elementName)
        return this->ReadMetadata(fields);

    if ("SchemaRegistry" == elementName)
        return this->ReadSchemaRegistry(fields);

    // Geometry components
    if ("SubDetector" == elementName)
    {
        this->ApplyMigrations(SUB_DETECTOR_COMPONENT, schemaVersion, fields);
        return this->ReadSubDetector(fields);
    }
    if ("LArTPC" == elementName)
    {
        this->ApplyMigrations(LAR_TPC_COMPONENT, schemaVersion, fields);
        return this->ReadLArTPC(fields);
    }
    if ("LineGap" == elementName)
    {
        this->ApplyMigrations(LINE_GAP_COMPONENT, schemaVersion, fields);
        return this->ReadLineGap(fields);
    }
    if ("BoxGap" == elementName)
    {
        this->ApplyMigrations(BOX_GAP_COMPONENT, schemaVersion, fields);
        return this->ReadBoxGap(fields);
    }
    if ("ConcentricGap" == elementName)
    {
        this->ApplyMigrations(CONCENTRIC_GAP_COMPONENT, schemaVersion, fields);
        return this->ReadConcentricGap(fields);
    }

    // Event components
    if ("CaloHit" == elementName)
    {
        this->ApplyMigrations(CALO_HIT_COMPONENT, schemaVersion, fields);
        return this->ReadCaloHit(fields);
    }
    if ("Track" == elementName)
    {
        this->ApplyMigrations(TRACK_COMPONENT, schemaVersion, fields);
        return this->ReadTrack(fields);
    }
    if ("MCParticle" == elementName)
    {
        this->ApplyMigrations(MC_PARTICLE_COMPONENT, schemaVersion, fields);
        return this->ReadMCParticle(fields);
    }
    if ("Relationship" == elementName)
    {
        this->ApplyMigrations(RELATIONSHIP_COMPONENT, schemaVersion, fields);
        return this->ReadRelationship(fields);
    }
    if ("EventInfo" == elementName)
    {
        this->ApplyMigrations(EVENT_INFO_COMPONENT, schemaVersion, fields);
        return this->ReadEventInformation(fields);
    }

    // Unknown element — skip with a warning.
    std::cout << "XmlFileReader: skipping unknown element <" << elementName << ">" << std::endl;
    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// FileReader dispatch entries
//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadNextGlobalHeaderComponent()
{
    // Guard: if we've moved past the header container, signal end.
    if (HEADER_CONTAINER != this->GetNextContainerId() && HEADER_CONTAINER == m_containerId)
    {
        m_containerId = UNKNOWN_CONTAINER;
        return STATUS_CODE_NOT_FOUND;
    }

    return this->ReadNextComponent(HEADER_CONTAINER);
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadNextGeometryComponent()
{
    return this->ReadNextComponent(GEOMETRY_CONTAINER);
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadNextEventComponent()
{
    return this->ReadNextComponent(EVENT_CONTAINER);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Global header deserialisers
//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadMetadata(const FieldMap &fields)
{
    if (HEADER_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    m_metadata.m_producerName      = fields.GetOrDefault<std::string>("producerName",      std::string());
    m_metadata.m_producerVersion   = fields.GetOrDefault<std::string>("producerVersion",   std::string());
    m_metadata.m_creationTimestamp = fields.GetOrDefault<std::string>("creationTimestamp", std::string());
    m_metadata.m_description       = fields.GetOrDefault<std::string>("description",       std::string());

    for (const auto &entry : fields.GetAllFields())
    {
        const std::string &tag    = entry.first;
        const std::string  prefix = "userParam:";

        if (tag.size() > prefix.size() && tag.substr(0, prefix.size()) == prefix)
        {
            std::string value;
            if (STATUS_CODE_SUCCESS == fields.Get(tag, value))
                m_metadata.m_userParameters[tag.substr(prefix.size())] = value;
        }
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadSchemaRegistry(const FieldMap &fields)
{
    if (HEADER_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    m_schemaRegistry.clear();

    for (const auto &entry : fields.GetAllFields())
    {
        try
        {
            const unsigned int componentIdVal = static_cast<unsigned int>(std::stoul(entry.first));
            unsigned int schemaVersion = 0;

            if (STATUS_CODE_SUCCESS == fields.Get(entry.first, schemaVersion))
            {
                ComponentSchemaVersion csv;
                csv.m_componentId   = static_cast<ComponentId>(componentIdVal);
                csv.m_schemaVersion = schemaVersion;
                m_schemaRegistry.push_back(csv);
            }
        }
        catch (const std::invalid_argument &) { /* non-numeric tag, skip */ }
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Geometry deserialisers — identical field names to BinaryFileReader
//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadSubDetector(const FieldMap &fields)
{
    if (GEOMETRY_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    PandoraApi::Geometry::SubDetector::Parameters *pParameters = m_pSubDetectorFactory->NewParameters();

    try
    {
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pSubDetectorFactory->Read(*pParameters, fields));

        pParameters->m_subDetectorName    = fields.GetOrDefault<std::string>("subDetectorName",    std::string());
        pParameters->m_subDetectorType    = fields.GetOrDefault<SubDetectorType>("subDetectorType",SUB_DETECTOR_OTHER);
        pParameters->m_innerRCoordinate   = fields.GetOrDefault<float>("innerRCoordinate",         0.f);
        pParameters->m_innerZCoordinate   = fields.GetOrDefault<float>("innerZCoordinate",         0.f);
        pParameters->m_innerPhiCoordinate = fields.GetOrDefault<float>("innerPhiCoordinate",       0.f);
        pParameters->m_innerSymmetryOrder = fields.GetOrDefault<unsigned int>("innerSymmetryOrder",0u);
        pParameters->m_outerRCoordinate   = fields.GetOrDefault<float>("outerRCoordinate",         0.f);
        pParameters->m_outerZCoordinate   = fields.GetOrDefault<float>("outerZCoordinate",         0.f);
        pParameters->m_outerPhiCoordinate = fields.GetOrDefault<float>("outerPhiCoordinate",       0.f);
        pParameters->m_outerSymmetryOrder = fields.GetOrDefault<unsigned int>("outerSymmetryOrder",0u);
        pParameters->m_isMirroredInZ      = fields.GetOrDefault<bool>("isMirroredInZ",             false);

        const unsigned int nLayers = fields.GetOrDefault<unsigned int>("nLayers", 0u);
        pParameters->m_nLayers = nLayers;

        for (unsigned int i = 0; i < nLayers; ++i)
        {
            const std::string prefix("layer" + std::to_string(i) + "_");
            PandoraApi::Geometry::LayerParameters layerParameters;
            layerParameters.m_closestDistanceToIp = fields.GetOrDefault<float>(prefix + "closestDistanceToIp", 0.f);
            layerParameters.m_nRadiationLengths   = fields.GetOrDefault<float>(prefix + "nRadiationLengths",   0.f);
            layerParameters.m_nInteractionLengths = fields.GetOrDefault<float>(prefix + "nInteractionLengths", 0.f);
            pParameters->m_layerParametersVector.push_back(layerParameters);
        }

        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=,
            PandoraApi::Geometry::SubDetector::Create(*m_pPandora, *pParameters, *m_pSubDetectorFactory));
        delete pParameters;
    }
    catch (StatusCodeException &e) { delete pParameters; return e.GetStatusCode(); }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadLArTPC(const FieldMap &fields)
{
    if (GEOMETRY_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    PandoraApi::Geometry::LArTPC::Parameters *pParameters = m_pLArTPCFactory->NewParameters();

    try
    {
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pLArTPCFactory->Read(*pParameters, fields));

        pParameters->m_larTPCVolumeId     = fields.GetOrDefault<unsigned int>("larTPCVolumeId",  0u);
        pParameters->m_centerX            = fields.GetOrDefault<float>("centerX",                0.f);
        pParameters->m_centerY            = fields.GetOrDefault<float>("centerY",                0.f);
        pParameters->m_centerZ            = fields.GetOrDefault<float>("centerZ",                0.f);
        pParameters->m_widthX             = fields.GetOrDefault<float>("widthX",                 0.f);
        pParameters->m_widthY             = fields.GetOrDefault<float>("widthY",                 0.f);
        pParameters->m_widthZ             = fields.GetOrDefault<float>("widthZ",                 0.f);
        pParameters->m_wirePitchU         = fields.GetOrDefault<float>("wirePitchU",             0.f);
        pParameters->m_wirePitchV         = fields.GetOrDefault<float>("wirePitchV",             0.f);
        pParameters->m_wirePitchW         = fields.GetOrDefault<float>("wirePitchW",             0.f);
        pParameters->m_wireAngleU         = fields.GetOrDefault<float>("wireAngleU",             0.f);
        pParameters->m_wireAngleV         = fields.GetOrDefault<float>("wireAngleV",             0.f);
        pParameters->m_wireAngleW         = fields.GetOrDefault<float>("wireAngleW",             0.f);
        pParameters->m_sigmaUVW           = fields.GetOrDefault<float>("sigmaUVW",               0.f);
        pParameters->m_isDriftInPositiveX = fields.GetOrDefault<bool>("isDriftInPositiveX",      false);

        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=,
            PandoraApi::Geometry::LArTPC::Create(*m_pPandora, *pParameters, *m_pLArTPCFactory));
        delete pParameters;
    }
    catch (StatusCodeException &e) { delete pParameters; return e.GetStatusCode(); }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadLineGap(const FieldMap &fields)
{
    if (GEOMETRY_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    PandoraApi::Geometry::LineGap::Parameters *pParameters = m_pLineGapFactory->NewParameters();

    try
    {
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pLineGapFactory->Read(*pParameters, fields));

        pParameters->m_lineGapType = fields.GetOrDefault<LineGapType>("lineGapType", TPC_WIRE_GAP_VIEW_U);
        pParameters->m_lineStartX  = fields.GetOrDefault<float>("lineStartX",        0.f);
        pParameters->m_lineEndX    = fields.GetOrDefault<float>("lineEndX",          0.f);
        pParameters->m_lineStartZ  = fields.GetOrDefault<float>("lineStartZ",        0.f);
        pParameters->m_lineEndZ    = fields.GetOrDefault<float>("lineEndZ",          0.f);

        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=,
            PandoraApi::Geometry::LineGap::Create(*m_pPandora, *pParameters, *m_pLineGapFactory));
        delete pParameters;
    }
    catch (StatusCodeException &e) { delete pParameters; return e.GetStatusCode(); }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadBoxGap(const FieldMap &fields)
{
    if (GEOMETRY_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    PandoraApi::Geometry::BoxGap::Parameters *pParameters = m_pBoxGapFactory->NewParameters();

    try
    {
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pBoxGapFactory->Read(*pParameters, fields));

        pParameters->m_vertex = fields.GetOrDefault<CartesianVector>("vertex", CartesianVector(0.f, 0.f, 0.f));
        pParameters->m_side1  = fields.GetOrDefault<CartesianVector>("side1",  CartesianVector(0.f, 0.f, 0.f));
        pParameters->m_side2  = fields.GetOrDefault<CartesianVector>("side2",  CartesianVector(0.f, 0.f, 0.f));
        pParameters->m_side3  = fields.GetOrDefault<CartesianVector>("side3",  CartesianVector(0.f, 0.f, 0.f));

        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=,
            PandoraApi::Geometry::BoxGap::Create(*m_pPandora, *pParameters, *m_pBoxGapFactory));
        delete pParameters;
    }
    catch (StatusCodeException &e) { delete pParameters; return e.GetStatusCode(); }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadConcentricGap(const FieldMap &fields)
{
    if (GEOMETRY_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    PandoraApi::Geometry::ConcentricGap::Parameters *pParameters = m_pConcentricGapFactory->NewParameters();

    try
    {
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pConcentricGapFactory->Read(*pParameters, fields));

        pParameters->m_minZCoordinate     = fields.GetOrDefault<float>("minZCoordinate",             0.f);
        pParameters->m_maxZCoordinate     = fields.GetOrDefault<float>("maxZCoordinate",             0.f);
        pParameters->m_innerRCoordinate   = fields.GetOrDefault<float>("innerRCoordinate",           0.f);
        pParameters->m_innerPhiCoordinate = fields.GetOrDefault<float>("innerPhiCoordinate",         0.f);
        pParameters->m_innerSymmetryOrder = fields.GetOrDefault<unsigned int>("innerSymmetryOrder",  0u);
        pParameters->m_outerRCoordinate   = fields.GetOrDefault<float>("outerRCoordinate",           0.f);
        pParameters->m_outerPhiCoordinate = fields.GetOrDefault<float>("outerPhiCoordinate",         0.f);
        pParameters->m_outerSymmetryOrder = fields.GetOrDefault<unsigned int>("outerSymmetryOrder",  0u);

        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=,
            PandoraApi::Geometry::ConcentricGap::Create(*m_pPandora, *pParameters, *m_pConcentricGapFactory));
        delete pParameters;
    }
    catch (StatusCodeException &e) { delete pParameters; return e.GetStatusCode(); }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Event deserialisers
//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadCaloHit(const FieldMap &fields)
{
    if (EVENT_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    PandoraApi::CaloHit::Parameters *pParameters = m_pCaloHitFactory->NewParameters();

    try
    {
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pCaloHitFactory->Read(*pParameters, fields));

        pParameters->m_cellGeometry            = fields.GetOrDefault<CellGeometry>("cellGeometry",           RECTANGULAR);
        pParameters->m_positionVector          = fields.GetOrDefault<CartesianVector>("positionVector",      CartesianVector(0.f,0.f,0.f));
        pParameters->m_expectedDirection       = fields.GetOrDefault<CartesianVector>("expectedDirection",   CartesianVector(0.f,0.f,0.f));
        pParameters->m_cellNormalVector        = fields.GetOrDefault<CartesianVector>("cellNormalVector",    CartesianVector(0.f,0.f,0.f));
        pParameters->m_cellThickness           = fields.GetOrDefault<float>("cellThickness",                 0.f);
        pParameters->m_nCellRadiationLengths   = fields.GetOrDefault<float>("nCellRadiationLengths",        0.f);
        pParameters->m_nCellInteractionLengths = fields.GetOrDefault<float>("nCellInteractionLengths",      0.f);
        pParameters->m_time                    = fields.GetOrDefault<float>("time",                          0.f);
        pParameters->m_inputEnergy             = fields.GetOrDefault<float>("inputEnergy",                   0.f);
        pParameters->m_mipEquivalentEnergy     = fields.GetOrDefault<float>("mipEquivalentEnergy",           0.f);
        pParameters->m_electromagneticEnergy   = fields.GetOrDefault<float>("electromagneticEnergy",        0.f);
        pParameters->m_hadronicEnergy          = fields.GetOrDefault<float>("hadronicEnergy",                0.f);
        pParameters->m_isDigital               = fields.GetOrDefault<bool>("isDigital",                      false);
        pParameters->m_hitType                 = fields.GetOrDefault<HitType>("hitType",                     ECAL);
        pParameters->m_hitRegion               = fields.GetOrDefault<HitRegion>("hitRegion",                 BARREL);
        pParameters->m_layer                   = fields.GetOrDefault<unsigned int>("layer",                  0u);
        pParameters->m_isInOuterSamplingLayer  = fields.GetOrDefault<bool>("isInOuterSamplingLayer",         false);
        pParameters->m_pParentAddress          = fields.GetOrDefault<const void *>("parentAddress",          nullptr);
        pParameters->m_cellSize0               = fields.GetOrDefault<float>("cellSize0",                     0.f);
        pParameters->m_cellSize1               = fields.GetOrDefault<float>("cellSize1",                     0.f);

        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=,
            PandoraApi::CaloHit::Create(*m_pPandora, *pParameters, *m_pCaloHitFactory));
        delete pParameters;
    }
    catch (StatusCodeException &e) { delete pParameters; return e.GetStatusCode(); }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadTrack(const FieldMap &fields)
{
    if (EVENT_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    PandoraApi::Track::Parameters *pParameters = m_pTrackFactory->NewParameters();

    try
    {
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pTrackFactory->Read(*pParameters, fields));

        pParameters->m_d0                      = fields.GetOrDefault<float>("d0",                             0.f);
        pParameters->m_z0                      = fields.GetOrDefault<float>("z0",                             0.f);
        pParameters->m_particleId              = fields.GetOrDefault<int>("particleId",                       0);
        pParameters->m_charge                  = fields.GetOrDefault<int>("charge",                           0);
        pParameters->m_mass                    = fields.GetOrDefault<float>("mass",                           0.f);
        pParameters->m_momentumAtDca           = fields.GetOrDefault<CartesianVector>("momentumAtDca",        CartesianVector(0.f,0.f,0.f));
        pParameters->m_trackStateAtStart       = fields.GetOrDefault<TrackState>("trackStateAtStart",         TrackState(0.f,0.f,0.f,0.f,0.f,0.f));
        pParameters->m_trackStateAtEnd         = fields.GetOrDefault<TrackState>("trackStateAtEnd",           TrackState(0.f,0.f,0.f,0.f,0.f,0.f));
        pParameters->m_trackStateAtCalorimeter = fields.GetOrDefault<TrackState>("trackStateAtCalorimeter",   TrackState(0.f,0.f,0.f,0.f,0.f,0.f));
        pParameters->m_timeAtCalorimeter       = fields.GetOrDefault<float>("timeAtCalorimeter",              0.f);
        pParameters->m_reachesCalorimeter      = fields.GetOrDefault<bool>("reachesCalorimeter",              false);
        pParameters->m_isProjectedToEndCap     = fields.GetOrDefault<bool>("isProjectedToEndCap",            false);
        pParameters->m_canFormPfo              = fields.GetOrDefault<bool>("canFormPfo",                      false);
        pParameters->m_canFormClusterlessPfo   = fields.GetOrDefault<bool>("canFormClusterlessPfo",          false);
        pParameters->m_pParentAddress          = fields.GetOrDefault<const void *>("parentAddress",           nullptr);

        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=,
            PandoraApi::Track::Create(*m_pPandora, *pParameters, *m_pTrackFactory));
        delete pParameters;
    }
    catch (StatusCodeException &e) { delete pParameters; return e.GetStatusCode(); }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadMCParticle(const FieldMap &fields)
{
    if (EVENT_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    PandoraApi::MCParticle::Parameters *pParameters = m_pMCParticleFactory->NewParameters();

    try
    {
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pMCParticleFactory->Read(*pParameters, fields));

        pParameters->m_energy         = fields.GetOrDefault<float>("energy",                               0.f);
        pParameters->m_momentum       = fields.GetOrDefault<CartesianVector>("momentum",                   CartesianVector(0.f,0.f,0.f));
        pParameters->m_vertex         = fields.GetOrDefault<CartesianVector>("vertex",                     CartesianVector(0.f,0.f,0.f));
        pParameters->m_endpoint       = fields.GetOrDefault<CartesianVector>("endpoint",                   CartesianVector(0.f,0.f,0.f));
        pParameters->m_particleId     = fields.GetOrDefault<int>("particleId",                             -std::numeric_limits<int>::max());
        pParameters->m_mcParticleType = fields.GetOrDefault<MCParticleType>("mcParticleType",              MC_3D);
        pParameters->m_pParentAddress = fields.GetOrDefault<const void *>("uid",                           nullptr);

        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=,
            PandoraApi::MCParticle::Create(*m_pPandora, *pParameters, *m_pMCParticleFactory));
        delete pParameters;
    }
    catch (StatusCodeException &e) { delete pParameters; return e.GetStatusCode(); }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadRelationship(const FieldMap &fields)
{
    if (EVENT_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    const RelationshipId relationshipId = fields.GetOrDefault<RelationshipId>("relationshipId", UNKNOWN_RELATIONSHIP);
    const uintptr_t addr1 = fields.GetOrDefault<uintptr_t>("address1", 0u);
    const uintptr_t addr2 = fields.GetOrDefault<uintptr_t>("address2", 0u);
    const float     weight= fields.GetOrDefault<float>("weight",        1.f);

    const void *const address1 = reinterpret_cast<const void *>(addr1);
    const void *const address2 = reinterpret_cast<const void *>(addr2);

    switch (relationshipId)
    {
        case CALO_HIT_TO_MC_RELATIONSHIP:
            return PandoraApi::SetCaloHitToMCParticleRelationship(*m_pPandora, address1, address2, weight);
        case TRACK_TO_MC_RELATIONSHIP:
            return PandoraApi::SetTrackToMCParticleRelationship(*m_pPandora, address1, address2, weight);
        case MC_PARENT_DAUGHTER_RELATIONSHIP:
            return PandoraApi::SetMCParentDaughterRelationship(*m_pPandora, address1, address2);
        case TRACK_PARENT_DAUGHTER_RELATIONSHIP:
            return PandoraApi::SetTrackParentDaughterRelationship(*m_pPandora, address1, address2);
        case TRACK_SIBLING_RELATIONSHIP:
            return PandoraApi::SetTrackSiblingRelationship(*m_pPandora, address1, address2);
        default:
            return STATUS_CODE_FAILURE;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode XmlFileReader::ReadEventInformation(const FieldMap &fields)
{
    if (EVENT_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    const unsigned int run    = fields.GetOrDefault<unsigned int>("run",    0u);
    const unsigned int subrun = fields.GetOrDefault<unsigned int>("subrun", 0u);
    const unsigned int event  = fields.GetOrDefault<unsigned int>("event",  0u);

    return PandoraApi::SetEventInformation(*m_pPandora, run, subrun, event);
}

} // namespace pandora
