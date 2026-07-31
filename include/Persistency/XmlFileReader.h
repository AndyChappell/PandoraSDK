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

#include "Pandora/Pandora.h"

#include "Persistency/FileReader.h"
#include "Persistency/Persistency.h"

namespace pandora
{

/**
 *  @brief  XmlFileReader
 *
 *  Reads Pandora objects from an XML file written by XmlFileWriter.
 *
 *  Migration registration
 *  ----------------------
 *  Identical API to BinaryFileReader::RegisterMigration.
 */
class XmlFileReader : public FileReader
{
public:
    XmlFileReader(const pandora::Pandora &pandora, const std::string &fileName);
    ~XmlFileReader();

    typedef std::function<void(FieldMap &)> MigrationFn;

    void RegisterMigration(const ComponentId componentId, const unsigned int fromVersion,
        const unsigned int toVersion, MigrationFn fn);

private:
    StatusCode ReadHeader();
    StatusCode GoToNextContainer();
    ContainerId GetNextContainerId();
    StatusCode GoToGeometry(const unsigned int geometryNumber);
    StatusCode GoToEvent(const unsigned int eventNumber);
    StatusCode ReadNextGlobalHeaderComponent();
    StatusCode ReadNextGeometryComponent();
    StatusCode ReadNextEventComponent();

    StatusCode ReadNextComponent([[maybe_unused]] const ContainerId expectedContainer);
    StatusCode ReadComponentFields(unsigned int &schemaVersion, FieldMap &fields) const;
    void ApplyMigrations(const ComponentId componentId,
        const unsigned int fileSchemaVersion, FieldMap &fields) const;

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

    TiXmlDocument *m_pXmlDocument;
    TiXmlNode     *m_pContainerXmlNode;
    TiXmlElement  *m_pCurrentXmlElement;
    bool           m_isAtFileStart;

    std::unordered_map<MigrationKey, MigrationFn, MigrationKeyHash> m_migrations;
};

} // namespace pandora

#endif // #ifndef PANDORA_XML_FILE_READER_H
