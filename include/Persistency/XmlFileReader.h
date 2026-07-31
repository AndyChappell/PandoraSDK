/**
 *  @file   PandoraSDK/include/Persistency/XmlFileReader.h
 *
 *  @brief  Header file for the xml file reader class.
 *
 *  $Log: $
 */
#ifndef PANDORA_XML_FILE_READER_H
#define PANDORA_XML_FILE_READER_H 1

#include "Helpers/XmlHelper.h"

#include "Objects/CartesianVector.h"
#include "Objects/TrackState.h"

#include "Pandora/Pandora.h"

#include "Persistency/FileReader.h"
#include "Persistency/Persistency.h"   // FieldMap

namespace pandora
{

/**
 *  @brief  XmlFileReader
 *
 *  Reads Pandora objects from an XML file written by XmlFileWriter. The file
 *  uses the same tagged-field, per-component-versioned structure as the binary
 *  format; each component is an XML element with a schemaVersion attribute and
 *  child elements whose names are the field tags.
 *
 *  Dispatch and migration
 *  ----------------------
 *  ReadNextGlobalHeaderComponent / ReadNextGeometryComponent /
 *  ReadNextEventComponent all call ReadNextComponent(), which:
 *    1. Advances m_pCurrentXmlElement to the next sibling.
 *    2. Reads all child elements into a FieldMap.
 *    3. Applies any registered migration chain.
 *    4. Dispatches to the typed deserialiser by element name.
 *
 *  Unknown element names are logged and skipped without error, providing
 *  forward compatibility identical to the binary reader.
 *
 *  Migration registration
 *  ----------------------
 *  Identical API to BinaryFileReader::RegisterMigration. The same migration
 *  functions work for both readers since they operate on FieldMap, not on
 *  the file format.
 *
 *  ReadVariable
 *  ------------
 *  The keyed ReadVariable<T>(xmlKey, value) template is kept public for
 *  backward compatibility with legacy ObjectFactory::Read implementations.
 *  New factory code should override Read(Parameters &, const FieldMap &).
 */
class XmlFileReader : public FileReader
{
public:
    /**
     *  @brief  Constructor
     *
     *  @param  pandora   the pandora instance
     *  @param  fileName  the name of the file containing the pandora objects
     */
    XmlFileReader(const pandora::Pandora &pandora, const std::string &fileName);

    /**
     *  @brief  Destructor
     */
    ~XmlFileReader();

    typedef std::function<void(FieldMap &)> MigrationFn;

    /**
     *  @brief  Register a schema migration for a component type.
     *          Identical contract to BinaryFileReader::RegisterMigration.
     */
    void RegisterMigration(const ComponentId componentId, const unsigned int fromVersion,
        const unsigned int toVersion, MigrationFn fn);

    /**
     *  @brief  Read a typed value from the current XML element by key.
     *          Retained for backward compatibility with legacy factory Read methods.
     */
    template <typename T>
    StatusCode ReadVariable(const std::string &xmlKey, T &t);

private:
    // -----------------------------------------------------------------------
    // FileReader pure-virtual interface
    // -----------------------------------------------------------------------
    StatusCode ReadHeader();
    StatusCode GoToNextContainer();
    ContainerId GetNextContainerId();
    StatusCode GoToGeometry(const unsigned int geometryNumber);
    StatusCode GoToEvent(const unsigned int eventNumber);
    StatusCode ReadNextGlobalHeaderComponent();
    StatusCode ReadNextGeometryComponent();
    StatusCode ReadNextEventComponent();

    // -----------------------------------------------------------------------
    // Unified component read path
    // -----------------------------------------------------------------------

    /**
     *  @brief  Advance to the next component element, populate a FieldMap from
     *          its children, apply migrations, and dispatch to the typed
     *          deserialiser.
     *
     *  @param  expectedContainer  guard: checked against m_containerId
     */
    StatusCode ReadNextComponent([[maybe_unused]] const ContainerId expectedContainer);

    /**
     *  @brief  Populate a FieldMap by reading all child elements of
     *          m_pCurrentXmlElement. The schemaVersion is read from the
     *          "schemaVersion" attribute.
     *
     *  @param  schemaVersion  receives the attribute value (0 if absent)
     *  @param  fields         receives all child element (name, text) pairs
     */
    StatusCode ReadComponentFields(unsigned int &schemaVersion, FieldMap &fields) const;

    /**
     *  @brief  Apply the registered migration chain for componentId from
     *          fileSchemaVersion up to current.
     */
    void ApplyMigrations(const ComponentId componentId,
        const unsigned int fileSchemaVersion, FieldMap &fields) const;

    // -----------------------------------------------------------------------
    // Typed deserialisers — receive a migrated FieldMap
    // -----------------------------------------------------------------------
    StatusCode ReadVersion(const FieldMap &fields);
    StatusCode ReadMetadata(const FieldMap &fields);
    StatusCode ReadSchemaRegistry(const FieldMap &fields);

    StatusCode ReadSubDetector(const FieldMap &fields);
    StatusCode ReadLArTPC(const FieldMap &fields);
    StatusCode ReadLineGap(const FieldMap &fields);
    StatusCode ReadBoxGap(const FieldMap &fields);
    StatusCode ReadConcentricGap(const FieldMap &fields);

    StatusCode ReadCaloHit(const FieldMap &fields);
    StatusCode ReadTrack(const FieldMap &fields);
    StatusCode ReadMCParticle(const FieldMap &fields);
    StatusCode ReadRelationship(const FieldMap &fields);
    StatusCode ReadEventInformation(const FieldMap &fields);

    // -----------------------------------------------------------------------
    // Migration table (same key type as BinaryFileReader)
    // -----------------------------------------------------------------------
    struct MigrationKey
    {
        ComponentId  m_componentId;
        unsigned int m_fromVersion;
        bool operator==(const MigrationKey &rhs) const
        {
            return m_componentId == rhs.m_componentId && m_fromVersion == rhs.m_fromVersion;
        }
    };

    struct MigrationKeyHash
    {
        std::size_t operator()(const MigrationKey &k) const
        {
            return std::hash<unsigned int>()(static_cast<unsigned int>(k.m_componentId))
                ^ (std::hash<unsigned int>()(k.m_fromVersion) << 16);
        }
    };

    // -----------------------------------------------------------------------
    // Data members
    // -----------------------------------------------------------------------
    TiXmlDocument *m_pXmlDocument;      ///< The XML document (owned)
    TiXmlNode     *m_pContainerXmlNode; ///< Current container node (Header/Geometry/Event)
    TiXmlElement  *m_pCurrentXmlElement;///< Current component element
    bool           m_isAtFileStart;     ///< Whether the reader is at file start

    std::unordered_map<MigrationKey, MigrationFn, MigrationKeyHash> m_migrations;
};

//------------------------------------------------------------------------------------------------------------------------------------------
// ReadVariable — unchanged in behaviour from the original
//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
inline StatusCode XmlFileReader::ReadVariable(const std::string &xmlKey, T &t)
{
    if (!m_pCurrentXmlElement)
        return STATUS_CODE_FAILURE;

    return XmlHelper::ReadValue(TiXmlHandle(m_pCurrentXmlElement), xmlKey, t);
}

template <>
inline StatusCode XmlFileReader::ReadVariable(const std::string &xmlKey, IntVector &t)
{
    if (!m_pCurrentXmlElement)
        return STATUS_CODE_FAILURE;

    return XmlHelper::ReadVectorOfValues(TiXmlHandle(m_pCurrentXmlElement), xmlKey, t);
}

template <>
inline StatusCode XmlFileReader::ReadVariable(const std::string &xmlKey, FloatVector &t)
{
    if (!m_pCurrentXmlElement)
        return STATUS_CODE_FAILURE;

    return XmlHelper::ReadVectorOfValues(TiXmlHandle(m_pCurrentXmlElement), xmlKey, t);
}

} // namespace pandora

#endif // #ifndef PANDORA_XML_FILE_READER_H
